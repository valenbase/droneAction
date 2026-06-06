#pragma once

#include <QObject>
#include <QString>

class VideoService : public QObject {
    Q_OBJECT

public:
    explicit VideoService(QObject *parent = nullptr);

    void start(const QString &rtspUrl);
    void stop();
    QString url() const;
    QString status() const;

signals:
    void statusChanged(const QString &status);

private:
    QString m_url;
    QString m_status = QStringLiteral("offline");
};
