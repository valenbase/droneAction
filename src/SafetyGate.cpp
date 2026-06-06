#include "SafetyGate.h"

#include <QtGlobal>

namespace {
double absoluteOrDefault(const DroneCommand &command, double fallback)
{
    return command.hasValue ? qAbs(command.value) : fallback;
}

double capped(double value, double limit)
{
    return qBound(-limit, value, limit);
}
}

QString SafetyDecision::statusText() const
{
    switch (code) {
    case SafetyDecisionCode::Accepted:
        return QStringLiteral("accepted");
    case SafetyDecisionCode::NeedsConfirmation:
        return QStringLiteral("needs_confirmation");
    case SafetyDecisionCode::Rejected:
        break;
    }
    return QStringLiteral("rejected");
}

QJsonObject SafetyDecision::toAck() const
{
    return {
        {QStringLiteral("version"), 1},
        {QStringLiteral("request_id"), command.requestId},
        {QStringLiteral("command"), CommandParser::kindToString(command.kind)},
        {QStringLiteral("accepted"), accepted()},
        {QStringLiteral("status"), statusText()},
        {QStringLiteral("reason"), reason}
    };
}

void SafetyGate::setConfig(const AppConfig &config)
{
    m_config = config;
}

void SafetyGate::setControlAuthorized(bool enabled)
{
    m_controlAuthorized = enabled;
}

bool SafetyGate::controlAuthorized() const
{
    return m_controlAuthorized;
}

SafetyDecision SafetyGate::evaluate(const DroneCommand &input, const TelemetrySnapshot &snapshot) const
{
    DroneCommand command = input;
    if (command.kind == CommandKind::Unknown) {
        return {SafetyDecisionCode::Rejected, QStringLiteral("unknown command"), command};
    }

    if (command.kind == CommandKind::PlanA || command.kind == CommandKind::PlanB || command.kind == CommandKind::Digit) {
        return {SafetyDecisionCode::Rejected, QStringLiteral("mission command is not bound in v1"), command};
    }

    if (CommandParser::isMovement(command.kind) && !m_controlAuthorized) {
        return {SafetyDecisionCode::Rejected, QStringLiteral("control authorization is disabled"), command};
    }

    if ((CommandParser::isMovement(command.kind) || CommandParser::isHighRisk(command.kind)) && !snapshot.connected) {
        return {SafetyDecisionCode::Rejected, QStringLiteral("aircraft telemetry is offline"), command};
    }

    if (CommandParser::isHighRisk(command.kind) && m_config.requireHighRiskConfirmation) {
        return {SafetyDecisionCode::NeedsConfirmation, QStringLiteral("high-risk action requires UI confirmation"), command};
    }

    if (CommandParser::isMovement(command.kind)) {
        const bool isYaw = CommandParser::isYaw(command.kind);
        const bool isVertical = CommandParser::isVertical(command.kind);
        const double fallback = isYaw ? 20.0 : (isVertical ? 0.5 : 0.8);
        const double limit = isYaw ? m_config.maxYawDegS : (isVertical ? m_config.maxVerticalMps : m_config.maxVelocityMps);
        command.value = capped(absoluteOrDefault(command, fallback), limit);
        command.hasValue = true;
        if (command.durationMs <= 0) {
            command.durationMs = 1000;
        }
        command.durationMs = qBound(100, command.durationMs, m_config.maxCommandDurationMs);
    }

    return {SafetyDecisionCode::Accepted, QStringLiteral("command passed safety gate"), command};
}
