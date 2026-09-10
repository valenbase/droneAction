#include "TelemetryService.h"

#include "ArduPilotControl.h"
#include "ControlWatchdog.h"

#include <QMetaObject>
#include <QPointer>
#include <QtMath>
#include <atomic>
#include <cmath>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <thread>
#include <utility>

#ifdef DRONEACTION_WITH_MAVSDK
#include <mavsdk/component_type.h>
#include <mavsdk/connection_result.h>
#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/mavlink_passthrough/mavlink_passthrough.h>
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

QString publicDisplayText(QString message)
{
    message.replace(QStringLiteral("PX4"), QStringLiteral("aircraft"), Qt::CaseInsensitive);
    message.replace(QStringLiteral("MAVLink"), QStringLiteral("vehicle link"), Qt::CaseInsensitive);
    message.replace(QStringLiteral("MAVSDK"), QStringLiteral("command link"), Qt::CaseInsensitive);
    message.replace(QStringLiteral("Offboard"), QStringLiteral("guided control"), Qt::CaseInsensitive);
    return message;
}

#ifdef DRONEACTION_WITH_MAVSDK
// PX4 custom_mode stores the main mode in bits 16-23; POSCTL is main mode 3.
constexpr std::uint32_t Px4PositionCustomMode = 3U << 16U;
constexpr std::uint32_t Px4OffboardCustomMode = 6U << 16U;

template <typename Result>
QString resultToString(Result result)
{
    std::ostringstream stream;
    stream << result;
    return QString::fromStdString(stream.str());
}

mavsdk::Mavsdk::Configuration groundStationConfiguration()
{
    mavsdk::Mavsdk::Configuration configuration {mavsdk::ComponentType::GroundStation};
    configuration.set_system_id(255);
    configuration.set_component_id(MAV_COMP_ID_ONBOARD_COMPUTER);
    return configuration;
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
        return QStringLiteral("Guided control");
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

QString statusTextTypeToString(mavsdk::Telemetry::StatusTextType type)
{
    switch (type) {
    case mavsdk::Telemetry::StatusTextType::Debug:
        return QStringLiteral("debug");
    case mavsdk::Telemetry::StatusTextType::Info:
        return QStringLiteral("info");
    case mavsdk::Telemetry::StatusTextType::Notice:
        return QStringLiteral("notice");
    case mavsdk::Telemetry::StatusTextType::Warning:
        return QStringLiteral("warning");
    case mavsdk::Telemetry::StatusTextType::Error:
        return QStringLiteral("error");
    case mavsdk::Telemetry::StatusTextType::Critical:
        return QStringLiteral("critical");
    case mavsdk::Telemetry::StatusTextType::Alert:
        return QStringLiteral("alert");
    case mavsdk::Telemetry::StatusTextType::Emergency:
        return QStringLiteral("emergency");
    }
    return QStringLiteral("unknown");
}

bool shouldReportStatusText(mavsdk::Telemetry::StatusTextType type)
{
    return type == mavsdk::Telemetry::StatusTextType::Warning
        || type == mavsdk::Telemetry::StatusTextType::Error
        || type == mavsdk::Telemetry::StatusTextType::Critical
        || type == mavsdk::Telemetry::StatusTextType::Alert
        || type == mavsdk::Telemetry::StatusTextType::Emergency;
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

bool isMultirotor(mavsdk::Vehicle vehicle)
{
    switch (vehicle) {
    case mavsdk::Vehicle::Quadrotor:
    case mavsdk::Vehicle::Coaxial:
    case mavsdk::Vehicle::Helicopter:
    case mavsdk::Vehicle::Hexarotor:
    case mavsdk::Vehicle::Octorotor:
    case mavsdk::Vehicle::Tricopter:
    case mavsdk::Vehicle::Dodecarotor:
    case mavsdk::Vehicle::Decarotor:
    case mavsdk::Vehicle::GenericMultirotor:
        return true;
    default:
        return false;
    }
}

mavsdk::MavlinkPassthrough::Result sendCommandLong(mavsdk::MavlinkPassthrough &passthrough,
                                                    std::uint16_t command,
                                                    float param1 = 0.0f,
                                                    float param2 = 0.0f,
                                                    float param3 = 0.0f,
                                                    float param4 = 0.0f,
                                                    float param5 = 0.0f,
                                                    float param6 = 0.0f,
                                                    float param7 = 0.0f)
{
    return passthrough.send_command_long({
        passthrough.get_target_sysid(),
        passthrough.get_target_compid(),
        command,
        param1,
        param2,
        param3,
        param4,
        param5,
        param6,
        param7
    });
}

mavsdk::MavlinkPassthrough::Result sendSetModeMessage(
    mavsdk::MavlinkPassthrough &passthrough,
    std::uint32_t customMode)
{
    const std::uint8_t targetSystem = passthrough.get_target_sysid();
    return passthrough.queue_message(
        [targetSystem, customMode](MavlinkAddress address, std::uint8_t channel) {
            mavlink_message_t message {};
            mavlink_msg_set_mode_pack_chan(
                address.system_id,
                address.component_id,
                channel,
                &message,
                targetSystem,
                MAV_MODE_FLAG_CUSTOM_MODE_ENABLED,
                customMode);
            return message;
        });
}

mavsdk::MavlinkPassthrough::Result sendBodyVelocity(
    mavsdk::MavlinkPassthrough &passthrough,
    const ArduPilotControl::BodyVelocityTarget &target)
{
    const std::uint8_t targetSystem = passthrough.get_target_sysid();
    const std::uint8_t targetComponent = passthrough.get_target_compid();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch());
    const std::uint32_t senderTimeMs = static_cast<std::uint32_t>(elapsed.count());

    return passthrough.queue_message(
        [target, targetSystem, targetComponent, senderTimeMs](MavlinkAddress address,
                                                              std::uint8_t channel) {
            mavlink_message_t message {};
            mavlink_msg_set_position_target_local_ned_pack_chan(
                address.system_id,
                address.component_id,
                channel,
                &message,
                senderTimeMs,
                targetSystem,
                targetComponent,
                MAV_FRAME_BODY_OFFSET_NED,
                ArduPilotControl::VelocityAndYawRateTypeMask,
                0.0f,
                0.0f,
                0.0f,
                target.forwardMps,
                target.rightMps,
                target.downMps,
                0.0f,
                0.0f,
                0.0f,
                0.0f,
                target.yawRateRadS);
            return message;
        });
}
#endif
}

