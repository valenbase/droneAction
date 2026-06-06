#pragma once

#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QPixmap>
#include <QSet>
#include <QWidget>

class MapWidget : public QWidget {
public:
    explicit MapWidget(QWidget *parent = nullptr);

    void setDronePosition(double latitudeDeg, double longitudeDeg, double headingDeg, bool valid);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QPointF geoToWorldPixel(double latitudeDeg, double longitudeDeg) const;
    QString tileKey(int x, int y) const;
    QUrl tileUrl(int x, int y) const;
    void requestTile(int x, int y);
    void drawFallback(QPainter &painter, const QRect &area);
    void drawDroneMarker(QPainter &painter, const QPointF &screenPosition);

    QNetworkAccessManager m_network;
    QNetworkDiskCache m_diskCache;
    QHash<QString, QPixmap> m_tiles;
    QSet<QString> m_pendingTiles;
    double m_latitudeDeg = 0.0;
    double m_longitudeDeg = 0.0;
    double m_headingDeg = 0.0;
    int m_zoom = 16;
    bool m_positionValid = false;
};
