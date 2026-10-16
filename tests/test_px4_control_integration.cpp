#include "TelemetryService.h"
#include "UdpCommandServer.h"

#include <QElapsedTimer>
#include <QJsonDocument>
#include <QNetworkDatagram>
#include <QtTest/QtTest>
#include <mavsdk/mavlink/common/mavlink.h>

namespace {
constexpr uint32_t PositionMode = 3U << 16U;
constexpr uint32_t OffboardMode = 6U << 16U;
constexpr uint32_t AltitudeMode = 2U << 16U;

// A loopback-only aircraft fixture exercises the actual SDK and wire messages without hardware.
class AircraftPeer : public QObject {
public:
    struct Target {
        qint64 timeMs;
        float forward;
        float right;
        float down;
        float yawRate;
        bool isZero() const {
            return forward == 0 && right == 0 && down == 0 && yawRate == 0;
        }
    };

    QUdpSocket socket;
    QTimer heartbeatTimer;
    QElapsedTimer clock;
    QVector<Target> targets;
    uint32_t mode = PositionMode;
    bool acceptEntry = true;
    bool acceptExit = true;
    int entryRequests = 0;
    int exitRequests = 0;
    qint64 lastExitRequestMs = -1;

    AircraftPeer() {
        socket.bind(QHostAddress::LocalHost, 0);
        clock.start();
        heartbeatTimer.setInterval(50);
        connect(&heartbeatTimer, &QTimer::timeout, this, [this]() { heartbeat(); });
        connect(&socket, &QUdpSocket::readyRead, this, [this]() {
            while (socket.hasPendingDatagrams()) {
                const auto datagram = socket.receiveDatagram();
                peerAddress = datagram.senderAddress();
                peerPort = datagram.senderPort();
                for (const auto byte : datagram.data()) {
                    mavlink_message_t message {};
                    mavlink_status_t status {};
                    if (mavlink_parse_char(MAVLINK_COMM_0, static_cast<uint8_t>(byte), &message, &status)) {
                        receive(message);
                    }
                }
            }
        });
        heartbeatTimer.start();
    }

    void heartbeat() {
        mavlink_message_t message {};
        mavlink_msg_heartbeat_pack(1, 1, &message, MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_PX4,
                                  MAV_MODE_FLAG_CUSTOM_MODE_ENABLED | MAV_MODE_FLAG_SAFETY_ARMED,
                                  mode, MAV_STATE_ACTIVE);
        send(message);
    }

private:
    QHostAddress peerAddress;
    quint16 peerPort = 0;

    void send(const mavlink_message_t &message) {
        if (!peerPort) return;
        uint8_t bytes[MAVLINK_MAX_PACKET_LEN];
        const auto size = mavlink_msg_to_send_buffer(bytes, &message);
        socket.writeDatagram(reinterpret_cast<const char *>(bytes), size, peerAddress, peerPort);
    }

