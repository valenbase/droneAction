#include "TelemetryService.h"

#include <QMetaObject>
#include <QPointer>
#include <QtMath>
#include <cmath>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <sstream>
#include <thread>

#ifdef DRONEACTION_WITH_MAVSDK
#include <mavsdk/component_type.h>
#include <mavsdk/connection_result.h>
#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/offboard/offboard.h>
#include <mavsdk/plugins/telemetry/telemetry.h>
#include <mavsdk/system.h>
#endif

namespace {
double finiteOr(double value, double fallback)
{
    return std::isfinite(value) ? value : fallback;
}

float finiteOr(float value, float fallback)
{
    return std::isfinite(value) ? value : fallback;
}

QString normalizedMavsdkUrl(const QString &url)
{
    QString value = url.trimmed();
    if (value.startsWith(QStringLiteral("udp://:"))) {
        return QStringLiteral("udpin://0.0.0.0:%1").arg(value.mid(QStringLiteral("udp://:").size()));
    }
    if (value.startsWith(QStringLiteral("udp://"))) {
        return QStringLiteral("udpout://%1").arg(value.mid(QStringLiteral("udp://").size()));
    }
    if (value.startsWith(QStringLiteral("tcp://"))) {
        return QStringLiteral("tcpout://%1").arg(value.mid(QStringLiteral("tcp://").size()));
    }
    return value;
}

#ifdef DRONEACTION_WITH_MAVSDK
template <typename Result>
QString resultToString(Result result)
{
    std::ostringstream stream;
    stream << result;
    return QString::fromStdString(stream.str());
}

QString flightModeToString(mavsdk::Telemetry::FlightMode mode)
{
    switch (mode) {
    case mavsdk::Telemetry::FlightMode::Ready:
        return QStringLiteral("Ready");
    case mavsdk::Telemetry::FlightMode::Takeoff:
        return QStringLiteral("Takeoff");
    case mavsdk::Telemetry::FlightMode::Hold:
        return QStringLiteral("Hold");
    case mavsdk::Telemetry::FlightMode::Mission:
        return QStringLiteral("Mission");
    case mavsdk::Telemetry::FlightMode::ReturnToLaunch:
        return QStringLiteral("Return to launch");
    case mavsdk::Telemetry::FlightMode::Land:
        return QStringLiteral("Land");
    case mavsdk::Telemetry::FlightMode::Offboard:
        return QStringLiteral("Offboard");
    case mavsdk::Telemetry::FlightMode::FollowMe:
        return QStringLiteral("Follow me");
    case mavsdk::Telemetry::FlightMode::Manual:
        return QStringLiteral("Manual");
    case mavsdk::Telemetry::FlightMode::Altctl:
        return QStringLiteral("Altitude control");
    case mavsdk::Telemetry::FlightMode::Posctl:
        return QStringLiteral("Position control");
    case mavsdk::Telemetry::FlightMode::Acro:
        return QStringLiteral("Acro");
    case mavsdk::Telemetry::FlightMode::Stabilized:
        return QStringLiteral("Stabilized");
    case mavsdk::Telemetry::FlightMode::Rattitude:
        return QStringLiteral("Rattitude");
    case mavsdk::Telemetry::FlightMode::Unknown:
        break;
    }
    return QStringLiteral("Unknown");
}

mavsdk::Offboard::VelocityBodyYawspeed velocitySetpoint(const DroneCommand &command)
{
    mavsdk::Offboard::VelocityBodyYawspeed setpoint {};
    const float value = static_cast<float>(command.value);
    switch (command.kind) {
    case CommandKind::Forward:
        setpoint.forward_m_s = value;
        break;
    case CommandKind::Backward:
        setpoint.forward_m_s = -value;
        break;
    case CommandKind::Right:
        setpoint.right_m_s = value;
        break;
    case CommandKind::Left:
        setpoint.right_m_s = -value;
        break;
    case CommandKind::Up:
        setpoint.down_m_s = -value;
        break;
    case CommandKind::Down:
        setpoint.down_m_s = value;
        break;
    case CommandKind::YawRight:
        setpoint.yawspeed_deg_s = value;
        break;
    case CommandKind::YawLeft:
        setpoint.yawspeed_deg_s = -value;
        break;
    case CommandKind::Hold:
        break;
    default:
        break;
    }
    return setpoint;
}

bool waitForOffboardConfirmation(mavsdk::Telemetry &telemetry,
                                 mavsdk::Offboard &offboard,
                                 const mavsdk::Offboard::VelocityBodyYawspeed &holdSetpoint,
                                 std::chrono::milliseconds timeout)
{
    struct ModeWaitState {
        std::mutex mutex;
        std::condition_variable changed;
        bool offboardFlightModeSeen = false;
        bool modeUpdateSeen = false;
    };
    auto state = std::make_shared<ModeWaitState>();

    const auto handle = telemetry.subscribe_flight_mode([state](mavsdk::Telemetry::FlightMode mode) {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->modeUpdateSeen = true;
        state->offboardFlightModeSeen = mode == mavsdk::Telemetry::FlightMode::Offboard;
        state->changed.notify_all();
    });

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    bool confirmed = false;
    while (std::chrono::steady_clock::now() < deadline) {
        offboard.set_velocity_body(holdSetpoint);
        if (offboard.is_active()) {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->modeUpdateSeen || state->offboardFlightModeSeen) {
                confirmed = true;
                break;
            }
        }

        std::unique_lock<std::mutex> lock(state->mutex);
        state->changed.wait_for(lock, std::chrono::milliseconds(100));
    }

