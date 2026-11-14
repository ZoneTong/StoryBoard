#include "SettingsDialog.h"

#include "I18n.h"
#include "SettingsDialogBrushPage.h"
#include "SettingsDialogCanvasPage.h"
#include "SettingsDialogLineArtPage.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(const AppSettings& settings, QWidget* parent) : QDialog(parent), m_settings(settings)
{
    setWindowTitle(tr("Settings"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));
    setModal(true);
    setMinimumSize(900, 560);
    resize(1060, 700);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(18, 14, 18, 12);
    rootLayout->setSpacing(10);

    auto* headerRow = new QHBoxLayout();
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(10);
    auto* title = new QLabel(tr("Board settings"), this);
    title->setObjectName(QStringLiteral("DialogTitle"));
    headerRow->addWidget(title);
    headerRow->addStretch(1);
    auto* languageLabel = new QLabel(tr("Interface language"), this);
    languageLabel->setObjectName(QStringLiteral("DialogHint"));
    headerRow->addWidget(languageLabel);
    m_languageCombo = new QComboBox(this);
    m_languageCombo->setFixedHeight(30);
    m_languageCombo->setMinimumWidth(150);
    m_languageCombo->addItem(tr("Follow system"), static_cast<int>(UiLanguage::System));
    m_languageCombo->addItem(tr("Simplified Chinese"), static_cast<int>(UiLanguage::Chinese));
    m_languageCombo->addItem(QStringLiteral("English"), static_cast<int>(UiLanguage::English));
    m_languageCombo->setCurrentIndex(m_languageCombo->findData(static_cast<int>(uiLanguageSetting())));
    m_languageCombo->setToolTip(tr("The interface updates immediately and this choice is remembered."));
    headerRow->addWidget(m_languageCombo);
    rootLayout->addLayout(headerRow);

    auto* hint = new QLabel(
        tr("These settings become the defaults for new brushes and canvases and are saved automatically."), this);
    hint->setObjectName(QStringLiteral("DialogHint"));
    rootLayout->addWidget(hint);

    m_brushPage = new SettingsDialogBrushPage(m_settings, this);
    m_canvasPage = new SettingsDialogCanvasPage(m_settings, this);
    m_lineArtPage = new SettingsDialogLineArtPage(m_settings, this);

    // 设置项变多后单列会超出屏幕：整页放进滚动区，宽屏时每行最多两列并排。
    // 线稿化那组字段最多、标签最长，单独占满一整行，避免被两列挤压导致右侧文字截断。
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("SettingsScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* content = new QWidget(scroll);
    auto* grid = new QGridLayout(content);
    grid->setContentsMargins(0, 0, 8, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(10);
    // 第一行「画笔默认值 | 画布（含存储目录）」，线稿化整组占满下一行。
    grid->addWidget(m_brushPage->group(), 0, 0, Qt::AlignTop);
    grid->addWidget(m_canvasPage->group(), 0, 1, Qt::AlignTop);
    grid->addWidget(m_lineArtPage->group(), 1, 0, 1, 2);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(2, 1);
    scroll->setWidget(content);
    rootLayout->addWidget(scroll);

    // 最小宽度跟着内容走：英文标签更长、系统 DPI 更高时也不会把最右侧的标签和按钮裁掉。
    const int contentMinWidth = content->minimumSizeHint().width();
    setMinimumWidth(qMax(900, contentMinWidth + 60));
    // 高度按内容算：能一屏放下就不开滚动；屏幕不够高时取屏幕可用高度的九成，其余交给滚动条。
    const int wantedHeight = content->sizeHint().height() + 110;
    const QRect available = screen() ? screen()->availableGeometry() : QRect(0, 0, 1920, 1080);
    const int fittedHeight = qBound(560, wantedHeight, qMax(560, available.height() - 60));
    resize(qMax(1060, minimumWidth() + 40), fittedHeight);

    auto* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Save"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    buttons->button(QDialogButtonBox::Reset)->setText(tr("Restore defaults"));
    rootLayout->addWidget(buttons);

    connect(m_brushPage, &SettingsDialogBrushPage::brushColorChosen, this, &SettingsDialog::setBrushColor);
    connect(m_canvasPage, &SettingsDialogCanvasPage::canvasColorChosen, this, &SettingsDialog::setCanvasColor);
    connect(m_canvasPage, &SettingsDialogCanvasPage::openFolderRequested, this, &SettingsDialog::openCanvasesFolder);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        m_brushPage->writeTo(m_settings);
        m_canvasPage->writeTo(m_settings);
        m_lineArtPage->writeTo(m_settings);
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Reset), &QPushButton::clicked, this, &SettingsDialog::resetToDefaults);
}

void SettingsDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (m_heightFitted) {
        return;
    }
    m_heightFitted = true;

    QScrollArea* scroll = findChild<QScrollArea*>(QStringLiteral("SettingsScroll"));
    if (!scroll || !scroll->widget()) {
        return;
    }
    // 显示之后布局才真正跑过一遍：按内容真实高度补窗口高度，屏幕不够高时仍然保留滚动条兜底。
    const QWidget* content = scroll->widget();
    const int chrome = height() - scroll->height();
    int contentHeight = content->sizeHint().height();
    if (content->hasHeightForWidth()) {
        contentHeight = qMax(contentHeight, content->heightForWidth(content->width()));
    }
    const QScreen* scr = screen();
    const int maxHeight = scr ? scr->availableGeometry().height() - 40 : contentHeight + chrome;
    const int target = qBound(minimumHeight(), contentHeight + chrome, qMax(minimumHeight(), maxHeight));
    if (target > height()) {
        resize(width(), target);
    }
}

void SettingsDialog::setBrushColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    m_settings.brushColor = color;
    m_brushPage->setBrushColor(color);
}

void SettingsDialog::setCanvasColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    m_settings.canvasColor = color;
    m_canvasPage->setCanvasColor(color);
}

void SettingsDialog::resetToDefaults()
{
    const AppSettings defaults;
    if (m_languageCombo) {
        m_languageCombo->setCurrentIndex(m_languageCombo->findData(static_cast<int>(UiLanguage::System)));
    }
    m_brushPage->reset(defaults);
    m_canvasPage->reset(defaults);
    m_lineArtPage->reset(defaults);
    m_settings.brushColor = defaults.brushColor;
    m_settings.canvasColor = defaults.canvasColor;
}

UiLanguage SettingsDialog::language() const
{
    if (!m_languageCombo) {
        return uiLanguageSetting();
    }
    return static_cast<UiLanguage>(m_languageCombo->currentData().toInt());
}

void SettingsDialog::openCanvasesFolder()
{
    const QString dir =
        m_settings.canvasesDirectory.isEmpty() ? AppSettings::defaultCanvasesDir() : m_settings.canvasesDirectory;
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}
