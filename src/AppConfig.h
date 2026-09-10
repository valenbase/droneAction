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
    static constexpr int DefaultCommandStaleTimeoutMs = 500;
    static constexpr int DefaultControlExitTimeoutMs = 2000;
    static constexpr int DefaultControlEntryPrestreamMs = 1200;
    static constexpr int DefaultModeConfirmationTimeoutMs = 3000;

    static constexpr int MinCommandStaleTimeoutMs = 200;
    static constexpr int MaxCommandStaleTimeoutMs = 2000;
    static constexpr int MinControlExitTimeoutMs = 800;
    static constexpr int MaxControlExitTimeoutMs = 5000;
    static constexpr int MinControlEntryPrestreamMs = 1000;
    static constexpr int MaxControlEntryPrestreamMs = 3000;
    static constexpr int MinModeConfirmationTimeoutMs = 1000;
    static constexpr int MaxModeConfirmationTimeoutMs = 8000;

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
    int commandStaleTimeoutMs = DefaultCommandStaleTimeoutMs;
    int controlExitTimeoutMs = DefaultControlExitTimeoutMs;
    int controlEntryPrestreamMs = DefaultControlEntryPrestreamMs;
    int modeConfirmationTimeoutMs = DefaultModeConfirmationTimeoutMs;
    int modeExitConfirmationTimeoutMs = DefaultModeConfirmationTimeoutMs;
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
    void normalizeControlTiming();
};