    telemetry.unsubscribe_flight_mode(handle);
    return confirmed;
}
#endif
}

struct TelemetryService::MavsdkRuntime {
#ifdef DRONEACTION_WITH_MAVSDK
    mavsdk::Mavsdk mavsdk {mavsdk::Mavsdk::Configuration{mavsdk::ComponentType::GroundStation}};
    std::shared_ptr<mavsdk::System> system;
    std::shared_ptr<mavsdk::Telemetry> telemetry;
    std::mutex mutex;
#endif
};

TelemetryService::TelemetryService(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(500);
    connect(&m_timer, &QTimer::timeout, this, &TelemetryService::updateSimulation);
}

TelemetryService::~TelemetryService()
{
    resetMavsdk();
}

void TelemetryService::connectToEndpoint(const AppConfig &config)
{
    resetMavsdk();
    m_config = config;
    m_endpoint = config.mavlinkUrl;
    m_runMode = config.runMode;
    m_tick = 0;
    if (m_runMode == AppRunMode::Px4Sitl) {
        m_homeLatitudeDeg = 47.397742;
        m_homeLongitudeDeg = 8.545594;
    } else {
        m_homeLatitudeDeg = 29.56301;
        m_homeLongitudeDeg = 106.55156;
    }

    m_snapshot = {};
    m_snapshot.connected = false;
    m_snapshot.armed = false;
    m_snapshot.mode = AppConfig::runModeDisplayName(m_runMode);
    m_snapshot.latitudeDeg = m_homeLatitudeDeg;
    m_snapshot.longitudeDeg = m_homeLongitudeDeg;
    m_snapshot.batteryPercent = 96.0;
    m_snapshot.gpsSatellites = 14;
    m_timer.stop();
    startMavsdk(config);
#ifndef DRONEACTION_WITH_MAVSDK
    m_snapshot.connected = true;
    m_timer.start();
#endif
    emit statusMessageChanged(m_runMode == AppRunMode::Px4Sitl
                                  ? QStringLiteral("PX4 SITL telemetry configured")
                                  : QStringLiteral("Aircraft telemetry configured"));
    emit snapshotChanged(m_snapshot);
}

void TelemetryService::disconnectFromEndpoint()
{
    m_timer.stop();
    resetMavsdk();
    m_snapshot = {};
    m_snapshot.mode = QStringLiteral("offline");
    emit statusMessageChanged(QStringLiteral("Telemetry offline"));
    emit snapshotChanged(m_snapshot);
}

