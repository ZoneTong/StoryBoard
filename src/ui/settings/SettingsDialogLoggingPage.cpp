#include "ui/settings/SettingsDialogLoggingPage.h"

#include "app/LogService.h"
#include "ui/settings/SettingsDialog.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QUrl>

SettingsDialogLoggingPage::SettingsDialogLoggingPage(const AppSettings& settings, QWidget* parent) : QWidget(parent)
{
    m_group = new QGroupBox(SettingsDialog::tr("Logging"), this);
    auto* form = new QFormLayout(m_group);
    form->setContentsMargins(12, 16, 12, 10);
    form->setSpacing(8);

    m_levelCombo = new QComboBox(m_group);
    m_levelCombo->setObjectName(QStringLiteral("LogLevelCombo"));
    m_levelCombo->setFixedHeight(30);
    m_levelCombo->addItem(SettingsDialog::tr("Errors only"), static_cast<int>(LogLevel::LOG_ERROR));
    m_levelCombo->addItem(SettingsDialog::tr("Errors and warnings"), static_cast<int>(LogLevel::LOG_WARNING));
    m_levelCombo->addItem(SettingsDialog::tr("Events and above"), static_cast<int>(LogLevel::LOG_EVENT));
    m_levelCombo->addItem(SettingsDialog::tr("Informational and above"), static_cast<int>(LogLevel::LOG_INFO));
    m_levelCombo->addItem(SettingsDialog::tr("Debug (verbose)"), static_cast<int>(LogLevel::LOG_DEBUG));
    // 每一项都写清额外包含什么，避免“数字越大越详细”这一步还要靠猜。
    m_levelCombo->setItemData(0, SettingsDialog::tr("Only software errors are written."), Qt::ToolTipRole);
    m_levelCombo->setItemData(
        1, SettingsDialog::tr("Errors plus warnings. Successful canvas operations and item events stay hidden."),
        Qt::ToolTipRole);
    m_levelCombo->setItemData(2, SettingsDialog::tr("Adds item creation, deletion and property changes."),
                              Qt::ToolTipRole);
    m_levelCombo->setItemData(
        3, SettingsDialog::tr("Adds successful canvas actions: new, open, save, switch and close."), Qt::ToolTipRole);
    m_levelCombo->setItemData(4, SettingsDialog::tr("Adds low-level debug output for troubleshooting."),
                              Qt::ToolTipRole);
    m_levelCombo->setCurrentIndex(m_levelCombo->findData(static_cast<int>(settings.logLevel)));
    form->addRow(SettingsDialog::tr("Log detail level"), m_levelCombo);

    auto* hint = new QLabel(
        SettingsDialog::tr("Item property changes are events. Successful canvas operations are informational."),
        m_group);
    hint->setObjectName(QStringLiteral("DialogHint"));
    hint->setWordWrap(true);
    form->addRow(hint);

    auto* pathEdit = new QLineEdit(QDir::toNativeSeparators(LogService::LogDirectory()), m_group);
    pathEdit->setReadOnly(true);
    pathEdit->setCursorPosition(0);
    auto* openButton = new QPushButton(SettingsDialog::tr("Open folder"), m_group);
    connect(openButton, &QPushButton::clicked, this, [this]() {
        const QString directory = LogService::LogDirectory();
        QDir().mkpath(directory);
        QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
    });
    auto* pathRow = new QHBoxLayout();
    pathRow->setSpacing(8);
    pathRow->addWidget(pathEdit, 1);
    pathRow->addWidget(openButton);
    form->addRow(SettingsDialog::tr("Log folder"), pathRow);
}

void SettingsDialogLoggingPage::WriteTo(AppSettings& settings) const
{
    settings.logLevel = static_cast<LogLevel>(m_levelCombo->currentData().toInt());
}

void SettingsDialogLoggingPage::Reset(const AppSettings& defaults)
{
    m_levelCombo->setCurrentIndex(m_levelCombo->findData(static_cast<int>(defaults.logLevel)));
}
