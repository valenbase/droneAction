#include "ArduPilotControl.h"

#include <QtTest/QtTest>
#include <QtMath>

class ArduPilotControlTests : public QObject {
    Q_OBJECT

private slots:
    void mapsBodyAxes();
    void mapsYawToRadians();
    void mapsGuidedModeForDisplay();
};

void ArduPilotControlTests::mapsBodyAxes()
{
    DroneCommand command;
    command.kind = CommandKind::Forward;
    command.value = 1.5;
    auto target = ArduPilotControl::bodyVelocityTarget(command);
    QCOMPARE(target.forwardMps, 1.5f);
    QCOMPARE(target.rightMps, 0.0f);
    QCOMPARE(target.downMps, 0.0f);

    command.kind = CommandKind::Left;
    command.value = 2.0;
    target = ArduPilotControl::bodyVelocityTarget(command);
    QCOMPARE(target.forwardMps, 0.0f);
    QCOMPARE(target.rightMps, -2.0f);

    command.kind = CommandKind::Up;
    command.value = 0.75;
    target = ArduPilotControl::bodyVelocityTarget(command);
    QCOMPARE(target.downMps, -0.75f);
}

void ArduPilotControlTests::mapsYawToRadians()
{
    DroneCommand command;
    command.kind = CommandKind::YawRight;
    command.value = 90.0;
    const auto target = ArduPilotControl::bodyVelocityTarget(command);
    QVERIFY(qAbs(target.yawRateRadS - qDegreesToRadians(90.0f)) < 0.0001f);
}

void ArduPilotControlTests::mapsGuidedModeForDisplay()
{
    QCOMPARE(ArduPilotControl::copterModeDisplayName(ArduPilotControl::CopterGuidedMode),
             QStringLiteral("Guided control"));
    QCOMPARE(ArduPilotControl::copterModeDisplayName(99), QStringLiteral("Mode 99"));
}

QTEST_GUILESS_MAIN(ArduPilotControlTests)

#include "test_ardupilot_control.moc"
