#include "SettingsDialog.h"

#include "IconFactory.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(const AppSettings& settings, QWidget* parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(QStringLiteral("设置"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));
    setModal(true);
    setMinimumWidth(600);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(22, 20, 22, 18);
    rootLayout->setSpacing(14);

    auto* title = new QLabel(QStringLiteral("画板配置"), this);
    title->setObjectName(QStringLiteral("DialogTitle"));
    rootLayout->addWidget(title);

    auto* hint = new QLabel(QStringLiteral("这里的设置会作为新画笔、新画板的默认值，并自动保存。"), this);
    hint->setObjectName(QStringLiteral("DialogHint"));
    rootLayout->addWidget(hint);

    auto* brushGroup = new QGroupBox(QStringLiteral("画笔默认值"), this);
    auto* brushForm = new QFormLayout(brushGroup);
    brushForm->setContentsMargins(16, 18, 16, 16);
    brushForm->setSpacing(10);

    m_brushColorButton = new QPushButton(brushGroup);
    m_brushColorButton->setIconSize(QSize(18, 18));
    brushForm->addRow(QStringLiteral("默认颜色"), m_brushColorButton);

    m_brushWidthSpin = new QSpinBox(brushGroup);
    m_brushWidthSpin->setRange(1, 64);
    m_brushWidthSpin->setSuffix(QStringLiteral(" px"));
    m_brushWidthSpin->setValue(m_settings.brushWidth);
    m_brushWidthSpin->setFixedHeight(30);
    brushForm->addRow(QStringLiteral("默认粗细"), m_brushWidthSpin);

    rootLayout->addWidget(brushGroup);

    auto* boardGroup = new QGroupBox(QStringLiteral("画板"), this);
    auto* boardForm = new QFormLayout(boardGroup);
    boardForm->setContentsMargins(16, 18, 16, 16);
    boardForm->setSpacing(10);

    auto* sizeRow = new QHBoxLayout();
    sizeRow->setSpacing(8);
    m_boardWidthSpin = new QSpinBox(boardGroup);
    m_boardWidthSpin->setRange(200, 12000);
    m_boardWidthSpin->setValue(static_cast<int>(m_settings.boardSize.width()));
    m_boardWidthSpin->setSuffix(QStringLiteral(" px"));
    m_boardWidthSpin->setFixedHeight(30);
    auto* multiply = new QLabel(QStringLiteral("×"), boardGroup);
    m_boardHeightSpin = new QSpinBox(boardGroup);
    m_boardHeightSpin->setRange(200, 12000);
    m_boardHeightSpin->setValue(static_cast<int>(m_settings.boardSize.height()));
    m_boardHeightSpin->setSuffix(QStringLiteral(" px"));
    m_boardHeightSpin->setFixedHeight(30);
    sizeRow->addWidget(m_boardWidthSpin, 1);
    sizeRow->addWidget(multiply);
    sizeRow->addWidget(m_boardHeightSpin, 1);
    boardForm->addRow(QStringLiteral("画板尺寸"), sizeRow);

    m_gridCheck = new QCheckBox(QStringLiteral("显示辅助网格"), boardGroup);
    m_gridCheck->setChecked(m_settings.showGrid);
    boardForm->addRow(QString(), m_gridCheck);

    m_canvasColorButton = new QPushButton(boardGroup);
    m_canvasColorButton->setIconSize(QSize(18, 18));
    boardForm->addRow(QStringLiteral("画布外围颜色"), m_canvasColorButton);

    auto* boardHint = new QLabel(QStringLiteral("修改画板尺寸不会移动已有图元，可通过撤销恢复。"), boardGroup);
    boardHint->setObjectName(QStringLiteral("DialogHint"));
    boardHint->setWordWrap(true);
    boardForm->addRow(QString(), boardHint);

    rootLayout->addWidget(boardGroup);

    auto* storageGroup = new QGroupBox(QStringLiteral("模板存储"), this);
    auto* storageLayout = new QHBoxLayout(storageGroup);
    storageLayout->setContentsMargins(16, 18, 16, 16);
    storageLayout->setSpacing(10);
    const QString templatesDir = m_settings.templatesDirectory.isEmpty()
        ? AppSettings::defaultTemplatesDir()
        : m_settings.templatesDirectory;
    auto* pathEdit = new QLineEdit(QDir::toNativeSeparators(templatesDir), storageGroup);
    pathEdit->setReadOnly(true);
    pathEdit->setCursorPosition(0);
    pathEdit->setToolTip(QDir::toNativeSeparators(templatesDir));
    auto* openButton = new QPushButton(QStringLiteral("打开文件夹"), storageGroup);
    storageLayout->addWidget(pathEdit, 1);
    storageLayout->addWidget(openButton);

    rootLayout->addWidget(storageGroup);
    rootLayout->addStretch();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel
                                             | QDialogButtonBox::Reset,
                                         this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    buttons->button(QDialogButtonBox::Reset)->setText(QStringLiteral("恢复默认值"));
    rootLayout->addWidget(buttons);

    refreshColorButton(m_brushColorButton, m_settings.brushColor);
    refreshColorButton(m_canvasColorButton, m_settings.canvasColor);

    connect(m_brushColorButton, &QPushButton::clicked, this, [this]() {
        QColor color = m_settings.brushColor;
        if (pickColor(color, QStringLiteral("选择默认画笔颜色"), true))
            setBrushColor(color);
    });
    connect(m_canvasColorButton, &QPushButton::clicked, this, [this]() {
        QColor color = m_settings.canvasColor;
        if (pickColor(color, QStringLiteral("选择画布外围颜色"), false))
            setCanvasColor(color);
    });
    connect(openButton, &QPushButton::clicked, this, &SettingsDialog::openTemplatesFolder);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        m_settings.brushWidth = m_brushWidthSpin->value();
        m_settings.boardSize = QSizeF(m_boardWidthSpin->value(), m_boardHeightSpin->value());
        m_settings.showGrid = m_gridCheck->isChecked();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Reset), &QPushButton::clicked,
            this, &SettingsDialog::resetToDefaults);
}

