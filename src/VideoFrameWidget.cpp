#include "VideoFrameWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QSizePolicy>

VideoFrameWidget::VideoFrameWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumHeight(360);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void VideoFrameWidget::setFrame(const QImage &frame)
{
    m_frame = frame;
    update();
}

QSize VideoFrameWidget::sizeHint() const
{
    return QSize(640, 360);
}

void VideoFrameWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.fillRect(rect(), QColor(QStringLiteral("#05080c")));

    const QRectF contentRect = rect().adjusted(1, 1, -1, -1);
    QPainterPath clipPath;
    clipPath.addRoundedRect(contentRect, 7, 7);
    painter.setClipPath(clipPath);

    if (!m_frame.isNull()) {
        const QSize targetSize = m_frame.size().scaled(contentRect.size().toSize(), Qt::KeepAspectRatio);
        const QRectF targetRect(
            contentRect.center().x() - targetSize.width() / 2.0,
            contentRect.center().y() - targetSize.height() / 2.0,
            targetSize.width(),
            targetSize.height());
        painter.drawImage(targetRect, m_frame);
    }

    painter.setClipping(false);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(QStringLiteral("#33475b")), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(contentRect, 8, 8);
}
