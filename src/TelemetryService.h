#pragma once

#include "SafetyGate.h"

#include <functional>
#include <memory>
#include <QObject>
#include <QTimer>

struct CommandExecutionResult {
    QString status;
    QString reason;
};

class TelemetryService : public QObject {
    Q_OBJECT

public:
    explicit TelemetryService(QObject *parent = nullptr);
    ~TelemetryService() override;

    void connectToEndpoint(const AppConfig &config);
    void disconnectFromEndpoint();
    CommandExecutionResult executeCommand(const DroneCommand &command);
    TelemetrySnapshot snapshot() const;
    QString endpoint() const;

signals:
    void snapshotChanged(const TelemetrySnapshot &snapshot);
    void statusMessageChanged(const QString &message);

private slots:
    void updateSimulation();

private:
    struct MavsdkRuntime;
    void startMavsdk(const AppConfig &config);
    void resetMavsdk();
    void applySnapshotUpdate(const std::function<void(TelemetrySnapshot *)> &update);

    QTimer m_timer;
    TelemetrySnapshot m_snapshot;
    QString m_endpoint;
    AppConfig m_config;
    AppRunMode m_runMode = AppRunMode::RealAircraft;
    double m_homeLatitudeDeg = 29.56301;
    double m_homeLongitudeDeg = 106.55156;
    int m_tick = 0;
    std::shared_ptr<MavsdkRuntime> m_mavsdk;
};
