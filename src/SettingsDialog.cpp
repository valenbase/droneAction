#include "SettingsDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {
void setDialogCardStyle(QWidget *widget)
{
    widget->setStyleSheet(QStringLiteral(
        "QGroupBox { background: #101820; border: 1px solid #263645; border-radius: 8px; color: #e8eef4; "
        "margin-top: 16px; padding: 12px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; color: #8fb3d9; }"));
}

QDoubleSpinBox *makeDoubleSpinBox(double value, double minimum, double maximum, double step, QWidget *parent)
{
    auto *spin = new QDoubleSpinBox(parent);
    spin->setRange(minimum, maximum);
    spin->setSingleStep(step);
    spin->setDecimals(1);
    spin->setValue(value);
    return spin;
}

QSpinBox *makeMillisecondSpinBox(int value, int minimum, int maximum, QWidget *parent)
{
    auto *spin = new QSpinBox(parent);
    spin->setRange(minimum, maximum);
    spin->setSingleStep(100);
    spin->setSuffix(QStringLiteral(" ms"));
    spin->setValue(value);
    return spin;
}
}

SettingsDialog::SettingsDialog(const AppConfig &config, QWidget *parent)
    : QDialog(parent)
    , m_initialConfig(config)
{
    setWindowTitle(QStringLiteral("Settings"));
    setModal(true);
    resize(600, 680);
    setMinimumSize(540, 520);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget(scrollArea);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 6, 0);
    contentLayout->setSpacing(12);

    auto *interfaces = new QGroupBox(QStringLiteral("Interfaces"), content);
    setDialogCardStyle(interfaces);
    auto *interfaceForm = new QFormLayout(interfaces);
    m_mavlinkUrl = new QLineEdit(config.mavlinkUrl, interfaces);
    m_videoUrl = new QLineEdit(config.videoUrl, interfaces);
    m_udpBind = new QLineEdit(QStringLiteral("%1:%2").arg(config.udpBindAddress).arg(config.udpBindPort), interfaces);
    m_allowedIps = new QLineEdit(config.rk3588AllowedIps.join(QStringLiteral(",")), interfaces);
    interfaceForm->addRow(QStringLiteral("Vehicle Link"), m_mavlinkUrl);
    interfaceForm->addRow(QStringLiteral("RTSP"), m_videoUrl);
    interfaceForm->addRow(QStringLiteral("RK3588 UDP"), m_udpBind);
    interfaceForm->addRow(QStringLiteral("Allowed IPs"), m_allowedIps);
    contentLayout->addWidget(interfaces);

    auto *safety = new QGroupBox(QStringLiteral("Safety"), content);
    setDialogCardStyle(safety);
    auto *safetyForm = new QFormLayout(safety);
    m_maxVelocity = makeDoubleSpinBox(config.maxVelocityMps, 0.1, 30.0, 0.1, safety);
    m_maxVertical = makeDoubleSpinBox(config.maxVerticalMps, 0.1, 10.0, 0.1, safety);
    m_maxYaw = makeDoubleSpinBox(config.maxYawDegS, 1.0, 180.0, 1.0, safety);
    m_maxDuration = makeMillisecondSpinBox(
        config.maxCommandDurationMs, 100, AppConfig::UiMaxCommandDurationMs, safety);
    m_requireHighRiskConfirmation = new QCheckBox(QStringLiteral("Require confirmation"), safety);
    m_requireHighRiskConfirmation->setChecked(config.requireHighRiskConfirmation);
    m_mavlinkCommandExecution = new QCheckBox(QStringLiteral("Enable command execution"), safety);
    m_mavlinkCommandExecution->setChecked(config.mavlinkCommandExecutionEnabled);
    safetyForm->addRow(QStringLiteral("Max velocity m/s"), m_maxVelocity);
    safetyForm->addRow(QStringLiteral("Max vertical m/s"), m_maxVertical);
    safetyForm->addRow(QStringLiteral("Max yaw deg/s"), m_maxYaw);
    safetyForm->addRow(QStringLiteral("Max duration ms"), m_maxDuration);
    safetyForm->addRow(QStringLiteral("High-risk actions"), m_requireHighRiskConfirmation);
    safetyForm->addRow(QStringLiteral("Remote execution"), m_mavlinkCommandExecution);
    contentLayout->addWidget(safety);

    auto *controlTiming = new QGroupBox(QStringLiteral("Control timing"), content);
    setDialogCardStyle(controlTiming);
    auto *timingForm = new QFormLayout(controlTiming);
    m_commandStaleTimeout = makeMillisecondSpinBox(
        config.commandStaleTimeoutMs,
        AppConfig::MinCommandStaleTimeoutMs,
        AppConfig::MaxCommandStaleTimeoutMs,
        controlTiming);
    m_commandStaleTimeout->setToolTip(
        QStringLiteral("Send zero velocity when no fresh command arrives within this time."));
    m_controlExitTimeout = makeMillisecondSpinBox(
        config.controlExitTimeoutMs,
        AppConfig::MinControlExitTimeoutMs,
        AppConfig::MaxControlExitTimeoutMs,
        controlTiming);
    m_controlExitTimeout->setToolTip(
        QStringLiteral("Cancel entry when guided control is not confirmed and commands stay quiet. "
                       "After guided control is active, a quiet stream hovers at zero velocity."));
    m_controlEntryPrestream = makeMillisecondSpinBox(
        config.controlEntryPrestreamMs,
        AppConfig::MinControlEntryPrestreamMs,
        AppConfig::MaxControlEntryPrestreamMs,
        controlTiming);
    m_modeConfirmationTimeout = makeMillisecondSpinBox(
        config.modeConfirmationTimeoutMs,
        AppConfig::MinModeConfirmationTimeoutMs,
        AppConfig::MaxModeConfirmationTimeoutMs,
        controlTiming);
    m_modeExitConfirmationTimeout = makeMillisecondSpinBox(
        config.modeExitConfirmationTimeoutMs,
        AppConfig::MinModeConfirmationTimeoutMs,
        AppConfig::MaxModeConfirmationTimeoutMs,
        controlTiming);
    timingForm->addRow(QStringLiteral("No-update stop delay"), m_commandStaleTimeout);
    timingForm->addRow(QStringLiteral("Entry abort delay"), m_controlExitTimeout);
    timingForm->addRow(QStringLiteral("Entry stream warm-up"), m_controlEntryPrestream);
    timingForm->addRow(QStringLiteral("Entry confirmation"), m_modeConfirmationTimeout);
    timingForm->addRow(QStringLiteral("Exit confirmation"), m_modeExitConfirmationTimeout);
    contentLayout->addWidget(controlTiming);
    contentLayout->addStretch();
    scrollArea->setWidget(content);
    layout->addWidget(scrollArea, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    layout->addWidget(buttons);

    setStyleSheet(QStringLiteral(
        "QDialog, QScrollArea, QScrollArea > QWidget > QWidget { background: #071018; color: #dbe7f2; "
        "font-family: Inter, Arial, sans-serif; }"
        "QLabel { color: #dbe7f2; background: transparent; }"
        "QLineEdit, QSpinBox, QDoubleSpinBox { background: #0c1721; color: #edf4fb; border: 1px solid #30475d; "
        "border-radius: 6px; padding: 6px; }"
        "QPushButton { background: #1d6fbd; color: white; border: 0; border-radius: 6px; padding: 8px 14px; }"
        "QPushButton[text=\"Cancel\"] { background: #253443; }"
        "QCheckBox { color: #e8eef4; spacing: 8px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; border: 2px solid #8fb3d9; "
        "border-radius: 3px; background: #071018; }"
        "QCheckBox::indicator:checked { background: #23c0a4; border-color: #8fffea; }"));
}

