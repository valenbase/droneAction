#pragma once

#include "SafetyGate.h"

#include <QObject>
#include <QTimer>

class TelemetryService : public QObject {
    Q_OBJECT

public:
    explicit TelemetryService(QObject *parent = nullptr);

    void connectToEndpoint(const QString &mavlinkUrl);
    void disconnectFromEndpoint();
    TelemetrySnapshot snapshot() const;
    QString endpoint() const;

signals:
    void snapshotChanged(const TelemetrySnapshot &snapshot);
    void statusMessageChanged(const QString &message);

private slots:
    void updateSimulation();

private:
    QTimer m_timer;
    TelemetrySnapshot m_snapshot;
    QString m_endpoint;
    int m_tick = 0;
};
