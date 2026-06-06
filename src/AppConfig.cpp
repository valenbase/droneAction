#include "AppConfig.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

namespace {
QString configDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return base + QStringLiteral("/droneaction");
}

QString stringValue(const QJsonObject &object, const char *key, const QString &fallback)
{
    const auto value = object.value(QLatin1String(key));
    return value.isString() ? value.toString() : fallback;
}

double doubleValue(const QJsonObject &object, const char *key, double fallback)
{
    const auto value = object.value(QLatin1String(key));
    return value.isDouble() ? value.toDouble() : fallback;
}

int intValue(const QJsonObject &object, const char *key, int fallback)
{
    const auto value = object.value(QLatin1String(key));
    return value.isDouble() ? value.toInt() : fallback;
}

bool boolValue(const QJsonObject &object, const char *key, bool fallback)
{
    const auto value = object.value(QLatin1String(key));
    return value.isBool() ? value.toBool() : fallback;
}
}

QString AppConfig::configPath()
{
    return configDir() + QStringLiteral("/config.json");
}

bool AppConfig::load(QString *errorMessage)
{
    QFile file(configPath());
    if (!file.exists()) {
        return save(errorMessage);
    }

    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    QJsonParseError parseError {};
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = parseError.errorString();
        }
        return false;
    }

    *this = fromJson(document.object());
    return true;
}

bool AppConfig::save(QString *errorMessage) const
{
    QDir dir(configDir());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot create config directory: %1").arg(configDir());
        }
        return false;
    }

    QFile file(configPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    return true;
}

QJsonObject AppConfig::toJson() const
{
    QJsonArray allowedIps;
    for (const auto &ip : rk3588AllowedIps) {
        allowedIps.append(ip);
    }

    return {
        {QStringLiteral("run_mode"), runModeKey(runMode)},
        {QStringLiteral("mavlink_url"), mavlinkUrl},
        {QStringLiteral("video_url"), videoUrl},
        {QStringLiteral("rk3588_udp_bind"), QStringLiteral("%1:%2").arg(udpBindAddress).arg(udpBindPort)},
        {QStringLiteral("rk3588_allowed_ips"), allowedIps},
        {QStringLiteral("max_velocity_mps"), maxVelocityMps},
        {QStringLiteral("max_vertical_mps"), maxVerticalMps},
        {QStringLiteral("max_yaw_deg_s"), maxYawDegS},
        {QStringLiteral("max_command_duration_ms"), maxCommandDurationMs},
        {QStringLiteral("require_high_risk_confirmation"), requireHighRiskConfirmation},
        {QStringLiteral("mavlink_command_execution_enabled"), mavlinkCommandExecutionEnabled}
    };
}

AppConfig AppConfig::fromJson(const QJsonObject &object)
{
    AppConfig config;
    config.runMode = runModeFromKey(stringValue(object, "run_mode", runModeKey(config.runMode)), config.runMode);
    config.mavlinkUrl = stringValue(object, "mavlink_url", config.mavlinkUrl);
    config.videoUrl = stringValue(object, "video_url", config.videoUrl);

    const QString bind = stringValue(object, "rk3588_udp_bind",
                                     QStringLiteral("%1:%2").arg(config.udpBindAddress).arg(config.udpBindPort));
    const int separator = bind.lastIndexOf(QLatin1Char(':'));
    if (separator > 0) {
        config.udpBindAddress = bind.left(separator);
        bool ok = false;
        const int port = bind.mid(separator + 1).toInt(&ok);
        if (ok && port > 0 && port <= 65535) {
            config.udpBindPort = static_cast<quint16>(port);
        }
    }

    const auto allowed = object.value(QStringLiteral("rk3588_allowed_ips"));
    if (allowed.isArray()) {
        config.rk3588AllowedIps.clear();
        for (const auto &entry : allowed.toArray()) {
            if (entry.isString()) {
                config.rk3588AllowedIps.append(entry.toString());
            }
        }
    }

    config.maxVelocityMps = doubleValue(object, "max_velocity_mps", config.maxVelocityMps);
    config.maxVerticalMps = doubleValue(object, "max_vertical_mps", config.maxVerticalMps);
    config.maxYawDegS = doubleValue(object, "max_yaw_deg_s", config.maxYawDegS);
    config.maxCommandDurationMs = intValue(object, "max_command_duration_ms", config.maxCommandDurationMs);
    const bool sitlMode = config.runMode == AppRunMode::Px4Sitl;
    config.requireHighRiskConfirmation = boolValue(object, "require_high_risk_confirmation", !sitlMode);
    config.mavlinkCommandExecutionEnabled = boolValue(object, "mavlink_command_execution_enabled", sitlMode);
    return config;
}

QString AppConfig::runModeKey(AppRunMode mode)
{
    switch (mode) {
    case AppRunMode::Px4Sitl:
        return QStringLiteral("px4_sitl");
    case AppRunMode::RealAircraft:
        return QStringLiteral("real_aircraft");
    }
    return QStringLiteral("real_aircraft");
}

QString AppConfig::runModeDisplayName(AppRunMode mode)
{
    switch (mode) {
    case AppRunMode::Px4Sitl:
        return QStringLiteral("PX4 SITL");
    case AppRunMode::RealAircraft:
        return QStringLiteral("Real aircraft");
    }
    return QStringLiteral("Real aircraft");
}

AppRunMode AppConfig::runModeFromKey(const QString &key, AppRunMode fallback)
{
    const QString normalized = key.trimmed().toLower();
    if (normalized == QStringLiteral("px4_sitl") || normalized == QStringLiteral("sitl")) {
        return AppRunMode::Px4Sitl;
    }
    if (normalized == QStringLiteral("real_aircraft") || normalized == QStringLiteral("real")) {
        return AppRunMode::RealAircraft;
    }
    return fallback;
}

void AppConfig::applyRunModeDefaults(AppRunMode mode)
{
    runMode = mode;
    if (mode == AppRunMode::Px4Sitl) {
        mavlinkUrl = QStringLiteral("udp://:14540");
        videoUrl.clear();
        udpBindAddress = QStringLiteral("0.0.0.0");
        udpBindPort = 50050;
        requireHighRiskConfirmation = false;
        mavlinkCommandExecutionEnabled = true;
        return;
    }

    mavlinkUrl = QStringLiteral("tcp://192.168.43.1:5760");
    videoUrl = QStringLiteral("rtsp://192.168.144.108:554/stream=0");
    udpBindAddress = QStringLiteral("0.0.0.0");
    udpBindPort = 50050;
    requireHighRiskConfirmation = true;
    mavlinkCommandExecutionEnabled = false;
}
