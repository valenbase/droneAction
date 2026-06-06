#include "MapWidget.h"

#include <QDir>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QtMath>
#include <cmath>

namespace {
constexpr int tileSize = 256;
constexpr double minLatitude = -85.05112878;
constexpr double maxLatitude = 85.05112878;

QColor panelColor()
{
    return QColor(12, 20, 28);
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

int positiveModulo(int value, int modulo)
{
    const int result = value % modulo;
    return result < 0 ? result + modulo : result;
}
}

MapWidget::MapWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(320, 260);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheRoot.isEmpty()) {
        cacheRoot = QDir::tempPath() + QStringLiteral("/DroneAction");
    }
    m_diskCache.setCacheDirectory(cacheRoot + QStringLiteral("/osm-tiles"));
    m_diskCache.setMaximumCacheSize(200 * 1024 * 1024);
    m_network.setCache(&m_diskCache);
}

void MapWidget::setDronePosition(double latitudeDeg, double longitudeDeg, double headingDeg, bool valid)
{
    m_positionValid = valid
        && latitudeDeg >= -90.0
        && latitudeDeg <= 90.0
        && longitudeDeg >= -180.0
        && longitudeDeg <= 180.0;
    m_latitudeDeg = qBound(minLatitude, latitudeDeg, maxLatitude);
    m_longitudeDeg = longitudeDeg;
    m_headingDeg = std::fmod(headingDeg, 360.0);
    if (m_headingDeg < 0.0) {
        m_headingDeg += 360.0;
    }
    update();
}

void MapWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF panel = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(borderColor(), 1.0));
    painter.setBrush(panelColor());
    painter.drawRoundedRect(panel, 8, 8);

    const QRect mapRect = rect().adjusted(10, 34, -10, -32);
    painter.save();
    QPainterPath clip;
    clip.addRoundedRect(QRectF(mapRect), 6, 6);
    painter.setClipPath(clip);
    painter.fillRect(mapRect, QColor(8, 15, 22));

    if (!m_positionValid) {
        drawFallback(painter, mapRect);
    } else {
        const QPointF centerWorld = geoToWorldPixel(m_latitudeDeg, m_longitudeDeg);
        const QPointF topLeft = centerWorld - QPointF(mapRect.width() / 2.0, mapRect.height() / 2.0);
        const int minTileX = static_cast<int>(std::floor(topLeft.x() / tileSize));
        const int maxTileX = static_cast<int>(std::floor((topLeft.x() + mapRect.width()) / tileSize));
        const int minTileY = static_cast<int>(std::floor(topLeft.y() / tileSize));
        const int maxTileY = static_cast<int>(std::floor((topLeft.y() + mapRect.height()) / tileSize));
        const int tilesPerAxis = 1 << m_zoom;

        for (int tileY = minTileY; tileY <= maxTileY; ++tileY) {
            if (tileY < 0 || tileY >= tilesPerAxis) {
                continue;
            }
            for (int tileX = minTileX; tileX <= maxTileX; ++tileX) {
                const int wrappedX = positiveModulo(tileX, tilesPerAxis);
                const QString key = tileKey(wrappedX, tileY);
                const QPointF tileWorld(tileX * tileSize, tileY * tileSize);
                const QPointF screen = QPointF(mapRect.left(), mapRect.top()) + tileWorld - topLeft;
                const QRectF target(screen, QSizeF(tileSize, tileSize));
                const auto it = m_tiles.constFind(key);
                if (it != m_tiles.constEnd()) {
                    painter.drawPixmap(target.toRect(), it.value());
                } else {
                    painter.fillRect(target, QColor(13, 27, 36));
                    painter.setPen(QPen(QColor(30, 53, 65), 1));
                    painter.drawRect(target.adjusted(0, 0, -1, -1));
                    requestTile(wrappedX, tileY);
                }
            }
        }

        painter.setPen(QPen(QColor(255, 255, 255, 46), 1));
        painter.drawLine(mapRect.center().x(), mapRect.top(), mapRect.center().x(), mapRect.bottom());
        painter.drawLine(mapRect.left(), mapRect.center().y(), mapRect.right(), mapRect.center().y());
        drawDroneMarker(painter, mapRect.center());
    }
    painter.restore();

    painter.setPen(mutedTextColor());
    painter.setFont(QFont(QStringLiteral("Arial"), 10, QFont::DemiBold));
    painter.drawText(QRectF(12, 8, width() - 24, 20), Qt::AlignCenter, QStringLiteral("GPS MAP"));

    painter.setPen(textColor());
    painter.setFont(QFont(QStringLiteral("Arial"), 10));
    const QString coordinates = m_positionValid
        ? QStringLiteral("%1, %2  Z%3")
              .arg(m_latitudeDeg, 0, 'f', 6)
              .arg(m_longitudeDeg, 0, 'f', 6)
              .arg(m_zoom)
        : QStringLiteral("Waiting for valid GPS fix");
    painter.drawText(QRectF(14, height() - 26, width() - 28, 18), Qt::AlignLeft | Qt::AlignVCenter, coordinates);
    painter.setPen(mutedTextColor());
    painter.drawText(QRectF(14, height() - 26, width() - 28, 18), Qt::AlignRight | Qt::AlignVCenter,
                     QStringLiteral("OpenStreetMap"));
}

void MapWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}

QPointF MapWidget::geoToWorldPixel(double latitudeDeg, double longitudeDeg) const
{
    const double lat = qBound(minLatitude, latitudeDeg, maxLatitude);
    const double sinLat = qSin(qDegreesToRadians(lat));
    const double mapSize = static_cast<double>(tileSize * (1 << m_zoom));
    const double x = (longitudeDeg + 180.0) / 360.0 * mapSize;
    const double y = (0.5 - qLn((1.0 + sinLat) / (1.0 - sinLat)) / (4.0 * M_PI)) * mapSize;
    return QPointF(x, y);
}

QString MapWidget::tileKey(int x, int y) const
{
    return QStringLiteral("%1/%2/%3").arg(m_zoom).arg(x).arg(y);
}

QUrl MapWidget::tileUrl(int x, int y) const
{
    return QUrl(QStringLiteral("https://tile.openstreetmap.org/%1/%2/%3.png").arg(m_zoom).arg(x).arg(y));
}

void MapWidget::requestTile(int x, int y)
{
    const QString key = tileKey(x, y);
    if (m_tiles.contains(key) || m_pendingTiles.contains(key)) {
        return;
    }

    m_pendingTiles.insert(key);
    QNetworkRequest request(tileUrl(x, y));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("DroneAction/0.1 Qt OSM flight monitor"));
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
    auto *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, key]() {
        m_pendingTiles.remove(key);
        if (reply->error() == QNetworkReply::NoError) {
            QPixmap pixmap;
            if (pixmap.loadFromData(reply->readAll())) {
                m_tiles.insert(key, pixmap);
                update();
            }
        }
        reply->deleteLater();
    });
}

void MapWidget::drawFallback(QPainter &painter, const QRect &area)
{
    painter.fillRect(area, QColor(8, 15, 22));
    painter.setPen(QPen(QColor(31, 51, 64), 1));
    const int spacing = 36;
    for (int x = area.left(); x < area.right(); x += spacing) {
        painter.drawLine(x, area.top(), x, area.bottom());
    }
    for (int y = area.top(); y < area.bottom(); y += spacing) {
        painter.drawLine(area.left(), y, area.right(), y);
    }
    painter.setPen(mutedTextColor());
    painter.setFont(QFont(QStringLiteral("Arial"), 12, QFont::DemiBold));
    painter.drawText(area, Qt::AlignCenter, QStringLiteral("NO GPS FIX"));
}

void MapWidget::drawDroneMarker(QPainter &painter, const QPointF &screenPosition)
{
    painter.save();
    painter.translate(screenPosition);
    painter.rotate(m_headingDeg);

    QPainterPath aircraft;
    aircraft.moveTo(0, -20);
    aircraft.lineTo(12, 14);
    aircraft.lineTo(0, 8);
    aircraft.lineTo(-12, 14);
    aircraft.closeSubpath();

    painter.setBrush(QColor(255, 210, 92));
    painter.setPen(QPen(QColor(6, 12, 18), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(aircraft);
    painter.setPen(QPen(QColor(255, 210, 92, 120), 2));
    painter.drawEllipse(QPointF(0, 0), 24, 24);
    painter.restore();
}
