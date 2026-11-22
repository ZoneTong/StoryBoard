#include "app/I18n.h"
#include "app/Settings.h"
#include "canvas/CanvasView.h"
#include "io/CustomShapeTemplates.h"
#include "items/CanvasTypes.h"
#include "items/lines/CurveItem.h"
#include "items/polygon/PolygonItem.h"
#include "items/shapes/ShapeAbstractItem.h"
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
#include "ui/panels/itemdialog/ItemPropertiesDialog.h"
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


void selftest::RunLayerModelTests(Context& ctx)
{
    CanvasScene& layerScene = ctx.layerScene;
    QUndoStack& layerUndo = ctx.layerUndo;

    layerUndo.setUndoLimit(100);
    layerScene.SetUndoStack(&layerUndo);
    layerScene.ApplyCanvasSize(QSizeF(400, 300));

    ctx.Check(layerScene.Layers().size() == 2 &&
                  layerScene.Layers().constFirst().id == CanvasScene::BackgroundLayerId &&
                  layerScene.LayerIndex(CanvasScene::BackgroundLayerId) == 0,
              QStringLiteral("默认包含背景第 0 层"));
    ctx.Check(layerScene.ActiveLayerId() == layerScene.Layers().at(1).id, QStringLiteral("默认激活底层用户图层"));


    auto* lower = MakeLineItem(QPointF(20, 20), QPointF(180, 120), QColor(QStringLiteral("#DC2626")));
    layerScene.AddItemsWithUndo({lower}, QStringLiteral("添加底层图元"));
    ctx.Check(lower->LayerId() == layerScene.ActiveLayerId() && lower->ZOrder() == 1,
              QStringLiteral("新图元进入当前图层并从 height 1 开始"),
              QStringLiteral("layer=%1 height=%2").arg(lower->LayerId()).arg(lower->ZOrder()));

    auto* upper = MakeLineItem(QPointF(40, 40), QPointF(200, 160), QColor(QStringLiteral("#2563EB")));
    layerScene.AddItemsWithUndo({upper}, QStringLiteral("添加同层图元"));
    ctx.Check(upper->ZOrder() == 2 && upper->zValue() > lower->zValue(),
              QStringLiteral("同层新图元 height 递增且覆盖旧图元"));

    ctx.Check(layerScene.AddLayerWithUndo(QStringLiteral("顶层")), QStringLiteral("新建图层进入撤销栈"));
    const int topLayerId = layerScene.ActiveLayerId();
    ctx.Check(layerScene.UserLayerCount() == 2 && topLayerId > CanvasScene::BackgroundLayerId,
              QStringLiteral("新图层位于最上层并成为当前图层"));

    auto* topLine = MakeLineItem(QPointF(60, 60), QPointF(220, 180), QColor(QStringLiteral("#059669")));
    layerScene.AddItemsWithUndo({topLine}, QStringLiteral("添加上层图元"));
    ctx.Check(topLine->LayerId() == topLayerId && topLine->ZOrder() == 1, QStringLiteral("上层图元使用新图层"));
    ctx.Check(topLine->zValue() > upper->zValue(), QStringLiteral("高图层的低 height 图元仍覆盖低图层图元"));

    ctx.Check(layerScene.MoveLayerWithUndo(topLayerId, -1), QStringLiteral("图层下移一层"));
    ctx.Check(topLine->zValue() < lower->zValue(), QStringLiteral("图层下移后绘制顺序同步降低"));
    layerUndo.undo();
    ctx.Check(topLine->zValue() > upper->zValue(), QStringLiteral("撤销图层下移恢复绘制顺序"));
    layerUndo.redo();
    ctx.Check(topLine->zValue() < lower->zValue(), QStringLiteral("重做图层下移"));
    layerUndo.undo();

    ctx.Check(!layerScene.MoveLayerWithUndo(CanvasScene::BackgroundLayerId, 1), QStringLiteral("背景第 0 层不能上移"));

    QString layerError;
    ctx.Check(!layerScene.RemoveLayerWithUndo(topLayerId, &layerError) && !layerError.isEmpty(),
              QStringLiteral("非空图层不可直接删除"));

    ctx.Check(layerScene.AddLayerWithUndo(QStringLiteral("临时空层")), QStringLiteral("新建临时图层"));
    const int temporaryLayerId = layerScene.ActiveLayerId();
    ctx.Check(layerScene.RemoveLayerWithUndo(temporaryLayerId), QStringLiteral("空图层可以删除"));
    ctx.Check(!layerScene.HasLayer(temporaryLayerId) && layerScene.UserLayerCount() == 2,
              QStringLiteral("删除空图层后图层数量减少"));
    layerUndo.undo();
    ctx.Check(layerScene.HasLayer(temporaryLayerId) && layerScene.ActiveLayerId() == temporaryLayerId,
              QStringLiteral("撤销删除图层"));
    layerUndo.redo();
    ctx.Check(!layerScene.HasLayer(temporaryLayerId), QStringLiteral("重做删除图层"));

    ctx.Check(layerScene.AdjustItemHeight(upper, 1), QStringLiteral("图元高度 +1 进入撤销栈"));
    ctx.Check(upper->ZOrder() == 3 && upper->zValue() > lower->zValue(),
              QStringLiteral("height +1 后图元位于同层更高处"));
    layerUndo.undo();
    ctx.Check(upper->ZOrder() == 2, QStringLiteral("撤销 height +1"));
    layerUndo.redo();
    ctx.Check(upper->ZOrder() == 3, QStringLiteral("重做 height +1"));
    layerUndo.undo();

    ctx.Check(layerScene.AdjustItemHeight(lower, 1), QStringLiteral("height +1 遇到同高图元时交换"));
    ctx.Check(lower->ZOrder() == 2 && upper->ZOrder() == 1 && lower->zValue() > upper->zValue(),
              QStringLiteral("height +1 跨过同层相邻图元"));
    layerUndo.undo();
    ctx.Check(lower->ZOrder() == 1 && upper->ZOrder() == 2 && lower->zValue() < upper->zValue(),
              QStringLiteral("撤销 height 交换"));
    layerUndo.redo();
    ctx.Check(lower->ZOrder() == 2 && upper->ZOrder() == 1, QStringLiteral("重做 height 交换"));
    layerUndo.undo();

    ctx.Check(layerScene.AdjustItemHeight(upper, -1), QStringLiteral("图元高度 -1 进入撤销栈"));
    ctx.Check(upper->ZOrder() == 1 && lower->ZOrder() == 2 && upper->zValue() < lower->zValue(),
              QStringLiteral("height -1 后图元位于同层更低处"));
    layerUndo.undo();
    ctx.Check(upper->ZOrder() == 2 && lower->ZOrder() == 1, QStringLiteral("撤销 height -1"));

    ctx.Check(layerScene.AdjustItemHeight(lower, -1) && lower->ZOrder() == 0, QStringLiteral("height 可以降到 0"));
    ctx.Check(!layerScene.AdjustItemHeight(lower, -1) && lower->ZOrder() == 0, QStringLiteral("height 不会低于 0"));
    layerUndo.undo();
    ctx.Check(lower->ZOrder() == 1, QStringLiteral("撤销 height 降到 0"));

    // 需求 24：height 合法值包含 0；打开属性窗时不能把 0 夹成 1，也不能
    // 因为层内存在 height 空洞而把当前值收紧。
    lower->SetZOrder(0);
    const int zeroHeightUndoBefore = layerUndo.count();
    {
        ItemPropertiesDialog zeroHeightDialog(&layerScene, lower);
        zeroHeightDialog.show();
        QApplication::processEvents();
        QSpinBox* zeroHeightValue = zeroHeightDialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesHeightValue"));
        ctx.Check(zeroHeightValue && zeroHeightValue->value() == 0, QStringLiteral("属性窗显示 height 0 且不夹紧"),
                  zeroHeightValue ? QString::number(zeroHeightValue->value()) : QString());
        ctx.Check(zeroHeightValue && zeroHeightValue->maximum() >= 3, QStringLiteral("属性窗 height 上限允许层内空洞"));
        zeroHeightDialog.accept();
        QApplication::processEvents();
    }
    ctx.Check(layerUndo.count() == zeroHeightUndoBefore, QStringLiteral("height 0 属性窗未修改时不产生撤销记录"));
    lower->SetZOrder(1);

    ctx.Check(layerScene.MoveItemToAdjacentLayer(lower, 1), QStringLiteral("图元移到上一层图层"));
    ctx.Check(lower->LayerId() == topLayerId && lower->zValue() > topLine->zValue(),
              QStringLiteral("跨图层移动后覆盖目标层图元"));
    layerUndo.undo();
    ctx.Check(lower->LayerId() == layerScene.Layers().at(1).id && lower->ZOrder() == 1,
              QStringLiteral("撤销图元跨图层移动"));
    layerUndo.redo();
    ctx.Check(lower->LayerId() == topLayerId, QStringLiteral("重做图元跨图层移动"));
    layerUndo.undo();

    const QJsonObject layerRoot = layerScene.ToJson();
    ctx.Check(layerRoot.value(QStringLiteral("version")).toInt() == 2 &&
                  layerRoot.value(QStringLiteral("layers")).toArray().size() == 3 &&
                  layerRoot.value(QStringLiteral("activeLayer")).toInt() == layerScene.ActiveLayerId(),
              QStringLiteral("画布保存图层与当前图层"));
    const QByteArray layerBytes = QJsonDocument(layerRoot).toJson(QJsonDocument::Compact);
    ctx.Check(layerBytes.contains("\"layer\":") && layerBytes.contains("\"height\":"),
              QStringLiteral("画布保存图元 layer 与 height"));

    CanvasScene layerLoaded;
    QString layerLoadError;
    ctx.Check(layerLoaded.FromJson(layerRoot, &layerLoadError), QStringLiteral("图层画布反序列化"), layerLoadError);
    ctx.Check(layerLoaded.Layers() == layerScene.Layers() && layerLoaded.ActiveLayerId() == layerScene.ActiveLayerId(),
              QStringLiteral("图层顺序与当前图层恢复"));
    ctx.Check(layerLoaded.CanvasItemCount() == 3, QStringLiteral("图层画布图元数量一致"));
    bool layerRoundTrip = layerLoaded.CanvasItemCount() == layerScene.CanvasItemCount();
    for (const CanvasLayer& entry : layerScene.Layers()) {
        if (layerLoaded.LayerItemCount(entry.id) != layerScene.LayerItemCount(entry.id)) {
            layerRoundTrip = false;
            break;
        }
    }
    ctx.Check(layerRoundTrip, QStringLiteral("图层内图元归属恢复"));

    QJsonObject missingLayersRoot = layerRoot;
    missingLayersRoot.remove(QStringLiteral("layers"));
    missingLayersRoot.remove(QStringLiteral("activeLayer"));
    CanvasScene missingLayersScene;
    QString missingLayersError;
    ctx.Check(!missingLayersScene.FromJson(missingLayersRoot, &missingLayersError),
              QStringLiteral("缺少图层数组的画布被拒绝"));

    // ---------------------------------------------------------------

    ctx.lowerLine = lower;
    ctx.upperLine = upper;
    ctx.topLine = topLine;
    ctx.topLayerId = topLayerId;
}

