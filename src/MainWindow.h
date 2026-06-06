#pragma once

#include "AppConfig.h"
#include "InstrumentWidgets.h"
#include "SafetyGate.h"
#include "TelemetryService.h"
#include "UdpCommandServer.h"
#include "VideoService.h"

#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QCheckBox>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void startSystems();
    void stopSystems();
    void updateTelemetry(const TelemetrySnapshot &snapshot);
    void appendStatus(const QString &message);
    void appendCommand(const QString &sender, const DroneCommand &command, const SafetyDecision &decision);
    void updateSafetyAuthorization(bool enabled);

private:
    void loadConfig();
    AppConfig configFromUi() const;

    AppConfig m_config;
    SafetyGate m_safetyGate;
    TelemetryService m_telemetry;
    VideoService m_video;
    UdpCommandServer m_udpServer;

    QLineEdit *m_mavlinkUrl = nullptr;
    QLineEdit *m_videoUrl = nullptr;
    QLineEdit *m_udpBind = nullptr;
    QCheckBox *m_authorizeControl = nullptr;
    QLabel *m_linkStatus = nullptr;
    QLabel *m_videoStatus = nullptr;
    AttitudeIndicatorWidget *m_attitude = nullptr;
    CircularGaugeWidget *m_altitude = nullptr;
    CircularGaugeWidget *m_groundSpeed = nullptr;
    CircularGaugeWidget *m_verticalSpeed = nullptr;
    CompassWidget *m_heading = nullptr;
    CircularGaugeWidget *m_battery = nullptr;
    CircularGaugeWidget *m_gps = nullptr;
    QLabel *m_mode = nullptr;
    QPlainTextEdit *m_eventLog = nullptr;
    QPlainTextEdit *m_commandLog = nullptr;
};
