#include "AppConfig.h"
#include "VideoService.h"

#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("DroneAction"));
    QCoreApplication::setOrganizationName(QStringLiteral("valenbase"));

    AppConfig config;
    QString error;
    if (!config.load(&error)) {
        qCritical().noquote() << "config_error:" << error;
        return 2;
    }
    if (config.videoUrl.trimmed().isEmpty()) {
        qCritical() << "config_error: video URL is empty";
        return 2;
    }

    VideoService video;
    QObject::connect(&video, &VideoService::statusChanged, &app,
                     [](const QString &status) { qInfo().noquote() << "status:" << status; });
    QObject::connect(&video, &VideoService::frameReady, &app,
                     [&app](const QImage &frame) {
                         if (frame.isNull()) {
                             return;
                         }
                         qInfo() << "frame:" << frame.size();
                         app.exit(0);
                     });

    QTimer::singleShot(12000, &app, [&app]() {
        qCritical() << "timeout: no decoded video frame";
        app.exit(1);
    });
    video.start(config.videoUrl);
    return app.exec();
}
