#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

enum class CommandKind {
    Unknown,
    Forward,
    Backward,
    Up,
    Down,
    Left,
    Right,
    YawLeft,
    YawRight,
    Takeoff,
    Land,
    ReturnToLaunch,
    Hold,
    PlanA,
    PlanB,
    Digit
};

struct DroneCommand {
    int version = 1;
    QString requestId;
    QString sessionId;
    quint64 sequence = 0;
    bool hasSequence = false;
    CommandKind kind = CommandKind::Unknown;
    QString rawCommand;
    double value = 0.0;
    bool hasValue = false;
    QString unit;
    int durationMs = 0;
    qint64 timestampMs = 0;
};

struct ParseResult {
    bool ok = false;
    QString error;
    DroneCommand command;
};

class CommandParser {
public:
    static ParseResult parseJsonDatagram(const QByteArray &payload);
    static CommandKind kindFromToken(const QString &token);
    static QString kindToString(CommandKind kind);
    static bool isMovement(CommandKind kind);
    static bool isVertical(CommandKind kind);
    static bool isYaw(CommandKind kind);
    static bool isHighRisk(CommandKind kind);
};
