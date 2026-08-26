#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QTimer>

#include <atomic>
#include <memory>

class VideoService : public QObject {
    Q_OBJECT

public:
    explicit VideoService(QObject *parent = nullptr);
    ~VideoService() override;

    void start(const QString &rtspUrl);
    void stop();
    QString url() const;
    QString status() const;

signals:
    void frameReady(const QImage &frame);
    void statusChanged(const QString &status);

private:
    class Backend;

    void setStatus(const QString &status);
    void startPipeline();
    void scheduleReconnect(const QString &reason);
    void pollBus();
    void checkStreamHealth();
    void queueFrame(QImage frame);
    void deliverPendingFrame();
    void clearPendingFrame();

    std::unique_ptr<Backend> m_backend;
    QTimer m_busTimer;
    QTimer m_healthTimer;
    QTimer m_reconnectTimer;
    QElapsedTimer m_connectionClock;
    QElapsedTimer m_lastFrameClock;
    QMutex m_frameMutex;
    QImage m_pendingFrame;
    std::atomic_bool m_acceptFrames = false;
    bool m_frameDeliveryScheduled = false;
    bool m_runningRequested = false;
    bool m_receivedFrame = false;
    QString m_url;
    QString m_status = QStringLiteral("offline");
};
