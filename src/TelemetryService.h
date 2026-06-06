#pragma once

#include "SafetyGate.h"

#include <QObject>
#include <QTimer>

class TelemetryService : public QObject {
    Q_OBJECT

public:
    explicit TelemetryService(QObject *parent = nullptr);

    void connectToEndpoint(const AppConfig &config);
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
    AppRunMode m_runMode = AppRunMode::RealAircraft;
    double m_homeLatitudeDeg = 29.56301;
    double m_homeLongitudeDeg = 106.55156;
    int m_tick = 0;
};
