#include "SettingsDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
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
}

SettingsDialog::SettingsDialog(const AppConfig &config, QWidget *parent)
    : QDialog(parent)
    , m_initialConfig(config)
{
    setWindowTitle(QStringLiteral("Settings"));
    setModal(true);
    resize(560, 420);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto *interfaces = new QGroupBox(QStringLiteral("Interfaces"), this);
    setDialogCardStyle(interfaces);
    auto *interfaceForm = new QFormLayout(interfaces);
    m_mavlinkUrl = new QLineEdit(config.mavlinkUrl, interfaces);
    m_videoUrl = new QLineEdit(config.videoUrl, interfaces);
    m_udpBind = new QLineEdit(QStringLiteral("%1:%2").arg(config.udpBindAddress).arg(config.udpBindPort), interfaces);
    m_allowedIps = new QLineEdit(config.rk3588AllowedIps.join(QStringLiteral(",")), interfaces);
    interfaceForm->addRow(QStringLiteral("MAVLink"), m_mavlinkUrl);
    interfaceForm->addRow(QStringLiteral("RTSP"), m_videoUrl);
    interfaceForm->addRow(QStringLiteral("RK3588 UDP"), m_udpBind);
    interfaceForm->addRow(QStringLiteral("Allowed IPs"), m_allowedIps);
    layout->addWidget(interfaces);

    auto *safety = new QGroupBox(QStringLiteral("Safety"), this);
    setDialogCardStyle(safety);
    auto *safetyForm = new QFormLayout(safety);
    m_maxVelocity = makeDoubleSpinBox(config.maxVelocityMps, 0.1, 30.0, 0.1, safety);
    m_maxVertical = makeDoubleSpinBox(config.maxVerticalMps, 0.1, 10.0, 0.1, safety);
    m_maxYaw = makeDoubleSpinBox(config.maxYawDegS, 1.0, 180.0, 1.0, safety);
    m_maxDuration = new QSpinBox(safety);
    m_maxDuration->setRange(100, 60000);
    m_maxDuration->setSingleStep(100);
    m_maxDuration->setValue(config.maxCommandDurationMs);
    m_requireHighRiskConfirmation = new QCheckBox(QStringLiteral("Require confirmation"), safety);
    m_requireHighRiskConfirmation->setChecked(config.requireHighRiskConfirmation);
    safetyForm->addRow(QStringLiteral("Max velocity m/s"), m_maxVelocity);
    safetyForm->addRow(QStringLiteral("Max vertical m/s"), m_maxVertical);
    safetyForm->addRow(QStringLiteral("Max yaw deg/s"), m_maxYaw);
    safetyForm->addRow(QStringLiteral("Max duration ms"), m_maxDuration);
    safetyForm->addRow(QStringLiteral("High-risk actions"), m_requireHighRiskConfirmation);
    layout->addWidget(safety);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    layout->addWidget(buttons);

    setStyleSheet(QStringLiteral(
        "QDialog { background: #071018; color: #dbe7f2; font-family: Inter, Arial, sans-serif; }"
        "QLineEdit, QSpinBox, QDoubleSpinBox { background: #0c1721; color: #edf4fb; border: 1px solid #30475d; "
        "border-radius: 6px; padding: 6px; }"
        "QPushButton { background: #1d6fbd; color: white; border: 0; border-radius: 6px; padding: 8px 14px; }"
        "QPushButton[text=\"Cancel\"] { background: #253443; }"
        "QCheckBox { color: #e8eef4; }"));
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
    config->requireHighRiskConfirmation = m_requireHighRiskConfirmation->isChecked();
    return true;
}
