#include "ui/dialogs/ExportDialog.h"

#include "app/I18n.h"
#include "canvas/CanvasView.h"
#include "scene/CanvasScene.h"
#include "ui/icons/IconFactory.h"

#include <QCheckBox>
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

ExportDialog::ExportDialog(CanvasScene* scene, const QString& startDirectory, QWidget* parent, CanvasView* view,
                           bool fitViewSize)
    : QDialog(parent), m_scene(scene), m_view(view)
{
    setWindowTitle(tr("Export image"));
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
    m_scopeCombo->setObjectName(QStringLiteral("ExportScopeCombo"));
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

    // 输出尺寸与它的“自适应尺寸”勾选项同一行：勾上后范围与比例改由当前视图决定，
    // 输出像素正好等于全景视图在屏幕上的像素尺寸。
    auto* sizeRow = new QWidget(group);
    auto* sizeLayout = new QHBoxLayout(sizeRow);
    sizeLayout->setContentsMargins(0, 0, 0, 0);
    sizeLayout->setSpacing(10);
    m_sizeLabel = new QLabel(sizeRow);
    m_sizeLabel->setObjectName(QStringLiteral("DialogHint"));
    m_fitViewCheck = new QCheckBox(tr("Fit to view size"), sizeRow);
    m_fitViewCheck->setObjectName(QStringLiteral("ExportFitViewCheck"));
    m_fitViewCheck->setToolTip(tr("Match the output pixels to the overview view shown on screen"));
    m_fitViewCheck->setChecked(m_view != nullptr && fitViewSize);
    // 没有视图（例如示例画布）时不给这个勾选项，避免勾了却算不出尺寸。
    m_fitViewCheck->setEnabled(m_view != nullptr);
    sizeLayout->addWidget(m_sizeLabel, 1);
    sizeLayout->addWidget(m_fitViewCheck);
    form->addRow(tr("Output size"), sizeRow);

    rootLayout->addWidget(group);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Export"));
    buttons->button(QDialogButtonBox::Ok)->setIcon(IconFactory::Make(IconId::EXPORT, QColor(255, 255, 255), 18));
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

    connect(m_scopeCombo, &QComboBox::currentIndexChanged, this, [this]() { UpdateEstimatedSize(); });
    connect(m_layerModeCombo, &QComboBox::currentIndexChanged, this, [this]() {
        m_pathEdit->setToolTip(SeparateLayersSelected() ? tr("Base path: each visible layer gets its own file")
                                                        : m_pathEdit->text());
        UpdateEstimatedSize();
    });
    connect(m_scaleSpin, &QDoubleSpinBox::valueChanged, this, [this]() { UpdateEstimatedSize(); });
    connect(m_fitViewCheck, &QCheckBox::toggled, this, [this](bool) { ApplyFitViewState(); });
    connect(m_formatCombo, &QComboBox::currentIndexChanged, this, [this]() {
        UpdatePathExtension();
        UpdateEstimatedSize();
    });
    connect(browseButton, &QPushButton::clicked, this, &ExportDialog::BrowseForFile);
    connect(buttons, &QDialogButtonBox::accepted, this, &ExportDialog::ValidateAndAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    ApplyFitViewState();
}

void ExportDialog::ApplyFitViewState()
{
    // 勾上后范围与比例都由视图接管，这两个控件留在原位但不再起作用。
    const bool fitView = FitViewSelected();
    m_scopeCombo->setEnabled(!fitView);
    m_scaleSpin->setEnabled(!fitView);
    UpdateEstimatedSize();
}

QRectF ExportDialog::SelectedArea() const
{
    if (m_scopeCombo->currentIndex() == 1) {
        QRectF content = m_scene->ContentRect();
        if (content.isEmpty()) {
            content = m_scene->CanvasRect();
        }
        return content.adjusted(-24.0, -24.0, 24.0, 24.0);
    }
    return m_scene->CanvasRect();
}

bool ExportDialog::FitViewSelected() const
{
    return m_view != nullptr && m_fitViewCheck && m_fitViewCheck->isChecked();
}

QRectF ExportDialog::ExportArea() const
{
    if (!FitViewSelected()) {
        return SelectedArea();
    }
    // 可见场景区域 = 视口四角反映射，再乘回当前缩放就是屏幕上的像素尺寸。
    return m_view->mapToScene(m_view->viewport()->rect()).boundingRect();
}

qreal ExportDialog::ExportScale() const
{
    return FitViewSelected() ? m_view->transform().m11() : m_scaleSpin->value();
}

QString ExportDialog::SelectedFormat() const
{
    return m_formatCombo->currentData().toString();
}

QString ExportDialog::FileFilter() const
{
    const QString format = SelectedFormat();
    if (format == QStringLiteral("JPEG")) {
        return tr("JPEG images (*.jpg *.jpeg)");
    }
    if (format == QStringLiteral("PDF")) {
        return tr("PDF documents (*.pdf)");
    }
    return tr("PNG images (*.png)");
}

bool ExportDialog::SeparateLayersSelected() const
{
    return m_layerModeCombo && m_layerModeCombo->currentData().toBool();
}

QString ExportDialog::FormatSuffix(const QString& format)
{
    return format == QStringLiteral("JPEG") ? QStringLiteral("jpg") : format.toLower();
}

QString ExportDialog::LayerFileComponent(const QString& name, int layerId)
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

QStringList ExportDialog::OutputFilePaths() const
{
    const QString basePath = m_pathEdit->text().trimmed();
    if (basePath.isEmpty()) {
        return QStringList();
    }

    if (!SeparateLayersSelected()) {
        return QStringList{basePath};
    }

    QStringList paths;
    if (!m_scene) {
        return paths;
    }

    const QFileInfo baseInfo(basePath);
    const QString baseName =
        baseInfo.completeBaseName().isEmpty() ? QStringLiteral("StoryBoard") : baseInfo.completeBaseName();
    const QString suffix = FormatSuffix(SelectedFormat());
    const QDir directory(baseInfo.absolutePath());

    for (const CanvasLayer& layer : m_scene->Layers()) {
        if (!layer.visible) {
            continue;
        }
        paths.append(directory.filePath(QStringLiteral("%1_L%2_%3.%4")
                                            .arg(baseName)
                                            .arg(layer.id)
                                            .arg(LayerFileComponent(layer.name, layer.id), suffix)));
    }
    return paths;
}

ExportDialog::Options ExportDialog::options() const
{
    Options result;
    result.area = ExportArea();
    result.scale = ExportScale();
    result.format = SelectedFormat();
    result.separateLayers = SeparateLayersSelected();
    result.fitViewSize = FitViewSelected();
    result.filePath = m_pathEdit->text().trimmed();
    result.filePaths = OutputFilePaths();
    if (result.separateLayers && m_scene) {
        for (const CanvasLayer& layer : m_scene->Layers()) {
            if (layer.visible) {
                result.layerIds.append(layer.id);
            }
        }
    }
    return result;
}

void ExportDialog::UpdateEstimatedSize()
{
    const QRectF area = ExportArea();
    const qreal scale = ExportScale();
    int width = qMax(1, qCeil(area.width() * scale));
    int height = qMax(1, qCeil(area.height() * scale));
    constexpr int MAX_SIDE = 12000;
    if (width > MAX_SIDE || height > MAX_SIDE) {
        const qreal factor = qMin(static_cast<qreal>(MAX_SIDE) / width, static_cast<qreal>(MAX_SIDE) / height);
        width = qMax(1, qCeil(width * factor));
        height = qMax(1, qCeil(height * factor));
    }
    QString text = QStringLiteral("%1 × %2 px").arg(width).arg(height);
    if (SeparateLayersSelected()) {
        int count = 0;
        if (m_scene) {
            for (const CanvasLayer& layer : m_scene->Layers()) {
                if (layer.visible) {
                    ++count;
                }
            }
        }
        text += tr(" · %1 files").arg(count);
    }
    m_sizeLabel->setText(text);
}

void ExportDialog::UpdatePathExtension()
{
    const QString format = SelectedFormat();
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

void ExportDialog::BrowseForFile()
{
    const QString current = m_pathEdit->text().trimmed();
    const QString directory = current.isEmpty() ? QString() : QFileInfo(current).absolutePath();
    QString path = QFileDialog::getSaveFileName(this, tr("Export image"), directory, FileFilter());
    if (path.isEmpty()) {
        return;
    }
    QFileInfo info(path);
    if (info.suffix().isEmpty()) {
        const QString suffix =
            SelectedFormat() == QStringLiteral("JPEG") ? QStringLiteral("jpg") : SelectedFormat().toLower();
        path += QLatin1Char('.') + suffix;
    }
    m_pathEdit->setText(path);
    m_pathEdit->setCursorPosition(0);
    m_pathEdit->setToolTip(path);
}

void ExportDialog::ValidateAndAccept()
{
    QString path = m_pathEdit->text().trimmed();
    if (path.isEmpty()) {
        QMessageBox::warning(this, tr("Export image"), tr("Choose an export path first."));
        return;
    }

    const QString expectedSuffix =
        SelectedFormat() == QStringLiteral("JPEG") ? QStringLiteral("jpg") : SelectedFormat().toLower();
    QFileInfo info(path);
    const QString currentSuffix = info.suffix().toLower();
    const bool suffixMatches = currentSuffix == expectedSuffix ||
                               (SelectedFormat() == QStringLiteral("JPEG") && currentSuffix == QStringLiteral("jpeg"));
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
        QMessageBox::warning(this, tr("Export image"),
                             tr("Could not create the output folder:\n%1").arg(info.absolutePath()));
        return;
    }

    if (SeparateLayersSelected()) {
        const QStringList paths = OutputFilePaths();
        if (paths.isEmpty()) {
            QMessageBox::warning(this, tr("Export image"), tr("No visible layers to export."));
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