void SettingsDialog::refreshColorButton(QPushButton* button, const QColor& color)
{
    button->setIcon(makeColorIcon(color, 18));
    const QString text = color.alpha() < 255 ? color.name(QColor::HexArgb).toUpper()
                                             : color.name(QColor::HexRgb).toUpper();
    button->setText(text);
}

void SettingsDialog::setBrushColor(const QColor& color)
{
    if (!color.isValid())
        return;
    m_settings.brushColor = color;
    refreshColorButton(m_brushColorButton, m_settings.brushColor);
}

void SettingsDialog::setCanvasColor(const QColor& color)
{
    if (!color.isValid())
        return;
    m_settings.canvasColor = color;
    refreshColorButton(m_canvasColorButton, m_settings.canvasColor);
}

bool SettingsDialog::pickColor(QColor& target, const QString& title, bool allowAlpha)
{
    QColorDialog::ColorDialogOptions options;
    if (allowAlpha)
        options |= QColorDialog::ShowAlphaChannel;
    const QColor color = QColorDialog::getColor(target, this, title, options);
    if (!color.isValid())
        return false;
    target = color;
    return true;
}

void SettingsDialog::resetToDefaults()
{
    const AppSettings defaults;
    m_brushWidthSpin->setValue(defaults.brushWidth);
    m_boardWidthSpin->setValue(static_cast<int>(defaults.boardSize.width()));
    m_boardHeightSpin->setValue(static_cast<int>(defaults.boardSize.height()));
    m_gridCheck->setChecked(defaults.showGrid);
    m_settings.brushColor = defaults.brushColor;
    m_settings.canvasColor = defaults.canvasColor;
    refreshColorButton(m_brushColorButton, defaults.brushColor);
    refreshColorButton(m_canvasColorButton, defaults.canvasColor);
}

void SettingsDialog::openTemplatesFolder()
{
    const QString dir = m_settings.templatesDirectory.isEmpty()
        ? AppSettings::defaultTemplatesDir()
        : m_settings.templatesDirectory;
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}
