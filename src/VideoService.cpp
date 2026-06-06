#include "VideoService.h"

VideoService::VideoService(QObject *parent)
    : QObject(parent)
{
}

void VideoService::start(const QString &rtspUrl)
{
    m_url = rtspUrl;
    m_status = QStringLiteral("RTSP configured; waiting for video backend");
    emit statusChanged(m_status);
}

void VideoService::stop()
{
    m_status = QStringLiteral("offline");
    emit statusChanged(m_status);
}

QString VideoService::url() const
{
    return m_url;
}

QString VideoService::status() const
{
    return m_status;
}
