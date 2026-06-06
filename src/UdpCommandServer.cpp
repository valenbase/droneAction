#include "UdpCommandServer.h"

#include <QJsonDocument>
#include <QNetworkDatagram>

UdpCommandServer::UdpCommandServer(SafetyGate *safetyGate, TelemetryService *telemetryService, QObject *parent)
    : QObject(parent)
    , m_safetyGate(safetyGate)
    , m_telemetryService(telemetryService)
{
    connect(&m_socket, &QUdpSocket::readyRead, this, &UdpCommandServer::readPendingDatagrams);
}

bool UdpCommandServer::start(const AppConfig &config, QString *errorMessage)
{
    stop();
    m_config = config;
    const QHostAddress bindAddress(config.udpBindAddress);
    if (!m_socket.bind(bindAddress, config.udpBindPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        if (errorMessage) {
            *errorMessage = m_socket.errorString();
        }
        return false;
    }

    emit statusChanged(QStringLiteral("RK3588 UDP server listening on %1:%2")
                           .arg(config.udpBindAddress)
                           .arg(config.udpBindPort));
    return true;
}

void UdpCommandServer::stop()
{
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.close();
        emit statusChanged(QStringLiteral("RK3588 UDP server stopped"));
    }
}

bool UdpCommandServer::isRunning() const
{
    return m_socket.state() == QAbstractSocket::BoundState;
}

void UdpCommandServer::readPendingDatagrams()
{
    while (m_socket.hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_socket.receiveDatagram();
        const QHostAddress senderAddress = datagram.senderAddress();
        const QString sender = QStringLiteral("%1:%2").arg(senderAddress.toString()).arg(datagram.senderPort());

        if (!senderAllowed(senderAddress)) {
            const QString reason = QStringLiteral("sender IP is not allowed");
            emit commandRejected(sender, reason);
            sendAck(senderAddress, datagram.senderPort(), {
                {QStringLiteral("version"), 1},
                {QStringLiteral("accepted"), false},
                {QStringLiteral("status"), QStringLiteral("rejected")},
                {QStringLiteral("reason"), reason}
            });
            continue;
        }

        const ParseResult parse = CommandParser::parseJsonDatagram(datagram.data());
        if (!parse.ok) {
            emit commandRejected(sender, parse.error);
            sendAck(senderAddress, datagram.senderPort(), {
                {QStringLiteral("version"), 1},
                {QStringLiteral("accepted"), false},
                {QStringLiteral("status"), QStringLiteral("rejected")},
                {QStringLiteral("reason"), parse.error}
            });
            continue;
        }

        const SafetyDecision decision = m_safetyGate->evaluate(parse.command, m_telemetryService->snapshot());
        emit commandObserved(sender, parse.command, decision);
        sendAck(senderAddress, datagram.senderPort(), decision.toAck());
    }
}

bool UdpCommandServer::senderAllowed(const QHostAddress &sender) const
{
    if (m_config.rk3588AllowedIps.isEmpty()) {
        return true;
    }
    return m_config.rk3588AllowedIps.contains(sender.toString());
}

void UdpCommandServer::sendAck(const QHostAddress &sender, quint16 port, const QJsonObject &ack)
{
    const QByteArray payload = QJsonDocument(ack).toJson(QJsonDocument::Compact);
    m_socket.writeDatagram(payload, sender, port);
}
