#include "Alignment.h"
#include "BoolOps.h"
#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CanvasView.h"
#include "Commands.h"
#include "CompositeShapeItem.h"
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
#include "StrokeItem.h"
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


void selftest::runLayerModelTests(Context& ctx)
{
    CanvasScene& layerScene = ctx.layerScene;
    QUndoStack& layerUndo = ctx.layerUndo;

    layerUndo.setUndoLimit(100);
    layerScene.setUndoStack(&layerUndo);
    layerScene.applyCanvasSize(QSizeF(400, 300));

    ctx.check(layerScene.layers().size() == 2 &&
                  layerScene.layers().constFirst().id == CanvasScene::BackgroundLayerId &&
                  layerScene.layerIndex(CanvasScene::BackgroundLayerId) == 0,
              QStringLiteral("默认包含背景第 0 层"));
    ctx.check(layerScene.activeLayerId() == layerScene.layers().at(1).id, QStringLiteral("默认激活底层用户图层"));


    auto* lower = makeLineItem(QPointF(20, 20), QPointF(180, 120), QColor(QStringLiteral("#DC2626")));
    layerScene.addItemsWithUndo({lower}, QStringLiteral("添加底层图元"));
    ctx.check(lower->layerId() == layerScene.activeLayerId() && lower->height() == 1,
              QStringLiteral("新图元进入当前图层并从 height 1 开始"),
              QStringLiteral("layer=%1 height=%2").arg(lower->layerId()).arg(lower->height()));

    auto* upper = makeLineItem(QPointF(40, 40), QPointF(200, 160), QColor(QStringLiteral("#2563EB")));
    layerScene.addItemsWithUndo({upper}, QStringLiteral("添加同层图元"));
    ctx.check(upper->height() == 2 && upper->zValue() > lower->zValue(),
              QStringLiteral("同层新图元 height 递增且覆盖旧图元"));

    ctx.check(layerScene.addLayerWithUndo(QStringLiteral("顶层")), QStringLiteral("新建图层进入撤销栈"));
    const int topLayerId = layerScene.activeLayerId();
    ctx.check(layerScene.userLayerCount() == 2 && topLayerId > CanvasScene::BackgroundLayerId,
              QStringLiteral("新图层位于最上层并成为当前图层"));

    auto* topLine = makeLineItem(QPointF(60, 60), QPointF(220, 180), QColor(QStringLiteral("#059669")));
    layerScene.addItemsWithUndo({topLine}, QStringLiteral("添加上层图元"));
    ctx.check(topLine->layerId() == topLayerId && topLine->height() == 1, QStringLiteral("上层图元使用新图层"));
    ctx.check(topLine->zValue() > upper->zValue(), QStringLiteral("高图层的低 height 图元仍覆盖低图层图元"));

    ctx.check(layerScene.moveLayerWithUndo(topLayerId, -1), QStringLiteral("图层下移一层"));
    ctx.check(topLine->zValue() < lower->zValue(), QStringLiteral("图层下移后绘制顺序同步降低"));
    layerUndo.undo();
    ctx.check(topLine->zValue() > upper->zValue(), QStringLiteral("撤销图层下移恢复绘制顺序"));
    layerUndo.redo();
    ctx.check(topLine->zValue() < lower->zValue(), QStringLiteral("重做图层下移"));
    layerUndo.undo();

    ctx.check(!layerScene.moveLayerWithUndo(CanvasScene::BackgroundLayerId, 1), QStringLiteral("背景第 0 层不能上移"));

    QString layerError;
    ctx.check(!layerScene.removeLayerWithUndo(topLayerId, &layerError) && !layerError.isEmpty(),
              QStringLiteral("非空图层不可直接删除"));

    ctx.check(layerScene.addLayerWithUndo(QStringLiteral("临时空层")), QStringLiteral("新建临时图层"));
    const int temporaryLayerId = layerScene.activeLayerId();
    ctx.check(layerScene.removeLayerWithUndo(temporaryLayerId), QStringLiteral("空图层可以删除"));
    ctx.check(!layerScene.hasLayer(temporaryLayerId) && layerScene.userLayerCount() == 2,
              QStringLiteral("删除空图层后图层数量减少"));
    layerUndo.undo();
    ctx.check(layerScene.hasLayer(temporaryLayerId) && layerScene.activeLayerId() == temporaryLayerId,
              QStringLiteral("撤销删除图层"));
    layerUndo.redo();
    ctx.check(!layerScene.hasLayer(temporaryLayerId), QStringLiteral("重做删除图层"));

    ctx.check(layerScene.adjustItemHeight(upper, 1), QStringLiteral("图元高度 +1 进入撤销栈"));
    ctx.check(upper->height() == 3 && upper->zValue() > lower->zValue(),
              QStringLiteral("height +1 后图元位于同层更高处"));
    layerUndo.undo();
    ctx.check(upper->height() == 2, QStringLiteral("撤销 height +1"));
    layerUndo.redo();
    ctx.check(upper->height() == 3, QStringLiteral("重做 height +1"));
    layerUndo.undo();

    ctx.check(layerScene.adjustItemHeight(lower, 1), QStringLiteral("height +1 遇到同高图元时交换"));
    ctx.check(lower->height() == 2 && upper->height() == 1 && lower->zValue() > upper->zValue(),
              QStringLiteral("height +1 跨过同层相邻图元"));
    layerUndo.undo();
    ctx.check(lower->height() == 1 && upper->height() == 2 && lower->zValue() < upper->zValue(),
              QStringLiteral("撤销 height 交换"));
    layerUndo.redo();
    ctx.check(lower->height() == 2 && upper->height() == 1, QStringLiteral("重做 height 交换"));
    layerUndo.undo();

    ctx.check(layerScene.adjustItemHeight(upper, -1), QStringLiteral("图元高度 -1 进入撤销栈"));
    ctx.check(upper->height() == 1 && lower->height() == 2 && upper->zValue() < lower->zValue(),
              QStringLiteral("height -1 后图元位于同层更低处"));
    layerUndo.undo();
    ctx.check(upper->height() == 2 && lower->height() == 1, QStringLiteral("撤销 height -1"));

    ctx.check(layerScene.adjustItemHeight(lower, -1) && lower->height() == 0, QStringLiteral("height 可以降到 0"));
    ctx.check(!layerScene.adjustItemHeight(lower, -1) && lower->height() == 0, QStringLiteral("height 不会低于 0"));
    layerUndo.undo();
    ctx.check(lower->height() == 1, QStringLiteral("撤销 height 降到 0"));

    // 需求 24：height 合法值包含 0；打开属性窗时不能把 0 夹成 1，也不能
    // 因为层内存在 height 空洞而把当前值收紧。
    lower->setHeight(0);
    const int zeroHeightUndoBefore = layerUndo.count();
    {
        ItemPropertiesDialog zeroHeightDialog(&layerScene, lower);
        zeroHeightDialog.show();
        QApplication::processEvents();
        QSpinBox* zeroHeightValue = zeroHeightDialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesHeightValue"));
        ctx.check(zeroHeightValue && zeroHeightValue->value() == 0, QStringLiteral("属性窗显示 height 0 且不夹紧"),
                  zeroHeightValue ? QString::number(zeroHeightValue->value()) : QString());
        ctx.check(zeroHeightValue && zeroHeightValue->maximum() >= 3, QStringLiteral("属性窗 height 上限允许层内空洞"));
        zeroHeightDialog.accept();
        QApplication::processEvents();
    }
    ctx.check(layerUndo.count() == zeroHeightUndoBefore, QStringLiteral("height 0 属性窗未修改时不产生撤销记录"));
    lower->setHeight(1);

    ctx.check(layerScene.moveItemToAdjacentLayer(lower, 1), QStringLiteral("图元移到上一层图层"));
    ctx.check(lower->layerId() == topLayerId && lower->zValue() > topLine->zValue(),
              QStringLiteral("跨图层移动后覆盖目标层图元"));
    layerUndo.undo();
    ctx.check(lower->layerId() == layerScene.layers().at(1).id && lower->height() == 1,
              QStringLiteral("撤销图元跨图层移动"));
    layerUndo.redo();
    ctx.check(lower->layerId() == topLayerId, QStringLiteral("重做图元跨图层移动"));
    layerUndo.undo();

    const QJsonObject layerRoot = layerScene.toJson();
    ctx.check(layerRoot.value(QStringLiteral("version")).toInt() == 2 &&
                  layerRoot.value(QStringLiteral("layers")).toArray().size() == 3 &&
                  layerRoot.value(QStringLiteral("activeLayer")).toInt() == layerScene.activeLayerId(),
              QStringLiteral("画布保存图层与当前图层"));
    const QByteArray layerBytes = QJsonDocument(layerRoot).toJson(QJsonDocument::Compact);
    ctx.check(layerBytes.contains("\"layer\":") && layerBytes.contains("\"height\":"),
              QStringLiteral("画布保存图元 layer 与 height"));

    CanvasScene layerLoaded;
    QString layerLoadError;
    ctx.check(layerLoaded.fromJson(layerRoot, &layerLoadError), QStringLiteral("图层画布反序列化"), layerLoadError);
    ctx.check(layerLoaded.layers() == layerScene.layers() && layerLoaded.activeLayerId() == layerScene.activeLayerId(),
              QStringLiteral("图层顺序与当前图层恢复"));
    ctx.check(layerLoaded.canvasItemCount() == 3, QStringLiteral("图层画布图元数量一致"));
    bool layerRoundTrip = layerLoaded.canvasItemCount() == layerScene.canvasItemCount();
    for (const CanvasLayer& entry : layerScene.layers()) {
        if (layerLoaded.layerItemCount(entry.id) != layerScene.layerItemCount(entry.id)) {
            layerRoundTrip = false;
            break;
        }
    }
    ctx.check(layerRoundTrip, QStringLiteral("图层内图元归属恢复"));

    QJsonObject missingLayersRoot = layerRoot;
    missingLayersRoot.remove(QStringLiteral("layers"));
    missingLayersRoot.remove(QStringLiteral("activeLayer"));
    CanvasScene missingLayersScene;
    QString missingLayersError;
    ctx.check(!missingLayersScene.fromJson(missingLayersRoot, &missingLayersError),
              QStringLiteral("缺少图层数组的画布被拒绝"));

    // ---------------------------------------------------------------

    ctx.lowerLine = lower;
    ctx.upperLine = upper;
    ctx.topLine = topLine;
    ctx.topLayerId = topLayerId;
}

