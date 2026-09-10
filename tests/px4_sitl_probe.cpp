#include "TelemetryService.h"
#include "UdpCommandServer.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QTimer>
#include <cmath>
#include <mavsdk/mavlink/common/mavlink.h>

// Dedicated to the isolated local SIH simulator, instance 9 (system 10).
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    if (app.arguments() != QStringList{app.arguments().first(), QStringLiteral("--sitl-local")}) {
        qCritical() << "Use --sitl-local with a local SIH simulator instance 9.";
        return 2;
    }
    AppConfig config;
    config.applyRunModeDefaults(AppRunMode::Simulation);
    config.mavlinkUrl = QStringLiteral("udpin://127.0.0.1:14549");
    config.udpBindAddress = QStringLiteral("127.0.0.1");
    config.udpBindPort = 50059;
    TelemetryService telemetry;
    SafetyGate gate;
    gate.setConfig(config);
    gate.setControlAuthorized(true);
    telemetry.setControlAuthorized(true);
    UdpCommandServer server(&gate, &telemetry);
    QUdpSocket commands;
    QUdpSocket sticks;
    int sequence = 0;
    const auto send = [&](const QString &command, double value = 1.0) {
        const QJsonObject object {
            {QStringLiteral("version"), 1},
            {QStringLiteral("request_id"), QString::number(++sequence)},
            {QStringLiteral("session_id"), QStringLiteral("sitl-probe")},
            {QStringLiteral("sequence"), sequence},
            {QStringLiteral("command"), command},
            {QStringLiteral("value"), value},
            {QStringLiteral("duration_ms"), 1000}
        };
        commands.writeDatagram(QJsonDocument(object).toJson(QJsonDocument::Compact),
                               QHostAddress::LocalHost, config.udpBindPort);
    };
    QObject::connect(&telemetry, &TelemetryService::statusMessageChanged, [](const QString &message) {
        qInfo().noquote() << "STATUS" << message;
    });
    QObject::connect(&commands, &QUdpSocket::readyRead, [&]() {
        while (commands.hasPendingDatagrams()) {
            QByteArray bytes;
            bytes.resize(static_cast<int>(commands.pendingDatagramSize()));
            commands.readDatagram(bytes.data(), bytes.size());
            const auto ack = QJsonDocument::fromJson(bytes).object();
            if (!ack.value(QStringLiteral("accepted")).toBool()) qWarning().noquote() << "ACK" << bytes;
        }
    });

    enum class Stage { Ready, Takeoff, Entering, Moving, Watchdog, Position, Land };
    Stage stage = Stage::Ready;
    QElapsedTimer timer;
    timer.start();
    QElapsedTimer logTimer;
    logTimer.start();
    QTimer loop;
    loop.setInterval(50);
    double maxSpeed = 0;
    double stoppedSpeed = 100;
    double positionSpeedSquaredSum = 0;
    int positionSpeedSamples = 0;
    qint64 exitDelay = 0;
    bool zeroObserved = false;
    QObject::connect(&loop, &QTimer::timeout, [&]() {
        mavlink_manual_control_t manual {};
        manual.target = 10;
        manual.z = 500;
        mavlink_message_t message {};
        mavlink_msg_manual_control_encode(255, 190, &message, &manual);
        uint8_t bytes[MAVLINK_MAX_PACKET_LEN];
        const auto size = mavlink_msg_to_send_buffer(bytes, &message);
        sticks.writeDatagram(reinterpret_cast<const char *>(bytes), size, QHostAddress::LocalHost, 18579);

        const auto snapshot = telemetry.snapshot();
        if (logTimer.elapsed() >= 1000) {
            qInfo() << "STATE" << static_cast<int>(stage) << snapshot.mode << snapshot.altitudeM
                    << snapshot.groundSpeedMps << "armed" << snapshot.armed;
            logTimer.restart();
        }
        switch (stage) {
        case Stage::Ready:
            if (timer.elapsed() >= 8000 && snapshot.connected) {
                send(QStringLiteral("takeoff"), 3.0);
                stage = Stage::Takeoff;
                timer.restart();
            }
            break;
        case Stage::Takeoff:
            if (snapshot.armed && snapshot.altitudeM > 2.5 && snapshot.mode == QStringLiteral("Hold")) {
                stage = Stage::Entering;
                timer.restart();
            }
            break;
        case Stage::Entering:
            send(QStringLiteral("forward"));
            if (snapshot.mode == QStringLiteral("Guided control")) {
                stage = Stage::Moving;
                timer.restart();
            }
            break;
        case Stage::Moving:
            send(QStringLiteral("forward"));
            maxSpeed = qMax(maxSpeed, snapshot.groundSpeedMps);
            if (timer.elapsed() >= 5000) {
                stage = Stage::Watchdog;
                timer.restart();
            }
            break;
        case Stage::Watchdog:
            if (snapshot.mode == QStringLiteral("Guided control") && timer.elapsed() > 500
                && snapshot.groundSpeedMps < 0.3) zeroObserved = true;
            if (snapshot.mode == QStringLiteral("Position control") && !telemetry.externalControlActive()) {
                exitDelay = timer.elapsed();
                stage = Stage::Position;
                timer.restart();
            }
            break;
        case Stage::Position:
            if (timer.elapsed() >= 2000) {
                positionSpeedSquaredSum += snapshot.groundSpeedMps * snapshot.groundSpeedMps;
                ++positionSpeedSamples;
            }
            if (timer.elapsed() >= 3000) {
                stoppedSpeed = std::sqrt(positionSpeedSquaredSum / qMax(1, positionSpeedSamples));
                send(QStringLiteral("land"));
                stage = Stage::Land;
                timer.restart();
            }
            break;
        case Stage::Land:
            if (!snapshot.armed && snapshot.altitudeM < 0.4) {
                const bool passed = maxSpeed > 0.5 && stoppedSpeed < 0.3 && zeroObserved
                    && exitDelay >= 1800 && exitDelay < 4000;
                qInfo() << "RESULT" << (passed ? "PASS" : "FAIL") << "max_speed" << maxSpeed
                        << "stopped_speed_rms" << stoppedSpeed << "exit_delay_ms" << exitDelay
                        << "zero_before_exit" << zeroObserved;
                loop.stop();
                app.exit(passed ? 0 : 1);
            }
            break;
        }
    });
    QTimer::singleShot(75000, &app, [&]() {
        qCritical() << "RESULT TIMEOUT stage" << static_cast<int>(stage);
        loop.stop();
        app.exit(1);
    });
    QString error;
    if (!server.start(config, &error)) {
        qCritical() << error;
        return 2;
    }
    telemetry.connectToEndpoint(config);
    loop.start();
    const int result = app.exec();
    telemetry.disconnectFromEndpoint();
    return result;
}
