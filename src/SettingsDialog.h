#pragma once

#include "AppConfig.h"

#include <QCheckBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QSpinBox>

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(const AppConfig &config, QWidget *parent = nullptr);

    AppConfig config() const;

private slots:
    void accept() override;

private:
    bool applyToConfig(AppConfig *config, QString *errorMessage) const;

    AppConfig m_initialConfig;
    QLineEdit *m_mavlinkUrl = nullptr;
    QLineEdit *m_videoUrl = nullptr;
    QLineEdit *m_udpBind = nullptr;
    QLineEdit *m_allowedIps = nullptr;
    QDoubleSpinBox *m_maxVelocity = nullptr;
    QDoubleSpinBox *m_maxVertical = nullptr;
    QDoubleSpinBox *m_maxYaw = nullptr;
    QSpinBox *m_maxDuration = nullptr;
    QSpinBox *m_commandStaleTimeout = nullptr;
    QSpinBox *m_controlExitTimeout = nullptr;
    QSpinBox *m_controlEntryPrestream = nullptr;
    QSpinBox *m_modeConfirmationTimeout = nullptr;
    QSpinBox *m_modeExitConfirmationTimeout = nullptr;
    QCheckBox *m_requireHighRiskConfirmation = nullptr;
    QCheckBox *m_mavlinkCommandExecution = nullptr;
};
