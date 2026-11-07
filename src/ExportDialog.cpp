#include "I18n.h"

#include "ExportDialog.h"

#include "BoardScene.h"
#include "IconFactory.h"

#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QtMath>

ExportDialog::ExportDialog(BoardScene* scene, const QString& startDirectory, QWidget* parent)
    : QDialog(parent)
    , m_scene(scene)
{
    setWindowTitle(uiText(QStringLiteral("导出画板")));
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));
    setModal(true);
    setMinimumWidth(580);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(22, 20, 22, 18);
    rootLayout->setSpacing(14);

    auto* title = new QLabel(uiText(QStringLiteral("导出当前画板")), this);
    title->setObjectName(QStringLiteral("DialogTitle"));
    rootLayout->addWidget(title);

    auto* hint = new QLabel(uiText(QStringLiteral("将画板渲染为图片或 PDF，背景与图元会一并导出（不含网格与选中框）。")), this);
    hint->setObjectName(QStringLiteral("DialogHint"));
    hint->setWordWrap(true);
    rootLayout->addWidget(hint);

    auto* group = new QGroupBox(uiText(QStringLiteral("输出设置")), this);
    auto* form = new QFormLayout(group);
    form->setContentsMargins(16, 18, 16, 16);
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_scopeCombo = new QComboBox(group);
    m_scopeCombo->addItem(uiText(QStringLiteral("画板区域")));
    m_scopeCombo->addItem(uiText(QStringLiteral("全部图元")));
    form->addRow(uiText(QStringLiteral("导出范围")), m_scopeCombo);

    m_scaleSpin = new QDoubleSpinBox(group);
    m_scaleSpin->setRange(0.25, 4.0);
    m_scaleSpin->setSingleStep(0.25);
    m_scaleSpin->setDecimals(2);
    m_scaleSpin->setValue(1.0);
    m_scaleSpin->setSuffix(QStringLiteral(" ×"));
    m_scaleSpin->setFixedHeight(30);
    form->addRow(uiText(QStringLiteral("输出比例")), m_scaleSpin);

    m_formatCombo = new QComboBox(group);
    m_formatCombo->addItem(uiText(QStringLiteral("PNG 图片")), QStringLiteral("PNG"));
    m_formatCombo->addItem(uiText(QStringLiteral("JPEG 图片")), QStringLiteral("JPEG"));
    m_formatCombo->addItem(uiText(QStringLiteral("PDF 文档")), QStringLiteral("PDF"));
    form->addRow(uiText(QStringLiteral("输出格式")), m_formatCombo);

    auto* fileRow = new QHBoxLayout();
    fileRow->setSpacing(8);
    m_pathEdit = new QLineEdit(group);
    m_pathEdit->setPlaceholderText(uiText(QStringLiteral("选择导出文件路径")));
    auto* browseButton = new QPushButton(uiText(QStringLiteral("浏览…")), group);
    fileRow->addWidget(m_pathEdit, 1);
    fileRow->addWidget(browseButton);
    form->addRow(uiText(QStringLiteral("输出文件")), fileRow);

    m_sizeLabel = new QLabel(group);
    m_sizeLabel->setObjectName(QStringLiteral("DialogHint"));
    form->addRow(uiText(QStringLiteral("输出尺寸")), m_sizeLabel);

    rootLayout->addWidget(group);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(uiText(QStringLiteral("导出")));
    buttons->button(QDialogButtonBox::Ok)->setIcon(makeIcon(IconId::Export, QColor(255, 255, 255), 18));
    buttons->button(QDialogButtonBox::Cancel)->setText(uiText(QStringLiteral("取消")));
    rootLayout->addWidget(buttons);

    QString directory = startDirectory;
    if (directory.isEmpty())
        directory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (directory.isEmpty())
        directory = QDir::homePath();
    QDir().mkpath(directory);

    const QString fileName = QStringLiteral("StoryBoard_%1.png")
                                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    m_pathEdit->setText(QDir(directory).filePath(fileName));
    m_pathEdit->setCursorPosition(0);
    m_pathEdit->setToolTip(m_pathEdit->text());

    connect(m_scopeCombo, &QComboBox::currentIndexChanged, this, [this]() { updateEstimatedSize(); });
    connect(m_scaleSpin, &QDoubleSpinBox::valueChanged, this, [this]() { updateEstimatedSize(); });
    connect(m_formatCombo, &QComboBox::currentIndexChanged, this, [this]() {
        updatePathExtension();
        updateEstimatedSize();
    });
    connect(browseButton, &QPushButton::clicked, this, &ExportDialog::browseForFile);
    connect(buttons, &QDialogButtonBox::accepted, this, &ExportDialog::validateAndAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updateEstimatedSize();
}

QRectF ExportDialog::selectedArea() const
{
    if (m_scopeCombo->currentIndex() == 1) {
        QRectF content = m_scene->contentRect();
        if (content.isEmpty())
            content = m_scene->boardRect();
        return content.adjusted(-24.0, -24.0, 24.0, 24.0);
    }
    return m_scene->boardRect();
}

