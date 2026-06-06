#include "AppConfig.h"
#include "MainWindow.h"
#include "RunModeDialog.h"

#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("DroneAction"));
    QApplication::setOrganizationName(QStringLiteral("valenbase"));

    AppConfig config;
    QString error;
    if (!config.load(&error)) {
        QMessageBox::warning(nullptr, QStringLiteral("Config"), error);
    }

    RunModeDialog runModeDialog(config.runMode);
    if (runModeDialog.exec() != QDialog::Accepted) {
        return 0;
    }

    const AppRunMode selectedMode = runModeDialog.selectedMode();
    if (selectedMode != config.runMode) {
        config.applyRunModeDefaults(selectedMode);
    } else {
        config.runMode = selectedMode;
    }
    if (!config.save(&error)) {
        QMessageBox::warning(nullptr, QStringLiteral("Config"), error);
    }

    MainWindow window(config);
    window.show();
    return QApplication::exec();
}
