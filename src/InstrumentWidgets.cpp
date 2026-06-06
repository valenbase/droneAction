#include "InstrumentWidgets.h"

#include <QFontMetrics>
#include <QList>
#include <QPainter>
#include <QPainterPath>
#include <QSizePolicy>
#include <QStringList>
#include <QtMath>
#include <cmath>

namespace {
constexpr int startAngleDeg = 225;
constexpr int sweepAngleDeg = 270;

QColor panelColor()
{
    return QColor(16, 24, 32);
}

QColor borderColor()
{
    return QColor(38, 54, 69);
}

QColor textColor()
{
    return QColor(232, 238, 244);
}

QColor mutedTextColor()
{
    return QColor(143, 179, 217);
}

QRectF squareInside(const QRect &rect, int margin)
{
    const int side = qMin(rect.width(), rect.height()) - margin * 2;
    return QRectF(rect.center().x() - side / 2.0,
                  rect.center().y() - side / 2.0,
                  side,
                  side);
}

QPointF pointOnCircle(const QPointF &center, double radius, double angleDeg)
{
    const double radians = qDegreesToRadians(angleDeg);
    return QPointF(center.x() + radius * qCos(radians),
                   center.y() - radius * qSin(radians));
}

double normalizedValue(double value, double minimum, double maximum)
{
    if (qFuzzyCompare(minimum, maximum)) {
        return 0.0;
    }
    return qBound(0.0, (value - minimum) / (maximum - minimum), 1.0);
}

void drawPanel(QPainter &painter, const QRectF &rect)
{
    painter.setPen(QPen(borderColor(), 1.0));
    painter.setBrush(panelColor());
    painter.drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
}
}

