#include "ExportDialog.h"

#include "CanvasScene.h"
#include "I18n.h"
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
#include <QRegularExpression>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QtMath>

ExportDialog::ExportDialog(CanvasScene* scene, const QString& startDirectory, QWidget* parent)
    : QDialog(parent), m_scene(scene)
{
    setWindowTitle(tr("Export canvas"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));
    setModal(true);
    setMinimumWidth(580);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(22, 20, 22, 18);
    rootLayout->setSpacing(14);

    auto* title = new QLabel(tr("Export the current canvas"), this);
    title->setObjectName(QStringLiteral("DialogTitle"));
    rootLayout->addWidget(title);

    auto* hint = new QLabel(tr("Render the canvas to an image or PDF. The background and items are included (grid and "
                               "selection outlines are not)."),
                            this);
    hint->setObjectName(QStringLiteral("DialogHint"));
    hint->setWordWrap(true);
    rootLayout->addWidget(hint);

    auto* group = new QGroupBox(tr("Output settings"), this);
    auto* form = new QFormLayout(group);
    form->setContentsMargins(16, 18, 16, 16);
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_scopeCombo = new QComboBox(group);
    m_scopeCombo->addItem(tr("Canvas area"));
    m_scopeCombo->addItem(tr("All items"));
    form->addRow(tr("Export area"), m_scopeCombo);

    m_scaleSpin = new QDoubleSpinBox(group);
    m_scaleSpin->setRange(0.25, 4.0);
    m_scaleSpin->setSingleStep(0.25);
    m_scaleSpin->setDecimals(2);
    m_scaleSpin->setValue(1.0);
    m_scaleSpin->setSuffix(QStringLiteral(" ×"));
    m_scaleSpin->setFixedHeight(30);
    form->addRow(tr("Output scale"), m_scaleSpin);

    m_formatCombo = new QComboBox(group);
    m_formatCombo->addItem(tr("PNG image"), QStringLiteral("PNG"));
    m_formatCombo->addItem(tr("JPEG image"), QStringLiteral("JPEG"));
    m_formatCombo->addItem(tr("PDF document"), QStringLiteral("PDF"));
    form->addRow(tr("Output format"), m_formatCombo);

    m_layerModeCombo = new QComboBox(group);
    m_layerModeCombo->setObjectName(QStringLiteral("ExportLayerModeCombo"));
    m_layerModeCombo->addItem(tr("Merge visible layers (one image)"), false);
    m_layerModeCombo->addItem(tr("Each visible layer (one file per layer)"), true);
    form->addRow(tr("Export mode"), m_layerModeCombo);

    auto* fileRow = new QHBoxLayout();
    fileRow->setSpacing(8);
    m_pathEdit = new QLineEdit(group);
    m_pathEdit->setPlaceholderText(tr("Choose the export file path"));
    auto* browseButton = new QPushButton(tr("Browse…"), group);
    fileRow->addWidget(m_pathEdit, 1);
    fileRow->addWidget(browseButton);
    form->addRow(tr("Output file"), fileRow);

    m_sizeLabel = new QLabel(group);
    m_sizeLabel->setObjectName(QStringLiteral("DialogHint"));
    form->addRow(tr("Output size"), m_sizeLabel);

    rootLayout->addWidget(group);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Export"));
    buttons->button(QDialogButtonBox::Ok)->setIcon(makeIcon(IconId::Export, QColor(255, 255, 255), 18));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    rootLayout->addWidget(buttons);

    QString directory = startDirectory;
    if (directory.isEmpty()) {
        directory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }
    if (directory.isEmpty()) {
        directory = QDir::homePath();
    }
    QDir().mkpath(directory);

    const QString fileName = QStringLiteral("StoryBoard_%1.png")
                                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    m_pathEdit->setText(QDir(directory).filePath(fileName));
    m_pathEdit->setCursorPosition(0);
    m_pathEdit->setToolTip(m_pathEdit->text());

    connect(m_scopeCombo, &QComboBox::currentIndexChanged, this, [this]() { updateEstimatedSize(); });
    connect(m_layerModeCombo, &QComboBox::currentIndexChanged, this, [this]() {
        m_pathEdit->setToolTip(separateLayersSelected() ? tr("Base path: each visible layer gets its own file")
                                                        : m_pathEdit->text());
        updateEstimatedSize();
    });
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
        if (content.isEmpty()) {
            content = m_scene->canvasRect();
        }
        return content.adjusted(-24.0, -24.0, 24.0, 24.0);
    }
    return m_scene->canvasRect();
}

QString ExportDialog::selectedFormat() const
{
    return m_formatCombo->currentData().toString();
}

QString ExportDialog::fileFilter() const
{
    const QString format = selectedFormat();
    if (format == QStringLiteral("JPEG")) {
        return tr("JPEG images (*.jpg *.jpeg)");
    }
    if (format == QStringLiteral("PDF")) {
        return tr("PDF documents (*.pdf)");
    }
    return tr("PNG images (*.png)");
}

bool ExportDialog::separateLayersSelected() const
{
    return m_layerModeCombo && m_layerModeCombo->currentData().toBool();
}

QString ExportDialog::formatSuffix(const QString& format)
{
    return format == QStringLiteral("JPEG") ? QStringLiteral("jpg") : format.toLower();
}