struct TelemetryService::MavsdkRuntime {
#ifdef DRONEACTION_WITH_MAVSDK
    enum class ControlPhase {
        Idle,
        Entering,
        Active,
        ZeroVelocity,
        Exiting
    };

    mavsdk::Mavsdk mavsdk {groundStationConfiguration()};
    std::shared_ptr<mavsdk::System> system;
    std::shared_ptr<mavsdk::Telemetry> telemetry;
    std::shared_ptr<mavsdk::MavlinkPassthrough> passthrough;
    mavsdk::Telemetry::FlightMode flightMode = mavsdk::Telemetry::FlightMode::Unknown;
    bool flightModeKnown = false;
    mavsdk::Autopilot autopilot = mavsdk::Autopilot::Unknown;
    mavsdk::Vehicle vehicle = mavsdk::Vehicle::Unknown;
    std::uint32_t customMode = 0;
    std::uint8_t baseMode = 0;
    bool heartbeatModeKnown = false;
    std::condition_variable stateChanged;
    std::mutex executionMutex;
    std::atomic<std::uint64_t> commandGeneration {0};
    std::atomic_bool stopRequested {false};
    std::mutex mutex;

    AppConfig config;
    std::mutex controlMutex;
    std::condition_variable controlChanged;
    std::thread controlThread;
    ControlPhase controlPhase = ControlPhase::Idle;
    mavsdk::Offboard::VelocityBodyYawspeed controlTarget {};
    std::chrono::steady_clock::time_point lastControlCommandAt {};
    std::chrono::steady_clock::time_point controlTargetExpiresAt {};
    bool hasControlTarget = false;
    bool controlExitRequested = false;
    bool controlAuthorized = false;
    bool manualTakeoverLatched = false;
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
    if (m_runMode == AppRunMode::Simulation) {
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
    emit statusMessageChanged(m_runMode == AppRunMode::Simulation
                                  ? QStringLiteral("Simulation telemetry configured")
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

void TelemetryService::setControlAuthorized(bool enabled)
{
    m_controlAuthorized = enabled;
#ifdef DRONEACTION_WITH_MAVSDK
    auto runtime = m_mavsdk;
    if (!runtime) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(runtime->controlMutex);
        const bool wasAuthorized = runtime->controlAuthorized;
        runtime->controlAuthorized = enabled;
        if (!enabled) {
            runtime->controlTarget = {};
            runtime->hasControlTarget = false;
            runtime->controlExitRequested = true;
        } else if (!wasAuthorized) {
            runtime->manualTakeoverLatched = false;
            runtime->controlExitRequested = false;
            runtime->hasControlTarget = false;
        }
    }
    runtime->controlChanged.notify_all();
#endif
}

bool TelemetryService::externalControlActive() const
{
#ifdef DRONEACTION_WITH_MAVSDK
    auto runtime = m_mavsdk;
    if (!runtime) {
        return false;
    }
    std::lock_guard<std::mutex> lock(runtime->controlMutex);
    return runtime->controlPhase != MavsdkRuntime::ControlPhase::Idle;
#else
    return false;
#endif
}

CommandExecutionResult TelemetryService::queuePx4MovementCommand(const DroneCommand &command)
{
#ifndef DRONEACTION_WITH_MAVSDK
    Q_UNUSED(command)
    return {QStringLiteral("unavailable"), QStringLiteral("command execution support is unavailable")};
#else
    auto runtime = m_mavsdk;
    if (!runtime) {
        return {QStringLiteral("not_ready"), QStringLiteral("Vehicle link is not initialized")};
    }

    {
        std::lock_guard<std::mutex> lock(runtime->controlMutex);
        if (!runtime->controlAuthorized) {
            return {QStringLiteral("disabled"), QStringLiteral("movement control authorization is disabled")};
        }
        if (runtime->manualTakeoverLatched) {
            return {QStringLiteral("manual_takeover"),
                    QStringLiteral("manual takeover is latched; disable and re-enable movement control")};
        }
        if (runtime->controlPhase == MavsdkRuntime::ControlPhase::Exiting
            || (runtime->controlExitRequested
                && runtime->controlPhase != MavsdkRuntime::ControlPhase::Idle)) {
            return {QStringLiteral("busy"), QStringLiteral("external control is waiting for a safe mode exit")};
        }

        runtime->controlTarget = velocitySetpoint(command);
        runtime->lastControlCommandAt = std::chrono::steady_clock::now();
        runtime->controlTargetExpiresAt = runtime->lastControlCommandAt
            + std::chrono::milliseconds(command.durationMs > 0 ? command.durationMs : 1000);
        runtime->hasControlTarget = true;
        runtime->controlExitRequested = false;
        if (runtime->controlPhase == MavsdkRuntime::ControlPhase::Idle) {
            runtime->controlPhase = MavsdkRuntime::ControlPhase::Entering;
        }

        if (!runtime->controlThread.joinable()) {
            QPointer<TelemetryService> self(this);
            runtime->controlThread = std::thread([self, runtime]() {
                using Clock = std::chrono::steady_clock;
                constexpr auto updateInterval = std::chrono::milliseconds(50);
                const mavsdk::Offboard::VelocityBodyYawspeed zero {};

                const auto report = [self](const QString &message) {
                    if (!self) {
                        return;
                    }
                    const QString displayMessage = publicDisplayText(message);
                    QMetaObject::invokeMethod(self, [self, displayMessage]() {
                        if (self) {
                            emit self->statusMessageChanged(displayMessage);
                        }
                    }, Qt::QueuedConnection);
                };

                const auto finishSession = [runtime](bool latchManualTakeover) {
                    {
                        std::lock_guard<std::mutex> lock(runtime->controlMutex);
                        runtime->controlTarget = {};
                        runtime->hasControlTarget = false;
                        runtime->controlExitRequested = false;
                        runtime->controlPhase = MavsdkRuntime::ControlPhase::Idle;
                        if (latchManualTakeover) {
                            runtime->manualTakeoverLatched = true;
                        }
                    }
                    runtime->controlChanged.notify_all();
                };

                const auto waitForUpdate = [runtime, updateInterval]() {
                    std::unique_lock<std::mutex> lock(runtime->controlMutex);
                    const auto deadline = Clock::now() + updateInterval;
                    runtime->controlChanged.wait_until(lock, deadline, [runtime]() {
                        return runtime->stopRequested.load();
                    });
                };

                struct TargetState {
                    mavsdk::Offboard::VelocityBodyYawspeed target {};
                    ControlWatchdogStage watchdogStage = ControlWatchdogStage::ExitControl;
                    bool stop = false;
                    bool exitRequested = false;
                };

                const auto readTargetState = [runtime]() {
                    TargetState state;
                    const auto now = Clock::now();
                    std::lock_guard<std::mutex> lock(runtime->controlMutex);
                    state.stop = runtime->stopRequested.load();
                    state.exitRequested = runtime->controlExitRequested || !runtime->controlAuthorized;
                    state.target = runtime->controlTarget;
                    if (!runtime->hasControlTarget) {
                        state.watchdogStage = ControlWatchdogStage::ExitControl;
                    } else {
                        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - runtime->lastControlCommandAt);
                        state.watchdogStage = ControlWatchdog::stageForElapsedMs(
                            elapsed.count(), runtime->config);
                        if (state.watchdogStage == ControlWatchdogStage::UseTarget
                            && now >= runtime->controlTargetExpiresAt) {
                            state.watchdogStage = ControlWatchdogStage::ZeroVelocity;
                        }
                    }
                    return state;
                };

                const auto currentFlightMode = [runtime]() {
                    std::lock_guard<std::mutex> lock(runtime->mutex);
                    const bool known = runtime->flightModeKnown && runtime->system
                        && runtime->system->is_connected()
                        && runtime->flightMode != mavsdk::Telemetry::FlightMode::Unknown;
                    return std::make_pair(known, runtime->flightMode);
                };

                while (!runtime->stopRequested.load()) {
                    {
                        std::unique_lock<std::mutex> lock(runtime->controlMutex);
                        runtime->controlChanged.wait(lock, [runtime]() {
                            return runtime->stopRequested.load()
                                || (runtime->controlPhase != MavsdkRuntime::ControlPhase::Idle
                                    && runtime->controlExitRequested)
                                || (runtime->controlAuthorized && runtime->hasControlTarget
                                    && !runtime->manualTakeoverLatched);
                        });
                        if (runtime->stopRequested.load()) {
                            break;
                        }
                        runtime->controlPhase = MavsdkRuntime::ControlPhase::Entering;
                    }

                    std::shared_ptr<mavsdk::System> system;
                    {
                        std::lock_guard<std::mutex> lock(runtime->mutex);
                        system = runtime->system;
                    }
                    if (!system || !system->is_connected()) {
                        report(QStringLiteral("External control could not start because the aircraft is disconnected"));
                        finishSession(false);
                        continue;
                    }

                    auto offboard = std::make_unique<mavsdk::Offboard>(system);
                    std::shared_ptr<mavsdk::MavlinkPassthrough> passthrough;
                    {
                        std::lock_guard<std::mutex> lock(runtime->mutex);
                        passthrough = runtime->passthrough;
                    }

                    const auto leaveControl = [&]() {
                        {
                            std::lock_guard<std::mutex> lock(runtime->controlMutex);
                            runtime->controlPhase = MavsdkRuntime::ControlPhase::Exiting;
                            runtime->controlTarget = {};
                            runtime->hasControlTarget = false;
                            runtime->controlExitRequested = true;
                        }
                        report(QStringLiteral("Command stream unavailable; requesting position control"));

                        while (!runtime->stopRequested.load()) {
                            offboard->set_velocity_body(zero);
                            const auto [modeKnown, mode] = currentFlightMode();
                            if (modeKnown && mode != mavsdk::Telemetry::FlightMode::Offboard) {
                                const bool manualModeSelected = mode != mavsdk::Telemetry::FlightMode::Posctl;
                                offboard.reset();
                                finishSession(manualModeSelected);
                                report(manualModeSelected
                                           ? QStringLiteral("Flight mode changed to %1; manual takeover locked")
                                                 .arg(flightModeToString(mode))
                                           : QStringLiteral("External control exited; position control confirmed"));
                                return true;
                            }

                            const auto modeResult = passthrough
                                ? sendSetModeMessage(*passthrough, Px4PositionCustomMode)
                                : mavsdk::MavlinkPassthrough::Result::CommandNoSystem;
                            if (modeResult != mavsdk::MavlinkPassthrough::Result::Success) {
                                report(QStringLiteral("Position control request was not sent: %1; continuing zero velocity")
                                           .arg(resultToString(modeResult)));
                            }

                            const auto confirmationDeadline = Clock::now()
                                + std::chrono::milliseconds(runtime->config.modeExitConfirmationTimeoutMs);
                            while (!runtime->stopRequested.load() && Clock::now() < confirmationDeadline) {
                                offboard->set_velocity_body(zero);
                                const auto [confirmedKnown, confirmedMode] = currentFlightMode();
                                if (confirmedKnown && confirmedMode != mavsdk::Telemetry::FlightMode::Offboard) {
                                    const bool manualModeSelected = confirmedMode
                                        != mavsdk::Telemetry::FlightMode::Posctl;
                                    offboard.reset();
                                    finishSession(manualModeSelected);
                                    report(manualModeSelected
                                               ? QStringLiteral("Flight mode changed to %1; manual takeover locked")
                                                     .arg(flightModeToString(confirmedMode))
                                               : QStringLiteral("External control exited; position control confirmed"));
                                    return true;
                                }
                                waitForUpdate();
                            }
                            if (!runtime->stopRequested.load()) {
                                report(QStringLiteral("Position control was not confirmed; continuing zero velocity and retrying"));
                            }
                        }
                        return false;
                    };

                    const auto [initialModeKnown, initialMode] = currentFlightMode();
                    const bool alreadyOffboard = initialModeKnown
                        && initialMode == mavsdk::Telemetry::FlightMode::Offboard;
                    report(QStringLiteral("Preparing continuous external control stream"));
                    const auto prestreamDeadline = Clock::now()
                        + std::chrono::milliseconds(alreadyOffboard ? 0 : runtime->config.controlEntryPrestreamMs);
                    bool entryAborted = false;
                    bool entryManualTakeover = false;
                    while (!runtime->stopRequested.load() && Clock::now() < prestreamDeadline) {
                        const auto [modeKnown, mode] = currentFlightMode();
                        if (initialModeKnown && modeKnown && mode != initialMode
                            && mode != mavsdk::Telemetry::FlightMode::Offboard) {
                            entryManualTakeover = true;
                            entryAborted = true;
                            break;
                        }
                        const TargetState state = readTargetState();
                        if (state.stop || !system->is_connected()
                            || state.exitRequested
                            || state.watchdogStage == ControlWatchdogStage::ExitControl) {
                            entryAborted = true;
                            break;
                        }
                        const auto &setpoint = state.watchdogStage == ControlWatchdogStage::UseTarget
                            ? state.target
                            : zero;
                        const auto setResult = offboard->set_velocity_body(setpoint);
                        if (setResult != mavsdk::Offboard::Result::Success) {
                            report(QStringLiteral("Control stream preparation failed: %1")
                                       .arg(resultToString(setResult)));
                            entryAborted = true;
                            break;
                        }
                        waitForUpdate();
                    }
                    if (runtime->stopRequested.load()) {
                        break;
                    }
                    if (entryAborted) {
                        offboard.reset();
                        finishSession(entryManualTakeover);
                        report(QStringLiteral("External control entry cancelled before mode request"));
                        continue;
                    }

                    if (!alreadyOffboard) {
                        report(QStringLiteral("Requesting external control mode"));
                        // Queue one mode message so ACK retries cannot block the watchdog or undo takeover.
                        const auto startResult = passthrough
                            ? sendSetModeMessage(*passthrough, Px4OffboardCustomMode)
                            : mavsdk::MavlinkPassthrough::Result::CommandNoSystem;
                        if (startResult != mavsdk::MavlinkPassthrough::Result::Success) {
                            report(QStringLiteral("Control mode request failed: %1").arg(resultToString(startResult)));
                            leaveControl();
                            continue;
                        }
                    }

                    bool controlConfirmed = false;
                    const auto entryConfirmationDeadline = Clock::now()
                        + std::chrono::milliseconds(runtime->config.modeConfirmationTimeoutMs);
                    while (!runtime->stopRequested.load() && Clock::now() < entryConfirmationDeadline) {
                        const auto [modeKnown, mode] = currentFlightMode();
                        if (modeKnown && mode == mavsdk::Telemetry::FlightMode::Offboard) {
                            controlConfirmed = true;
                            break;
                        }
                        if (initialModeKnown && modeKnown && mode != initialMode) {
                            entryManualTakeover = true;
                            break;
                        }

                        const TargetState state = readTargetState();
                        if (!system->is_connected() || state.exitRequested
                            || state.watchdogStage == ControlWatchdogStage::ExitControl) {
                            break;
                        }
                        const auto &setpoint = state.watchdogStage == ControlWatchdogStage::UseTarget
                            ? state.target
                            : zero;
                        const auto setResult = offboard->set_velocity_body(setpoint);
                        if (setResult != mavsdk::Offboard::Result::Success) {
                            report(QStringLiteral("Control stream failed while waiting for mode confirmation: %1")
                                       .arg(resultToString(setResult)));
                            break;
                        }
                        waitForUpdate();
                    }

                    if (!controlConfirmed) {
                        report(QStringLiteral("External control mode was not confirmed"));
                        if (entryManualTakeover) {
                            offboard.reset();
                            finishSession(true);
                            report(QStringLiteral("Flight mode changed during entry; manual takeover locked"));
                        } else {
                            leaveControl();
                        }
                        continue;
                    }

                    {
                        std::lock_guard<std::mutex> lock(runtime->controlMutex);
                        runtime->controlPhase = MavsdkRuntime::ControlPhase::Active;
                    }
                    report(QStringLiteral("External control mode confirmed"));
                    bool zeroVelocityReported = false;

                    while (!runtime->stopRequested.load()) {
                        const auto [modeKnown, mode] = currentFlightMode();
                        if (modeKnown && mode != mavsdk::Telemetry::FlightMode::Offboard) {
                            offboard.reset();
                            finishSession(true);
                            report(QStringLiteral("Flight mode changed to %1; manual takeover locked")
                                       .arg(flightModeToString(mode)));
                            break;
                        }

                        const TargetState state = readTargetState();
                        if (!system->is_connected() || state.exitRequested
                            || state.watchdogStage == ControlWatchdogStage::ExitControl) {
                            leaveControl();
                            break;
                        }

                        const bool useZero = state.watchdogStage == ControlWatchdogStage::ZeroVelocity;
                        {
                            std::lock_guard<std::mutex> lock(runtime->controlMutex);
                            runtime->controlPhase = useZero
                                ? MavsdkRuntime::ControlPhase::ZeroVelocity
                                : MavsdkRuntime::ControlPhase::Active;
                        }
                        if (useZero && !zeroVelocityReported) {
                            report(QStringLiteral("Command updates delayed; holding zero velocity"));
                            zeroVelocityReported = true;
                        } else if (!useZero && zeroVelocityReported) {
                            report(QStringLiteral("Command updates restored"));
                            zeroVelocityReported = false;
                        }

                        const auto setResult = offboard->set_velocity_body(useZero ? zero : state.target);
                        if (setResult != mavsdk::Offboard::Result::Success) {
                            report(QStringLiteral("Control stream update failed: %1")
                                       .arg(resultToString(setResult)));
                            leaveControl();
                            break;
                        }
                        waitForUpdate();
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(runtime->controlMutex);
                    runtime->controlPhase = MavsdkRuntime::ControlPhase::Idle;
                    runtime->hasControlTarget = false;
                }
                runtime->controlChanged.notify_all();
            });
        }
    }
    runtime->controlChanged.notify_all();
    return {QStringLiteral("queued"), QStringLiteral("continuous control target updated")};
#endif
}

CommandExecutionResult TelemetryService::executeCommand(const DroneCommand &command)
{
    if (!m_config.mavlinkCommandExecutionEnabled) {
        return {QStringLiteral("disabled"),
                m_config.runMode == AppRunMode::RealAircraft
                    ? QStringLiteral("Command execution is disabled for real-aircraft mode")
                    : QStringLiteral("Command execution is disabled")};
    }

#ifndef DRONEACTION_WITH_MAVSDK
    Q_UNUSED(command)
    return {QStringLiteral("unavailable"), QStringLiteral("application was built without command execution support")};
#else
    if (!m_mavsdk) {
        return {QStringLiteral("not_ready"), QStringLiteral("Vehicle link is not initialized")};
    }

    auto runtime = m_mavsdk;
    std::shared_ptr<mavsdk::System> system;
    std::shared_ptr<mavsdk::Telemetry> telemetry;
    std::shared_ptr<mavsdk::MavlinkPassthrough> passthrough;
    mavsdk::Autopilot autopilot = mavsdk::Autopilot::Unknown;
    mavsdk::Vehicle vehicle = mavsdk::Vehicle::Unknown;
    {
        std::lock_guard<std::mutex> lock(runtime->mutex);
        system = runtime->system;
        telemetry = runtime->telemetry;
        passthrough = runtime->passthrough;
        autopilot = runtime->autopilot;
        vehicle = runtime->vehicle;
    }
    if (!system || !system->is_connected()) {
        return {QStringLiteral("not_ready"), QStringLiteral("Aircraft is not connected")};
    }
    if (autopilot == mavsdk::Autopilot::Unknown) {
        return {QStringLiteral("not_ready"), QStringLiteral("Waiting for aircraft type identification")};
    }
    if (autopilot == mavsdk::Autopilot::ArduPilot && !passthrough) {
        return {QStringLiteral("not_ready"), QStringLiteral("Guided-control link is not ready")};
    }
    if (CommandParser::isMovement(command.kind)
        && autopilot != mavsdk::Autopilot::ArduPilot
        && !telemetry) {
        return {QStringLiteral("not_ready"), QStringLiteral("Aircraft state is not ready for control-mode confirmation")};
    }

    if (autopilot != mavsdk::Autopilot::ArduPilot && CommandParser::isMovement(command.kind)) {
        if (command.kind != CommandKind::Hold) {
            return queuePx4MovementCommand(command);
        }

        bool exitRequested = false;
        {
            std::lock_guard<std::mutex> lock(runtime->controlMutex);
            exitRequested = runtime->controlPhase != MavsdkRuntime::ControlPhase::Idle;
            if (exitRequested) {
                runtime->controlTarget = {};
                runtime->hasControlTarget = false;
                runtime->controlExitRequested = true;
            }
        }
        if (exitRequested) {
            runtime->controlChanged.notify_all();
            return {QStringLiteral("queued"), QStringLiteral("safe external-control exit requested")};
        }
    }

    if (autopilot != mavsdk::Autopilot::ArduPilot
        && externalControlActive()
        && (command.kind == CommandKind::Takeoff
            || command.kind == CommandKind::Land
            || command.kind == CommandKind::ReturnToLaunch)) {
        return {QStringLiteral("busy"),
                QStringLiteral("exit external control before requesting this action")};
    }

    const std::uint64_t generation = runtime->commandGeneration.fetch_add(1) + 1;
    QPointer<TelemetryService> self(this);
    std::thread([self,
                 runtime,
                 system,
                 telemetry,
                 passthrough,
                 command,
                 autopilot,
                 vehicle,
                 generation]() {
        std::unique_lock<std::mutex> executionLock(runtime->executionMutex);
        if (runtime->stopRequested || runtime->commandGeneration != generation) {
            return;
        }

        mavsdk::Action action(system);
        const auto report = [self](const QString &message) {
            if (!self) {
                return;
            }
            const QString displayMessage = publicDisplayText(message);
            QMetaObject::invokeMethod(self, [self, displayMessage]() {
                if (self) {
                    emit self->statusMessageChanged(displayMessage);
                }
            }, Qt::QueuedConnection);
        };

        if (autopilot == mavsdk::Autopilot::ArduPilot) {
            if (vehicle != mavsdk::Vehicle::Unknown && !isMultirotor(vehicle)) {
                report(QStringLiteral("Guided movement is currently enabled only for multirotor aircraft"));
                return;
            }

            const auto commandStillActive = [runtime, generation]() {
                return !runtime->stopRequested && runtime->commandGeneration == generation;
            };
            const auto guidedModeKnown = [runtime]() {
                return runtime->heartbeatModeKnown
                    && runtime->customMode == ArduPilotControl::CopterGuidedMode;
            };
            const auto waitForState = [runtime, &commandStillActive](const auto &predicate,
                                                                     std::chrono::milliseconds timeout) {
                std::unique_lock<std::mutex> lock(runtime->mutex);
                runtime->stateChanged.wait_for(lock, timeout, [&]() {
                    return !commandStillActive() || predicate();
                });
                return commandStillActive() && predicate();
            };
            const auto ensureGuided = [runtime,
                                       telemetry,
                                       passthrough,
                                       &report,
                                       &guidedModeKnown,
                                       &waitForState,
                                       &commandStillActive]() {
                {
                    std::lock_guard<std::mutex> lock(runtime->mutex);
                    if (guidedModeKnown()) {
                        return true;
                    }
                }

                if (!commandStillActive()) {
                    return false;
                }
                report(QStringLiteral("Requesting guided control mode"));
                const auto modeResult = sendCommandLong(
                    *passthrough,
                    MAV_CMD_DO_SET_MODE,
                    static_cast<float>(MAV_MODE_FLAG_CUSTOM_MODE_ENABLED),
                    static_cast<float>(ArduPilotControl::CopterGuidedMode));
                const bool modeAckTimedOut =
                    modeResult == mavsdk::MavlinkPassthrough::Result::CommandTimeout;
                if (modeResult != mavsdk::MavlinkPassthrough::Result::Success && !modeAckTimedOut) {
                    report(QStringLiteral("Aircraft rejected guided control: %1").arg(resultToString(modeResult)));
                    return false;
                }
                if (modeAckTimedOut) {
                    bool guidedConfirmed = false;
                    {
                        std::lock_guard<std::mutex> lock(runtime->mutex);
                        guidedConfirmed = guidedModeKnown();
                    }
                    if (!guidedConfirmed && commandStillActive()) {
                        report(QStringLiteral("Guided control response timed out; retrying a compatible mode request"));
                        const auto retryResult = sendSetModeMessage(
                            *passthrough, ArduPilotControl::CopterGuidedMode);
                        if (retryResult != mavsdk::MavlinkPassthrough::Result::Success) {
                            report(QStringLiteral("Compatible mode request failed: %1")
                                       .arg(resultToString(retryResult)));
                        }
                    }
                }

                if (!waitForState(guidedModeKnown, std::chrono::milliseconds(5000))) {
                    if (telemetry) {
                        const auto health = telemetry->health();
                        report(QStringLiteral(
                                   "Aircraft readiness: local position %1; global position %2; home %3; armable %4")
                                   .arg(health.is_local_position_ok ? QStringLiteral("ready")
                                                                    : QStringLiteral("not ready"))
                                   .arg(health.is_global_position_ok ? QStringLiteral("ready")
                                                                     : QStringLiteral("not ready"))
                                   .arg(health.is_home_position_ok ? QStringLiteral("ready")
                                                                   : QStringLiteral("not ready"))
                                   .arg(health.is_armable ? QStringLiteral("ready")
                                                          : QStringLiteral("not ready")));
                    }
                    report(modeAckTimedOut
                               ? QStringLiteral("Guided control was not confirmed after the response timeout; command aborted")
                               : QStringLiteral("Guided control was not confirmed; command aborted"));
                    return false;
                }
                report(QStringLiteral("Guided control confirmed"));
                return true;
            };

            if (CommandParser::isMovement(command.kind)) {
                if (!ensureGuided()) {
                    return;
                }

                const auto target = ArduPilotControl::bodyVelocityTarget(command);
                const ArduPilotControl::BodyVelocityTarget zero {};
                const QString commandName = CommandParser::kindToString(command.kind);
                const auto deadline = std::chrono::steady_clock::now()
                    + std::chrono::milliseconds(command.durationMs > 0 ? command.durationMs : 1000);
                bool completed = true;

                report(QStringLiteral("Executing %1").arg(commandName));
                while (std::chrono::steady_clock::now() < deadline) {
                    if (!commandStillActive()) {
                        completed = false;
                        break;
                    }

                    {
                        std::lock_guard<std::mutex> lock(runtime->mutex);
                        if (runtime->heartbeatModeKnown
                            && runtime->customMode != ArduPilotControl::CopterGuidedMode) {
                            completed = false;
                            report(QStringLiteral("Flight mode changed; movement stopped"));
                            break;
                        }
                    }

                    const auto sendResult = sendBodyVelocity(*passthrough, target);
                    if (sendResult != mavsdk::MavlinkPassthrough::Result::Success) {
                        completed = false;
                        report(QStringLiteral("Movement update failed: %1").arg(resultToString(sendResult)));
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }

                if (commandStillActive()) {
                    for (int i = 0; i < 3; ++i) {
                        sendBodyVelocity(*passthrough, zero);
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    }
                }
                if (completed) {
                    report(QStringLiteral("Command completed: %1").arg(commandName));
                }
                return;
            }

            if (command.kind == CommandKind::Takeoff) {
                if (!ensureGuided()) {
                    return;
                }

                bool armed = false;
                {
                    std::lock_guard<std::mutex> lock(runtime->mutex);
                    armed = (runtime->baseMode & MAV_MODE_FLAG_SAFETY_ARMED) != 0;
                }
                if (!armed) {
                    report(QStringLiteral("Requesting motor arm"));
                    const auto armResult = sendCommandLong(
                        *passthrough, MAV_CMD_COMPONENT_ARM_DISARM, 1.0f);
                    const bool armAckTimedOut =
                        armResult == mavsdk::MavlinkPassthrough::Result::CommandTimeout;
                    if (armResult != mavsdk::MavlinkPassthrough::Result::Success && !armAckTimedOut) {
                        report(QStringLiteral("Arm rejected: %1").arg(resultToString(armResult)));
                        return;
                    }
                    if (armAckTimedOut) {
                        report(QStringLiteral("Arm response timed out; checking aircraft state"));
                    }
                    const auto armedPredicate = [runtime]() {
                        return runtime->heartbeatModeKnown
                            && (runtime->baseMode & MAV_MODE_FLAG_SAFETY_ARMED) != 0;
                    };
                    if (!waitForState(armedPredicate, std::chrono::milliseconds(5000))) {
                        report(armAckTimedOut
                                   ? QStringLiteral("Motor arm was not confirmed after the response timeout; takeoff aborted")
                                   : QStringLiteral("Motor arm was not confirmed; takeoff aborted"));
                        return;
                    }
                    report(QStringLiteral("Motor arm confirmed"));
                }

                if (!commandStillActive()) {
                    return;
                }
                const float altitudeM = static_cast<float>(
                    command.hasValue && command.value > 0.0 ? qBound(1.0, command.value, 120.0) : 2.5);
                const auto takeoffResult = sendCommandLong(
                    *passthrough, MAV_CMD_NAV_TAKEOFF, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, altitudeM);
                if (takeoffResult == mavsdk::MavlinkPassthrough::Result::CommandTimeout) {
                    report(QStringLiteral("Takeoff response timed out; verify aircraft climb state"));
                    return;
                }
                if (takeoffResult != mavsdk::MavlinkPassthrough::Result::Success) {
                    report(QStringLiteral("Takeoff rejected: %1").arg(resultToString(takeoffResult)));
                    return;
                }
                report(QStringLiteral("Takeoff accepted to %1 m").arg(altitudeM, 0, 'f', 1));
                return;
            }

            if (command.kind == CommandKind::Land) {
                const auto result = sendCommandLong(*passthrough, MAV_CMD_NAV_LAND);
                report(result == mavsdk::MavlinkPassthrough::Result::Success
                           ? QStringLiteral("Land command accepted")
                           : QStringLiteral("Land command rejected: %1").arg(resultToString(result)));
                return;
            }

            if (command.kind == CommandKind::ReturnToLaunch) {
                const auto result = sendCommandLong(*passthrough, MAV_CMD_NAV_RETURN_TO_LAUNCH);
                report(result == mavsdk::MavlinkPassthrough::Result::Success
                           ? QStringLiteral("Return command accepted")
                           : QStringLiteral("Return command rejected: %1").arg(resultToString(result)));
                return;
            }

            report(QStringLiteral("Command ignored: %1").arg(CommandParser::kindToString(command.kind)));
            return;
        }

        if (command.kind == CommandKind::Hold) {
            const auto result = action.hold();
            report(result == mavsdk::Action::Result::Success
                       ? QStringLiteral("Position hold accepted")
                       : QStringLiteral("Position hold rejected: %1").arg(resultToString(result)));
            return;
        }

        if (command.kind == CommandKind::Takeoff) {
            if (command.hasValue && command.value > 0.0) {
                action.set_takeoff_altitude(static_cast<float>(command.value));
            }
            const auto armResult = action.arm();
            if (armResult != mavsdk::Action::Result::Success) {
                report(QStringLiteral("Arm failed: %1").arg(resultToString(armResult)));
                return;
            }
            const auto takeoffResult = action.takeoff();
            report(QStringLiteral("Takeoff command: %1").arg(resultToString(takeoffResult)));
            return;
        }

        if (command.kind == CommandKind::Land) {
            const auto result = action.land();
            report(QStringLiteral("Land command: %1").arg(resultToString(result)));
            return;
        }

        if (command.kind == CommandKind::ReturnToLaunch) {
            const auto result = action.return_to_launch();
            report(QStringLiteral("Return command: %1").arg(resultToString(result)));
            return;
        }

        report(QStringLiteral("Command ignored: %1").arg(CommandParser::kindToString(command.kind)));
    }).detach();

    return {QStringLiteral("queued"), QStringLiteral("command queued for execution")};
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
    emit statusMessageChanged(QStringLiteral("Command-link support is not enabled; using simulated telemetry"));
#else
    if (config.mavlinkUrl.trimmed().isEmpty()) {
        emit statusMessageChanged(QStringLiteral("Vehicle link endpoint is empty"));
        return;
    }

    m_mavsdk = std::make_shared<MavsdkRuntime>();
    m_mavsdk->config = config;
    m_mavsdk->controlAuthorized = m_controlAuthorized;
    const QString mavsdkUrl = normalizedMavsdkUrl(config.mavlinkUrl);
    const auto connectionResult = m_mavsdk->mavsdk.add_any_connection(mavsdkUrl.toStdString());
    if (connectionResult != mavsdk::ConnectionResult::Success) {
        emit statusMessageChanged(QStringLiteral("Vehicle link connection failed: %1").arg(resultToString(connectionResult)));
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
                    emit self->statusMessageChanged(QStringLiteral("Waiting for aircraft heartbeat"));
                }
            }, Qt::QueuedConnection);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(runtime->mutex);
            runtime->system = system.value();
            runtime->autopilot = runtime->system->autopilot_type();
            runtime->vehicle = runtime->system->vehicle_type();
            runtime->telemetry = std::make_shared<mavsdk::Telemetry>(runtime->system);
            runtime->passthrough = std::make_shared<mavsdk::MavlinkPassthrough>(runtime->system);
        }

