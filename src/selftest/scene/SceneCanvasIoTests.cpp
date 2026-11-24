#include "app/I18n.h"
#include "app/Settings.h"
#include "canvas/CanvasView.h"
#include "io/CustomShapeTemplates.h"
#include "items/CanvasTypes.h"
#include "items/lines/CurveItem.h"
#include "items/lines/LineAbstractItem.h"
#include "items/polygon/PolygonItem.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "items/shapes/ShapeFactory.h"
#include "items/text/TextItem.h"
#include "lineart/LineArtConverter.h"
#include "lineart/LineArtDialog.h"
#include "lineart/LineArtPresets.h"
#include "ops/Alignment.h"
#include "ops/BoolOps.h"
#include "scene/CanvasScene.h"
#include "scene/Commands.h"
#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"
#include "selftest/core/SelftestHelpers.h"
#include "ui/dialogs/ExportDialog.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/panels/layers/LayerPanel.h"
#include "ui/panels/properties/PropertiesPanel.h"
#include "ui/settings/SettingsDialog.h"

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


void selftest::RunSceneCanvasIoTests(Context& ctx)
{
    CanvasScene scene;
    QUndoStack undo;
    undo.setUndoLimit(100);
    scene.SetUndoStack(&undo);
    scene.ApplyCanvasSize(QSizeF(800, 600));
    scene.SetShowGrid(true);
    ctx.Check(qFuzzyCompare(scene.CanvasSize().width(), 800.0) && qFuzzyCompare(scene.CanvasSize().height(), 600.0),
              QStringLiteral("设置画板尺寸"), GeometryText(QPointF(), scene.CanvasSize()));

    const QImage defaultImage = scene.RenderImage(scene.CanvasRect(), 1.0);
    ctx.Check(defaultImage.pixelColor(5, 5).alpha() == 0, QStringLiteral("默认背景层为无色纯透明"));

    CanvasBackground background;
    background.type = BackgroundType::COLOR;
    background.color = QColor(QStringLiteral("#FEF3C7"));
    scene.ReplaceCanvasBackground(background);
    ctx.Check(scene.canvasBackground().type == BackgroundType::COLOR &&
                  scene.canvasBackground().color == background.color,
              QStringLiteral("设置背景颜色"));
    ctx.Check(undo.canUndo(), QStringLiteral("背景替换进入撤销栈"));

    undo.undo();
    ctx.Check(scene.canvasBackground().type == BackgroundType::NONE, QStringLiteral("撤销背景替换"));
    undo.redo();
    ctx.Check(scene.canvasBackground().color == background.color, QStringLiteral("重做背景替换"));

    auto* stroke = new CurveItem();
    stroke->BeginStroke(QPointF(90, 120), QColor(QStringLiteral("#EF4444")), 6.0);
    for (int i = 1; i <= 60; ++i) {
        const qreal x = 90.0 + i * 7.0;
        const qreal y = 220.0 + qSin(i * 0.32) * 80.0;
        stroke->AppendScenePoint(QPointF(x, y));
    }
    stroke->EndStroke();
    stroke->SetStrokePattern(Qt::DashLine);
    ctx.Check(!stroke->IsEmpty(), QStringLiteral("生成自由曲线"));
    scene.AddItemsWithUndo({stroke}, QStringLiteral("添加曲线"));
    ctx.Check(scene.CanvasItemCount() == 1, QStringLiteral("曲线加入场景"));
    ctx.Check(undo.canUndo(), QStringLiteral("添加曲线进入撤销栈"));

    const QPointF strokePos = stroke->pos();
    const QSizeF strokeSize = stroke->ContentSize();

    undo.undo();
    ctx.Check(scene.CanvasItemCount() == 0, QStringLiteral("撤销添加曲线"));
    undo.redo();
    ctx.Check(scene.CanvasItemCount() == 1, QStringLiteral("重做添加曲线"));
    ctx.Check(stroke->pos() == strokePos && stroke->ContentSize() == strokeSize, QStringLiteral("重做后曲线几何一致"),
              GeometryText(stroke->pos(), stroke->ContentSize()));

    ItemPlacement beforeMove{stroke, stroke->pos(), stroke->ContentSize()};
    stroke->setPos(stroke->pos() + QPointF(45.0, 30.0));
    ItemPlacement afterMove{stroke, stroke->pos(), stroke->ContentSize()};
    scene.PushGeometry({beforeMove}, {afterMove}, QStringLiteral("移动图元"));
    undo.undo();
    ctx.Check(stroke->pos() == strokePos, QStringLiteral("撤销移动图元"));
    undo.redo();
    ctx.Check(stroke->pos() == strokePos + QPointF(45.0, 30.0), QStringLiteral("重做移动图元"));

    ItemPlacement beforeResize{stroke, stroke->pos(), stroke->ContentSize()};
    stroke->SetContentSize(stroke->ContentSize() * 1.35);
    ItemPlacement afterResize{stroke, stroke->pos(), stroke->ContentSize()};
    scene.PushGeometry({beforeResize}, {afterResize}, QStringLiteral("缩放图元"));
    const QSizeF resized = stroke->ContentSize();
    undo.undo();
    ctx.Check(stroke->ContentSize() == strokeSize, QStringLiteral("撤销缩放图元"));
    undo.redo();
    ctx.Check(stroke->ContentSize() == resized, QStringLiteral("重做缩放图元"));

    const QPixmap shot = MakeShotImage(320, 200, QStringLiteral("SHOT 01"), QColor(QStringLiteral("#60A5FA")),
                                       QColor(QStringLiteral("#1D4ED8")));
    auto* imageItem = ShapeFactory::Create(ShapeType::RECTANGLE);
    imageItem->SetStyle(QColor(Qt::transparent), QColor(Qt::transparent), 1.0);
    imageItem->SetFillStyle(FillStyle::NONE);
    imageItem->SetFillImage(shot, ImageFill::Mode::STRETCH);
    imageItem->SetContentSize(QSizeF(320, 200));
    imageItem->setPos(QPointF(400, 260));
    scene.AddItemsWithUndo({imageItem}, QStringLiteral("插入图片"));
    ctx.Check(scene.CanvasItemCount() == 2, QStringLiteral("插入图片图元"));

    // 透明度是所有图元共有属性：曲线与图片都要随画布一起保存和恢复。
    stroke->SetItemOpacity(0.35);
    imageItem->SetItemOpacity(0.6);

    const QJsonObject root = scene.ToJson();
    const QByteArray jsonBytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    ctx.Check(jsonBytes.contains("StoryBoard") && jsonBytes.contains("\"freehand\"") &&
                  jsonBytes.contains("\"shape\"") && jsonBytes.contains("\"images\""),
              QStringLiteral("画布序列化为 JSON"));

    CanvasScene loaded;
    QUndoStack loadedUndo;
    loaded.SetUndoStack(&loadedUndo);
    QString loadError;
    ctx.Check(loaded.FromJson(root, &loadError), QStringLiteral("反序列化画布"), loadError);
    ctx.Check(loaded.CanvasItemCount() == 2, QStringLiteral("画布图元数量一致"));
    ctx.Check(loaded.canvasBackground().type == BackgroundType::COLOR &&
                  loaded.canvasBackground().color == background.color,
              QStringLiteral("画布背景恢复"));

    LineAbstractItem* loadedStroke = nullptr;
    ShapeAbstractItem* loadedImage = nullptr;
    const QList<CanvasItem*> loadedItems = loaded.CanvasItems();
    for (CanvasItem* item : loadedItems) {
        if (dynamic_cast<LineAbstractItem*>(item)) {
            loadedStroke = static_cast<LineAbstractItem*>(item);
        } else if (item->Kind() == QStringLiteral("shape")) {
            if (static_cast<ShapeAbstractItem*>(item)->HasFillImage()) {
                loadedImage = static_cast<ShapeAbstractItem*>(item);
            }
        }
    }
    ctx.Check(loadedStroke != nullptr, QStringLiteral("画布包含曲线"));
    if (loadedStroke) {
        ctx.Check(loadedStroke->StrokeColor() == QColor(QStringLiteral("#EF4444")) &&
                      loadedStroke->strokePattern() == Qt::DashLine,
                  QStringLiteral("曲线颜色与描线样式恢复"),
                  QString::number(static_cast<int>(loadedStroke->strokePattern())));
        ctx.Check(qAbs(loadedStroke->ContentSize().width() - resized.width()) < 0.5 &&
                      qAbs(loadedStroke->ContentSize().height() - resized.height()) < 0.5,
                  QStringLiteral("曲线缩放状态恢复"));
    }
    ctx.Check(loadedImage != nullptr && !loadedImage->FillImage().isNull(), QStringLiteral("图片内容恢复"));
    ctx.Check(loadedStroke && qAbs(loadedStroke->ItemOpacity() - 0.35) < 0.01 && loadedImage &&
                  qAbs(loadedImage->ItemOpacity() - 0.6) < 0.01,
              QStringLiteral("曲线与图片的透明度随画布恢复"),
              QStringLiteral("stroke=%1 image=%2")
                  .arg(loadedStroke ? loadedStroke->ItemOpacity() : -1.0)
                  .arg(loadedImage ? loadedImage->ItemOpacity() : -1.0));

    const QImage rendered = loaded.RenderImage(loaded.CanvasRect(), 1.0);
    ctx.Check(rendered.size() == QSize(800, 600), QStringLiteral("导出图像尺寸正确"));
    ctx.Check(!rendered.isNull() && rendered.pixelColor(8, 8) == background.color, QStringLiteral("导出包含背景颜色"));

    const QString jpegPath = QDir(ctx.workDirectory).filePath(QStringLiteral("selftest.jpg"));
    QImageWriter jpegWriter(jpegPath, "JPEG");
    jpegWriter.setQuality(90);
    ctx.Check(jpegWriter.write(rendered), QStringLiteral("导出 JPEG"), jpegWriter.errorString());

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
    ctx.Check(pdfOk && QFileInfo(pdfPath).size() > 0, QStringLiteral("导出 PDF"));

    CanvasScene imageBackgroundScene;
    QPixmap backgroundPixmap(64, 48);
    backgroundPixmap.fill(QColor(QStringLiteral("#10B981")));
    CanvasBackground imageBackground;
    imageBackground.type = BackgroundType::IMAGE;
    imageBackground.image = backgroundPixmap;
    imageBackground.mode = BackgroundMode::TILE;
    imageBackgroundScene.ReplaceCanvasBackground(imageBackground);

    CanvasScene imageBackgroundLoaded;
    QString imageBackgroundError;
    ctx.Check(imageBackgroundLoaded.FromJson(imageBackgroundScene.ToJson(), &imageBackgroundError),
              QStringLiteral("背景图片序列化"), imageBackgroundError);
    ctx.Check(imageBackgroundLoaded.canvasBackground().type == BackgroundType::IMAGE &&
                  !imageBackgroundLoaded.canvasBackground().image.isNull() &&
                  imageBackgroundLoaded.canvasBackground().mode == BackgroundMode::TILE,
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
    reloaded.SetUndoStack(&reloadedUndo);
    QFile canvasFile(canvasPath);
    ctx.Check(canvasFile.open(QIODevice::ReadOnly), QStringLiteral("读取画布文件"));
    const QJsonDocument reloadedDocument = QJsonDocument::fromJson(canvasFile.readAll());
    ctx.Check(reloadedDocument.isObject(), QStringLiteral("画布文件 JSON 有效"));
    ctx.Check(reloaded.FromJson(reloadedDocument.object()), QStringLiteral("从文件载入画布"));
    ctx.Check(reloaded.CanvasItemCount() == 2, QStringLiteral("文件载入图元数量一致"));

    reloaded.SetMode(ToolMode::EDIT);
    const QList<CanvasItem*> reloadedItems = reloaded.CanvasItems();
    if (!reloadedItems.isEmpty()) {
        reloadedItems.constFirst()->setSelected(true);
        reloaded.DeleteSelected();
        ctx.Check(reloaded.CanvasItemCount() == 1, QStringLiteral("删除选中图元"));
        reloadedUndo.undo();
        ctx.Check(reloaded.CanvasItemCount() == 2, QStringLiteral("撤销删除图元"));
        reloadedUndo.redo();
        ctx.Check(reloaded.CanvasItemCount() == 1, QStringLiteral("重做删除图元"));
    }

    scene.SetCanvasSizeWithUndo(QSizeF(1000, 700));
    ctx.Check(qFuzzyCompare(scene.CanvasSize().width(), 1000.0), QStringLiteral("调整画板尺寸"));
    undo.undo();
    ctx.Check(qFuzzyCompare(scene.CanvasSize().width(), 800.0), QStringLiteral("撤销调整画板尺寸"));

    // 透明背景在屏幕视图上画白 / 浅灰棋盘格，离屏导出图仍保持纯透明。
    {
        CanvasScene checkerScene;
        checkerScene.ApplyCanvasSize(QSizeF(200, 120));
        checkerScene.SetShowGrid(false);

        const QImage checkerExport = checkerScene.RenderImage(checkerScene.CanvasRect(), 1.0);
        ctx.Check(checkerExport.pixelColor(5, 5).alpha() == 0, QStringLiteral("棋盘格不进入离屏导出图"));

        CanvasView checkerView(&checkerScene);
        checkerView.resize(260, 180);
        checkerView.SetMode(ToolMode::VIEW);
        checkerView.show();
        QApplication::processEvents();
        checkerView.viewport()->repaint();
        QApplication::processEvents();

        const QImage viewImage = checkerView.viewport()->grab().toImage();
        // 棋盘格按固定 64px 铺格（长度单位基准），不再按画布等分：
        // 首行 / 首列各取前 16 个格心采样，格数与步长任一不符都会失配。
        const QRectF checkerCanvas = checkerScene.CanvasRect();
        constexpr qreal TILE_WIDTH = 64.0;
        constexpr qreal TILE_HEIGHT = 64.0;
        const int sampleCount =
            qMin(16, qMin(qFloor(checkerCanvas.width() / TILE_WIDTH), qFloor(checkerCanvas.height() / TILE_HEIGHT)));
        int mismatchedCells = 0;
        for (int index = 0; index < sampleCount; ++index) {
            const bool expectWhite = (index % 2) == 0;
            const QPoint columnPoint = checkerView.mapFromScene(
                QPointF(checkerCanvas.left() + TILE_WIDTH * (index + 0.5), checkerCanvas.top() + TILE_HEIGHT * 0.5));
            const QPoint rowPoint = checkerView.mapFromScene(
                QPointF(checkerCanvas.left() + TILE_WIDTH * 0.5, checkerCanvas.top() + TILE_HEIGHT * (index + 0.5)));
            for (const QColor& cellPixel : {viewImage.pixelColor(columnPoint), viewImage.pixelColor(rowPoint)}) {
                const bool isWhite = cellPixel.red() > 245 && cellPixel.green() > 245 && cellPixel.blue() > 245;
                const bool isGray = cellPixel.red() < 235 && cellPixel.blue() < 245;
                if (!(expectWhite ? isWhite : isGray)) {
                    ++mismatchedCells;
                }
            }
        }
        ctx.Check(mismatchedCells == 0, QStringLiteral("视图透明背景按画布 16×16 等分显示棋盘格"),
                  QStringLiteral("tile=%1x%2 mismatched=%3").arg(TILE_WIDTH).arg(TILE_HEIGHT).arg(mismatchedCells));
    }

    // ---------------------------------------------------------------
}
