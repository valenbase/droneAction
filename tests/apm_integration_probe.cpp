#include "TelemetryService.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTimer>

namespace {
enum class Stage {
    WaitingForConnection,
    WaitingForReady,
    TakingOff,
    Moving,
    Landing,
    Finished
};

DroneCommand command(CommandKind kind, double value = 0.0, int durationMs = 0)
{
    DroneCommand result;
    result.kind = kind;
    result.value = value;
    result.hasValue = value > 0.0;
    result.durationMs = durationMs;
    return result;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QString endpoint = argc > 1
        ? QString::fromLocal8Bit(argv[1])
        : QStringLiteral("tcp://127.0.0.1:5760");

    AppConfig config;
    config.applyRunModeDefaults(AppRunMode::Simulation);
    config.mavlinkUrl = endpoint;
    config.mavlinkCommandExecutionEnabled = true;

    TelemetryService telemetry;
    Stage stage = Stage::WaitingForConnection;
    QElapsedTimer stageTimer;
    QElapsedTimer snapshotLogTimer;
    snapshotLogTimer.start();
    double maximumGroundSpeed = 0.0;
    int exitCode = 1;

    QObject::connect(&telemetry, &TelemetryService::statusMessageChanged,
                     [](const QString &message) {
        qInfo().noquote() << "STATUS" << message;
    });
    QObject::connect(&telemetry, &TelemetryService::snapshotChanged,
                     [&maximumGroundSpeed, &snapshotLogTimer](const TelemetrySnapshot &snapshot) {
        maximumGroundSpeed = qMax(maximumGroundSpeed, snapshot.groundSpeedMps);
        if (snapshotLogTimer.elapsed() < 1000) {
            return;
        }
        snapshotLogTimer.restart();
        qInfo().noquote()
            << "SNAPSHOT"
            << "connected=" << snapshot.connected
            << "armed=" << snapshot.armed
            << "mode=" << snapshot.mode
            << "altitude=" << snapshot.altitudeM
            << "speed=" << snapshot.groundSpeedMps;
    });

    QTimer poll;
    poll.setInterval(200);
    QObject::connect(&poll, &QTimer::timeout, [&]() {
        const TelemetrySnapshot snapshot = telemetry.snapshot();

        if (stage == Stage::WaitingForConnection
            && snapshot.connected
            && snapshot.mode != QStringLiteral("Connected")) {
            stageTimer.start();
            stage = Stage::WaitingForReady;
            return;
        }

        if (stage == Stage::WaitingForReady && stageTimer.elapsed() >= 5000
            && !snapshot.armed && snapshot.altitudeM < 0.5) {
            const auto result = telemetry.executeCommand(command(CommandKind::Takeoff, 3.0));
            qInfo().noquote() << "TAKEOFF" << result.status << result.reason;
            if (result.status != QStringLiteral("queued")) {
                qCritical().noquote() << "RESULT FAIL takeoff was not queued";
                app.exit(4);
                return;
            }
            stage = Stage::TakingOff;
            stageTimer.restart();
            return;
        }

        if (stage == Stage::TakingOff && snapshot.armed && snapshot.altitudeM >= 1.5) {
            maximumGroundSpeed = 0.0;
            const auto result = telemetry.executeCommand(command(CommandKind::Forward, 1.0, 2000));
            qInfo().noquote() << "FORWARD" << result.status << result.reason;
            stage = Stage::Moving;
            stageTimer.restart();
            return;
        }

        if (stage == Stage::Moving && stageTimer.elapsed() >= 3500) {
            const auto result = telemetry.executeCommand(command(CommandKind::Land));
            qInfo().noquote() << "LAND" << result.status << result.reason;
            stage = Stage::Landing;
            stageTimer.restart();
            return;
        }

        if (stage == Stage::Landing && !snapshot.armed && snapshot.altitudeM < 0.35) {
            exitCode = maximumGroundSpeed >= 0.35 ? 0 : 2;
            qInfo().noquote() << "RESULT"
                              << (exitCode == 0 ? "PASS" : "FAIL")
                              << "maximum_ground_speed=" << maximumGroundSpeed;
            stage = Stage::Finished;
            app.exit(exitCode);
        }
    });

    QTimer::singleShot(70000, &app, [&]() {
        if (stage == Stage::Finished) {
            return;
        }
        qCritical().noquote() << "RESULT TIMEOUT stage=" << static_cast<int>(stage)
                              << "maximum_ground_speed=" << maximumGroundSpeed;
        telemetry.executeCommand(command(CommandKind::Land));
        app.exit(3);
    });

    telemetry.connectToEndpoint(config);
    poll.start();
    return app.exec();
}