CommandExecutionResult TelemetryService::executeCommand(const DroneCommand &command)
{
    if (!m_config.mavlinkCommandExecutionEnabled) {
        return {QStringLiteral("disabled"),
                m_config.runMode == AppRunMode::RealAircraft
                    ? QStringLiteral("MAVLink command execution is disabled for real-aircraft mode")
                    : QStringLiteral("MAVLink command execution is disabled")};
    }

#ifndef DRONEACTION_WITH_MAVSDK
    Q_UNUSED(command)
    return {QStringLiteral("unavailable"), QStringLiteral("application was built without MAVSDK")};
#else
    if (!m_mavsdk) {
        return {QStringLiteral("not_ready"), QStringLiteral("MAVSDK connection is not initialized")};
    }

    std::shared_ptr<mavsdk::System> system;
    std::shared_ptr<mavsdk::Telemetry> telemetry;
    {
        std::lock_guard<std::mutex> lock(m_mavsdk->mutex);
        system = m_mavsdk->system;
        telemetry = m_mavsdk->telemetry;
    }
    if (!system || !system->is_connected()) {
        return {QStringLiteral("not_ready"), QStringLiteral("MAVSDK autopilot is not connected")};
    }
    if (CommandParser::isMovement(command.kind) && !telemetry) {
        return {QStringLiteral("not_ready"), QStringLiteral("MAVSDK telemetry is not ready for offboard confirmation")};
    }

    QPointer<TelemetryService> self(this);
    std::thread([self, system, telemetry, command]() {
        mavsdk::Action action(system);
        const auto report = [self](const QString &message) {
            if (!self) {
                return;
            }
            QMetaObject::invokeMethod(self, [self, message]() {
                if (self) {
                    emit self->statusMessageChanged(message);
                }
            }, Qt::QueuedConnection);
        };

        if (CommandParser::isMovement(command.kind)) {
            mavsdk::Offboard offboard(system);
            const auto setpoint = velocitySetpoint(command);
            const mavsdk::Offboard::VelocityBodyYawspeed zero {};
            report(QStringLiteral("MAVSDK offboard switch requested for %1").arg(CommandParser::kindToString(command.kind)));
            for (int i = 0; i < 5; ++i) {
                const auto seedResult = offboard.set_velocity_body(zero);
                if (seedResult != mavsdk::Offboard::Result::Success) {
                    report(QStringLiteral("MAVSDK zero setpoint failed before offboard start: %1").arg(resultToString(seedResult)));
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            const auto startResult = offboard.start();
            if (startResult != mavsdk::Offboard::Result::Success) {
                report(QStringLiteral("MAVSDK offboard start failed: %1").arg(resultToString(startResult)));
                return;
            }
            if (!waitForOffboardConfirmation(*telemetry, offboard, zero, std::chrono::milliseconds(2500))) {
                offboard.set_velocity_body(zero);
                offboard.stop();
                report(QStringLiteral("MAVSDK offboard mode confirmation timed out; movement aborted"));
                return;
            }
            report(QStringLiteral("MAVSDK offboard mode confirmed; executing %1").arg(CommandParser::kindToString(command.kind)));

            const auto deadline = std::chrono::steady_clock::now()
                + std::chrono::milliseconds(command.durationMs > 0 ? command.durationMs : 1000);
            while (std::chrono::steady_clock::now() < deadline) {
                const auto setResult = offboard.set_velocity_body(setpoint);
                if (setResult != mavsdk::Offboard::Result::Success) {
                    report(QStringLiteral("MAVSDK velocity setpoint failed: %1").arg(resultToString(setResult)));
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            offboard.set_velocity_body(zero);
            if (command.kind == CommandKind::Hold) {
                action.hold();
            } else {
                offboard.stop();
            }
            report(QStringLiteral("MAVSDK command completed: %1").arg(CommandParser::kindToString(command.kind)));
            return;
        }

        if (command.kind == CommandKind::Takeoff) {
            if (command.hasValue && command.value > 0.0) {
                action.set_takeoff_altitude(static_cast<float>(command.value));
            }
            const auto armResult = action.arm();
            if (armResult != mavsdk::Action::Result::Success) {
                report(QStringLiteral("MAVSDK arm failed: %1").arg(resultToString(armResult)));
                return;
            }
            const auto takeoffResult = action.takeoff();
            report(QStringLiteral("MAVSDK takeoff: %1").arg(resultToString(takeoffResult)));
            return;
        }

        if (command.kind == CommandKind::Land) {
            const auto result = action.land();
            report(QStringLiteral("MAVSDK land: %1").arg(resultToString(result)));
            return;
        }

        if (command.kind == CommandKind::ReturnToLaunch) {
            const auto result = action.return_to_launch();
            report(QStringLiteral("MAVSDK RTL: %1").arg(resultToString(result)));
            return;
        }

        report(QStringLiteral("MAVSDK command ignored: %1").arg(CommandParser::kindToString(command.kind)));
    }).detach();

    return {QStringLiteral("queued"), QStringLiteral("command queued for MAVSDK execution")};
#endif
}

TelemetrySnapshot TelemetryService::snapshot() const
{
    return m_snapshot;
}

QString TelemetryService::endpoint() const
{
    return m_endpoint;
}

void TelemetryService::startMavsdk(const AppConfig &config)
{
#ifndef DRONEACTION_WITH_MAVSDK
    Q_UNUSED(config)
    emit statusMessageChanged(QStringLiteral("MAVSDK support is not enabled; using simulated telemetry"));
#else
    if (config.mavlinkUrl.trimmed().isEmpty()) {
        emit statusMessageChanged(QStringLiteral("MAVSDK endpoint is empty"));
        return;
    }

    m_mavsdk = std::make_shared<MavsdkRuntime>();
    const QString mavsdkUrl = normalizedMavsdkUrl(config.mavlinkUrl);
    const auto connectionResult = m_mavsdk->mavsdk.add_any_connection(mavsdkUrl.toStdString());
    if (connectionResult != mavsdk::ConnectionResult::Success) {
        emit statusMessageChanged(QStringLiteral("MAVSDK connection failed: %1").arg(resultToString(connectionResult)));
        return;
    }

    QPointer<TelemetryService> self(this);
    auto runtime = m_mavsdk;
    std::thread([self, runtime]() {
        auto system = runtime->mavsdk.first_autopilot(5.0);
        if (!self) {
            return;
        }
        if (!system) {
            QMetaObject::invokeMethod(self, [self]() {
                if (self) {
                    emit self->statusMessageChanged(QStringLiteral("MAVSDK waiting for autopilot heartbeat"));
                }
            }, Qt::QueuedConnection);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(runtime->mutex);
            runtime->system = system.value();
            runtime->telemetry = std::make_shared<mavsdk::Telemetry>(runtime->system);
        }

        QMetaObject::invokeMethod(self, [self]() {
            if (self) {
                self->applySnapshotUpdate([](TelemetrySnapshot *snapshot) {
                    snapshot->connected = true;
                    snapshot->mode = QStringLiteral("MAVSDK connected");
                });
                emit self->statusMessageChanged(QStringLiteral("MAVSDK autopilot connected"));
            }
        }, Qt::QueuedConnection);

        mavsdk::Telemetry *telemetry = nullptr;
        {
            std::lock_guard<std::mutex> lock(runtime->mutex);
            telemetry = runtime->telemetry.get();
        }
        if (!telemetry) {
            return;
        }

        telemetry->set_rate_position(5.0);
        telemetry->set_rate_attitude_euler(10.0);
        telemetry->set_rate_velocity_ned(5.0);
        telemetry->set_rate_battery(1.0);
        telemetry->set_rate_gps_info(1.0);

        telemetry->subscribe_flight_mode([self](mavsdk::Telemetry::FlightMode mode) {
            if (!self) {
                return;
            }
            QMetaObject::invokeMethod(self, [self, mode]() {
                if (!self) {
                    return;
                }
                self->applySnapshotUpdate([mode](TelemetrySnapshot *snapshot) {
                    snapshot->connected = true;
                    snapshot->mode = flightModeToString(mode);
                });
            }, Qt::QueuedConnection);
        });

        telemetry->subscribe_position([self](mavsdk::Telemetry::Position position) {
            if (!self) {
                return;
            }
            QMetaObject::invokeMethod(self, [self, position]() {
                if (!self) {
                    return;
                }
                self->applySnapshotUpdate([position](TelemetrySnapshot *snapshot) {
                    snapshot->connected = true;
                    snapshot->latitudeDeg = finiteOr(position.latitude_deg, snapshot->latitudeDeg);
                    snapshot->longitudeDeg = finiteOr(position.longitude_deg, snapshot->longitudeDeg);
                    snapshot->altitudeM = finiteOr(static_cast<double>(position.relative_altitude_m), snapshot->altitudeM);
                });
            }, Qt::QueuedConnection);
        });

        telemetry->subscribe_attitude_euler([self](mavsdk::Telemetry::EulerAngle attitude) {
            if (!self) {
                return;
            }
            QMetaObject::invokeMethod(self, [self, attitude]() {
                if (!self) {
                    return;
                }
                self->applySnapshotUpdate([attitude](TelemetrySnapshot *snapshot) {
                    snapshot->rollDeg = finiteOr(attitude.roll_deg, static_cast<float>(snapshot->rollDeg));
                    snapshot->pitchDeg = finiteOr(attitude.pitch_deg, static_cast<float>(snapshot->pitchDeg));
                    snapshot->headingDeg = finiteOr(attitude.yaw_deg, static_cast<float>(snapshot->headingDeg));
                });
            }, Qt::QueuedConnection);
        });

        telemetry->subscribe_velocity_ned([self](mavsdk::Telemetry::VelocityNed velocity) {
            if (!self) {
                return;
            }
            QMetaObject::invokeMethod(self, [self, velocity]() {
                if (!self) {
                    return;
                }
                self->applySnapshotUpdate([velocity](TelemetrySnapshot *snapshot) {
                    const double north = velocity.north_m_s;
                    const double east = velocity.east_m_s;
                    snapshot->groundSpeedMps = std::sqrt(north * north + east * east);
                    snapshot->verticalSpeedMps = -velocity.down_m_s;
                });
            }, Qt::QueuedConnection);
        });

        telemetry->subscribe_battery([self](mavsdk::Telemetry::Battery battery) {
            if (!self) {
                return;
            }
            QMetaObject::invokeMethod(self, [self, battery]() {
                if (!self) {
                    return;
                }
                self->applySnapshotUpdate([battery](TelemetrySnapshot *snapshot) {
                    snapshot->batteryPercent = finiteOr(battery.remaining_percent, static_cast<float>(snapshot->batteryPercent));
                });
            }, Qt::QueuedConnection);
        });

        telemetry->subscribe_gps_info([self](mavsdk::Telemetry::GpsInfo gps) {
            if (!self) {
                return;
            }
            QMetaObject::invokeMethod(self, [self, gps]() {
                if (!self) {
                    return;
                }
                self->applySnapshotUpdate([gps](TelemetrySnapshot *snapshot) {
                    snapshot->gpsSatellites = gps.num_satellites;
                });
            }, Qt::QueuedConnection);
        });

        telemetry->subscribe_armed([self](bool armed) {
            if (!self) {
                return;
            }
            QMetaObject::invokeMethod(self, [self, armed]() {
                if (!self) {
                    return;
                }
                self->applySnapshotUpdate([armed](TelemetrySnapshot *snapshot) {
                    snapshot->armed = armed;
                });
            }, Qt::QueuedConnection);
        });
    }).detach();
#endif
}

void TelemetryService::resetMavsdk()
{
#ifdef DRONEACTION_WITH_MAVSDK
    m_mavsdk.reset();
#endif
}

void TelemetryService::applySnapshotUpdate(const std::function<void(TelemetrySnapshot *)> &update)
{
    update(&m_snapshot);
    emit snapshotChanged(m_snapshot);
}

void TelemetryService::updateSimulation()
{
    ++m_tick;
    const double t = static_cast<double>(m_tick) / 10.0;
    m_snapshot.altitudeM = 24.0 + 2.5 * qSin(t);
    m_snapshot.groundSpeedMps = 5.5 + 1.2 * qCos(t / 2.0);
    m_snapshot.verticalSpeedMps = 0.25 * qSin(t * 1.7);
    m_snapshot.rollDeg = 16.0 * qSin(t * 0.8);
    m_snapshot.pitchDeg = 7.5 * qSin(t * 1.1);
    m_snapshot.headingDeg = std::fmod(m_snapshot.headingDeg + 2.0, 360.0);
    m_snapshot.latitudeDeg = m_homeLatitudeDeg + 0.0012 * qSin(t / 3.0);
    m_snapshot.longitudeDeg = m_homeLongitudeDeg + 0.0012 * qCos(t / 3.0);
    m_snapshot.batteryPercent = qMax(15.0, m_snapshot.batteryPercent - 0.02);
    emit snapshotChanged(m_snapshot);
}