void selftest::runLayerVisibilityLockTests(Context& ctx)
{
    CanvasScene& layerScene = ctx.layerScene;
    StrokeItem* lower = ctx.lowerLine;
    StrokeItem* upper = ctx.upperLine;
    StrokeItem* topLine = ctx.topLine;
    const int& topLayerId = ctx.topLayerId;

    const int lowerLayerId = layerScene.layers().at(1).id;
    layerScene.setMode(ToolMode::Edit);
    ctx.check(layerScene.isItemEditable(topLine) && layerScene.isItemEditable(lower) &&
                  layerScene.isItemEditable(upper),
              QStringLiteral("编辑模式下可见且未锁定图层的图元都可编辑"));
    ctx.check((topLine->flags() & QGraphicsItem::ItemIsSelectable) != 0 &&
                  (lower->flags() & QGraphicsItem::ItemIsSelectable) != 0 &&
                  (upper->flags() & QGraphicsItem::ItemIsSelectable) != 0,
              QStringLiteral("可见且未锁定图层的图元都可选中"));

    layerScene.selectAllItems();
    ctx.check(topLine->isSelected() && lower->isSelected() && upper->isSelected(),
              QStringLiteral("全选包含所有可见且未锁定图层的图元"));
    layerScene.clearSelection();

    ctx.check(layerScene.setLayerLocked(lowerLayerId, true) && layerScene.isLayerLocked(lowerLayerId),
              QStringLiteral("锁定图层"));
    ctx.check(!layerScene.isItemEditable(lower) && (lower->flags() & QGraphicsItem::ItemIsSelectable) == 0,
              QStringLiteral("锁定图层上的图元不可编辑也不可选中"));
    ctx.check(layerScene.isItemEditable(topLine), QStringLiteral("锁定一个图层不影响其他图层编辑"));
    ctx.check(layerScene.isLayerLocked(CanvasScene::BackgroundLayerId), QStringLiteral("背景层默认锁定"));
    ctx.check(!layerScene.setLayerLocked(CanvasScene::BackgroundLayerId, false) &&
                  layerScene.isLayerLocked(CanvasScene::BackgroundLayerId),
              QStringLiteral("背景层锁定状态不可更改"));
    ctx.check(layerScene.setLayerLocked(lowerLayerId, false) && layerScene.isItemEditable(lower),
              QStringLiteral("解锁图层后图元恢复可编辑"));

    layerScene.setActiveLayer(lowerLayerId);
    ctx.check(layerScene.activeLayerId() == lowerLayerId && layerScene.isLayerVisible(lowerLayerId),
              QStringLiteral("切换当前图层后该图层可见"));
    ctx.check(layerScene.isItemEditable(topLine) && layerScene.isItemEditable(lower),
              QStringLiteral("切换当前图层不改变其他可见图层的编辑权限"));

    ctx.check(layerScene.setLayerVisible(topLayerId, false), QStringLiteral("隐藏非当前图层"));
    ctx.check(!layerScene.isLayerVisible(topLayerId) && !topLine->isVisible(), QStringLiteral("隐藏图层后图元不可见"));
    ctx.check(!layerScene.isItemEditable(topLine) && !topLine->isSelected(),
              QStringLiteral("隐藏图层上的图元不可编辑"));
    ctx.check(layerScene.isItemEditable(lower), QStringLiteral("隐藏其他图层不影响当前图层编辑"));

    ctx.check(layerScene.setLayerVisible(topLayerId, true), QStringLiteral("显示非当前图层"));
    ctx.check(topLine->isVisible() && layerScene.isItemEditable(topLine),
              QStringLiteral("重新显示图层后图元恢复可编辑"));

    layerScene.setActiveLayer(topLayerId);
    ctx.check(layerScene.setLayerVisible(topLayerId, false), QStringLiteral("隐藏当前图层"));
    ctx.check(layerScene.activeLayerId() != topLayerId && !layerScene.isLayerVisible(topLayerId) &&
                  layerScene.isLayerVisible(layerScene.activeLayerId()),
              QStringLiteral("隐藏当前图层自动切换到其他可见图层"));
    ctx.check(!layerScene.isItemEditable(topLine), QStringLiteral("隐藏后原当前图层图元不可编辑"));

    const QRectF hiddenContent = layerScene.contentRect();
    ctx.check(hiddenContent.right() < topLine->sceneContentRect().right() - 1.0,
              QStringLiteral("隐藏图层不参与内容范围"),
              QStringLiteral("contentRight=%1 hiddenItemRight=%2")
                  .arg(hiddenContent.right())
                  .arg(topLine->sceneContentRect().right()));

    layerScene.setActiveLayer(topLayerId);
    ctx.check(layerScene.isLayerVisible(topLayerId) && topLine->isVisible() && layerScene.isItemEditable(topLine),
              QStringLiteral("选中隐藏图层时自动显示并允许编辑"));

    BackgroundState visibilityBackground;
    visibilityBackground.type = BackgroundType::Color;
    visibilityBackground.color = QColor(QStringLiteral("#FEF3C7"));
    layerScene.applyBackground(visibilityBackground);
    ctx.check(layerScene.setLayerVisible(CanvasScene::BackgroundLayerId, false), QStringLiteral("隐藏背景层"));
    const QImage noBackgroundImage = layerScene.renderImage(layerScene.canvasRect(), 1.0);
    ctx.check(noBackgroundImage.pixelColor(5, 5) == QColor(Qt::white), QStringLiteral("隐藏背景层后导出为白色底"));
    ctx.check(layerScene.setLayerVisible(CanvasScene::BackgroundLayerId, true), QStringLiteral("显示背景层"));
    const QImage backgroundImage = layerScene.renderImage(layerScene.canvasRect(), 1.0);
    ctx.check(backgroundImage.pixelColor(5, 5) == visibilityBackground.color,
              QStringLiteral("显示背景层后恢复背景颜色"));

    ctx.check(layerScene.setLayerVisible(lowerLayerId, false), QStringLiteral("隐藏带图元的下层"));
    ctx.check(layerScene.setLayerVisible(CanvasScene::BackgroundLayerId, false), QStringLiteral("画布保存前隐藏背景"));
    const QJsonObject visibilityRoot = layerScene.toJson();
    const QJsonArray visibilityLayers = visibilityRoot.value(QStringLiteral("layers")).toArray();
    bool lowerHiddenInJson = false;
    bool backgroundHiddenInJson = false;
    for (const QJsonValue& value : visibilityLayers) {
        const QJsonObject layerJson = value.toObject();
        const int id = layerJson.value(QStringLiteral("id")).toInt();
        if (id == lowerLayerId) {
            lowerHiddenInJson = !layerJson.value(QStringLiteral("visible")).toBool(true);
        }
        if (id == CanvasScene::BackgroundLayerId) {
            backgroundHiddenInJson = !layerJson.value(QStringLiteral("visible")).toBool(true);
        }
    }
    ctx.check(lowerHiddenInJson && backgroundHiddenInJson, QStringLiteral("画布保存图层显示/隐藏状态"));

    CanvasScene visibilityLoaded;
    QString visibilityLoadError;
    ctx.check(visibilityLoaded.fromJson(visibilityRoot, &visibilityLoadError),
              QStringLiteral("图层显示状态画布反序列化"), visibilityLoadError);
    ctx.check(!visibilityLoaded.isLayerVisible(lowerLayerId) &&
                  !visibilityLoaded.isLayerVisible(CanvasScene::BackgroundLayerId) &&
                  visibilityLoaded.isLayerVisible(topLayerId),
              QStringLiteral("画布恢复图层显示/隐藏状态"));

    QJsonObject hiddenActiveRoot = visibilityRoot;
    QJsonArray hiddenActiveLayers = hiddenActiveRoot.value(QStringLiteral("layers")).toArray();
    for (int i = 0; i < hiddenActiveLayers.size(); ++i) {
        QJsonObject layerJson = hiddenActiveLayers.at(i).toObject();
        if (layerJson.value(QStringLiteral("id")).toInt() == topLayerId) {
            layerJson.insert(QStringLiteral("visible"), false);
            hiddenActiveLayers.replace(i, layerJson);
        }
    }
    hiddenActiveRoot.insert(QStringLiteral("layers"), hiddenActiveLayers);
    CanvasScene hiddenActiveScene;
    QString hiddenActiveError;
    ctx.check(hiddenActiveScene.fromJson(hiddenActiveRoot, &hiddenActiveError),
              QStringLiteral("载入当前图层被隐藏的画布"), hiddenActiveError);
    ctx.check(hiddenActiveScene.activeLayerId() == topLayerId && hiddenActiveScene.isLayerVisible(topLayerId),
              QStringLiteral("载入时自动显示当前图层"));

    // A hidden layer must also be absent from actual rendering.
    CanvasScene hiddenPaintScene;
    hiddenPaintScene.applyCanvasSize(QSizeF(120, 120));
    hiddenPaintScene.setShowGrid(false);
    hiddenPaintScene.setMode(ToolMode::Edit);
    QPixmap redPaint(80, 80);
    redPaint.fill(QColor(QStringLiteral("#DC2626")));
    QPixmap bluePaint(80, 80);
    bluePaint.fill(QColor(QStringLiteral("#2563EB")));

    auto* redImage = new ImageItem();
    redImage->setImage(redPaint, QStringLiteral("red"));
    redImage->setContentSize(QSizeF(80, 80));
    redImage->setPos(QPointF(10, 10));
    hiddenPaintScene.addItemsWithUndo({redImage}, QStringLiteral("添加红色图元"));

    ctx.check(hiddenPaintScene.addLayerWithUndo(QStringLiteral("蓝色层")), QStringLiteral("隐藏渲染测试新建上层"));
    auto* blueImage = new ImageItem();
    blueImage->setImage(bluePaint, QStringLiteral("blue"));
    blueImage->setContentSize(QSizeF(80, 80));
    blueImage->setPos(QPointF(20, 20));
    hiddenPaintScene.addItemsWithUndo({blueImage}, QStringLiteral("添加蓝色图元"));
    const int hiddenBlueLayerId = hiddenPaintScene.activeLayerId();

    ctx.check(hiddenPaintScene.setLayerVisible(hiddenBlueLayerId, false), QStringLiteral("隐藏蓝色图层"));
    ctx.check(hiddenPaintScene.activeLayerId() != hiddenBlueLayerId, QStringLiteral("隐藏当前图层后自动切换编辑图层"));
    const QImage paintedHidden = hiddenPaintScene.renderImage(hiddenPaintScene.canvasRect(), 1.0);
    ctx.check(paintedHidden.pixelColor(30, 30) == QColor(QStringLiteral("#DC2626")),
              QStringLiteral("隐藏图层图元不参与渲染"));
    ctx.check(hiddenPaintScene.contentRect().right() < blueImage->sceneContentRect().right() - 1.0,
              QStringLiteral("隐藏图层图元不参与内容外接矩形"));

    ctx.check(hiddenPaintScene.setLayerVisible(hiddenBlueLayerId, true), QStringLiteral("显示蓝色图层"));
    const QImage paintedShown = hiddenPaintScene.renderImage(hiddenPaintScene.canvasRect(), 1.0);
    ctx.check(paintedShown.pixelColor(30, 30) == QColor(QStringLiteral("#2563EB")),
              QStringLiteral("显示图层后图元重新参与渲染"));
    ctx.check(hiddenPaintScene.isItemEditable(blueImage) && hiddenPaintScene.isItemEditable(redImage),
              QStringLiteral("显示非当前图层后其图元同样可编辑"));
    ctx.check(hiddenPaintScene.setLayerLocked(hiddenBlueLayerId, true), QStringLiteral("锁定蓝色图层"));
    ctx.check(!hiddenPaintScene.isItemEditable(blueImage) && hiddenPaintScene.isItemEditable(redImage),
              QStringLiteral("锁定蓝色图层后仅该层图元不可编辑"));
    ctx.check(hiddenPaintScene.setLayerLocked(hiddenBlueLayerId, false), QStringLiteral("解锁蓝色图层"));
    ctx.check(hiddenPaintScene.isItemEditable(blueImage), QStringLiteral("解锁后图元恢复可编辑"));

    CanvasScene singleLayerScene;
    singleLayerScene.setMode(ToolMode::Edit);
    const int singleLayerId = singleLayerScene.activeLayerId();
    ctx.check(!singleLayerScene.setLayerVisible(singleLayerId, false) && singleLayerScene.isLayerVisible(singleLayerId),
              QStringLiteral("最后一个可编辑图层不能被隐藏"));

    // ---------------------------------------------------------------
}
