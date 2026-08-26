#pragma once

#include <QImage>
#include <QWidget>

class VideoFrameWidget final : public QWidget {
public:
    explicit VideoFrameWidget(QWidget *parent = nullptr);

    void setFrame(const QImage &frame);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage m_frame;
};
