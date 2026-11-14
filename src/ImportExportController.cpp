#include "ImportExportController.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "CanvasView.h"
#include "ExportDialog.h"
#include "I18n.h"
#include "ImageItem.h"
#include "LineArtDialog.h"
#include "MainWindow.h"
#include "Settings.h"
#include "StrokeItem.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QMessageBox>
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

bool selfTestMode()
{
    return qApp && qApp->property("storyboardSelfTest").toBool();
}

// 与 MainWindow 相同的报错出口：自检（离屏）时不许弹模态框。
void reportWarning(QWidget* parent, const QString& title, const QString& text)
{
    if (selfTestMode()) {
        qWarning("self-test suppressed dialog [%s]: %s", qUtf8Printable(title), qUtf8Printable(text));
        return;
    }
    QMessageBox::warning(parent, title, text);
}

QString imageFilter()
{
    return MainWindow::tr("Image files (*.png *.jpg *.jpeg *.bmp *.gif);;All files (*.*)");
}

} // namespace

ImportExportController::ImportExportController(MainWindow* window) : m_window(window) {}

void ImportExportController::exportCanvas()
{
    m_window->m_view->finishCurrentStroke();
    QString startDirectory = m_window->m_settings.lastExportDir;
    if (startDirectory.isEmpty()) {
        startDirectory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }

    ExportDialog dialog(m_window->m_scene, startDirectory, m_window);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const ExportDialog::Options options = dialog.options();
    m_window->m_settings.lastExportDir = QFileInfo(options.filePath).absolutePath();
    m_window->m_settings.save();

    if (options.separateLayers) {
        const QStringList paths = options.filePaths;
        if (paths.isEmpty()) {
            reportWarning(m_window, MainWindow::tr("Export canvas"), MainWindow::tr("No visible layers to export."));
            return;
        }

        QStringList exported;
        for (int i = 0; i < paths.size(); ++i) {
            const int layerId = i < options.layerIds.size() ? options.layerIds.at(i) : -1;
            if (!exportToFile(paths.at(i), options.area, options.scale, options.format, false, layerId)) {
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

    exportToFile(options.filePath, options.area, options.scale, options.format);
}

bool ImportExportController::exportToFile(const QString& path, const QRectF& area, qreal scale, const QString& format,
                                          bool showCompletion, int layerId)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QImage image = layerId >= 0 ? m_window->m_scene->renderLayerImage(area, scale, layerId)
                                      : m_window->m_scene->renderImage(area, scale);
    QApplication::restoreOverrideCursor();

    if (image.isNull()) {
        reportWarning(m_window, MainWindow::tr("Export canvas"),
                      MainWindow::tr("Rendering the canvas failed: nothing to export."));
        return false;
    }

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
        reportWarning(
            m_window, MainWindow::tr("Export failed"),
            MainWindow::tr("Could not export the canvas:\n%1\n\n%2").arg(QDir::toNativeSeparators(path), errorMessage));
        return false;
    }

    m_window->m_settings.lastExportDir = QFileInfo(path).absolutePath();
    m_window->m_settings.save();
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

void ImportExportController::importImagesDialog()
{
    QString startDirectory = m_window->m_settings.lastImageDir;
    if (startDirectory.isEmpty()) {
        startDirectory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }

    const QStringList files =
        QFileDialog::getOpenFileNames(m_window, MainWindow::tr("Insert images"), startDirectory, imageFilter());
    if (!files.isEmpty()) {
        importImageFiles(files);
    }
}

void ImportExportController::importOutlineImageDialog()
{
    QString startDirectory = m_window->m_settings.lastImageDir;
    if (startDirectory.isEmpty()) {
        startDirectory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }

    const QString file =
        QFileDialog::getOpenFileName(m_window, MainWindow::tr("Import outline image"), startDirectory, imageFilter());
    if (file.isEmpty()) {
        return;
    }

    // 选完图片先弹参数：档位和各项数值都在这里，点“确定”才真正描线。
    // 参数只改进程内副本；只有点“保存到配置文件”才覆盖 StoryBoard.ini。
    LineArtDialog dialog(m_window->m_lineArtSettings, m_window);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    dialog.applyToSettings(m_window->m_lineArtSettings);
    if (dialog.saveRequested()) {
        const bool saved = m_window->m_lineArtSettings.saveLineArt();
        if (saved) {
            // 文件已经是新值了，主设置对象同步一份，避免之后整份保存把旧值写回去。
            m_window->m_settings.copyLineArtFrom(m_window->m_lineArtSettings);
            m_window->statusBar()->showMessage(MainWindow::tr("Image outline settings saved to the configuration file"),
                                               4000);
        } else {
            m_window->statusBar()->showMessage(
                MainWindow::tr("Could not write the configuration file; the settings apply "
                               "to this session only"),
                5000);
        }
    }
    importOutlineImageFile(file, dialog.options());
}

void ImportExportController::importOutlineImageFile(const QString& file, const LineArtConverter::Options& options)
{
    if (file.isEmpty()) {
        return;
    }

    if (m_window->m_view->mode() != ToolMode::Edit) {
        m_window->setToolMode(ToolMode::Edit);
    }

    if (!m_window->m_scene->isActiveLayerWritable()) {
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
        reportWarning(m_window, MainWindow::tr("Import outline image"),
                      MainWindow::tr("Could not read the image:\n%1").arg(QDir::toNativeSeparators(file)));
        return;
    }

    const LineArtConverter::Result traced = LineArtConverter::extract(image, options);
    if (!traced.ok || traced.outlines.isEmpty()) {
        reportWarning(m_window, MainWindow::tr("Import outline image"),
                      traced.error.isEmpty() ? MainWindow::tr("No color blocks were found in the image.")
                                             : uiText(traced.error));
        return;
    }

    // 和插入图片一致：等比缩到画布的 60% 以内，居中放到当前视图中心。
    const QSizeF processed(traced.imageSize.width(), traced.imageSize.height());
    qreal scale = qMin(1.0, qMin(m_window->m_scene->canvasSize().width() * 0.6 / processed.width(),
                                 m_window->m_scene->canvasSize().height() * 0.6 / processed.height()));
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
        auto* stroke = new StrokeItem();
        stroke->setStrokePoints(scenePoints, outline.color, options.strokeWidth, 0.0, true);
        items.append(stroke);
    }

    if (items.isEmpty()) {
        reportWarning(m_window, MainWindow::tr("Import outline image"),
                      MainWindow::tr("No color blocks were found in the image."));
        return;
    }

    m_window->m_scene->addItemsWithUndo(items, MainWindow::tr("Import outline image"));

    m_window->m_settings.lastImageDir = QFileInfo(file).absolutePath();
    m_window->m_settings.save();

    m_window->statusBar()->showMessage(MainWindow::tr("%1 stroke(s) created from the image outlines").arg(items.size()),
                                       5000);
    // 色块数量上限已放开：有多少块就描多少块，不再提示“只转换了前 N 个”。
}

void ImportExportController::importImageFiles(const QStringList& files)
{
    if (files.isEmpty()) {
        return;
    }

    if (m_window->m_view->mode() != ToolMode::Edit) {
        m_window->setToolMode(ToolMode::Edit);
    }

    if (!m_window->m_scene->isActiveLayerWritable()) {
        m_window->statusBar()->showMessage(
            MainWindow::tr("The current layer is locked or hidden; images cannot be "
                           "inserted. Unlock or switch layers in the layer panel first."),
            5000);
        return;
    }

    const QPointF viewCenter = m_window->m_view->mapToScene(m_window->m_view->viewport()->rect().center());
    const QSizeF maxSize(m_window->m_scene->canvasSize().width() * 0.6, m_window->m_scene->canvasSize().height() * 0.6);

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

        auto* item = new ImageItem();
        item->setImage(pixmap, QFileInfo(file).fileName());
        item->setContentSize(size);
        item->setPos(viewCenter - QPointF(size.width() / 2.0, size.height() / 2.0) +
                     QPointF(offsetIndex * 26.0, offsetIndex * 26.0));
        items.append(item);
        ++offsetIndex;
    }

    if (items.isEmpty()) {
        reportWarning(m_window, MainWindow::tr("Insert images"), MainWindow::tr("No usable image files."));
        return;
    }

    m_window->m_scene->addItemsWithUndo(items, MainWindow::tr("Insert images"));
    for (CanvasItem* item : items) {
        item->setSelected(true);
    }

    m_window->m_settings.lastImageDir = QFileInfo(files.constFirst()).absolutePath();
    m_window->m_settings.save();

    m_window->statusBar()->showMessage(MainWindow::tr("%1 image(s) inserted").arg(items.size()), 4000);
    if (!failed.isEmpty()) {
        reportWarning(m_window, MainWindow::tr("Some images were not inserted"),
                      MainWindow::tr("The following files could not be read:\n%1")
                          .arg(QDir::toNativeSeparators(failed.join(QLatin1Char('\n')))));
    }
}
