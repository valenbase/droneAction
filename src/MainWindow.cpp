#include "MainWindow.h"

#include <QDateTime>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QSplitter>
#include <QVBoxLayout>

namespace {
QString now()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
}

void setCardStyle(QWidget *widget)
{
    widget->setStyleSheet(QStringLiteral(
        "QFrame, QGroupBox { background: #101820; border: 1px solid #263645; border-radius: 8px; color: #e8eef4; }"
        "QGroupBox { margin-top: 16px; padding: 12px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; color: #8fb3d9; }"));
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_udpServer(&m_safetyGate, &m_telemetry, this)
{
    setWindowTitle(QStringLiteral("DroneAction PX4/G12 Monitor"));
    resize(1280, 820);
    setMinimumSize(1024, 680);

    auto *root = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(12);

    auto *header = new QHBoxLayout();
    auto *title = new QLabel(QStringLiteral("DroneAction"));
    title->setStyleSheet(QStringLiteral("font-size: 24px; font-weight: 700; color: #f6f8fb;"));
    header->addWidget(title);
    header->addStretch();

    auto *start = new QPushButton(QStringLiteral("Start"));
    auto *stop = new QPushButton(QStringLiteral("Stop"));
    header->addWidget(start);
    header->addWidget(stop);
    rootLayout->addLayout(header);

    auto *settings = new QGroupBox(QStringLiteral("Connection"));
    setCardStyle(settings);
    auto *form = new QFormLayout(settings);
    m_mavlinkUrl = new QLineEdit(settings);
    m_videoUrl = new QLineEdit(settings);
    m_udpBind = new QLineEdit(settings);
    form->addRow(QStringLiteral("MAVLink"), m_mavlinkUrl);
    form->addRow(QStringLiteral("RTSP"), m_videoUrl);
    form->addRow(QStringLiteral("RK3588 UDP"), m_udpBind);
    rootLayout->addWidget(settings);

    auto *splitter = new QSplitter(Qt::Horizontal, root);
    auto *left = new QWidget(splitter);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    auto *videoFrame = new QLabel(QStringLiteral("RTSP video surface\nwaiting for stream"));
    videoFrame->setAlignment(Qt::AlignCenter);
    videoFrame->setMinimumHeight(360);
    videoFrame->setStyleSheet(QStringLiteral(
        "background: #05080c; border: 1px solid #33475b; border-radius: 8px;"
        "color: #91a4b7; font-size: 20px;"));
    leftLayout->addWidget(videoFrame, 5);

    auto *telemetryGrid = new QGridLayout();
    telemetryGrid->setSpacing(10);
    telemetryGrid->addWidget(createTelemetryCard(QStringLiteral("Altitude"), &m_altitude), 0, 0);
    telemetryGrid->addWidget(createTelemetryCard(QStringLiteral("Ground speed"), &m_groundSpeed), 0, 1);
    telemetryGrid->addWidget(createTelemetryCard(QStringLiteral("Vertical speed"), &m_verticalSpeed), 0, 2);
    telemetryGrid->addWidget(createTelemetryCard(QStringLiteral("Heading"), &m_heading), 1, 0);
    telemetryGrid->addWidget(createTelemetryCard(QStringLiteral("Battery"), &m_battery), 1, 1);
    telemetryGrid->addWidget(createTelemetryCard(QStringLiteral("GPS"), &m_gps), 1, 2);
    leftLayout->addLayout(telemetryGrid);

    auto *right = new QWidget(splitter);
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(12, 0, 0, 0);

    auto *statusBox = new QGroupBox(QStringLiteral("Status"));
    setCardStyle(statusBox);
    auto *statusLayout = new QFormLayout(statusBox);
    m_linkStatus = new QLabel(QStringLiteral("offline"));
    m_videoStatus = new QLabel(QStringLiteral("offline"));
    m_mode = new QLabel(QStringLiteral("offline"));
    statusLayout->addRow(QStringLiteral("Telemetry"), m_linkStatus);
    statusLayout->addRow(QStringLiteral("Video"), m_videoStatus);
    statusLayout->addRow(QStringLiteral("Mode"), m_mode);
    rightLayout->addWidget(statusBox);

    auto *safetyBox = new QGroupBox(QStringLiteral("Safety Gate"));
    setCardStyle(safetyBox);
    auto *safetyLayout = new QVBoxLayout(safetyBox);
    m_authorizeControl = new QCheckBox(QStringLiteral("Authorize movement commands"));
    m_authorizeControl->setStyleSheet(QStringLiteral("color: #e8eef4;"));
    safetyLayout->addWidget(m_authorizeControl);
    auto *confirm = new QPushButton(QStringLiteral("Confirm high-risk action"));
    confirm->setEnabled(false);
    safetyLayout->addWidget(confirm);
    rightLayout->addWidget(safetyBox);

    m_commandLog = new QPlainTextEdit(right);
    m_commandLog->setReadOnly(true);
    m_commandLog->setPlaceholderText(QStringLiteral("RK3588 command decisions"));
    rightLayout->addWidget(m_commandLog, 2);

    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 2);
    rootLayout->addWidget(splitter, 1);

    m_eventLog = new QPlainTextEdit(root);
    m_eventLog->setReadOnly(true);
    m_eventLog->setMaximumHeight(130);
    rootLayout->addWidget(m_eventLog);

    setCentralWidget(root);
    setStyleSheet(QStringLiteral(
        "QMainWindow, QWidget { background: #071018; color: #dbe7f2; font-family: Inter, Arial, sans-serif; }"
        "QLineEdit { background: #0c1721; color: #edf4fb; border: 1px solid #30475d; border-radius: 6px; padding: 6px; }"
        "QPushButton { background: #1d6fbd; color: white; border: 0; border-radius: 6px; padding: 8px 14px; }"
        "QPushButton:disabled { background: #2b3a48; color: #7f8c99; }"
        "QPlainTextEdit { background: #08131d; color: #d8e4ef; border: 1px solid #243648; border-radius: 8px; padding: 8px; }"));

    connect(start, &QPushButton::clicked, this, &MainWindow::startSystems);
    connect(stop, &QPushButton::clicked, this, &MainWindow::stopSystems);
    connect(m_authorizeControl, &QCheckBox::toggled, this, &MainWindow::updateSafetyAuthorization);
    connect(&m_telemetry, &TelemetryService::snapshotChanged, this, &MainWindow::updateTelemetry);
    connect(&m_telemetry, &TelemetryService::statusMessageChanged, this, &MainWindow::appendStatus);
    connect(&m_video, &VideoService::statusChanged, m_videoStatus, &QLabel::setText);
    connect(&m_udpServer, &UdpCommandServer::statusChanged, this, &MainWindow::appendStatus);
    connect(&m_udpServer, &UdpCommandServer::commandRejected, this,
            [this](const QString &sender, const QString &reason) {
                m_commandLog->appendPlainText(QStringLiteral("[%1] %2 rejected: %3").arg(now(), sender, reason));
            });
    connect(&m_udpServer, &UdpCommandServer::commandObserved, this, &MainWindow::appendCommand);

    loadConfig();
}

