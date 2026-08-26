#include "AppConfig.h"

#include <QtTest/QtTest>

class AppConfigTests : public QObject {
    Q_OBJECT

private slots:
    void simulationDefaultsEnableCommandExecution();
    void oldSitlConfigEnablesCommandExecution();
    void realDefaultsDisableCommandExecution();
    void runModeRoundTripsJson();
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

QTEST_GUILESS_MAIN(AppConfigTests)

#include "test_app_config.moc"
