#pragma once

#include "AppConfig.h"
#include "CommandTypes.h"
#include "SafetyGate.h"
#include "TelemetryService.h"

#include <QObject>
#include <QHash>
#include <QHostAddress>
#include <QSet>
#include <QTimer>
#include <QUdpSocket>

class UdpCommandServer : public QObject {
    Q_OBJECT

public:
    explicit UdpCommandServer(SafetyGate *safetyGate, TelemetryService *telemetryService, QObject *parent = nullptr);

    bool start(const AppConfig &config, QString *errorMessage = nullptr);
    void stop();
    bool isRunning() const;
    bool hasPendingConfirmation() const;

public slots:
    void confirmPendingCommand();

signals:
    void commandObserved(const QString &sender, const DroneCommand &command, const SafetyDecision &decision);
    void commandRejected(const QString &sender, const QString &reason);
    void statusChanged(const QString &message);
    void confirmationPending(const QString &commandName);
    void confirmationCleared();

private slots:
    void readPendingDatagrams();

private:
    bool senderAllowed(const QHostAddress &sender) const;
    bool commandOrderAccepted(const QHostAddress &sender, const DroneCommand &command, QString *reason);
    void sendAck(const QHostAddress &sender, quint16 port, const QJsonObject &ack);
    void clearPendingConfirmation(const QString &reason, bool notifySender);

    struct PendingConfirmation {
        bool active = false;
        QHostAddress senderAddress;
        quint16 senderPort = 0;
        DroneCommand command;
    };

    struct SenderSequenceState {
        QString activeSessionId;
        quint64 lastSequence = 0;
        QSet<QString> retiredSessionIds;
    };

    QUdpSocket m_socket;
    QTimer m_pollTimer;
    QTimer m_confirmationTimer;
    AppConfig m_config;
    PendingConfirmation m_pendingConfirmation;
    QHash<QString, SenderSequenceState> m_senderSequenceStates;
    SafetyGate *m_safetyGate = nullptr;
    TelemetryService *m_telemetryService = nullptr;
};
