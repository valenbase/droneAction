#pragma once

#include "AppConfig.h"
#include "CommandTypes.h"

#include <QJsonObject>
#include <QString>

struct TelemetrySnapshot {
    bool connected = false;
    bool armed = false;
    QString mode = QStringLiteral("offline");
    double altitudeM = 0.0;
    double groundSpeedMps = 0.0;
    double verticalSpeedMps = 0.0;
    double rollDeg = 0.0;
    double pitchDeg = 0.0;
    double headingDeg = 0.0;
    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;
    double batteryPercent = 0.0;
    int gpsSatellites = 0;
};

enum class SafetyDecisionCode {
    Accepted,
    Rejected,
    NeedsConfirmation
};

struct SafetyDecision {
    SafetyDecisionCode code = SafetyDecisionCode::Rejected;
    QString reason;
    DroneCommand command;

    bool accepted() const { return code == SafetyDecisionCode::Accepted; }
    QString statusText() const;
    QJsonObject toAck() const;
};

class SafetyGate {
public:
    void setConfig(const AppConfig &config);
    void setControlAuthorized(bool enabled);
    bool controlAuthorized() const;

    SafetyDecision evaluate(const DroneCommand &command, const TelemetrySnapshot &snapshot) const;

private:
    AppConfig m_config;
    bool m_controlAuthorized = false;
};
