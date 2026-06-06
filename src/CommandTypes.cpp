#include "CommandTypes.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace {
QString normalized(QString value)
{
    return value.trimmed().toLower();
}
}

ParseResult CommandParser::parseJsonDatagram(const QByteArray &payload)
{
    QJsonParseError parseError {};
    const auto document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return {false, QStringLiteral("invalid JSON command"), {}};
    }

    const QJsonObject object = document.object();
    DroneCommand command;
    command.version = object.value(QStringLiteral("version")).toInt(1);
    if (command.version != 1) {
        return {false, QStringLiteral("unsupported command version"), {}};
    }

    command.requestId = object.value(QStringLiteral("request_id")).toString();
    command.rawCommand = object.value(QStringLiteral("command")).toString();
    if (command.rawCommand.trimmed().isEmpty()) {
        return {false, QStringLiteral("missing command"), {}};
    }

    command.kind = kindFromToken(command.rawCommand);
    if (command.kind == CommandKind::Unknown) {
        return {false, QStringLiteral("unknown command: %1").arg(command.rawCommand), command};
    }

    const auto value = object.value(QStringLiteral("value"));
    if (value.isDouble()) {
        command.value = value.toDouble();
        command.hasValue = true;
    }

    command.unit = object.value(QStringLiteral("unit")).toString();
    command.durationMs = object.value(QStringLiteral("duration_ms")).toInt(0);
    command.timestampMs = static_cast<qint64>(object.value(QStringLiteral("timestamp_ms")).toDouble(0.0));
    return {true, {}, command};
}

CommandKind CommandParser::kindFromToken(const QString &token)
{
    const QString value = normalized(token);
    if (value == QStringLiteral("forward") || value == QStringLiteral("前进")) {
        return CommandKind::Forward;
    }
    if (value == QStringLiteral("backward") || value == QStringLiteral("back") || value == QStringLiteral("后退")) {
        return CommandKind::Backward;
    }
    if (value == QStringLiteral("up") || value == QStringLiteral("升高")) {
        return CommandKind::Up;
    }
    if (value == QStringLiteral("down") || value == QStringLiteral("降低")) {
        return CommandKind::Down;
    }
    if (value == QStringLiteral("left") || value == QStringLiteral("左移")) {
        return CommandKind::Left;
    }
    if (value == QStringLiteral("right") || value == QStringLiteral("右移")) {
        return CommandKind::Right;
    }
    if (value == QStringLiteral("yaw_left") || value == QStringLiteral("turn_left") || value == QStringLiteral("左转")) {
        return CommandKind::YawLeft;
    }
    if (value == QStringLiteral("yaw_right") || value == QStringLiteral("turn_right") || value == QStringLiteral("右转")) {
        return CommandKind::YawRight;
    }
    if (value == QStringLiteral("takeoff") || value == QStringLiteral("起飞")) {
        return CommandKind::Takeoff;
    }
    if (value == QStringLiteral("land") || value == QStringLiteral("降落")) {
        return CommandKind::Land;
    }
    if (value == QStringLiteral("rtl") || value == QStringLiteral("return") || value == QStringLiteral("返航")) {
        return CommandKind::ReturnToLaunch;
    }
    if (value == QStringLiteral("hold") || value == QStringLiteral("hover") || value == QStringLiteral("悬停")) {
        return CommandKind::Hold;
    }
    if (value == QStringLiteral("a计划") || value == QStringLiteral("plan_a")) {
        return CommandKind::PlanA;
    }
    if (value == QStringLiteral("b计划") || value == QStringLiteral("plan_b")) {
        return CommandKind::PlanB;
    }
    if (value.size() == 1 && value.at(0).isDigit()) {
        return CommandKind::Digit;
    }
    return CommandKind::Unknown;
}

QString CommandParser::kindToString(CommandKind kind)
{
    switch (kind) {
    case CommandKind::Forward:
        return QStringLiteral("forward");
    case CommandKind::Backward:
        return QStringLiteral("backward");
    case CommandKind::Up:
        return QStringLiteral("up");
    case CommandKind::Down:
        return QStringLiteral("down");
    case CommandKind::Left:
        return QStringLiteral("left");
    case CommandKind::Right:
        return QStringLiteral("right");
    case CommandKind::YawLeft:
        return QStringLiteral("yaw_left");
    case CommandKind::YawRight:
        return QStringLiteral("yaw_right");
    case CommandKind::Takeoff:
        return QStringLiteral("takeoff");
    case CommandKind::Land:
        return QStringLiteral("land");
    case CommandKind::ReturnToLaunch:
        return QStringLiteral("rtl");
    case CommandKind::Hold:
        return QStringLiteral("hold");
    case CommandKind::PlanA:
        return QStringLiteral("plan_a");
    case CommandKind::PlanB:
        return QStringLiteral("plan_b");
    case CommandKind::Digit:
        return QStringLiteral("digit");
    case CommandKind::Unknown:
        break;
    }
    return QStringLiteral("unknown");
}

bool CommandParser::isMovement(CommandKind kind)
{
    return kind == CommandKind::Forward
        || kind == CommandKind::Backward
        || kind == CommandKind::Left
        || kind == CommandKind::Right
        || kind == CommandKind::Up
        || kind == CommandKind::Down
        || kind == CommandKind::YawLeft
        || kind == CommandKind::YawRight
        || kind == CommandKind::Hold;
}

bool CommandParser::isVertical(CommandKind kind)
{
    return kind == CommandKind::Up || kind == CommandKind::Down;
}

bool CommandParser::isYaw(CommandKind kind)
{
    return kind == CommandKind::YawLeft || kind == CommandKind::YawRight;
}

bool CommandParser::isHighRisk(CommandKind kind)
{
    return kind == CommandKind::Takeoff
        || kind == CommandKind::Land
        || kind == CommandKind::ReturnToLaunch;
}