    void receive(const mavlink_message_t &message) {
        if (message.msgid == MAVLINK_MSG_ID_SET_POSITION_TARGET_LOCAL_NED) {
            mavlink_set_position_target_local_ned_t target {};
            mavlink_msg_set_position_target_local_ned_decode(&message, &target);
            targets.append({clock.elapsed(), target.vx, target.vy, target.vz, target.yaw_rate});
        } else if (message.msgid == MAVLINK_MSG_ID_SET_MODE) {
            mavlink_set_mode_t request {};
            mavlink_msg_set_mode_decode(&message, &request);
            if (request.custom_mode == OffboardMode) {
                ++entryRequests;
                if (acceptEntry) mode = OffboardMode;
            } else if (request.custom_mode == PositionMode) {
                ++exitRequests;
                lastExitRequestMs = clock.elapsed();
                if (acceptExit) mode = PositionMode;
            }
            heartbeat();
        } else if (message.msgid == MAVLINK_MSG_ID_COMMAND_LONG) {
            mavlink_command_long_t request {};
            mavlink_msg_command_long_decode(&message, &request);
            mavlink_message_t response {};
            mavlink_command_ack_t ack {};
            ack.command = request.command;
            ack.result = MAV_RESULT_ACCEPTED;
            ack.target_system = message.sysid;
            ack.target_component = message.compid;
            mavlink_msg_command_ack_encode(1, 1, &response, &ack);
            send(response);
            if (request.command == MAV_CMD_REQUEST_AUTOPILOT_CAPABILITIES
                || (request.command == MAV_CMD_REQUEST_MESSAGE
                    && static_cast<int>(request.param1) == MAVLINK_MSG_ID_AUTOPILOT_VERSION)) {
                mavlink_autopilot_version_t version {};
                version.capabilities = MAV_PROTOCOL_CAPABILITY_MAVLINK2;
                version.flight_sw_version = (1U << 24U) | (15U << 16U);
                version.uid = 12345;
                mavlink_msg_autopilot_version_encode(1, 1, &response, &version);
                send(response);
            }
        }
    }
};

struct Fixture {
    AircraftPeer aircraft;
    TelemetryService telemetry;
    SafetyGate gate;
    UdpCommandServer server {&gate, &telemetry};
    QUdpSocket sender;
    QTimer updates;
    AppConfig config;
    int sequence = 0;
    QString direction = QStringLiteral("forward");

    Fixture() {
        config.applyRunModeDefaults(AppRunMode::Simulation);
        config.mavlinkUrl = QStringLiteral("udpout://127.0.0.1:%1").arg(aircraft.socket.localPort());
        config.commandStaleTimeoutMs = 200;
        config.controlExitTimeoutMs = 800;
        config.controlEntryPrestreamMs = 1000;
        config.modeConfirmationTimeoutMs = 1500;
        config.modeExitConfirmationTimeoutMs = 1000;
        QUdpSocket probe;
        probe.bind(QHostAddress::LocalHost, 0);
        config.udpBindPort = probe.localPort();
        probe.close();
        config.udpBindAddress = QStringLiteral("127.0.0.1");
        gate.setConfig(config);
        gate.setControlAuthorized(true);
        telemetry.setControlAuthorized(true);
        telemetry.connectToEndpoint(config);
        server.start(config);
        updates.setInterval(50);
        QObject::connect(&updates, &QTimer::timeout, &server, [this]() { send(direction); });
    }

    ~Fixture() {
        updates.stop();
        aircraft.mode = PositionMode;
        aircraft.heartbeat();
        QTest::qWait(120);
        telemetry.disconnectFromEndpoint();
    }

    void send(const QString &command) {
        const QJsonObject request {
            {QStringLiteral("version"), 1},
            {QStringLiteral("request_id"), QString::number(++sequence)},
            {QStringLiteral("session_id"), QStringLiteral("wire-test")},
            {QStringLiteral("sequence"), sequence},
            {QStringLiteral("command"), command},
            {QStringLiteral("value"), 1.0},
            {QStringLiteral("duration_ms"), 1000}
        };
        sender.writeDatagram(QJsonDocument(request).toJson(QJsonDocument::Compact),
                             QHostAddress::LocalHost, config.udpBindPort);
    }

    bool ready() const {
        return telemetry.snapshot().connected
            && telemetry.snapshot().mode == QStringLiteral("Position control");
    }
};
}

