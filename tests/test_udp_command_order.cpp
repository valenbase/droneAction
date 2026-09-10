#include "AppConfig.h"
#include "SafetyGate.h"
#include "TelemetryService.h"
#include "UdpCommandServer.h"

#include <QJsonDocument>
#include <QElapsedTimer>
#include <QNetworkDatagram>
#include <QCoreApplication>
#include <QEventLoop>
#include <QtTest/QtTest>

class UdpCommandOrderTests : public QObject {
    Q_OBJECT

private slots:
    void rejectsDuplicateAndRetiredSessionCommands();
};

namespace {
QJsonObject sendCommand(QUdpSocket *sender, quint16 port, const QJsonObject &command)
{
    const QByteArray payload = QJsonDocument(command).toJson(QJsonDocument::Compact);
    if (sender->writeDatagram(payload, QHostAddress::LocalHost, port) != payload.size()) {
        QTest::qFail("UDP command could not be sent", __FILE__, __LINE__);
        return {};
    }
    QElapsedTimer timer;
    timer.start();
    while (!sender->hasPendingDatagrams() && timer.elapsed() < 1000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QTest::qWait(1);
    }
    if (!sender->hasPendingDatagrams()) {
        QTest::qFail("ACK was not received", __FILE__, __LINE__);
        return {};
    }
    return QJsonDocument::fromJson(sender->receiveDatagram().data()).object();
}
}

void UdpCommandOrderTests::rejectsDuplicateAndRetiredSessionCommands()
{
    QUdpSocket portProbe;
    QVERIFY(portProbe.bind(QHostAddress::LocalHost, 0));
    const quint16 port = portProbe.localPort();
    portProbe.close();

    AppConfig config;
    config.applyRunModeDefaults(AppRunMode::Simulation);
    config.udpBindAddress = QStringLiteral("127.0.0.1");
    config.udpBindPort = port;

    SafetyGate safetyGate;
    safetyGate.setConfig(config);
    safetyGate.setControlAuthorized(true);
    TelemetryService telemetry;
    telemetry.setControlAuthorized(true);
    telemetry.connectToEndpoint(config);
    UdpCommandServer server(&safetyGate, &telemetry);
    QVERIFY(server.start(config));

    QUdpSocket sender;
    const QJsonObject first {
        {QStringLiteral("version"), 1},
        {QStringLiteral("request_id"), QStringLiteral("first")},
        {QStringLiteral("session_id"), QStringLiteral("session-a")},
        {QStringLiteral("sequence"), 1},
        {QStringLiteral("command"), QStringLiteral("forward")},
        {QStringLiteral("value"), 1.0}
    };
    const QJsonObject firstAck = sendCommand(&sender, port, first);
    QVERIFY(firstAck.value(QStringLiteral("reason")).toString()
                != QStringLiteral("duplicate or out-of-order command sequence"));

    const QJsonObject duplicateAck = sendCommand(&sender, port, first);
    QCOMPARE(duplicateAck.value(QStringLiteral("reason")).toString(),
             QStringLiteral("duplicate or out-of-order command sequence"));

    QJsonObject newSession = first;
    newSession.insert(QStringLiteral("request_id"), QStringLiteral("new-session"));
    newSession.insert(QStringLiteral("session_id"), QStringLiteral("session-b"));
    sendCommand(&sender, port, newSession);

    QJsonObject retired = first;
    retired.insert(QStringLiteral("request_id"), QStringLiteral("retired"));
    retired.insert(QStringLiteral("sequence"), 2);
    const QJsonObject retiredAck = sendCommand(&sender, port, retired);
    QCOMPARE(retiredAck.value(QStringLiteral("reason")).toString(),
             QStringLiteral("command belongs to a retired control session"));
}

QTEST_GUILESS_MAIN(UdpCommandOrderTests)

#include "test_udp_command_order.moc"