QString ExportDialog::layerFileComponent(const QString& name, int layerId)
{
    QString clean = name.trimmed();
    clean.replace(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*\\x00-\\x1F]")), QStringLiteral("_"));
    clean = clean.simplified();
    while (!clean.isEmpty() && (clean.endsWith(QLatin1Char('.')) || clean.endsWith(QLatin1Char(' ')))) {
        clean.chop(1);
    }
    if (clean.isEmpty()) {
        clean = QStringLiteral("Layer%1").arg(layerId);
    }
    return clean.left(40);
}

QStringList ExportDialog::outputFilePaths() const
{
    const QString basePath = m_pathEdit->text().trimmed();
    if (basePath.isEmpty()) {
        return QStringList();
    }

    if (!separateLayersSelected()) {
        return QStringList{basePath};
    }

    QStringList paths;
    if (!m_scene) {
        return paths;
    }

    const QFileInfo baseInfo(basePath);
    const QString baseName =
        baseInfo.completeBaseName().isEmpty() ? QStringLiteral("StoryBoard") : baseInfo.completeBaseName();
    const QString suffix = formatSuffix(selectedFormat());
    const QDir directory(baseInfo.absolutePath());

    for (const CanvasLayer& layer : m_scene->layers()) {
        if (!layer.visible) {
            continue;
        }
        paths.append(directory.filePath(QStringLiteral("%1_L%2_%3.%4")
                                            .arg(baseName)
                                            .arg(layer.id)
                                            .arg(layerFileComponent(layer.name, layer.id), suffix)));
    }
    return paths;
}

ExportDialog::Options ExportDialog::options() const
{
    Options result;
    result.area = selectedArea();
    result.scale = m_scaleSpin->value();
    result.format = selectedFormat();
    result.separateLayers = separateLayersSelected();
    result.filePath = m_pathEdit->text().trimmed();
    result.filePaths = outputFilePaths();
    if (result.separateLayers && m_scene) {
        for (const CanvasLayer& layer : m_scene->layers()) {
            if (layer.visible) {
                result.layerIds.append(layer.id);
            }
        }
    }
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
        const qreal factor = qMin(static_cast<qreal>(maxSide) / width, static_cast<qreal>(maxSide) / height);
        width = qMax(1, qCeil(width * factor));
        height = qMax(1, qCeil(height * factor));
    }
    QString text = QStringLiteral("%1 × %2 px").arg(width).arg(height);
    if (separateLayersSelected()) {
        int count = 0;
        if (m_scene) {
            for (const CanvasLayer& layer : m_scene->layers()) {
                if (layer.visible) {
                    ++count;
                }
            }
        }
        text += tr(" · %1 files").arg(count);
    }
    m_sizeLabel->setText(text);
}

void ExportDialog::updatePathExtension()
{
    const QString format = selectedFormat();
    const QString suffix = format == QStringLiteral("JPEG") ? QStringLiteral("jpg") : format.toLower();

    QString path = m_pathEdit->text().trimmed();
    if (path.isEmpty()) {
        return;
    }

    const QFileInfo info(path);
    const QString currentSuffix = info.suffix().toLower();
    const bool imageSuffix = currentSuffix == QStringLiteral("png") || currentSuffix == QStringLiteral("jpg") ||
                             currentSuffix == QStringLiteral("jpeg") || currentSuffix == QStringLiteral("bmp") ||
                             currentSuffix == QStringLiteral("pdf");
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
    const QString directory = current.isEmpty() ? QString() : QFileInfo(current).absolutePath();
    QString path = QFileDialog::getSaveFileName(this, tr("Export canvas"), directory, fileFilter());
    if (path.isEmpty()) {
        return;
    }
    QFileInfo info(path);
    if (info.suffix().isEmpty()) {
        const QString suffix =
            selectedFormat() == QStringLiteral("JPEG") ? QStringLiteral("jpg") : selectedFormat().toLower();
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
        QMessageBox::warning(this, tr("Export canvas"), tr("Choose an export path first."));
        return;
    }

    const QString expectedSuffix =
        selectedFormat() == QStringLiteral("JPEG") ? QStringLiteral("jpg") : selectedFormat().toLower();
    QFileInfo info(path);
    const QString currentSuffix = info.suffix().toLower();
    const bool suffixMatches = currentSuffix == expectedSuffix ||
                               (selectedFormat() == QStringLiteral("JPEG") && currentSuffix == QStringLiteral("jpeg"));
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
        QMessageBox::warning(this, tr("Export canvas"),
                             tr("Could not create the output folder:\n%1").arg(info.absolutePath()));
        return;
    }

    if (separateLayersSelected()) {
        const QStringList paths = outputFilePaths();
        if (paths.isEmpty()) {
            QMessageBox::warning(this, tr("Export canvas"), tr("No visible layers to export."));
            return;
        }

        QStringList existing;
        for (const QString& outputPath : paths) {
            if (QFileInfo::exists(outputPath)) {
                existing.append(QDir::toNativeSeparators(outputPath));
            }
        }
        if (!existing.isEmpty()) {
            QStringList preview = existing.mid(0, 5);
            if (existing.size() > preview.size()) {
                preview.append(QStringLiteral("…"));
            }
            const QMessageBox::StandardButton answer = QMessageBox::question(
                this, tr("Overwrite files"),
                tr("Some output files already exist. Overwrite them?\n%1").arg(preview.join(QStringLiteral("\n"))),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes) {
                return;
            }
        }
    } else if (info.exists()) {
        const QMessageBox::StandardButton answer =
            QMessageBox::question(this, tr("Overwrite file"),
                                  tr("The file already exists. Overwrite it?\n%1").arg(QDir::toNativeSeparators(path)),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    accept();
}