class Px4ControlIntegrationTests : public QObject {
    Q_OBJECT
private slots:
    void commandGapHoversUntilAuthorizationDisabled() {
        Fixture f;
        QTRY_VERIFY_WITH_TIMEOUT(f.ready(), 6000);
        f.updates.start();
        QTRY_COMPARE_WITH_TIMEOUT(f.aircraft.mode, OffboardMode, 4000);
        f.direction = QStringLiteral("backward");
        QTRY_VERIFY_WITH_TIMEOUT(!f.aircraft.targets.isEmpty() && f.aircraft.targets.last().forward < -0.9f, 1000);
        QCOMPARE(f.aircraft.entryRequests, 1);
        f.updates.stop();
        QTRY_VERIFY_WITH_TIMEOUT(f.aircraft.targets.last().isZero(), 450);
        QTest::qWait(1200);
        QCOMPARE(f.aircraft.mode, OffboardMode);
        QCOMPARE(f.aircraft.exitRequests, 0);
        QVERIFY(f.telemetry.externalControlActive());
        const auto beforeHold = f.aircraft.targets.size();
        f.send(QStringLiteral("hold"));
        QTest::qWait(400);
        QCOMPARE(f.aircraft.mode, OffboardMode);
        QCOMPARE(f.aircraft.exitRequests, 0);
        QVERIFY(f.aircraft.targets.size() > beforeHold);
        QVERIFY(f.aircraft.targets.last().isZero());

        f.telemetry.setControlAuthorized(false);
        QTRY_COMPARE_WITH_TIMEOUT(f.aircraft.mode, PositionMode, 1800);
        QTRY_VERIFY_WITH_TIMEOUT(!f.telemetry.externalControlActive(), 500);
        QTest::qWait(100);
        const auto count = f.aircraft.targets.size();
        QTest::qWait(200);
        QCOMPARE(f.aircraft.targets.size(), count);
    }

    void refusedExitKeepsZeroStreamAndRejectsNewTargets() {
        Fixture f;
        QTRY_VERIFY_WITH_TIMEOUT(f.ready(), 6000);
        f.aircraft.acceptExit = false;
        f.updates.start();
        QTRY_COMPARE_WITH_TIMEOUT(f.aircraft.mode, OffboardMode, 4000);
        f.updates.stop();
        f.telemetry.setControlAuthorized(false);
        QTRY_VERIFY_WITH_TIMEOUT(f.aircraft.exitRequests > 0, 800);
        const auto before = f.aircraft.targets.size();
        f.updates.start();
        QTest::qWait(1200);
        QVERIFY(f.aircraft.exitRequests >= 2);
        QVERIFY(f.aircraft.targets.size() > before + 15);
        for (qsizetype i = before; i < f.aircraft.targets.size(); ++i) {
            QVERIFY(f.aircraft.targets[i].isZero());
            if (i > before) QVERIFY(f.aircraft.targets[i].timeMs - f.aircraft.targets[i - 1].timeMs < 200);
        }
        f.updates.stop();
        f.aircraft.acceptExit = true;
        QTRY_COMPARE_WITH_TIMEOUT(f.aircraft.mode, PositionMode, 1800);
    }

    void manualTakeoverPreventsReentry() {
        Fixture f;
        QTRY_VERIFY_WITH_TIMEOUT(f.ready(), 6000);
        f.updates.start();
        QTRY_COMPARE_WITH_TIMEOUT(f.aircraft.mode, OffboardMode, 4000);
        QTest::qWait(100);
        f.aircraft.mode = AltitudeMode;
        f.aircraft.heartbeat();
        QTRY_VERIFY_WITH_TIMEOUT(!f.telemetry.externalControlActive(), 500);
        QTest::qWait(1500);
        QCOMPARE(f.aircraft.mode, AltitudeMode);
        QCOMPARE(f.aircraft.entryRequests, 1);
        f.telemetry.setControlAuthorized(false);
        f.telemetry.setControlAuthorized(true);
        QTRY_COMPARE_WITH_TIMEOUT(f.aircraft.entryRequests, 2, 2500);
    }

    void delayedEntryDoesNotBlockWatchdog() {
        Fixture f;
        QTRY_VERIFY_WITH_TIMEOUT(f.ready(), 6000);
        f.aircraft.acceptEntry = false;
        f.updates.start();
        QTRY_COMPARE_WITH_TIMEOUT(f.aircraft.entryRequests, 1, 4000);
        f.updates.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!f.aircraft.targets.isEmpty() && f.aircraft.targets.last().isZero(), 450);
        QTRY_VERIFY_WITH_TIMEOUT(!f.telemetry.externalControlActive(), 1500);
        QCOMPARE(f.aircraft.mode, PositionMode);
        QCOMPARE(f.aircraft.entryRequests, 1);
    }
};

QTEST_GUILESS_MAIN(Px4ControlIntegrationTests)
#include "test_px4_control_integration.moc"