        QMetaObject::invokeMethod(self, [self]() {
            if (self) {
                self->applySnapshotUpdate([](TelemetrySnapshot *snapshot) {
                    snapshot->connected = true;
                    snapshot->mode = QStringLiteral("Connected");
                });
                emit self->statusMessageChanged(QStringLiteral("Aircraft connected"));
            }
        }, Qt::QueuedConnection);

        mavsdk::Telemetry *telemetry = nullptr;
        mavsdk::MavlinkPassthrough *passthrough = nullptr;
        {
            std::lock_guard<std::mutex> lock(runtime->mutex);
            telemetry = runtime->telemetry.get();
            passthrough = runtime->passthrough.get();
        }
        if (!telemetry || !passthrough) {
            return;
        }

        std::weak_ptr<MavsdkRuntime> weakRuntime = runtime;
        passthrough->subscribe_message(MAVLINK_MSG_ID_HEARTBEAT,
                                       [self, weakRuntime](const mavlink_message_t &message) {
            auto runtime = weakRuntime.lock();
            if (!runtime || !runtime->system || message.sysid != runtime->system->get_system_id()) {
                return;
            }

            mavlink_heartbeat_t heartbeat {};
            mavlink_msg_heartbeat_decode(&message, &heartbeat);
            if (heartbeat.autopilot == MAV_AUTOPILOT_INVALID) {
                return;
            }

            bool isArduPilot = false;
            QString mode;
            {
                std::lock_guard<std::mutex> lock(runtime->mutex);
                if (heartbeat.autopilot == MAV_AUTOPILOT_ARDUPILOTMEGA) {
                    runtime->autopilot = mavsdk::Autopilot::ArduPilot;
                } else if (heartbeat.autopilot == MAV_AUTOPILOT_PX4) {
                    runtime->autopilot = mavsdk::Autopilot::Px4;
                }
                runtime->customMode = heartbeat.custom_mode;
                runtime->baseMode = heartbeat.base_mode;
                runtime->heartbeatModeKnown = true;
                isArduPilot = runtime->autopilot == mavsdk::Autopilot::ArduPilot;
                if (isArduPilot) {
                    mode = ArduPilotControl::copterModeDisplayName(heartbeat.custom_mode);
                }
            }
            runtime->stateChanged.notify_all();

            if (!self || !isArduPilot) {
                return;
            }
            QMetaObject::invokeMethod(self, [self, mode, heartbeat]() {
                if (!self) {
                    return;
                }
                self->applySnapshotUpdate([mode, heartbeat](TelemetrySnapshot *snapshot) {
                    snapshot->connected = true;
                    snapshot->armed = (heartbeat.base_mode & MAV_MODE_FLAG_SAFETY_ARMED) != 0;
                    snapshot->mode = mode;
                });
            }, Qt::QueuedConnection);
        });

