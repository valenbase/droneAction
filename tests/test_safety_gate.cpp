#include "SafetyGate.h"

#include <QtTest/QtTest>

class SafetyGateTests : public QObject {
    Q_OBJECT

private slots:
    void movementRequiresAuthorization();
    void movementIsCappedAndAccepted();
    void sitlAllowsLongMovementDuration();
    void highRiskNeedsConfirmation();
};

void SafetyGateTests::movementRequiresAuthorization()
{
    SafetyGate gate;
    AppConfig config;
    gate.setConfig(config);

    TelemetrySnapshot snapshot;
    snapshot.connected = true;

    DroneCommand command;
    command.kind = CommandKind::Forward;

    const SafetyDecision decision = gate.evaluate(command, snapshot);
    QCOMPARE(static_cast<int>(decision.code), static_cast<int>(SafetyDecisionCode::Rejected));
}

void SafetyGateTests::movementIsCappedAndAccepted()
{
    SafetyGate gate;
    AppConfig config;
    config.maxVelocityMps = 2.0;
    gate.setConfig(config);
    gate.setControlAuthorized(true);

    TelemetrySnapshot snapshot;
    snapshot.connected = true;

    DroneCommand command;
    command.kind = CommandKind::Forward;
    command.value = 9.0;
    command.hasValue = true;
    command.durationMs = 9000;

    const SafetyDecision decision = gate.evaluate(command, snapshot);
    QCOMPARE(static_cast<int>(decision.code), static_cast<int>(SafetyDecisionCode::Accepted));
    QCOMPARE(decision.command.value, 2.0);
    QCOMPARE(decision.command.durationMs, config.maxCommandDurationMs);

    const QJsonObject ack = decision.toAck();
    QCOMPARE(ack.value(QStringLiteral("value")).toDouble(), 2.0);
    QCOMPARE(ack.value(QStringLiteral("duration_ms")).toInt(), config.maxCommandDurationMs);
}

void SafetyGateTests::sitlAllowsLongMovementDuration()
{
    SafetyGate gate;
    AppConfig config;
    config.applyRunModeDefaults(AppRunMode::Px4Sitl);
    gate.setConfig(config);
    gate.setControlAuthorized(true);

    TelemetrySnapshot snapshot;
    snapshot.connected = true;

    DroneCommand command;
    command.kind = CommandKind::Forward;
    command.value = 1.0;
    command.hasValue = true;
    command.unit = QStringLiteral("m/s");
    command.durationMs = 100000;

    const SafetyDecision decision = gate.evaluate(command, snapshot);
    QCOMPARE(static_cast<int>(decision.code), static_cast<int>(SafetyDecisionCode::Accepted));
    QCOMPARE(decision.command.durationMs, 100000);

    const QJsonObject ack = decision.toAck();
    QCOMPARE(ack.value(QStringLiteral("duration_ms")).toInt(), 100000);
}

void SafetyGateTests::highRiskNeedsConfirmation()
{
    SafetyGate gate;
    AppConfig config;
    config.requireHighRiskConfirmation = true;
    gate.setConfig(config);

    TelemetrySnapshot snapshot;
    snapshot.connected = true;

    DroneCommand command;
    command.kind = CommandKind::Takeoff;

    const SafetyDecision decision = gate.evaluate(command, snapshot);
    QCOMPARE(static_cast<int>(decision.code), static_cast<int>(SafetyDecisionCode::NeedsConfirmation));
}

QTEST_GUILESS_MAIN(SafetyGateTests)

#include "test_safety_gate.moc"