void MainWindow::startSystems()
{
    m_config = configFromUi();
    QString error;
    if (!m_config.save(&error)) {
        QMessageBox::warning(this, QStringLiteral("Config"), error);
    }
    m_safetyGate.setConfig(m_config);
    m_telemetry.connectToEndpoint(m_config.mavlinkUrl);
    m_video.start(m_config.videoUrl);
    if (!m_udpServer.start(m_config, &error)) {
        QMessageBox::warning(this, QStringLiteral("UDP"), error);
    }
}

void MainWindow::stopSystems()
{
    m_udpServer.stop();
    m_video.stop();
    m_telemetry.disconnectFromEndpoint();
}

void MainWindow::updateTelemetry(const TelemetrySnapshot &snapshot)
{
    m_linkStatus->setText(snapshot.connected ? QStringLiteral("live") : QStringLiteral("offline"));
    m_mode->setText(snapshot.mode);
    m_altitude->setText(QStringLiteral("%1 m").arg(snapshot.altitudeM, 0, 'f', 1));
    m_groundSpeed->setText(QStringLiteral("%1 m/s").arg(snapshot.groundSpeedMps, 0, 'f', 1));
    m_verticalSpeed->setText(QStringLiteral("%1 m/s").arg(snapshot.verticalSpeedMps, 0, 'f', 2));
    m_heading->setText(QStringLiteral("%1 deg").arg(snapshot.headingDeg, 0, 'f', 0));
    m_battery->setText(QStringLiteral("%1%").arg(snapshot.batteryPercent, 0, 'f', 0));
    m_gps->setText(QStringLiteral("%1 sats").arg(snapshot.gpsSatellites));
}

void MainWindow::appendStatus(const QString &message)
{
    m_eventLog->appendPlainText(QStringLiteral("[%1] %2").arg(now(), message));
}

void MainWindow::appendCommand(const QString &sender, const DroneCommand &command, const SafetyDecision &decision)
{
    const QString line = QStringLiteral("[%1] %2 %3 -> %4 (%5)")
                             .arg(now(), sender, CommandParser::kindToString(command.kind),
                                  decision.statusText(), decision.reason);
    m_commandLog->appendPlainText(line);
}

void MainWindow::updateSafetyAuthorization(bool enabled)
{
    m_safetyGate.setControlAuthorized(enabled);
    appendStatus(enabled ? QStringLiteral("Movement command authorization enabled")
                         : QStringLiteral("Movement command authorization disabled"));
}

QWidget *MainWindow::createTelemetryCard(const QString &title, QLabel **valueLabel)
{
    auto *frame = new QFrame();
    setCardStyle(frame);
    auto *layout = new QVBoxLayout(frame);
    auto *name = new QLabel(title);
    name->setStyleSheet(QStringLiteral("color: #8fb3d9; font-size: 12px;"));
    *valueLabel = new QLabel(QStringLiteral("--"));
    (*valueLabel)->setStyleSheet(QStringLiteral("color: #f5f9fc; font-size: 24px; font-weight: 700;"));
    layout->addWidget(name);
    layout->addWidget(*valueLabel);
    return frame;
}

void MainWindow::loadConfig()
{
    QString error;
    if (!m_config.load(&error)) {
        appendStatus(QStringLiteral("Config load failed: %1").arg(error));
    }
    m_safetyGate.setConfig(m_config);
    m_mavlinkUrl->setText(m_config.mavlinkUrl);
    m_videoUrl->setText(m_config.videoUrl);
    m_udpBind->setText(QStringLiteral("%1:%2").arg(m_config.udpBindAddress).arg(m_config.udpBindPort));
}

AppConfig MainWindow::configFromUi() const
{
    AppConfig config = m_config;
    config.mavlinkUrl = m_mavlinkUrl->text().trimmed();
    config.videoUrl = m_videoUrl->text().trimmed();
    const QString bind = m_udpBind->text().trimmed();
    const int separator = bind.lastIndexOf(QLatin1Char(':'));
    if (separator > 0) {
        config.udpBindAddress = bind.left(separator);
        bool ok = false;
        const int port = bind.mid(separator + 1).toInt(&ok);
        if (ok && port > 0 && port <= 65535) {
            config.udpBindPort = static_cast<quint16>(port);
        }
    }
    return config;
}
