#include "AppConfig.h"

#include <QtTest/QtTest>

class AppConfigTests : public QObject {
    Q_OBJECT

private slots:
    void sitlDefaultsUsePx4Udp();
    void oldSitlConfigEnablesCommandExecution();
    void realDefaultsDisableCommandExecution();
    void runModeRoundTripsJson();
};

void AppConfigTests::sitlDefaultsUsePx4Udp()
{
    AppConfig config;
    config.applyRunModeDefaults(AppRunMode::Px4Sitl);

    QCOMPARE(static_cast<int>(config.runMode), static_cast<int>(AppRunMode::Px4Sitl));
    QCOMPARE(config.mavlinkUrl, QStringLiteral("udp://:14540"));
    QVERIFY(config.videoUrl.isEmpty());
    QVERIFY(config.mavlinkCommandExecutionEnabled);
    QVERIFY(!config.requireHighRiskConfirmation);
    QCOMPARE(config.maxCommandDurationMs, AppConfig::Px4SitlDefaultMaxCommandDurationMs);
}

void AppConfigTests::realDefaultsDisableCommandExecution()
{
    AppConfig config;
    config.applyRunModeDefaults(AppRunMode::RealAircraft);

    QCOMPARE(static_cast<int>(config.runMode), static_cast<int>(AppRunMode::RealAircraft));
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
    QCOMPARE(parsed.maxCommandDurationMs, AppConfig::Px4SitlDefaultMaxCommandDurationMs);
}

void AppConfigTests::runModeRoundTripsJson()
{
    AppConfig config;
    config.applyRunModeDefaults(AppRunMode::Px4Sitl);

    const AppConfig parsed = AppConfig::fromJson(config.toJson());
    QCOMPARE(static_cast<int>(parsed.runMode), static_cast<int>(AppRunMode::Px4Sitl));
    QCOMPARE(parsed.mavlinkUrl, config.mavlinkUrl);
}

QTEST_GUILESS_MAIN(AppConfigTests)

#include "test_app_config.moc"
