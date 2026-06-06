#pragma once

#include "AppConfig.h"

#include <QDialog>

class RunModeDialog : public QDialog {
    Q_OBJECT

public:
    explicit RunModeDialog(AppRunMode currentMode, QWidget *parent = nullptr);

    AppRunMode selectedMode() const;

private:
    void choose(AppRunMode mode);

    AppRunMode m_selectedMode = AppRunMode::RealAircraft;
};