AppConfig SettingsDialog::config() const
{
    AppConfig updated = m_initialConfig;
    QString ignored;
    applyToConfig(&updated, &ignored);
    return updated;
}

void SettingsDialog::accept()
{
    AppConfig updated = m_initialConfig;
    QString error;
    if (!applyToConfig(&updated, &error)) {
        QMessageBox::warning(this, QStringLiteral("Settings"), error);
        return;
    }

    m_initialConfig = updated;
    QDialog::accept();
}

bool SettingsDialog::applyToConfig(AppConfig *config, QString *errorMessage) const
{
    const QString bind = m_udpBind->text().trimmed();
    const int separator = bind.lastIndexOf(QLatin1Char(':'));
    if (separator <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("RK3588 UDP must use address:port format.");
        }
        return false;
    }

    bool portOk = false;
    const int port = bind.mid(separator + 1).toInt(&portOk);
    if (!portOk || port <= 0 || port > 65535) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("RK3588 UDP port must be between 1 and 65535.");
        }
        return false;
    }

    config->mavlinkUrl = m_mavlinkUrl->text().trimmed();
    config->videoUrl = m_videoUrl->text().trimmed();
    config->udpBindAddress = bind.left(separator).trimmed();
    config->udpBindPort = static_cast<quint16>(port);
    config->rk3588AllowedIps.clear();
    for (const auto &entry : m_allowedIps->text().split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        config->rk3588AllowedIps.append(entry.trimmed());
    }
    config->maxVelocityMps = m_maxVelocity->value();
    config->maxVerticalMps = m_maxVertical->value();
    config->maxYawDegS = m_maxYaw->value();
    config->maxCommandDurationMs = m_maxDuration->value();
    config->commandStaleTimeoutMs = m_commandStaleTimeout->value();
    config->controlExitTimeoutMs = m_controlExitTimeout->value();
    config->controlEntryPrestreamMs = m_controlEntryPrestream->value();
    config->modeConfirmationTimeoutMs = m_modeConfirmationTimeout->value();
    config->modeExitConfirmationTimeoutMs = m_modeExitConfirmationTimeout->value();
    if (config->controlExitTimeoutMs <= config->commandStaleTimeoutMs) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                "Entry abort delay must be greater than the no-update stop delay.");
        }
        return false;
    }
    config->normalizeControlTiming();
    config->requireHighRiskConfirmation = m_requireHighRiskConfirmation->isChecked();
    config->mavlinkCommandExecutionEnabled = m_mavlinkCommandExecution->isChecked();
    return true;
}
