#include "io/ImportExportController.h"

#include "app/DocumentController.h"
#include "app/I18n.h"
#include "app/Settings.h"
#include "app/TestMode.h"
#include "canvas/CanvasView.h"
#include "items/CanvasItem.h"
#include "items/lines/CurveItem.h"
#include "items/properties/ImageFill.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "items/shapes/ShapeFactory.h"
#include "lineart/LineArtDialog.h"
#include "scene/CanvasScene.h"
#include "scene/SceneConstants.h"
#include "ui/dialogs/ExportDialog.h"
#include "ui/mainwindow/MainWindow.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QMessageBox>
#include <QMimeData>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPixmap>
#include <QPushButton>
#include <QRectF>
#include <QStandardPaths>
#include <QStatusBar>
#include <QUrl>
#include <QtMath>

namespace {

// 与 MainWindow 相同的报错出口：自检（离屏）时不许弹模态框。
void ReportWarning(QWidget* parent, const QString& title, const QString& text)
{
    if (TestMode::IsSelfTestMode()) {
        qWarning("self-test suppressed dialog [%s]: %s", qUtf8Printable(title), qUtf8Printable(text));
        return;
    }
    QMessageBox::warning(parent, title, text);
}

// JPEG / PDF 不保留透明通道：导出前把透明区域合成到白底上，避免透出黑色。
QImage FlattenOnWhite(const QImage& source)
{
    QImage result(source.size(), QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::white);
    QPainter painter(&result);
    painter.drawImage(QPointF(0.0, 0.0), source);
    painter.end();
    return result;
}

QString ImageFilter()
{
    return MainWindow::tr("Image files (*.png *.jpg *.jpeg *.bmp *.gif);;All files (*.*)");
}

bool HasImageSuffix(const QString& path)
{
    static const QStringList SUFFIXES = {QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
                                         QStringLiteral("bmp"), QStringLiteral("gif")};
    return SUFFIXES.contains(QFileInfo(path).suffix().toLower());
}

// 画布 / Excalidraw 场景都按“打开文档”处理：拖 .excalidraw 与拖 .sbcanvas 同一条路径。
bool IsCanvasDropPath(const QString& path)
{
    return path.endsWith(QStringLiteral(".sbcanvas"), Qt::CaseInsensitive) ||
           path.endsWith(QStringLiteral(".excalidraw"), Qt::CaseInsensitive);
}

} // namespace

ImportExportController::ImportExportController(MainWindow* window) : m_window(window) {}

void ImportExportController::ExportCanvas()
{
    m_window->m_view->FinishCurrentStroke();
    QString startDirectory = m_window->m_settings.lastExportDir;
    if (startDirectory.isEmpty()) {
        startDirectory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }

    ExportDialog dialog(m_window->m_scene, startDirectory, m_window, m_window->m_view,
                        m_window->m_settings.exportFitViewSize);
    const int dialogResult = dialog.exec();

    const ExportDialog::Options options = dialog.options();
    // 「自适应尺寸」是纯偏好，取消导出也应该记住，所以先落盘再判断是否继续导出。
    if (m_window->m_settings.exportFitViewSize != options.fitViewSize) {
        m_window->m_settings.exportFitViewSize = options.fitViewSize;
        m_window->m_settings.Save();
    }
    if (dialogResult != QDialog::Accepted) {
        return;
    }

    m_window->m_settings.lastExportDir = QFileInfo(options.filePath).absolutePath();
    m_window->m_settings.Save();

    if (options.separateLayers) {
        const QStringList paths = options.filePaths;
        if (paths.isEmpty()) {
            ReportWarning(m_window, MainWindow::tr("Export image"), MainWindow::tr("No visible layers to export."));
            return;
        }

        QStringList exported;
        for (int i = 0; i < paths.size(); ++i) {
            const int layerId = i < options.layerIds.size() ? options.layerIds.at(i) : -1;
            // 自适应尺寸 + 逐层导出：每张图只取本图层可见图元的包围框，不是全部图层的并集；
            // 与合并模式一致外留 2px，避免贴边像素被抗锯齿削掉一丝。
            QRectF area = options.area;
            if (options.fitViewSize && layerId >= 0) {
                const QRectF layerRect = m_window->m_scene->LayerContentRect(layerId);
                if (layerRect.isEmpty()) {
                    continue;
                }
                const qreal pad = scene_constants::EXPORT_FIT_PADDING;
                area = layerRect.adjusted(-pad, -pad, pad, pad);
            }
            if (!ExportToFile(paths.at(i), area, options.scale, options.format, false, layerId)) {
                return;
            }
            exported.append(paths.at(i));
        }

        const QString folder = QFileInfo(paths.constFirst()).absolutePath();
        m_window->statusBar()->showMessage(MainWindow::tr("Exported %1 visible layer file(s)").arg(exported.size()),
                                           5000);
        QMessageBox box(m_window);
        box.setIcon(QMessageBox::Information);
        box.setWindowTitle(MainWindow::tr("Export complete"));
        box.setText(MainWindow::tr("The visible layers were exported successfully."));
        box.setInformativeText(
            MainWindow::tr("%1 file(s) in\n%2").arg(exported.size()).arg(QDir::toNativeSeparators(folder)));
        QPushButton* openFolderButton = box.addButton(MainWindow::tr("Open folder"), QMessageBox::ActionRole);
        box.addButton(MainWindow::tr("Done"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == openFolderButton) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
        }
        return;
    }

    ExportToFile(options.filePath, options.area, options.scale, options.format);
}