void selftest::RunLayerVisibilityLockTests(Context& ctx)
{
    CanvasScene& layerScene = ctx.layerScene;
    CurveItem* lower = ctx.lowerLine;
    CurveItem* upper = ctx.upperLine;
    CurveItem* topLine = ctx.topLine;
    const int& topLayerId = ctx.topLayerId;

    const int lowerLayerId = layerScene.Layers().at(1).id;
    layerScene.SetMode(ToolMode::EDIT);
    ctx.Check(layerScene.IsItemEditable(topLine) && layerScene.IsItemEditable(lower) &&
                  layerScene.IsItemEditable(upper),
              QStringLiteral("编辑模式下可见且未锁定图层的图元都可编辑"));
    ctx.Check((topLine->flags() & QGraphicsItem::ItemIsSelectable) != 0 &&
                  (lower->flags() & QGraphicsItem::ItemIsSelectable) != 0 &&
                  (upper->flags() & QGraphicsItem::ItemIsSelectable) != 0,
              QStringLiteral("可见且未锁定图层的图元都可选中"));

    layerScene.SelectAllItems();
    ctx.Check(topLine->isSelected() && lower->isSelected() && upper->isSelected(),
              QStringLiteral("全选包含所有可见且未锁定图层的图元"));
    layerScene.clearSelection();

    ctx.Check(layerScene.SetLayerLocked(lowerLayerId, true) && layerScene.IsLayerLocked(lowerLayerId),
              QStringLiteral("锁定图层"));
    ctx.Check(!layerScene.IsItemEditable(lower) && (lower->flags() & QGraphicsItem::ItemIsSelectable) == 0,
              QStringLiteral("锁定图层上的图元不可编辑也不可选中"));
    ctx.Check(layerScene.IsItemEditable(topLine), QStringLiteral("锁定一个图层不影响其他图层编辑"));
    ctx.Check(layerScene.IsLayerLocked(CanvasScene::BackgroundLayerId), QStringLiteral("背景层默认锁定"));
    ctx.Check(!layerScene.SetLayerLocked(CanvasScene::BackgroundLayerId, false) &&
                  layerScene.IsLayerLocked(CanvasScene::BackgroundLayerId),
              QStringLiteral("背景层锁定状态不可更改"));
    ctx.Check(layerScene.SetLayerLocked(lowerLayerId, false) && layerScene.IsItemEditable(lower),
              QStringLiteral("解锁图层后图元恢复可编辑"));

    layerScene.SetActiveLayer(lowerLayerId);
    ctx.Check(layerScene.ActiveLayerId() == lowerLayerId && layerScene.IsLayerVisible(lowerLayerId),
              QStringLiteral("切换当前图层后该图层可见"));
    ctx.Check(layerScene.IsItemEditable(topLine) && layerScene.IsItemEditable(lower),
              QStringLiteral("切换当前图层不改变其他可见图层的编辑权限"));

    ctx.Check(layerScene.SetLayerVisible(topLayerId, false), QStringLiteral("隐藏非当前图层"));
    ctx.Check(!layerScene.IsLayerVisible(topLayerId) && !topLine->isVisible(), QStringLiteral("隐藏图层后图元不可见"));
    ctx.Check(!layerScene.IsItemEditable(topLine) && !topLine->isSelected(),
              QStringLiteral("隐藏图层上的图元不可编辑"));
    ctx.Check(layerScene.IsItemEditable(lower), QStringLiteral("隐藏其他图层不影响当前图层编辑"));

    ctx.Check(layerScene.SetLayerVisible(topLayerId, true), QStringLiteral("显示非当前图层"));
    ctx.Check(topLine->isVisible() && layerScene.IsItemEditable(topLine),
              QStringLiteral("重新显示图层后图元恢复可编辑"));

    layerScene.SetActiveLayer(topLayerId);
    ctx.Check(layerScene.SetLayerVisible(topLayerId, false), QStringLiteral("隐藏当前图层"));
    ctx.Check(layerScene.ActiveLayerId() != topLayerId && !layerScene.IsLayerVisible(topLayerId) &&
                  layerScene.IsLayerVisible(layerScene.ActiveLayerId()),
              QStringLiteral("隐藏当前图层自动切换到其他可见图层"));
    ctx.Check(!layerScene.IsItemEditable(topLine), QStringLiteral("隐藏后原当前图层图元不可编辑"));

    const QRectF hiddenContent = layerScene.ContentRect();
    ctx.Check(hiddenContent.right() < topLine->SceneContentRect().right() - 1.0,
              QStringLiteral("隐藏图层不参与内容范围"),
              QStringLiteral("contentRight=%1 hiddenItemRight=%2")
                  .arg(hiddenContent.right())
                  .arg(topLine->SceneContentRect().right()));

    layerScene.SetActiveLayer(topLayerId);
    ctx.Check(layerScene.IsLayerVisible(topLayerId) && topLine->isVisible() && layerScene.IsItemEditable(topLine),
              QStringLiteral("选中隐藏图层时自动显示并允许编辑"));

    CanvasBackground visibilityBackground;
    visibilityBackground.type = BackgroundType::COLOR;
    visibilityBackground.color = QColor(QStringLiteral("#FEF3C7"));
    layerScene.SetCanvasBackground(visibilityBackground);
    ctx.Check(layerScene.SetLayerVisible(CanvasScene::BackgroundLayerId, false), QStringLiteral("隐藏背景层"));
    const QImage noBackgroundImage = layerScene.RenderImage(layerScene.CanvasRect(), 1.0);
    ctx.Check(noBackgroundImage.pixelColor(5, 5).alpha() == 0, QStringLiteral("隐藏背景层后导出为透明底"));
    ctx.Check(layerScene.SetLayerVisible(CanvasScene::BackgroundLayerId, true), QStringLiteral("显示背景层"));
    const QImage backgroundImage = layerScene.RenderImage(layerScene.CanvasRect(), 1.0);
    ctx.Check(backgroundImage.pixelColor(5, 5) == visibilityBackground.color,
              QStringLiteral("显示背景层后恢复背景颜色"));

    ctx.Check(layerScene.SetLayerVisible(lowerLayerId, false), QStringLiteral("隐藏带图元的下层"));
    ctx.Check(layerScene.SetLayerVisible(CanvasScene::BackgroundLayerId, false), QStringLiteral("画布保存前隐藏背景"));
    const QJsonObject visibilityRoot = layerScene.ToJson();
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
    ctx.Check(lowerHiddenInJson && backgroundHiddenInJson, QStringLiteral("画布保存图层显示/隐藏状态"));

    CanvasScene visibilityLoaded;
    QString visibilityLoadError;
    ctx.Check(visibilityLoaded.FromJson(visibilityRoot, &visibilityLoadError),
              QStringLiteral("图层显示状态画布反序列化"), visibilityLoadError);
    ctx.Check(!visibilityLoaded.IsLayerVisible(lowerLayerId) &&
                  !visibilityLoaded.IsLayerVisible(CanvasScene::BackgroundLayerId) &&
                  visibilityLoaded.IsLayerVisible(topLayerId),
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
    ctx.Check(hiddenActiveScene.FromJson(hiddenActiveRoot, &hiddenActiveError),
              QStringLiteral("载入当前图层被隐藏的画布"), hiddenActiveError);
    ctx.Check(hiddenActiveScene.ActiveLayerId() == topLayerId && hiddenActiveScene.IsLayerVisible(topLayerId),
              QStringLiteral("载入时自动显示当前图层"));

    // A hidden layer must also be absent from actual rendering.
    CanvasScene hiddenPaintScene;
    hiddenPaintScene.ApplyCanvasSize(QSizeF(120, 120));
    hiddenPaintScene.SetShowGrid(false);
    hiddenPaintScene.SetMode(ToolMode::EDIT);
    const QColor redColor(QStringLiteral("#DC2626"));
    const QColor blueColor(QStringLiteral("#2563EB"));

    auto* redImage = CreateShapeItem(ShapeType::RECTANGLE);
    redImage->SetStyle(QColor(Qt::transparent), redColor, 1.0);
    redImage->SetContentSize(QSizeF(80, 80));
    redImage->setPos(QPointF(10, 10));
    hiddenPaintScene.AddItemsWithUndo({redImage}, QStringLiteral("添加红色图元"));

    ctx.Check(hiddenPaintScene.AddLayerWithUndo(QStringLiteral("蓝色层")), QStringLiteral("隐藏渲染测试新建上层"));
    auto* blueImage = CreateShapeItem(ShapeType::RECTANGLE);
    blueImage->SetStyle(QColor(Qt::transparent), blueColor, 1.0);
    blueImage->SetContentSize(QSizeF(80, 80));
    blueImage->setPos(QPointF(20, 20));
    hiddenPaintScene.AddItemsWithUndo({blueImage}, QStringLiteral("添加蓝色图元"));
    const int hiddenBlueLayerId = hiddenPaintScene.ActiveLayerId();

    ctx.Check(hiddenPaintScene.SetLayerVisible(hiddenBlueLayerId, false), QStringLiteral("隐藏蓝色图层"));
    ctx.Check(hiddenPaintScene.ActiveLayerId() != hiddenBlueLayerId, QStringLiteral("隐藏当前图层后自动切换编辑图层"));
    const QImage paintedHidden = hiddenPaintScene.RenderImage(hiddenPaintScene.CanvasRect(), 1.0);
    ctx.Check(paintedHidden.pixelColor(30, 30) == QColor(QStringLiteral("#DC2626")),
              QStringLiteral("隐藏图层图元不参与渲染"));
    ctx.Check(hiddenPaintScene.ContentRect().right() < blueImage->SceneContentRect().right() - 1.0,
              QStringLiteral("隐藏图层图元不参与内容外接矩形"));

    ctx.Check(hiddenPaintScene.SetLayerVisible(hiddenBlueLayerId, true), QStringLiteral("显示蓝色图层"));
    const QImage paintedShown = hiddenPaintScene.RenderImage(hiddenPaintScene.CanvasRect(), 1.0);
    ctx.Check(paintedShown.pixelColor(30, 30) == QColor(QStringLiteral("#2563EB")),
              QStringLiteral("显示图层后图元重新参与渲染"));
    ctx.Check(hiddenPaintScene.IsItemEditable(blueImage) && hiddenPaintScene.IsItemEditable(redImage),
              QStringLiteral("显示非当前图层后其图元同样可编辑"));
    ctx.Check(hiddenPaintScene.SetLayerLocked(hiddenBlueLayerId, true), QStringLiteral("锁定蓝色图层"));
    ctx.Check(!hiddenPaintScene.IsItemEditable(blueImage) && hiddenPaintScene.IsItemEditable(redImage),
              QStringLiteral("锁定蓝色图层后仅该层图元不可编辑"));
    ctx.Check(hiddenPaintScene.SetLayerLocked(hiddenBlueLayerId, false), QStringLiteral("解锁蓝色图层"));
    ctx.Check(hiddenPaintScene.IsItemEditable(blueImage), QStringLiteral("解锁后图元恢复可编辑"));

    CanvasScene singleLayerScene;
    singleLayerScene.SetMode(ToolMode::EDIT);
    const int singleLayerId = singleLayerScene.ActiveLayerId();
    ctx.Check(!singleLayerScene.SetLayerVisible(singleLayerId, false) && singleLayerScene.IsLayerVisible(singleLayerId),
              QStringLiteral("最后一个可编辑图层不能被隐藏"));

    // ---------------------------------------------------------------
}
