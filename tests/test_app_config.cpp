#include "AppConfig.h"

#include <QtTest/QtTest>

class AppConfigTests : public QObject {
    Q_OBJECT

private slots:
    void sitlDefaultsUsePx4Udp();
    void runModeRoundTripsJson();
};

void AppConfigTests::sitlDefaultsUsePx4Udp()
{
    AppConfig config;
    config.applyRunModeDefaults(AppRunMode::Px4Sitl);

    QCOMPARE(static_cast<int>(config.runMode), static_cast<int>(AppRunMode::Px4Sitl));
    QCOMPARE(config.mavlinkUrl, QStringLiteral("udp://:14540"));
    QVERIFY(config.videoUrl.isEmpty());
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