bool ImportExportController::ExportToFile(const QString& path, const QRectF& area, qreal scale, const QString& format,
                                          bool showCompletion, int layerId)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QImage rawImage = layerId >= 0 ? m_window->m_scene->RenderLayerImage(area, scale, layerId)
                                         : m_window->m_scene->RenderImage(area, scale);
    QApplication::restoreOverrideCursor();

    if (rawImage.isNull()) {
        ReportWarning(m_window, MainWindow::tr("Export image"),
                      MainWindow::tr("Rendering the canvas failed: nothing to export."));
        return false;
    }

    // PNG 保留透明背景；JPEG / PDF 不支持透明，先合成白底。
    const QImage image = format == QStringLiteral("PNG") ? rawImage : FlattenOnWhite(rawImage);

    bool success = false;
    QString errorMessage;
    if (format == QStringLiteral("PDF")) {
        {
            QPdfWriter writer(path);
            writer.setResolution(96);
            writer.setTitle(MainWindow::tr("StoryBoard Canvas"));
            writer.setCreator(QStringLiteral("StoryBoard"));
            writer.setPageMargins(QMarginsF(0.0, 0.0, 0.0, 0.0), QPageLayout::Point);
            const qreal widthPoints = image.width() * 72.0 / 96.0;
            const qreal heightPoints = image.height() * 72.0 / 96.0;
            writer.setPageSize(QPageSize(QSizeF(widthPoints, heightPoints), QPageSize::Point,
                                         QStringLiteral("StoryBoard"), QPageSize::ExactMatch));
            QPainter painter(&writer);
            if (painter.isActive()) {
                painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
                painter.drawImage(QRectF(0.0, 0.0, writer.width(), writer.height()), image);
                painter.end();
                success = true;
            } else {
                errorMessage = MainWindow::tr("Could not create the PDF output.");
            }
        }
        if (success && QFileInfo(path).size() <= 0) {
            success = false;
            errorMessage = MainWindow::tr("Failed to write the PDF file.");
        }
    } else {
        QImageWriter writer(path, format == QStringLiteral("JPEG") ? "JPEG" : "PNG");
        if (format == QStringLiteral("JPEG")) {
            writer.setQuality(92);
        }
        success = writer.write(image);
        if (!success) {
            errorMessage = writer.errorString();
        }
    }

    if (!success) {
        ReportWarning(
            m_window, MainWindow::tr("Export failed"),
            MainWindow::tr("Could not export the canvas:\n%1\n\n%2").arg(QDir::toNativeSeparators(path), errorMessage));
        return false;
    }

    m_window->m_settings.lastExportDir = QFileInfo(path).absolutePath();
    m_window->m_settings.Save();
    m_window->statusBar()->showMessage(MainWindow::tr("Canvas exported: %1").arg(QFileInfo(path).fileName()), 5000);

    if (!showCompletion) {
        return true;
    }

    QMessageBox box(m_window);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(MainWindow::tr("Export complete"));
    box.setText(MainWindow::tr("The canvas was exported successfully."));
    box.setInformativeText(QDir::toNativeSeparators(path));
    QPushButton* openFileButton = box.addButton(MainWindow::tr("Open file"), QMessageBox::AcceptRole);
    QPushButton* openFolderButton = box.addButton(MainWindow::tr("Open folder"), QMessageBox::ActionRole);
    box.addButton(MainWindow::tr("Done"), QMessageBox::RejectRole);
    box.exec();

    if (box.clickedButton() == openFileButton) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    } else if (box.clickedButton() == openFolderButton) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
    }

    return true;
}

void ImportExportController::ImportImagesDialog()
{
    QString startDirectory = m_window->m_settings.lastImageDir;
    if (startDirectory.isEmpty()) {
        startDirectory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }

    const QStringList files =
        QFileDialog::getOpenFileNames(m_window, MainWindow::tr("Insert images"), startDirectory, ImageFilter());
    if (!files.isEmpty()) {
        ImportImageFiles(files);
    }
}

