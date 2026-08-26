#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>

enum class AppRunMode {
    RealAircraft,
    Simulation
};

struct AppConfig {
    static constexpr int RealAircraftDefaultMaxCommandDurationMs = 3000;
    static constexpr int SimulationDefaultMaxCommandDurationMs = 120000;
    static constexpr int UiMaxCommandDurationMs = 300000;

    AppRunMode runMode = AppRunMode::RealAircraft;
    QString mavlinkUrl = QStringLiteral("tcp://192.168.43.1:5760");
    QString videoUrl = QStringLiteral("rtsp://192.168.144.123/stream=0");
    QString udpBindAddress = QStringLiteral("0.0.0.0");
    quint16 udpBindPort = 50050;
    QStringList rk3588AllowedIps;
    double maxVelocityMps = 2.0;
    double maxVerticalMps = 1.0;
    double maxYawDegS = 45.0;
    int maxCommandDurationMs = RealAircraftDefaultMaxCommandDurationMs;
    bool requireHighRiskConfirmation = true;
    bool mavlinkCommandExecutionEnabled = false;

    static QString configPath();

    bool load(QString *errorMessage = nullptr);
    bool save(QString *errorMessage = nullptr) const;

    QJsonObject toJson() const;
    static AppConfig fromJson(const QJsonObject &object);
    static QString runModeKey(AppRunMode mode);
    static QString runModeDisplayName(AppRunMode mode);
    static AppRunMode runModeFromKey(const QString &key, AppRunMode fallback = AppRunMode::RealAircraft);
    void applyRunModeDefaults(AppRunMode mode);
};
