#include "Alignment.h"
#include "BoolOps.h"
#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CanvasView.h"
#include "Commands.h"
#include "CompositeShapeItem.h"
#include "CurveItem.h"
#include "CustomShapeTemplates.h"
#include "ExportDialog.h"
#include "I18n.h"
#include "ImageItem.h"
#include "ItemDockPanel.h"
#include "ItemPropertiesDialog.h"
#include "LayerPanel.h"
#include "LineArtConverter.h"
#include "LineArtDialog.h"
#include "LineArtPresets.h"
#include "MainWindow.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "ShapeItem.h"
#include "TextItem.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"
#include "selftest/SelftestHelpers.h"

#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>
#include <QImage>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLineF>
#include <QLinearGradient>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMarginsF>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPolygonF>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSize>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>
#include <QWheelEvent>
#include <QWidget>
#include <QWidgetAction>
#include <QtMath>
#include <cstdlib>


void selftest::runSceneCanvasIoTests(Context& ctx)
{
    CanvasScene scene;
    QUndoStack undo;
    undo.setUndoLimit(100);
    scene.setUndoStack(&undo);
    scene.applyCanvasSize(QSizeF(800, 600));
    scene.setShowGrid(true);
    ctx.check(qFuzzyCompare(scene.canvasSize().width(), 800.0) && qFuzzyCompare(scene.canvasSize().height(), 600.0),
              QStringLiteral("设置画板尺寸"), geometryText(QPointF(), scene.canvasSize()));

    const QImage defaultImage = scene.renderImage(scene.canvasRect(), 1.0);
    ctx.check(defaultImage.pixelColor(5, 5).alpha() == 0, QStringLiteral("默认背景层为无色纯透明"));

    BackgroundState background;
    background.type = BackgroundType::Color;
    background.color = QColor(QStringLiteral("#FEF3C7"));
    scene.replaceBackground(background);
    ctx.check(scene.background().type == BackgroundType::Color && scene.background().color == background.color,
              QStringLiteral("设置背景颜色"));
    ctx.check(undo.canUndo(), QStringLiteral("背景替换进入撤销栈"));

    undo.undo();
    ctx.check(scene.background().type == BackgroundType::None, QStringLiteral("撤销背景替换"));
    undo.redo();
    ctx.check(scene.background().color == background.color, QStringLiteral("重做背景替换"));

    auto* stroke = new CurveItem();
    stroke->beginStroke(QPointF(90, 120), QColor(QStringLiteral("#EF4444")), 6.0);
    for (int i = 1; i <= 60; ++i) {
        const qreal x = 90.0 + i * 7.0;
        const qreal y = 220.0 + qSin(i * 0.32) * 80.0;
        stroke->appendScenePoint(QPointF(x, y));
    }
    stroke->endStroke();
    stroke->setStrokeStyle(Qt::DashLine);
    ctx.check(!stroke->isEmpty(), QStringLiteral("生成自由曲线"));
    scene.addItemsWithUndo({stroke}, QStringLiteral("添加曲线"));
    ctx.check(scene.canvasItemCount() == 1, QStringLiteral("曲线加入场景"));
    ctx.check(undo.canUndo(), QStringLiteral("添加曲线进入撤销栈"));

    const QPointF strokePos = stroke->pos();
    const QSizeF strokeSize = stroke->contentSize();

    undo.undo();
    ctx.check(scene.canvasItemCount() == 0, QStringLiteral("撤销添加曲线"));
    undo.redo();
    ctx.check(scene.canvasItemCount() == 1, QStringLiteral("重做添加曲线"));
    ctx.check(stroke->pos() == strokePos && stroke->contentSize() == strokeSize, QStringLiteral("重做后曲线几何一致"),
              geometryText(stroke->pos(), stroke->contentSize()));

    ItemGeometry beforeMove{stroke, stroke->pos(), stroke->contentSize()};
    stroke->setPos(stroke->pos() + QPointF(45.0, 30.0));
    ItemGeometry afterMove{stroke, stroke->pos(), stroke->contentSize()};
    scene.pushGeometry({beforeMove}, {afterMove}, QStringLiteral("移动图元"));
    undo.undo();
    ctx.check(stroke->pos() == strokePos, QStringLiteral("撤销移动图元"));
    undo.redo();
    ctx.check(stroke->pos() == strokePos + QPointF(45.0, 30.0), QStringLiteral("重做移动图元"));

    ItemGeometry beforeResize{stroke, stroke->pos(), stroke->contentSize()};
    stroke->setContentSize(stroke->contentSize() * 1.35);
    ItemGeometry afterResize{stroke, stroke->pos(), stroke->contentSize()};
    scene.pushGeometry({beforeResize}, {afterResize}, QStringLiteral("缩放图元"));
    const QSizeF resized = stroke->contentSize();
    undo.undo();
    ctx.check(stroke->contentSize() == strokeSize, QStringLiteral("撤销缩放图元"));
    undo.redo();
    ctx.check(stroke->contentSize() == resized, QStringLiteral("重做缩放图元"));

    const QPixmap shot = makeShotImage(320, 200, QStringLiteral("SHOT 01"), QColor(QStringLiteral("#60A5FA")),
                                       QColor(QStringLiteral("#1D4ED8")));
    auto* imageItem = new ImageItem();
    imageItem->setImage(shot, QStringLiteral("shot-01.png"));
    imageItem->setContentSize(QSizeF(320, 200));
    imageItem->setPos(QPointF(400, 260));
    scene.addItemsWithUndo({imageItem}, QStringLiteral("插入图片"));
    ctx.check(scene.canvasItemCount() == 2, QStringLiteral("插入图片图元"));

    // 透明度是所有图元共有属性：曲线与图片都要随画布一起保存和恢复。
    stroke->setItemOpacity(0.35);
    imageItem->setItemOpacity(0.6);

    const QJsonObject root = scene.toJson();
    const QByteArray jsonBytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    ctx.check(jsonBytes.contains("StoryBoard") && jsonBytes.contains("\"curve\"") && jsonBytes.contains("\"image\""),
              QStringLiteral("画布序列化为 JSON"));

    CanvasScene loaded;
    QUndoStack loadedUndo;
    loaded.setUndoStack(&loadedUndo);
    QString loadError;
    ctx.check(loaded.fromJson(root, &loadError), QStringLiteral("反序列化画布"), loadError);
    ctx.check(loaded.canvasItemCount() == 2, QStringLiteral("画布图元数量一致"));
    ctx.check(loaded.background().type == BackgroundType::Color && loaded.background().color == background.color,
              QStringLiteral("画布背景恢复"));

    CurveItem* loadedStroke = nullptr;
    ImageItem* loadedImage = nullptr;
    const QList<CanvasItem*> loadedItems = loaded.canvasItems();
    for (CanvasItem* item : loadedItems) {
        if (item->kind() == QStringLiteral("curve")) {
            loadedStroke = static_cast<CurveItem*>(item);
        } else if (item->kind() == QStringLiteral("image")) {
            loadedImage = static_cast<ImageItem*>(item);
        }
    }
    ctx.check(loadedStroke != nullptr, QStringLiteral("画布包含曲线"));
    if (loadedStroke) {
        ctx.check(loadedStroke->strokeColor() == QColor(QStringLiteral("#EF4444")) &&
                      loadedStroke->strokeStyle() == Qt::DashLine,
                  QStringLiteral("曲线颜色与描线样式恢复"),
                  QString::number(static_cast<int>(loadedStroke->strokeStyle())));
        ctx.check(qAbs(loadedStroke->contentSize().width() - resized.width()) < 0.5 &&
                      qAbs(loadedStroke->contentSize().height() - resized.height()) < 0.5,
                  QStringLiteral("曲线缩放状态恢复"));
    }
    ctx.check(loadedImage != nullptr && !loadedImage->image().isNull(), QStringLiteral("图片内容恢复"));
    ctx.check(loadedStroke && qAbs(loadedStroke->itemOpacity() - 0.35) < 0.01 && loadedImage &&
                  qAbs(loadedImage->itemOpacity() - 0.6) < 0.01,
              QStringLiteral("曲线与图片的透明度随画布恢复"),
              QStringLiteral("stroke=%1 image=%2")
                  .arg(loadedStroke ? loadedStroke->itemOpacity() : -1.0)
                  .arg(loadedImage ? loadedImage->itemOpacity() : -1.0));

    const QImage rendered = loaded.renderImage(loaded.canvasRect(), 1.0);
    ctx.check(rendered.size() == QSize(800, 600), QStringLiteral("导出图像尺寸正确"));
    ctx.check(!rendered.isNull() && rendered.pixelColor(8, 8) == background.color, QStringLiteral("导出包含背景颜色"));

    const QString jpegPath = QDir(ctx.workDirectory).filePath(QStringLiteral("selftest.jpg"));
    QImageWriter jpegWriter(jpegPath, "JPEG");
    jpegWriter.setQuality(90);
    ctx.check(jpegWriter.write(rendered), QStringLiteral("导出 JPEG"), jpegWriter.errorString());

    const QString pdfPath = QDir(ctx.workDirectory).filePath(QStringLiteral("selftest.pdf"));
    bool pdfOk = false;
    {
        QPdfWriter pdfWriter(pdfPath);
        pdfWriter.setResolution(96);
        pdfWriter.setTitle(QStringLiteral("StoryBoard 自检"));
        pdfWriter.setCreator(QStringLiteral("StoryBoard"));
        pdfWriter.setPageMargins(QMarginsF(0.0, 0.0, 0.0, 0.0), QPageLayout::Point);
        const qreal widthPoints = rendered.width() * 72.0 / 96.0;
        const qreal heightPoints = rendered.height() * 72.0 / 96.0;
        pdfWriter.setPageSize(QPageSize(QSizeF(widthPoints, heightPoints), QPageSize::Point,
                                        QStringLiteral("StoryBoard"), QPageSize::ExactMatch));
        QPainter pdfPainter(&pdfWriter);
        if (pdfPainter.isActive()) {
            pdfPainter.drawImage(QRectF(0.0, 0.0, pdfWriter.width(), pdfWriter.height()), rendered);
            pdfPainter.end();
            pdfOk = true;
        }
    }
    ctx.check(pdfOk && QFileInfo(pdfPath).size() > 0, QStringLiteral("导出 PDF"));

    CanvasScene imageBackgroundScene;
    QPixmap backgroundPixmap(64, 48);
    backgroundPixmap.fill(QColor(QStringLiteral("#10B981")));
    BackgroundState imageBackground;
    imageBackground.type = BackgroundType::Image;
    imageBackground.image = backgroundPixmap;
    imageBackground.mode = BackgroundMode::Tile;
    imageBackgroundScene.replaceBackground(imageBackground);

    CanvasScene imageBackgroundLoaded;
    QString imageBackgroundError;
    ctx.check(imageBackgroundLoaded.fromJson(imageBackgroundScene.toJson(), &imageBackgroundError),
              QStringLiteral("背景图片序列化"), imageBackgroundError);
    ctx.check(imageBackgroundLoaded.background().type == BackgroundType::Image &&
                  !imageBackgroundLoaded.background().image.isNull() &&
                  imageBackgroundLoaded.background().mode == BackgroundMode::Tile,
              QStringLiteral("反序列化背景图片与填充方式"));

    const QString canvasPath = QDir(ctx.workDirectory).filePath(QStringLiteral("selftest.sbcanvas"));
    {
        QFile file(canvasPath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(jsonBytes);
        }
    }
    CanvasScene reloaded;
    QUndoStack reloadedUndo;
    reloaded.setUndoStack(&reloadedUndo);
    QFile canvasFile(canvasPath);
    ctx.check(canvasFile.open(QIODevice::ReadOnly), QStringLiteral("读取画布文件"));
    const QJsonDocument reloadedDocument = QJsonDocument::fromJson(canvasFile.readAll());
    ctx.check(reloadedDocument.isObject(), QStringLiteral("画布文件 JSON 有效"));
    ctx.check(reloaded.fromJson(reloadedDocument.object()), QStringLiteral("从文件载入画布"));
    ctx.check(reloaded.canvasItemCount() == 2, QStringLiteral("文件载入图元数量一致"));

    reloaded.setMode(ToolMode::Edit);
    const QList<CanvasItem*> reloadedItems = reloaded.canvasItems();
    if (!reloadedItems.isEmpty()) {
        reloadedItems.constFirst()->setSelected(true);
        reloaded.deleteSelected();
        ctx.check(reloaded.canvasItemCount() == 1, QStringLiteral("删除选中图元"));
        reloadedUndo.undo();
        ctx.check(reloaded.canvasItemCount() == 2, QStringLiteral("撤销删除图元"));
        reloadedUndo.redo();
        ctx.check(reloaded.canvasItemCount() == 1, QStringLiteral("重做删除图元"));
    }

    scene.setCanvasSizeWithUndo(QSizeF(1000, 700));
    ctx.check(qFuzzyCompare(scene.canvasSize().width(), 1000.0), QStringLiteral("调整画板尺寸"));
    undo.undo();
    ctx.check(qFuzzyCompare(scene.canvasSize().width(), 800.0), QStringLiteral("撤销调整画板尺寸"));

    // 透明背景在屏幕视图上画白 / 浅灰棋盘格，离屏导出图仍保持纯透明。
    {
        CanvasScene checkerScene;
        checkerScene.applyCanvasSize(QSizeF(200, 120));
        checkerScene.setShowGrid(false);

        const QImage checkerExport = checkerScene.renderImage(checkerScene.canvasRect(), 1.0);
        ctx.check(checkerExport.pixelColor(5, 5).alpha() == 0, QStringLiteral("棋盘格不进入离屏导出图"));

        CanvasView checkerView(&checkerScene);
        checkerView.resize(260, 180);
        checkerView.setMode(ToolMode::View);
        checkerView.show();
        QApplication::processEvents();
        checkerView.viewport()->repaint();
        QApplication::processEvents();

        const QImage viewImage = checkerView.viewport()->grab().toImage();
        // 画布按 16×16 等分：首行 16 个格心与首列 16 个格心都必须白 / 灰交替，
        // 横向或纵向格子数多于、少于 16 都会在同一批采样点上失配。
        const QRectF checkerCanvas = checkerScene.canvasRect();
        const qreal tileWidth = checkerCanvas.width() / 16.0;
        const qreal tileHeight = checkerCanvas.height() / 16.0;
        int mismatchedCells = 0;
        for (int index = 0; index < 16; ++index) {
            const bool expectWhite = (index % 2) == 0;
            const QPoint columnPoint = checkerView.mapFromScene(
                QPointF(checkerCanvas.left() + tileWidth * (index + 0.5), checkerCanvas.top() + tileHeight * 0.5));
            const QPoint rowPoint = checkerView.mapFromScene(
                QPointF(checkerCanvas.left() + tileWidth * 0.5, checkerCanvas.top() + tileHeight * (index + 0.5)));
            for (const QColor& cellPixel : {viewImage.pixelColor(columnPoint), viewImage.pixelColor(rowPoint)}) {
                const bool isWhite = cellPixel.red() > 245 && cellPixel.green() > 245 && cellPixel.blue() > 245;
                const bool isGray = cellPixel.red() < 235 && cellPixel.blue() < 245;
                if (!(expectWhite ? isWhite : isGray)) {
                    ++mismatchedCells;
                }
            }
        }
        ctx.check(mismatchedCells == 0, QStringLiteral("视图透明背景按画布 16×16 等分显示棋盘格"),
                  QStringLiteral("tile=%1x%2 mismatched=%3").arg(tileWidth).arg(tileHeight).arg(mismatchedCells));
    }

    // ---------------------------------------------------------------
}
