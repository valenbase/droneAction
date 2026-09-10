#include "SettingsDialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QtTest/QtTest>

class SettingsDialogTests : public QObject {
    Q_OBJECT
private slots:
    void timingControlsRemainAccessibleOnSmallWindow() {
        SettingsDialog dialog{AppConfig{}};
        dialog.resize(540, 520);
        dialog.show();
        QTest::qWait(50);
        const auto buttons = dialog.findChild<QDialogButtonBox *>();
        const auto scroll = dialog.findChild<QScrollArea *>();
        QVERIFY(buttons);
        QVERIFY(scroll);
        const auto save = buttons->button(QDialogButtonBox::Save);
        QVERIFY(dialog.rect().contains(save->mapTo(&dialog, save->rect().center())));
        QCOMPARE(dialog.findChildren<QSpinBox *>().size(), 6);
        QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
        scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
        QTest::qWait(50);
        const auto timing = dialog.findChildren<QSpinBox *>().last();
        QVERIFY(scroll->viewport()->rect().contains(timing->mapTo(scroll->viewport(), timing->rect().center())));
        const auto output = qEnvironmentVariable("DRONEACTION_SCREENSHOT_DIR");
        if (!output.isEmpty()) {
            QDir().mkpath(output);
            QVERIFY(dialog.grab().save(output + QStringLiteral("/settings-small.png")));
        }
    }
};

QTEST_MAIN(SettingsDialogTests)
#include "test_settings_dialog.moc"