void ImportExportController::ImportOutlineImageDialog()
{
    QString startDirectory = m_window->m_settings.lastImageDir;
    if (startDirectory.isEmpty()) {
        startDirectory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }

    const QString file =
        QFileDialog::getOpenFileName(m_window, MainWindow::tr("Import outline image"), startDirectory, ImageFilter());
    if (file.isEmpty()) {
        return;
    }

    // 选完图片先弹参数：档位和各项数值都在这里，点“确定”才真正描线。
    // 参数只改进程内副本；只有点“保存到配置文件”才覆盖 StoryBoard.ini。
    LineArtDialog dialog(m_window->m_lineArtSettings, m_window);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    dialog.ApplyToSettings(m_window->m_lineArtSettings);
    if (dialog.SaveRequested()) {
        const bool saved = m_window->m_lineArtSettings.SaveLineArt();
        if (saved) {
            // 文件已经是新值了，主设置对象同步一份，避免之后整份保存把旧值写回去。
            m_window->m_settings.CopyLineArtFrom(m_window->m_lineArtSettings);
            m_window->statusBar()->showMessage(MainWindow::tr("Image outline settings saved to the configuration file"),
                                               4000);
        } else {
            m_window->statusBar()->showMessage(
                MainWindow::tr("Could not write the configuration file; the settings apply "
                               "to this session only"),
                5000);
        }
    }
    ImportOutlineImageFile(file, dialog.options());
}

void ImportExportController::ImportOutlineImageFile(const QString& file, const LineArtConverter::Options& options)
{
    if (file.isEmpty()) {
        return;
    }

    if (m_window->m_view->mode() != ToolMode::EDIT) {
        m_window->SetToolMode(ToolMode::EDIT);
    }

    if (!m_window->m_scene->IsActiveLayerWritable()) {
        m_window->statusBar()->showMessage(
            MainWindow::tr("The current layer is locked or hidden; the image outline cannot be imported. Unlock "
                           "or switch layers in the layer panel first."),
            5000);
        return;
    }

    QImageReader reader(file);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        ReportWarning(m_window, MainWindow::tr("Import outline image"),
                      MainWindow::tr("Could not read the image:\n%1").arg(QDir::toNativeSeparators(file)));
        return;
    }

    const LineArtConverter::Result traced = LineArtConverter::Extract(image, options);
    if (!traced.ok || traced.outlines.isEmpty()) {
        ReportWarning(m_window, MainWindow::tr("Import outline image"),
                      traced.error.isEmpty() ? MainWindow::tr("No color blocks were found in the image.")
                                             : UiText(traced.error));
        return;
    }

    // 和插入图片一致：等比缩到画布的 60% 以内，居中放到当前视图中心。
    const QSizeF processed(traced.imageSize.width(), traced.imageSize.height());
    qreal scale = qMin(1.0, qMin(m_window->m_scene->CanvasSize().width() * 0.6 / processed.width(),
                                 m_window->m_scene->CanvasSize().height() * 0.6 / processed.height()));
    if (!(scale > 0.0)) {
        scale = 1.0;
    }

    const QPointF viewCenter = m_window->m_view->mapToScene(m_window->m_view->viewport()->rect().center());
    const QSizeF placed(processed.width() * scale, processed.height() * scale);
    const QPointF topLeft = viewCenter - QPointF(placed.width() / 2.0, placed.height() / 2.0);

    QList<CanvasItem*> items;
    items.reserve(traced.outlines.size());
    for (const LineArtConverter::Outline& outline : traced.outlines) {
        if (outline.points.size() < 4) {
            continue;
        }

        QVector<QPointF> scenePoints;
        scenePoints.reserve(outline.points.size());
        for (const QPointF& point : outline.points) {
            scenePoints.append(topLeft + QPointF(point.x() * scale, point.y() * scale));
        }

        // 不再做额外平滑：轮廓已经按像素级 RDP 简化过，再平滑会把方角磨圆；
        // 折线模式让路径按直线段绘制，方角保持方正。
        auto* stroke = new CurveItem();
        stroke->SetStrokePoints(scenePoints, outline.color, options.strokeWidth, 0.0, true);
        stroke->SetStrokeSource(CurveSource::LINE_ART);
        items.append(stroke);
    }

    if (items.isEmpty()) {
        ReportWarning(m_window, MainWindow::tr("Import outline image"),
                      MainWindow::tr("No color blocks were found in the image."));
        return;
    }

    m_window->m_scene->AddItemsWithUndo(items, MainWindow::tr("Import outline image"));

    m_window->m_settings.lastImageDir = QFileInfo(file).absolutePath();
    m_window->m_settings.Save();

    m_window->statusBar()->showMessage(MainWindow::tr("%1 stroke(s) created from the image outlines").arg(items.size()),
                                       5000);
    // 色块数量上限已放开：有多少块就描多少块，不再提示“只转换了前 N 个”。
}

