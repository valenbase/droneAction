#pragma once

#include "AppConfig.h"
#include "CommandTypes.h"
#include "SafetyGate.h"
#include "TelemetryService.h"

#include <QObject>
#include <QHostAddress>
#include <QTimer>
#include <QUdpSocket>

class UdpCommandServer : public QObject {
    Q_OBJECT

public:
    explicit UdpCommandServer(SafetyGate *safetyGate, TelemetryService *telemetryService, QObject *parent = nullptr);

    bool start(const AppConfig &config, QString *errorMessage = nullptr);
    void stop();
    bool isRunning() const;

signals:
    void commandObserved(const QString &sender, const DroneCommand &command, const SafetyDecision &decision);
    void commandRejected(const QString &sender, const QString &reason);
    void statusChanged(const QString &message);

private slots:
    void readPendingDatagrams();

private:
    bool senderAllowed(const QHostAddress &sender) const;
    void sendAck(const QHostAddress &sender, quint16 port, const QJsonObject &ack);

    QUdpSocket m_socket;
    QTimer m_pollTimer;
    AppConfig m_config;
    SafetyGate *m_safetyGate = nullptr;
    TelemetryService *m_telemetryService = nullptr;
};