        system.value()->subscribe_is_connected([self, weakRuntime](bool connected) {
            if (auto runtime = weakRuntime.lock()) {
                runtime->stateChanged.notify_all();
                runtime->controlChanged.notify_all();
            }
            if (!self) {
                return;
            }
            QMetaObject::invokeMethod(self, [self, connected]() {
                if (!self) {
                    return;
                }
                self->applySnapshotUpdate([connected](TelemetrySnapshot *snapshot) {
                    snapshot->connected = connected;
                    if (!connected) {
                        snapshot->mode = QStringLiteral("offline");
                    }
                });
                if (!connected) {
                    emit self->statusMessageChanged(QStringLiteral("Aircraft connection lost"));
                }
            }, Qt::QueuedConnection);
        });

        telemetry->set_rate_position(5.0);
        telemetry->set_rate_attitude_euler(10.0);
        telemetry->set_rate_velocity_ned(5.0);
        telemetry->set_rate_battery(1.0);
        telemetry->set_rate_gps_info(1.0);

        telemetry->subscribe_flight_mode([self, weakRuntime](mavsdk::Telemetry::FlightMode mode) {
            bool useRawMode = false;
            if (auto runtime = weakRuntime.lock()) {
                {
                    std::lock_guard<std::mutex> lock(runtime->mutex);
                    runtime->flightMode = mode;
                    runtime->flightModeKnown = true;
                    useRawMode = runtime->autopilot == mavsdk::Autopilot::ArduPilot;
                }
                runtime->stateChanged.notify_all();
                runtime->controlChanged.notify_all();
            }
            if (!self || useRawMode) {
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

        telemetry->subscribe_status_text([self](mavsdk::Telemetry::StatusText status) {
            if (!self || !shouldReportStatusText(status.type)) {
                return;
            }
            const QString message = QStringLiteral("Aircraft %1: %2")
                                        .arg(statusTextTypeToString(status.type),
                                             publicDisplayText(QString::fromStdString(status.text)));
            QMetaObject::invokeMethod(self, [self, message]() {
                if (self) {
                    emit self->statusMessageChanged(message);
                }
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
    auto runtime = m_mavsdk;
    if (runtime) {
        {
            std::unique_lock<std::mutex> lock(runtime->controlMutex);
            if (runtime->controlPhase != MavsdkRuntime::ControlPhase::Idle) {
                runtime->controlTarget = {};
                runtime->hasControlTarget = false;
                runtime->controlExitRequested = true;
                runtime->controlChanged.notify_all();
                runtime->controlChanged.wait_for(
                    lock,
                    std::chrono::milliseconds(runtime->config.modeExitConfirmationTimeoutMs + 1000),
                    [runtime]() {
                        return runtime->controlPhase == MavsdkRuntime::ControlPhase::Idle;
                    });
            }
        }

        runtime->stopRequested = true;
        ++runtime->commandGeneration;
        runtime->stateChanged.notify_all();
        runtime->controlChanged.notify_all();
        if (runtime->controlThread.joinable()) {
            runtime->controlThread.join();
        }
    }
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