QString ExportDialog::selectedFormat() const
{
    return m_formatCombo->currentData().toString();
}

QString ExportDialog::fileFilter() const
{
    const QString format = selectedFormat();
    if (format == QStringLiteral("JPEG"))
        return uiText(QStringLiteral("JPEG 图片 (*.jpg *.jpeg)"));
    if (format == QStringLiteral("PDF"))
        return uiText(QStringLiteral("PDF 文档 (*.pdf)"));
    return uiText(QStringLiteral("PNG 图片 (*.png)"));
}

ExportDialog::Options ExportDialog::options() const
{
    Options result;
    result.area = selectedArea();
    result.scale = m_scaleSpin->value();
    result.format = selectedFormat();
    result.filePath = m_pathEdit->text().trimmed();
    return result;
}

void ExportDialog::updateEstimatedSize()
{
    const QRectF area = selectedArea();
    const qreal scale = m_scaleSpin->value();
    int width = qMax(1, qCeil(area.width() * scale));
    int height = qMax(1, qCeil(area.height() * scale));
    constexpr int maxSide = 12000;
    if (width > maxSide || height > maxSide) {
        const qreal factor = qMin(static_cast<qreal>(maxSide) / width,
                                  static_cast<qreal>(maxSide) / height);
        width = qMax(1, qCeil(width * factor));
        height = qMax(1, qCeil(height * factor));
    }
    m_sizeLabel->setText(QStringLiteral("%1 × %2 px").arg(width).arg(height));
}

void ExportDialog::updatePathExtension()
{
    const QString format = selectedFormat();
    const QString suffix = format == QStringLiteral("JPEG")
        ? QStringLiteral("jpg")
        : format.toLower();

    QString path = m_pathEdit->text().trimmed();
    if (path.isEmpty())
        return;

    const QFileInfo info(path);
    const QString currentSuffix = info.suffix().toLower();
    const bool imageSuffix = currentSuffix == QStringLiteral("png")
        || currentSuffix == QStringLiteral("jpg")
        || currentSuffix == QStringLiteral("jpeg")
        || currentSuffix == QStringLiteral("bmp")
        || currentSuffix == QStringLiteral("pdf");
    if (!imageSuffix) {
        m_pathEdit->setText(path + QLatin1Char('.') + suffix);
        m_pathEdit->setCursorPosition(0);
        return;
    }

    if (currentSuffix != suffix && !(format == QStringLiteral("JPEG") && currentSuffix == QStringLiteral("jpeg"))) {
        path.chop(currentSuffix.size());
        m_pathEdit->setText(path + suffix);
        m_pathEdit->setCursorPosition(0);
    }
}

void ExportDialog::browseForFile()
{
    const QString current = m_pathEdit->text().trimmed();
    const QString directory = current.isEmpty() ? QString()
                                                : QFileInfo(current).absolutePath();
    QString path = QFileDialog::getSaveFileName(this, uiText(QStringLiteral("导出画板")), directory,
                                                fileFilter());
    if (path.isEmpty())
        return;
    QFileInfo info(path);
    if (info.suffix().isEmpty()) {
        const QString suffix = selectedFormat() == QStringLiteral("JPEG")
            ? QStringLiteral("jpg")
            : selectedFormat().toLower();
        path += QLatin1Char('.') + suffix;
    }
    m_pathEdit->setText(path);
    m_pathEdit->setCursorPosition(0);
    m_pathEdit->setToolTip(path);
}

void ExportDialog::validateAndAccept()
{
    QString path = m_pathEdit->text().trimmed();
    if (path.isEmpty()) {
        QMessageBox::warning(this, uiText(QStringLiteral("导出画板")), uiText(QStringLiteral("请先选择导出文件路径。")));
        return;
    }

    const QString expectedSuffix = selectedFormat() == QStringLiteral("JPEG")
        ? QStringLiteral("jpg")
        : selectedFormat().toLower();
    QFileInfo info(path);
    const QString currentSuffix = info.suffix().toLower();
    const bool suffixMatches = currentSuffix == expectedSuffix
        || (selectedFormat() == QStringLiteral("JPEG") && currentSuffix == QStringLiteral("jpeg"));
    if (!suffixMatches) {
        if (currentSuffix.isEmpty()) {
            path += QLatin1Char('.') + expectedSuffix;
        } else {
            path.chop(currentSuffix.size());
            path += expectedSuffix;
        }
        m_pathEdit->setText(path);
        m_pathEdit->setCursorPosition(0);
        m_pathEdit->setToolTip(path);
        info = QFileInfo(path);
    }

    if (!QDir().mkpath(info.absolutePath())) {
        QMessageBox::warning(this, uiText(QStringLiteral("导出画板")),
                             uiText(QStringLiteral("无法创建输出目录：\n%1")).arg(info.absolutePath()));
        return;
    }

    if (info.exists()) {
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this, uiText(QStringLiteral("覆盖文件")),
            uiText(QStringLiteral("文件已存在，是否覆盖？\n%1")).arg(QDir::toNativeSeparators(path)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    accept();
}
