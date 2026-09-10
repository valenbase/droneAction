#include "AppConfig.h"

#include <QtTest/QtTest>

class AppConfigTests : public QObject {
    Q_OBJECT

private slots:
    void simulationDefaultsEnableCommandExecution();
    void oldSitlConfigEnablesCommandExecution();
    void realDefaultsDisableCommandExecution();
    void runModeRoundTripsJson();
    void controlTimingRoundTripsJson();
    void invalidControlTimingIsNormalized();
};

void AppConfigTests::simulationDefaultsEnableCommandExecution()
{
    AppConfig config;
    config.applyRunModeDefaults(AppRunMode::Simulation);

    QCOMPARE(static_cast<int>(config.runMode), static_cast<int>(AppRunMode::Simulation));
    QCOMPARE(config.mavlinkUrl, QStringLiteral("udp://:14540"));
    QVERIFY(config.videoUrl.isEmpty());
    QVERIFY(config.mavlinkCommandExecutionEnabled);
    QVERIFY(!config.requireHighRiskConfirmation);
    QCOMPARE(config.maxCommandDurationMs, AppConfig::SimulationDefaultMaxCommandDurationMs);
}

void AppConfigTests::realDefaultsDisableCommandExecution()
{
    AppConfig config;
    config.applyRunModeDefaults(AppRunMode::RealAircraft);

    QCOMPARE(static_cast<int>(config.runMode), static_cast<int>(AppRunMode::RealAircraft));
    QCOMPARE(config.videoUrl,
             QStringLiteral("rtsp://192.168.144.123/stream=0"));
    QVERIFY(config.requireHighRiskConfirmation);
    QVERIFY(!config.mavlinkCommandExecutionEnabled);
    QCOMPARE(config.maxCommandDurationMs, AppConfig::RealAircraftDefaultMaxCommandDurationMs);
}

void AppConfigTests::oldSitlConfigEnablesCommandExecution()
{
    const AppConfig parsed = AppConfig::fromJson({
        {QStringLiteral("run_mode"), QStringLiteral("px4_sitl")},
        {QStringLiteral("mavlink_url"), QStringLiteral("udp://:14540")}
    });

    QVERIFY(parsed.mavlinkCommandExecutionEnabled);
    QVERIFY(!parsed.requireHighRiskConfirmation);
    QCOMPARE(parsed.maxCommandDurationMs, AppConfig::SimulationDefaultMaxCommandDurationMs);
}

void AppConfigTests::runModeRoundTripsJson()
{
    AppConfig config;
    config.applyRunModeDefaults(AppRunMode::Simulation);

    const AppConfig parsed = AppConfig::fromJson(config.toJson());
    QCOMPARE(static_cast<int>(parsed.runMode), static_cast<int>(AppRunMode::Simulation));
    QCOMPARE(parsed.mavlinkUrl, config.mavlinkUrl);
}

void AppConfigTests::controlTimingRoundTripsJson()
{
    AppConfig config;
    config.commandStaleTimeoutMs = 700;
    config.controlExitTimeoutMs = 2600;
    config.controlEntryPrestreamMs = 1500;
    config.modeConfirmationTimeoutMs = 4200;
    config.modeExitConfirmationTimeoutMs = 4600;

    const AppConfig parsed = AppConfig::fromJson(config.toJson());
    QCOMPARE(parsed.commandStaleTimeoutMs, 700);
    QCOMPARE(parsed.controlExitTimeoutMs, 2600);
    QCOMPARE(parsed.controlEntryPrestreamMs, 1500);
    QCOMPARE(parsed.modeConfirmationTimeoutMs, 4200);
    QCOMPARE(parsed.modeExitConfirmationTimeoutMs, 4600);
}

void AppConfigTests::invalidControlTimingIsNormalized()
{
    const AppConfig parsed = AppConfig::fromJson({
        {QStringLiteral("command_stale_timeout_ms"), 50},
        {QStringLiteral("control_exit_timeout_ms"), 100},
        {QStringLiteral("control_entry_prestream_ms"), 200},
        {QStringLiteral("mode_confirmation_timeout_ms"), 100},
        {QStringLiteral("mode_exit_confirmation_timeout_ms"), 20000}
    });

    QCOMPARE(parsed.commandStaleTimeoutMs, AppConfig::MinCommandStaleTimeoutMs);
    QCOMPARE(parsed.controlExitTimeoutMs, AppConfig::MinControlExitTimeoutMs);
    QCOMPARE(parsed.controlEntryPrestreamMs, AppConfig::MinControlEntryPrestreamMs);
    QCOMPARE(parsed.modeConfirmationTimeoutMs, AppConfig::MinModeConfirmationTimeoutMs);
    QCOMPARE(parsed.modeExitConfirmationTimeoutMs, AppConfig::MaxModeConfirmationTimeoutMs);
}

QTEST_GUILESS_MAIN(AppConfigTests)

#include "test_app_config.moc"