void ImportExportController::ImportImageFiles(const QStringList& files)
{
    if (files.isEmpty()) {
        return;
    }

    if (m_window->m_view->mode() != ToolMode::EDIT) {
        m_window->SetToolMode(ToolMode::EDIT);
    }

    if (!m_window->m_scene->IsActiveLayerWritable()) {
        m_window->statusBar()->showMessage(
            MainWindow::tr("The current layer is locked or hidden; images cannot be "
                           "inserted. Unlock or switch layers in the layer panel first."),
            5000);
        return;
    }

    const QPointF viewCenter = m_window->m_view->mapToScene(m_window->m_view->viewport()->rect().center());
    const QSizeF maxSize(m_window->m_scene->CanvasSize().width() * 0.6, m_window->m_scene->CanvasSize().height() * 0.6);

    QList<CanvasItem*> items;
    QStringList failed;
    int offsetIndex = 0;

    for (const QString& file : files) {
        QImageReader reader(file);
        reader.setAutoTransform(true);
        const QImage image = reader.read();
        if (image.isNull()) {
            failed.append(file);
            continue;
        }

        const QPixmap pixmap = QPixmap::fromImage(image);
        if (pixmap.isNull()) {
            failed.append(file);
            continue;
        }

        qreal scaleFactor = 1.0;
        if (pixmap.width() > 0 && pixmap.height() > 0) {
            scaleFactor = qMin(1.0, qMin(maxSize.width() / pixmap.width(), maxSize.height() / pixmap.height()));
        }
        QSizeF size(pixmap.width() * scaleFactor, pixmap.height() * scaleFactor);
        size.setWidth(qMax<qreal>(16.0, size.width()));
        size.setHeight(qMax<qreal>(16.0, size.height()));

        // 需求 14：插入图片生成的是矩形图元，图片只作为它的背景（在填充样式之下）。
        // 填充与描线都是透明无色，图片本身承担视觉；不再产生图片类型图元。
        auto* item = ShapeFactory::Create(ShapeType::RECTANGLE);
        item->SetFillImage(pixmap, ImageFill::Mode::STRETCH);
        item->SetStyle(QColor(Qt::transparent), QColor(Qt::transparent), 1.0);
        item->SetFillStyle(FillStyle::NONE);
        item->SetName(QFileInfo(file).fileName());
        item->SetContentSize(size);
        item->setPos(viewCenter - QPointF(size.width() / 2.0, size.height() / 2.0) +
                     QPointF(offsetIndex * 26.0, offsetIndex * 26.0));
        items.append(item);
        ++offsetIndex;
    }

    if (items.isEmpty()) {
        ReportWarning(m_window, MainWindow::tr("Insert images"), MainWindow::tr("No usable image files."));
        return;
    }

    m_window->m_scene->AddItemsWithUndo(items, MainWindow::tr("Insert images"));
    for (CanvasItem* item : items) {
        item->setSelected(true);
    }

    m_window->m_settings.lastImageDir = QFileInfo(files.constFirst()).absolutePath();
    m_window->m_settings.Save();

    m_window->statusBar()->showMessage(MainWindow::tr("%1 image(s) inserted").arg(items.size()), 4000);
    if (!failed.isEmpty()) {
        ReportWarning(m_window, MainWindow::tr("Some images were not inserted"),
                      MainWindow::tr("The following files could not be read:\n%1")
                          .arg(QDir::toNativeSeparators(failed.join(QLatin1Char('\n')))));
    }
}

bool ImportExportController::AcceptsDropUrls(const QMimeData* mimeData) const
{
    if (!mimeData || !mimeData->hasUrls()) {
        return false;
    }
    for (const QUrl& url : mimeData->urls()) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QString path = url.toLocalFile();
        if (HasImageSuffix(path) || IsCanvasDropPath(path)) {
            return true;
        }
    }
    return false;
}

void ImportExportController::HandleDrop(QDropEvent* event)
{
    QStringList images;
    QString canvasPath;

    for (const QUrl& url : event->mimeData()->urls()) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QString path = url.toLocalFile();
        if (IsCanvasDropPath(path)) {
            canvasPath = path;
        } else if (HasImageSuffix(path)) {
            images.append(path);
        }
    }

    if (!canvasPath.isEmpty() && images.isEmpty()) {
        if (m_window->m_document->LoadCanvasFile(canvasPath, true)) {
            event->acceptProposedAction();
        }
        return;
    }

    if (!images.isEmpty()) {
        ImportImageFiles(images);
        event->acceptProposedAction();
    }
}
