#include "ControlWatchdog.h"

#include <QtTest/QtTest>

class ControlWatchdogTests : public QObject {
    Q_OBJECT

private slots:
    void changesStageAtConfiguredBoundaries();
};

void ControlWatchdogTests::changesStageAtConfiguredBoundaries()
{
    AppConfig config;
    config.commandStaleTimeoutMs = 500;
    config.controlExitTimeoutMs = 2000;

    QCOMPARE(static_cast<int>(ControlWatchdog::stageForElapsedMs(499, config)),
             static_cast<int>(ControlWatchdogStage::UseTarget));
    QCOMPARE(static_cast<int>(ControlWatchdog::stageForElapsedMs(500, config)),
             static_cast<int>(ControlWatchdogStage::ZeroVelocity));
    QCOMPARE(static_cast<int>(ControlWatchdog::stageForElapsedMs(1999, config)),
             static_cast<int>(ControlWatchdogStage::ZeroVelocity));
    QCOMPARE(static_cast<int>(ControlWatchdog::stageForElapsedMs(2000, config)),
             static_cast<int>(ControlWatchdogStage::ExitControl));
    QCOMPARE(static_cast<int>(ControlWatchdog::stageForElapsedMs(2000, config, false)),
             static_cast<int>(ControlWatchdogStage::ZeroVelocity));
    QCOMPARE(static_cast<int>(ControlWatchdog::stageForElapsedMs(10000, config, false)),
             static_cast<int>(ControlWatchdogStage::ZeroVelocity));
}

QTEST_GUILESS_MAIN(ControlWatchdogTests)

#include "test_control_watchdog.moc"
