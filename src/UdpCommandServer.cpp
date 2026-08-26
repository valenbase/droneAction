#include "UdpCommandServer.h"

#include <QJsonDocument>
#include <QNetworkDatagram>

UdpCommandServer::UdpCommandServer(SafetyGate *safetyGate, TelemetryService *telemetryService, QObject *parent)
    : QObject(parent)
    , m_safetyGate(safetyGate)
    , m_telemetryService(telemetryService)
{
    connect(&m_socket, &QUdpSocket::readyRead, this, &UdpCommandServer::readPendingDatagrams);
    m_pollTimer.setInterval(50);
    connect(&m_pollTimer, &QTimer::timeout, this, &UdpCommandServer::readPendingDatagrams);
    m_confirmationTimer.setSingleShot(true);
    m_confirmationTimer.setInterval(15000);
    connect(&m_confirmationTimer, &QTimer::timeout, this, [this]() {
        clearPendingConfirmation(QStringLiteral("operator confirmation timed out"), true);
    });
}

bool UdpCommandServer::start(const AppConfig &config, QString *errorMessage)
{
    stop();
    m_config = config;
    const QHostAddress bindAddress(config.udpBindAddress);
    if (!m_socket.bind(bindAddress, config.udpBindPort, QUdpSocket::ReuseAddressHint)) {
        if (errorMessage) {
            *errorMessage = m_socket.errorString();
        }
        return false;
    }

    m_pollTimer.start();
    emit statusChanged(QStringLiteral("RK3588 UDP server listening on %1:%2")
                           .arg(config.udpBindAddress)
                           .arg(config.udpBindPort));
    return true;
}

void UdpCommandServer::stop()
{
    clearPendingConfirmation(QStringLiteral("command server stopped"), true);
    m_pollTimer.stop();
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.close();
        emit statusChanged(QStringLiteral("RK3588 UDP server stopped"));
    }
}

bool UdpCommandServer::isRunning() const
{
    return m_socket.state() == QAbstractSocket::BoundState;
}

bool UdpCommandServer::hasPendingConfirmation() const
{
    return m_pendingConfirmation.active;
}

void UdpCommandServer::confirmPendingCommand()
{
    if (!m_pendingConfirmation.active) {
        return;
    }

    const PendingConfirmation pending = m_pendingConfirmation;
    m_pendingConfirmation = {};
    m_confirmationTimer.stop();
    emit confirmationCleared();

    QJsonObject ack {
        {QStringLiteral("version"), 1},
        {QStringLiteral("request_id"), pending.command.requestId},
        {QStringLiteral("command"), CommandParser::kindToString(pending.command.kind)},
        {QStringLiteral("accepted"), false},
        {QStringLiteral("status"), QStringLiteral("rejected")},
        {QStringLiteral("reason"), QStringLiteral("operator confirmed but execution was not queued")}
    };

    const CommandExecutionResult execution = m_telemetryService->executeCommand(pending.command);
    const bool queued = execution.status == QStringLiteral("queued");
    ack.insert(QStringLiteral("accepted"), queued);
    ack.insert(QStringLiteral("status"), queued ? QStringLiteral("accepted") : QStringLiteral("rejected"));
    ack.insert(QStringLiteral("reason"), queued
                   ? QStringLiteral("operator confirmed high-risk action")
                   : execution.reason);
    ack.insert(QStringLiteral("execution_status"), execution.status);
    ack.insert(QStringLiteral("execution_reason"), execution.reason);
    if (pending.command.hasValue) {
        ack.insert(QStringLiteral("value"), pending.command.value);
    }
    if (!pending.command.unit.isEmpty()) {
        ack.insert(QStringLiteral("unit"), pending.command.unit);
    }

    sendAck(pending.senderAddress, pending.senderPort, ack);
    emit statusChanged(queued
                           ? QStringLiteral("Confirmed command queued: %1")
                                 .arg(CommandParser::kindToString(pending.command.kind))
                           : QStringLiteral("Confirmed command rejected: %1")
                                 .arg(execution.reason));
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
        QJsonObject ack = decision.toAck();
        if (decision.accepted()) {
            const CommandExecutionResult execution = m_telemetryService->executeCommand(decision.command);
            ack.insert(QStringLiteral("execution_status"), execution.status);
            ack.insert(QStringLiteral("execution_reason"), execution.reason);
            emit statusChanged(QStringLiteral("Command execution %1: %2").arg(execution.status, execution.reason));
        } else if (decision.code == SafetyDecisionCode::NeedsConfirmation) {
            clearPendingConfirmation(QStringLiteral("superseded by a newer confirmation request"), true);
            m_pendingConfirmation.active = true;
            m_pendingConfirmation.senderAddress = senderAddress;
            m_pendingConfirmation.senderPort = datagram.senderPort();
            m_pendingConfirmation.command = decision.command;
            m_confirmationTimer.start();
            emit confirmationPending(CommandParser::kindToString(decision.command.kind));
            emit statusChanged(QStringLiteral("Operator confirmation requested for %1")
                                   .arg(CommandParser::kindToString(decision.command.kind)));
        }
        sendAck(senderAddress, datagram.senderPort(), ack);
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

void UdpCommandServer::clearPendingConfirmation(const QString &reason, bool notifySender)
{
    if (!m_pendingConfirmation.active) {
        return;
    }

    const PendingConfirmation pending = m_pendingConfirmation;
    m_pendingConfirmation = {};
    m_confirmationTimer.stop();
    if (notifySender && m_socket.state() == QAbstractSocket::BoundState) {
        sendAck(pending.senderAddress, pending.senderPort, {
            {QStringLiteral("version"), 1},
            {QStringLiteral("request_id"), pending.command.requestId},
            {QStringLiteral("command"), CommandParser::kindToString(pending.command.kind)},
            {QStringLiteral("accepted"), false},
            {QStringLiteral("status"), QStringLiteral("rejected")},
            {QStringLiteral("reason"), reason}
        });
    }
    emit confirmationCleared();
    emit statusChanged(QStringLiteral("Pending command cleared: %1").arg(reason));
}
