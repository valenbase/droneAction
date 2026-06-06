#include "TelemetryService.h"

#include <QtMath>
#include <cmath>

TelemetryService::TelemetryService(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(500);
    connect(&m_timer, &QTimer::timeout, this, &TelemetryService::updateSimulation);
}

void TelemetryService::connectToEndpoint(const QString &mavlinkUrl)
{
    m_endpoint = mavlinkUrl;
    m_snapshot.connected = true;
    m_snapshot.armed = false;
    m_snapshot.mode = QStringLiteral("SIMULATED G12 LINK");
    m_snapshot.batteryPercent = 96.0;
    m_snapshot.gpsSatellites = 14;
    m_timer.start();
    emit statusMessageChanged(QStringLiteral("Telemetry endpoint configured: %1").arg(mavlinkUrl));
    emit snapshotChanged(m_snapshot);
}

void TelemetryService::disconnectFromEndpoint()
{
    m_timer.stop();
    m_snapshot = {};
    m_snapshot.mode = QStringLiteral("offline");
    emit statusMessageChanged(QStringLiteral("Telemetry offline"));
    emit snapshotChanged(m_snapshot);
}

TelemetrySnapshot TelemetryService::snapshot() const
{
    return m_snapshot;
}

QString TelemetryService::endpoint() const
{
    return m_endpoint;
}

void TelemetryService::updateSimulation()
{
    ++m_tick;
    const double t = static_cast<double>(m_tick) / 10.0;
    m_snapshot.altitudeM = 24.0 + 2.5 * qSin(t);
    m_snapshot.groundSpeedMps = 5.5 + 1.2 * qCos(t / 2.0);
    m_snapshot.verticalSpeedMps = 0.25 * qSin(t * 1.7);
    m_snapshot.headingDeg = std::fmod(m_snapshot.headingDeg + 2.0, 360.0);
    m_snapshot.batteryPercent = qMax(15.0, m_snapshot.batteryPercent - 0.02);
    emit snapshotChanged(m_snapshot);
}
