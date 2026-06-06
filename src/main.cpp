#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("DroneAction"));
    QApplication::setOrganizationName(QStringLiteral("valenbase"));
    MainWindow window;
    window.show();
    return QApplication::exec();
}
