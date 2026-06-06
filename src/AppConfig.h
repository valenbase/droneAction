#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>

struct AppConfig {
    QString mavlinkUrl = QStringLiteral("tcp://192.168.43.1:5760");
    QString videoUrl = QStringLiteral("rtsp://192.168.144.108:554/stream=0");
    QString udpBindAddress = QStringLiteral("0.0.0.0");
    quint16 udpBindPort = 50050;
    QStringList rk3588AllowedIps;
    double maxVelocityMps = 2.0;
    double maxVerticalMps = 1.0;
    double maxYawDegS = 45.0;
    int maxCommandDurationMs = 3000;
    bool requireHighRiskConfirmation = true;

    static QString configPath();

    bool load(QString *errorMessage = nullptr);
    bool save(QString *errorMessage = nullptr) const;

    QJsonObject toJson() const;
    static AppConfig fromJson(const QJsonObject &object);
};