AttitudeIndicatorWidget::AttitudeIndicatorWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(260, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void AttitudeIndicatorWidget::setAttitude(double rollDeg, double pitchDeg)
{
    m_rollDeg = qBound(-80.0, rollDeg, 80.0);
    m_pitchDeg = qBound(-45.0, pitchDeg, 45.0);
    update();
}

void AttitudeIndicatorWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    drawPanel(painter, rect());

    const QRectF dial = squareInside(rect(), 20);
    const QPointF center = dial.center();
    const double radius = dial.width() / 2.0;

    QPainterPath clipPath;
    clipPath.addEllipse(dial);
    painter.save();
    painter.setClipPath(clipPath);
    painter.translate(center);
    painter.rotate(-m_rollDeg);
    painter.translate(0, m_pitchDeg * radius / 28.0);

    QRectF horizonRect(-radius * 1.8, -radius * 1.8, radius * 3.6, radius * 1.8);
    painter.fillRect(horizonRect, QColor(49, 112, 168));
    painter.fillRect(QRectF(-radius * 1.8, 0, radius * 3.6, radius * 1.8), QColor(126, 84, 46));
    painter.setPen(QPen(QColor(245, 249, 252), 2));
    painter.drawLine(QPointF(-radius * 1.8, 0), QPointF(radius * 1.8, 0));

    painter.setPen(QPen(QColor(222, 232, 240), 1));
    for (int pitch = -30; pitch <= 30; pitch += 10) {
        if (pitch == 0) {
            continue;
        }
        const double y = -pitch * radius / 28.0;
        const double mark = (pitch % 20 == 0) ? radius * 0.24 : radius * 0.16;
        painter.drawLine(QPointF(-mark, y), QPointF(mark, y));
        painter.drawText(QRectF(mark + 6, y - 8, 36, 16), Qt::AlignLeft | Qt::AlignVCenter, QString::number(pitch));
        painter.drawText(QRectF(-mark - 42, y - 8, 36, 16), Qt::AlignRight | Qt::AlignVCenter, QString::number(pitch));
    }
    painter.restore();

    painter.setPen(QPen(QColor(6, 12, 18), 8));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(dial.adjusted(3, 3, -3, -3));
    painter.setPen(QPen(borderColor(), 2));
    painter.drawEllipse(dial.adjusted(4, 4, -4, -4));

    painter.save();
    painter.translate(center);
    painter.setPen(QPen(QColor(245, 249, 252), 2));
    for (int roll = -60; roll <= 60; roll += 15) {
        painter.save();
        painter.rotate(roll);
        const double inner = radius * ((roll % 30 == 0) ? 0.78 : 0.84);
        painter.drawLine(QPointF(0, -inner), QPointF(0, -radius * 0.92));
        painter.restore();
    }

    QPainterPath aircraft;
    aircraft.moveTo(-radius * 0.52, 0);
    aircraft.lineTo(-radius * 0.16, 0);
    aircraft.lineTo(0, radius * 0.07);
    aircraft.lineTo(radius * 0.16, 0);
    aircraft.lineTo(radius * 0.52, 0);
    painter.setPen(QPen(QColor(255, 210, 92), 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(aircraft);
    painter.setBrush(QColor(255, 210, 92));
    painter.drawEllipse(QPointF(0, 0), 4, 4);
    painter.restore();

    painter.setPen(mutedTextColor());
    painter.setFont(QFont(QStringLiteral("Arial"), 10, QFont::DemiBold));
    painter.drawText(QRectF(12, 8, width() - 24, 24), Qt::AlignCenter, QStringLiteral("ATTITUDE"));
    painter.setPen(textColor());
    painter.drawText(QRectF(16, height() - 32, width() - 32, 22), Qt::AlignCenter,
                     QStringLiteral("ROLL %1   PITCH %2")
                         .arg(m_rollDeg, 0, 'f', 0)
                         .arg(m_pitchDeg, 0, 'f', 0));
}

CircularGaugeWidget::CircularGaugeWidget(const QString &title,
                                         const QString &unit,
                                         double minimum,
                                         double maximum,
                                         QWidget *parent)
    : QWidget(parent)
    , m_title(title)
    , m_unit(unit)
    , m_minimum(minimum)
    , m_maximum(maximum)
{
    setMinimumSize(190, 170);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void CircularGaugeWidget::setValue(double value)
{
    m_value = value;
    update();
}

void CircularGaugeWidget::setAccentColor(const QColor &color)
{
    m_accent = color;
    update();
}

void CircularGaugeWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    drawPanel(painter, rect());

    const QRectF dial = squareInside(rect(), 24);
    const QPointF center = dial.center();
    const double radius = dial.width() / 2.0;
    const double ratio = normalizedValue(m_value, m_minimum, m_maximum);
    const double needleAngle = startAngleDeg - ratio * sweepAngleDeg;

    painter.setPen(mutedTextColor());
    painter.setFont(QFont(QStringLiteral("Arial"), 10, QFont::DemiBold));
    painter.drawText(QRectF(12, 8, width() - 24, 22), Qt::AlignCenter, m_title.toUpper());

    QRectF arcRect(center.x() - radius * 0.72, center.y() - radius * 0.72,
                   radius * 1.44, radius * 1.44);
    painter.setPen(QPen(QColor(42, 61, 77), 10, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(arcRect, (startAngleDeg - sweepAngleDeg) * 16, sweepAngleDeg * 16);
    painter.setPen(QPen(m_accent, 10, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(arcRect, (startAngleDeg - ratio * sweepAngleDeg) * 16,
                    (ratio * sweepAngleDeg) * 16);

    painter.setPen(QPen(QColor(169, 190, 208), 1));
    for (int i = 0; i <= 10; ++i) {
        const double tickRatio = i / 10.0;
        const double angle = startAngleDeg - tickRatio * sweepAngleDeg;
        const QPointF outer = pointOnCircle(center, radius * 0.84, angle);
        const QPointF inner = pointOnCircle(center, radius * ((i % 2 == 0) ? 0.70 : 0.76), angle);
        painter.drawLine(inner, outer);
    }

    painter.setPen(QPen(QColor(245, 249, 252), 3, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(center, pointOnCircle(center, radius * 0.62, needleAngle));
    painter.setBrush(QColor(245, 249, 252));
    painter.drawEllipse(center, 5, 5);

    painter.setPen(textColor());
    painter.setFont(QFont(QStringLiteral("Arial"), 20, QFont::Bold));
    painter.drawText(QRectF(8, center.y() + radius * 0.18, width() - 16, 28),
                     Qt::AlignCenter, QString::number(m_value, 'f', (m_maximum <= 10.0) ? 1 : 0));
    painter.setPen(mutedTextColor());
    painter.setFont(QFont(QStringLiteral("Arial"), 10));
    painter.drawText(QRectF(8, center.y() + radius * 0.18 + 28, width() - 16, 18),
                     Qt::AlignCenter, m_unit);
}

CompassWidget::CompassWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(190, 170);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void CompassWidget::setHeading(double headingDeg)
{
    m_headingDeg = std::fmod(headingDeg, 360.0);
    if (m_headingDeg < 0.0) {
        m_headingDeg += 360.0;
    }
    update();
}

void CompassWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    drawPanel(painter, rect());

    const QRectF dial = squareInside(rect(), 24);
    const QPointF center = dial.center();
    const double radius = dial.width() / 2.0;

    painter.setPen(mutedTextColor());
    painter.setFont(QFont(QStringLiteral("Arial"), 10, QFont::DemiBold));
    painter.drawText(QRectF(12, 8, width() - 24, 22), Qt::AlignCenter, QStringLiteral("HEADING"));

    painter.setPen(QPen(QColor(42, 61, 77), 2));
    painter.setBrush(QColor(7, 16, 24));
    painter.drawEllipse(QRectF(center.x() - radius * 0.74, center.y() - radius * 0.74,
                               radius * 1.48, radius * 1.48));

    painter.save();
    painter.translate(center);
    painter.rotate(-m_headingDeg);
    painter.setPen(QPen(QColor(169, 190, 208), 1));
    for (int deg = 0; deg < 360; deg += 15) {
        painter.save();
        painter.rotate(deg);
        const double inner = radius * ((deg % 45 == 0) ? 0.58 : 0.66);
        painter.drawLine(QPointF(0, -inner), QPointF(0, -radius * 0.72));
        painter.restore();
    }

    painter.setFont(QFont(QStringLiteral("Arial"), 12, QFont::Bold));
    const QStringList labels = {QStringLiteral("N"), QStringLiteral("E"), QStringLiteral("S"), QStringLiteral("W")};
    const QList<double> angles = {0.0, 90.0, 180.0, 270.0};
    for (int i = 0; i < labels.size(); ++i) {
        const QPointF pos = pointOnCircle(QPointF(0, 0), radius * 0.48, 90.0 - angles.at(i));
        painter.save();
        painter.translate(pos);
        painter.rotate(m_headingDeg);
        painter.setPen(labels.at(i) == QStringLiteral("N") ? QColor(255, 92, 92) : textColor());
        painter.drawText(QRectF(-14, -10, 28, 20), Qt::AlignCenter, labels.at(i));
        painter.restore();
    }
    painter.restore();

    QPainterPath needle;
    needle.moveTo(center.x(), center.y() - radius * 0.66);
    needle.lineTo(center.x() - 7, center.y() + 5);
    needle.lineTo(center.x() + 7, center.y() + 5);
    needle.closeSubpath();
    painter.setBrush(QColor(255, 92, 92));
    painter.setPen(Qt::NoPen);
    painter.drawPath(needle);
    painter.setBrush(QColor(245, 249, 252));
    painter.drawEllipse(center, 5, 5);

    painter.setPen(textColor());
    painter.setFont(QFont(QStringLiteral("Arial"), 18, QFont::Bold));
    painter.drawText(QRectF(8, height() - 42, width() - 16, 28),
                     Qt::AlignCenter, QStringLiteral("%1 deg").arg(m_headingDeg, 0, 'f', 0));
}
