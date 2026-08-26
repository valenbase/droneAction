#include "RunModeDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
QPushButton *modeButton(const QString &title, const QString &detail, bool selected, QWidget *parent)
{
    auto *button = new QPushButton(QStringLiteral("%1\n%2").arg(title, detail), parent);
    button->setMinimumSize(220, 96);
    button->setCheckable(true);
    button->setChecked(selected);
    button->setStyleSheet(QStringLiteral(
        "QPushButton { background: #101820; color: #e8eef4; border: 1px solid #30475d; border-radius: 8px; "
        "font-size: 16px; font-weight: 700; padding: 12px; text-align: left; }"
        "QPushButton:checked { border: 2px solid #23c0a4; background: #102a2b; }"
        "QPushButton:hover { background: #142638; }"));
    return button;
}
}

RunModeDialog::RunModeDialog(AppRunMode currentMode, QWidget *parent)
    : QDialog(parent)
    , m_selectedMode(currentMode)
{
    setWindowTitle(QStringLiteral("Run Mode"));
    setModal(true);
    setFixedSize(560, 250);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("Select Runtime"), this);
    title->setStyleSheet(QStringLiteral("font-size: 22px; font-weight: 700; color: #f6f8fb;"));
    layout->addWidget(title);

    auto *buttons = new QHBoxLayout();
    auto *sitl = modeButton(QStringLiteral("Simulation"),
                            QStringLiteral("Local test profile"),
                            currentMode == AppRunMode::Simulation,
                            this);
    auto *real = modeButton(QStringLiteral("Real aircraft"),
                            QStringLiteral("G12 shared-network profile"),
                            currentMode == AppRunMode::RealAircraft,
                            this);
    buttons->addWidget(sitl);
    buttons->addWidget(real);
    layout->addLayout(buttons);

    connect(sitl, &QPushButton::clicked, this, [this]() { choose(AppRunMode::Simulation); });
    connect(real, &QPushButton::clicked, this, [this]() { choose(AppRunMode::RealAircraft); });

    setStyleSheet(QStringLiteral(
        "QDialog { background: #071018; color: #dbe7f2; font-family: Inter, Arial, sans-serif; }"));
}

AppRunMode RunModeDialog::selectedMode() const
{
    return m_selectedMode;
}

void RunModeDialog::choose(AppRunMode mode)
{
    m_selectedMode = mode;
    accept();
}
