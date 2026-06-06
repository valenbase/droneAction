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
        {QStringLiteral("mavlink_url"), mavlinkUrl},
        {QStringLiteral("video_url"), videoUrl},
        {QStringLiteral("rk3588_udp_bind"), QStringLiteral("%1:%2").arg(udpBindAddress).arg(udpBindPort)},
        {QStringLiteral("rk3588_allowed_ips"), allowedIps},
        {QStringLiteral("max_velocity_mps"), maxVelocityMps},
        {QStringLiteral("max_vertical_mps"), maxVerticalMps},
        {QStringLiteral("max_yaw_deg_s"), maxYawDegS},
        {QStringLiteral("max_command_duration_ms"), maxCommandDurationMs},
        {QStringLiteral("require_high_risk_confirmation"), requireHighRiskConfirmation}
    };
}

AppConfig AppConfig::fromJson(const QJsonObject &object)
{
    AppConfig config;
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
    config.requireHighRiskConfirmation = boolValue(object, "require_high_risk_confirmation",
                                                   config.requireHighRiskConfirmation);
    return config;
}
