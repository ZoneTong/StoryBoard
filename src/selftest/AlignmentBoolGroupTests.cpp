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


void selftest::runAlignmentTests(Context& ctx)
{
    // 多图元对齐：左对齐后边缘一致、整体一条撤销记录，等间距至少要三个图元。
    {
        CanvasScene alignScene;
        alignScene.applyCanvasSize(QSizeF(800.0, 600.0));
        alignScene.setMode(ToolMode::Edit);
        QUndoStack alignUndo;
        alignScene.setUndoStack(&alignUndo);

        auto* leftItem = new ShapeItem(ShapeType::Rectangle);
        leftItem->setContentSize(QSizeF(120.0, 90.0));
        alignScene.addItemsWithUndo({leftItem}, QStringLiteral("Add align item"));
        auto* rightItem = new ShapeItem(ShapeType::Ellipse);
        rightItem->setContentSize(QSizeF(80.0, 60.0));
        alignScene.addItemsWithUndo({rightItem}, QStringLiteral("Add align item"));
        leftItem->setPos(QPointF(40.0, 60.0));
        rightItem->setPos(QPointF(240.0, 180.0));

        alignScene.clearSelection();
        leftItem->setSelected(true);
        rightItem->setSelected(true);
        ctx.check(Alignment::supports(&alignScene, Alignment::Mode::Left) &&
                      !Alignment::supports(&alignScene, Alignment::Mode::DistributeHorizontal),
                  QStringLiteral("对齐需要两个图元、等间距需要三个图元"));

        const int undoBefore = alignUndo.count();
        const int moved = Alignment::apply(&alignScene, Alignment::Mode::Left, QStringLiteral("Align items left"));
        ctx.check(
            moved == 1 && qAbs(leftItem->sceneContentRect().left() - rightItem->sceneContentRect().left()) < 0.01,
            QStringLiteral("左对齐后两图元左边缘一致"),
            QStringLiteral("dx=%1").arg(rightItem->sceneContentRect().left() - leftItem->sceneContentRect().left()));
        ctx.check(alignUndo.count() == undoBefore + 1, QStringLiteral("对齐整体只占一条撤销记录"));
        alignUndo.undo();
        ctx.check(qAbs(rightItem->pos().x() - 240.0) < 0.01, QStringLiteral("对齐可撤销"));

        // 等间距：三个图元按空隙均分。
        auto* thirdItem = new ShapeItem(ShapeType::Rectangle);
        thirdItem->setContentSize(QSizeF(60.0, 40.0));
        alignScene.addItemsWithUndo({thirdItem}, QStringLiteral("Add align item"));
        thirdItem->setPos(QPointF(620.0, 300.0));
        alignScene.clearSelection();
        leftItem->setSelected(true);
        rightItem->setSelected(true);
        thirdItem->setSelected(true);
        ctx.check(Alignment::supports(&alignScene, Alignment::Mode::DistributeHorizontal),
                  QStringLiteral("三个图元可以水平等间距"));
    }
}

void selftest::runAlignmentMenuTests(Context& ctx)
{
    // 菜单保留对齐下拉；主工具栏不再放对齐按钮，入口集中到菜单与编辑图元卡。
    {
        MainWindow alignWindow;
        QMenu* alignMenu = alignWindow.findChild<QMenu*>(QStringLiteral("AlignMenu"));
        QToolButton* alignButton = alignWindow.findChild<QToolButton*>(QStringLiteral("AlignToolButton"));
        ctx.check(alignMenu && alignMenu->actions().size() == 8, QStringLiteral("编辑菜单提供 8 项对齐动作"),
                  alignMenu ? QString::number(alignMenu->actions().size()) : QString());
        ctx.check(!alignButton, QStringLiteral("工具栏不再显示对齐按钮"));

        QAction* redoAction = nullptr;
        QAction* zoomInAction = nullptr;
        for (QAction* action : alignWindow.findChildren<QAction*>()) {
            if (action->text() == QStringLiteral("重做")) {
                redoAction = action;
            } else if (action->text() == QStringLiteral("放大")) {
                zoomInAction = action;
            }
        }
        ctx.check(redoAction && redoAction->shortcuts().size() == 1, QStringLiteral("重做只保留一个组合键"),
                  redoAction ? redoAction->shortcut().toString() : QString());
        ctx.check(zoomInAction && zoomInAction->shortcuts().size() == 1, QStringLiteral("放大只保留一个组合键"),
                  zoomInAction ? zoomInAction->shortcut().toString() : QString());
    }
}

void selftest::runGroupBoolOpsTests(Context& ctx)
{
    // 分组层级与布尔运算。组可以再被组包住：单击组内图元选中整棵组树。
    {
        QUndoStack groupUndo;
        CanvasScene groupScene;
        groupScene.setUndoStack(&groupUndo);
        groupScene.setMode(ToolMode::Edit);
        groupScene.applyCanvasSize(QSizeF(400.0, 300.0));
        auto makeRect = [&groupScene](qreal x, qreal y) {
            auto* item = new ShapeItem();
            item->setShapeType(ShapeType::Rectangle);
            item->setPos(QPointF(x, y));
            item->setContentSize(QSizeF(40.0, 40.0));
            item->setStyle(QColor(30, 41, 59), QColor(30, 41, 59, 36), 1.0);
            item->setFill(QColor(30, 41, 59, 36), FillStyle::Solid);
            groupScene.addItemsWithUndo({item}, QStringLiteral("Add group item"));
            return item;
        };
        CanvasItem* first = makeRect(10.0, 10.0);
        CanvasItem* second = makeRect(20.0, 20.0);
        CanvasItem* third = makeRect(30.0, 30.0);
        first->setSelected(true);
        second->setSelected(true);
        const int innerGroup = groupScene.groupSelectedItems();
        ctx.check(innerGroup == 2 && first->groupId() > 0 && groupScene.itemsInGroup(first->groupId()).size() == 2,
                  QStringLiteral("两个同层图元可以组合成一组"), QStringLiteral("grouped=%1").arg(innerGroup));

        third->setSelected(true);
        groupScene.groupSelectedItems();
        const int outerGroup = first->groupId() > 0 ? groupScene.outermostGroup(first->groupId()) : 0;
        ctx.check(
            outerGroup > 0 && groupScene.itemsInGroupTree(outerGroup).size() == 3,
            QStringLiteral("再和其他图元组合时形成外层组，覆盖整棵组树"),
            QStringLiteral("outer=%1 members=%2").arg(outerGroup).arg(groupScene.itemsInGroupTree(outerGroup).size()));

        groupScene.clearSelection();
        const bool selectedWholeTree = groupScene.selectGroupOfItem(second);
        ctx.check(selectedWholeTree && groupScene.selectedCanvasItems().size() == 3,
                  QStringLiteral("单击组内图元时选中整棵组树"),
                  QStringLiteral("selected=%1").arg(groupScene.selectedCanvasItems().size()));
        // 撤销分组后成员回到散图元状态。
        groupScene.undoStack()->undo();
        ctx.check(third->groupId() == 0 && groupScene.groupParents().isEmpty(),
                  QStringLiteral("撤销外层分组还原组层级"));
    }
    {
        QUndoStack boolUndo;
        CanvasScene boolScene;
        boolScene.setUndoStack(&boolUndo);
        boolScene.setMode(ToolMode::Edit);
        boolScene.applyCanvasSize(QSizeF(400.0, 300.0));
        auto makeRect = [&boolScene](qreal x, const QColor& color) {
            auto* item = new ShapeItem();
            item->setShapeType(ShapeType::Rectangle);
            item->setPos(QPointF(x, 40.0));
            item->setContentSize(QSizeF(60.0, 60.0));
            item->setStyle(color, color.lighter(160), 1.0);
            boolScene.addItemsWithUndo({item}, QStringLiteral("Add bool item"));
            return item;
        };
        CanvasItem* lower = makeRect(20.0, QColor(200, 40, 40));
        CanvasItem* upper = makeRect(50.0, QColor(40, 90, 200));
        lower->setSelected(true);
        upper->setSelected(true);
        ctx.check(BoolOps::supports(&boolScene), QStringLiteral("两个图元可以进行合并"), QStringLiteral("items=2"));
        const bool applied = BoolOps::apply(&boolScene, BoolOps::Operation::Union, QStringLiteral("合并"));
        const QList<CanvasItem*> selected = boolScene.selectedCanvasItems();
        auto* composite = selected.isEmpty() ? nullptr : dynamic_cast<CompositeShapeItem*>(selected.constFirst());
        ctx.check(applied && composite && composite->layerCount() == 1,
                  QStringLiteral("联合运算生成单层 donor 样式多边形"),
                  QStringLiteral("ok=%1 layers=%2").arg(applied ? 1 : 0).arg(composite ? composite->layerCount() : -1));

        // 零面积直线参与合并：线宽带只做填充、不再叠一层描线，线宽保持自身值。
        {
            QUndoStack undo;
            CanvasScene scene;
            scene.setUndoStack(&undo);
            scene.setMode(ToolMode::Edit);
            scene.applyCanvasSize(QSizeF(320.0, 220.0));
            auto* diamond = new ShapeItem();
            diamond->setShapeType(ShapeType::Diamond);
            diamond->setPos(QPointF(70.0, 40.0));
            diamond->setContentSize(QSizeF(160.0, 120.0));
            diamond->setStyle(QColor(20, 20, 20), QColor(255, 255, 255), 4.0);
            diamond->setFill(QColor(255, 255, 255), FillStyle::Solid);
            auto* line = new ShapeItem();
            line->setShapeType(ShapeType::Line);
            line->setPos(QPointF(30.0, 80.0));
            line->setContentSize(QSizeF(250.0, 40.0));
            line->setStyle(QColor(30, 90, 200), QColor(30, 90, 200), 2.0);
            scene.addItemsWithUndo({line, diamond}, QStringLiteral("Add line and diamond"));
            line->setSelected(true);
            diamond->setSelected(true);
            const bool lineMerged = BoolOps::apply(&scene, BoolOps::Operation::Union, QStringLiteral("合并"));
            const QList<CanvasItem*> lineSelected = scene.selectedCanvasItems();
            auto* lineShell =
                lineSelected.isEmpty() ? nullptr : dynamic_cast<CompositeShapeItem*>(lineSelected.constFirst());

            int lineThickness = -1;
            if (lineShell) {
                const QImage rendered = scene.renderImage(QRectF(0.0, 0.0, 320.0, 220.0), 1.0);
                int count = 0;
                for (int y = 80; y < 140; ++y) {
                    const QColor pixel = rendered.pixelColor(45, y);
                    if (pixel.alpha() > 0 && (pixel.red() < 245 || pixel.green() < 245 || pixel.blue() < 245)) {
                        ++count;
                    }
                }
                lineThickness = count;
            }
            ctx.check(lineMerged && lineShell && lineThickness > 0 && lineThickness <= 3,
                      QStringLiteral("直线与菱形合并后直线保持自身线宽"),
                      QStringLiteral("ok=%1 thickness=%2 layers=%3")
                          .arg(lineMerged ? 1 : 0)
                          .arg(lineThickness)
                          .arg(lineShell ? lineShell->layerCount() : -1));
            bool bandLayerFound = false;
            if (lineShell) {
                for (const CompositeShapeItem::Layer& layer : lineShell->layers()) {
                    if (!qFuzzyIsNull(layer.strokeWidth)) {
                        continue;
                    }
                    bandLayerFound = true;
                    break;
                }
            }
            ctx.check(lineShell && lineShell->layerCount() == 2 && bandLayerFound,
                      QStringLiteral("直线合并结果单独使用无描线的线宽带"),
                      QStringLiteral("layers=%1 band=%2")
                          .arg(lineShell ? lineShell->layerCount() : -1)
                          .arg(bandLayerFound ? 1 : 0));
        }

        // 文字参与合并：只填字形、不描边；不透明层直接覆盖，避免抗锯齿边缘露出白色重影。
        {
            QUndoStack undo;
            CanvasScene scene;
            scene.setUndoStack(&undo);
            scene.setMode(ToolMode::Edit);
            scene.applyCanvasSize(QSizeF(320.0, 220.0));
            auto* diamond = new ShapeItem();
            diamond->setShapeType(ShapeType::Diamond);
            diamond->setPos(QPointF(60.0, 30.0));
            diamond->setContentSize(QSizeF(180.0, 140.0));
            diamond->setStyle(QColor(20, 20, 20), QColor(255, 255, 255), 4.0);
            diamond->setFill(QColor(255, 255, 255), FillStyle::Solid);
            auto* text = new TextItem();
            text->setTextFormat(QStringLiteral("Ag文"), QFont(QStringLiteral("Microsoft YaHei"), 24), QColor(0, 0, 0));
            text->setContentSize(QSizeF(150.0, 50.0));
            text->setPos(QPointF(75.0, 80.0));
            scene.addItemsWithUndo({diamond, text}, QStringLiteral("Add diamond and text"));
            diamond->setSelected(true);
            text->setSelected(true);
            const bool textMerged = BoolOps::apply(&scene, BoolOps::Operation::Union, QStringLiteral("合并"));
            const QList<CanvasItem*> textSelected = scene.selectedCanvasItems();
            auto* textShell =
                textSelected.isEmpty() ? nullptr : dynamic_cast<CompositeShapeItem*>(textSelected.constFirst());

            int whitePixels = -1;
            bool textLayerFound = false;
            if (textShell) {
                const QImage rendered = scene.renderImage(QRectF(0.0, 0.0, 320.0, 220.0), 1.0);
                int count = 0;
                for (int y = 85; y < 125; ++y) {
                    for (int x = 100; x < 200; ++x) {
                        const QColor pixel = rendered.pixelColor(x, y);
                        if (pixel.red() > 240 && pixel.green() > 240 && pixel.blue() > 240) {
                            ++count;
                        }
                    }
                }
                whitePixels = count;
                for (const CompositeShapeItem::Layer& layer : textShell->layers()) {
                    if (qFuzzyIsNull(layer.strokeWidth) && layer.fill == QColor(0, 0, 0)) {
                        textLayerFound = true;
                        break;
                    }
                }
            }
            ctx.check(textMerged && textShell && textLayerFound && whitePixels == 0,
                      QStringLiteral("黑色文字合并后字形外无白色重影"),
                      QStringLiteral("ok=%1 textLayer=%2 white=%3 layers=%4")
                          .arg(textMerged ? 1 : 0)
                          .arg(textLayerFound ? 1 : 0)
                          .arg(whitePixels)
                          .arg(textShell ? textShell->layerCount() : -1));
        }
    }
    {
        // 组合（挖掉重叠）与拆分（切成互不重叠的多块）：
        // 两个 60×60 矩形横向错开 30，重叠区 30×60。
        auto runOperation = [](BoolOps::Operation operation, int& layerCount, qreal& area, int& itemCount) {
            QUndoStack undo;
            CanvasScene scene;
            scene.setUndoStack(&undo);
            scene.setMode(ToolMode::Edit);
            scene.applyCanvasSize(QSizeF(400.0, 300.0));
            auto makeRect = [&scene](qreal x, const QColor& color) {
                auto* item = new ShapeItem();
                item->setShapeType(ShapeType::Rectangle);
                item->setPos(QPointF(x, 40.0));
                item->setContentSize(QSizeF(60.0, 60.0));
                item->setStyle(color, color.lighter(160), 1.0);
                scene.addItemsWithUndo({item}, QStringLiteral("Add bool item"));
                return item;
            };
            CanvasItem* lower = makeRect(20.0, QColor(200, 40, 40));
            CanvasItem* upper = makeRect(50.0, QColor(40, 90, 200));
            lower->setSelected(true);
            upper->setSelected(true);
            const bool ok = BoolOps::apply(&scene, operation, QStringLiteral("bool"));
            layerCount = 0;
            area = 0.0;
            const QList<CanvasItem*> selected = scene.selectedCanvasItems();
            itemCount = ok ? selected.size() : 0;
            for (CanvasItem* item : selected) {
                if (auto* composite = dynamic_cast<CompositeShapeItem*>(item)) {
                    layerCount += composite->layerCount();
                    if (area <= 0.0) {
                        area = composite->outlineShape().boundingRect().width();
                    }
                }
            }
            return ok;
        };
        int combineLayers = 0;
        qreal combineWidth = 0.0;
        int combineItems = 0;
        const bool combineOk = runOperation(BoolOps::Operation::Combine, combineLayers, combineWidth, combineItems);
        ctx.check(
            combineOk && combineLayers == 1 && combineItems == 1,
            QStringLiteral("组合运算生成单个单层结果，重叠区被挖空"),
            QStringLiteral("ok=%1 layers=%2 items=%3").arg(combineOk ? 1 : 0).arg(combineLayers).arg(combineItems));

        int fragmentLayers = 0;
        qreal fragmentWidth = 0.0;
        int fragmentItems = 0;
        const bool fragmentOk =
            runOperation(BoolOps::Operation::Fragment, fragmentLayers, fragmentWidth, fragmentItems);
        ctx.check(
            fragmentOk && fragmentItems == 3 && fragmentLayers == 3,
            QStringLiteral("拆分把重叠区切成三个可分别选中的多边形"),
            QStringLiteral("ok=%1 items=%2 layers=%3").arg(fragmentOk ? 1 : 0).arg(fragmentItems).arg(fragmentLayers));
    }
    {
        // 联合的重叠区必须整块取高优先级图元的颜色（含轮廓色），不留低层的描边。
        QUndoStack unionUndo;
        CanvasScene unionScene;
        unionScene.setUndoStack(&unionUndo);
        unionScene.setMode(ToolMode::Edit);
        unionScene.applyCanvasSize(QSizeF(300.0, 200.0));
        const QColor lowerFill(30, 160, 90);
        const QColor lowerStroke(200, 40, 40);
        const QColor upperFill(40, 90, 200);
        const QColor upperStroke(40, 90, 200);
        auto makeRect = [&unionScene](qreal x, const QColor& fill, const QColor& stroke) {
            auto* item = new ShapeItem();
            item->setShapeType(ShapeType::Rectangle);
            item->setPos(QPointF(x, 60.0));
            item->setContentSize(QSizeF(80.0, 80.0));
            item->setStyle(stroke, fill, 6.0);
            item->setFill(fill, FillStyle::Solid);
            unionScene.addItemsWithUndo({item}, QStringLiteral("Add union item"));
            return item;
        };
        CanvasItem* lower = makeRect(40.0, lowerFill, lowerStroke);
        CanvasItem* upper = makeRect(80.0, upperFill, upperStroke);
        lower->setSelected(true);
        upper->setSelected(true);
        const bool unionOk = BoolOps::apply(&unionScene, BoolOps::Operation::Union, QStringLiteral("联合"));
        const QImage rendered = unionScene.renderImage(QRectF(0.0, 0.0, 300.0, 200.0), 1.0);
        const QColor overlapPixel = rendered.pixelColor(90, 100); // 两块都覆盖
        const QColor lowerPixel = rendered.pixelColor(50, 100);   // 只有下层
        const QColor upperEdge = rendered.pixelColor(80, 100);    // 上层左边框，落在下层区域内
        ctx.check(unionOk && overlapPixel == upperFill, QStringLiteral("联合后重叠区按高优先级图元的填充色显示"),
                  QStringLiteral("ok=%1 pixel=%2 expected=%3")
                      .arg(unionOk ? 1 : 0)
                      .arg(overlapPixel.name())
                      .arg(upperFill.name()));
        ctx.check(lowerPixel == upperFill, QStringLiteral("联合后整体改用高优先级图元的填充色"),
                  QStringLiteral("pixel=%1 expected=%2").arg(lowerPixel.name()).arg(upperFill.name()));
        ctx.check(upperEdge == upperFill || upperEdge == upperStroke,
                  QStringLiteral("重叠区内的轮廓改取高优先级图元的颜色"),
                  QStringLiteral("pixel=%1").arg(upperEdge.name()));
    }
    {
        // 重叠区的取色优先级：高层填充透明时落到次优先级；交界轮廓被填充色同化。
        QUndoStack fallbackUndo;
        CanvasScene fallbackScene;
        fallbackScene.setUndoStack(&fallbackUndo);
        fallbackScene.setMode(ToolMode::Edit);
        fallbackScene.applyCanvasSize(QSizeF(300.0, 200.0));
        const QColor lowerFill(30, 160, 90);
        const QColor upperFill(40, 90, 200);
        const QColor upperStroke(220, 40, 40);
        auto makeRect = [&fallbackScene](qreal x, const QColor& fill, const QColor& stroke, qreal width) {
            auto* item = new ShapeItem();
            item->setShapeType(ShapeType::Rectangle);
            item->setPos(QPointF(x, 60.0));
            item->setContentSize(QSizeF(80.0, 80.0));
            item->setStyle(stroke, fill, width);
            item->setFill(fill, FillStyle::Solid);
            fallbackScene.addItemsWithUndo({item}, QStringLiteral("Add fallback item"));
            return item;
        };
        CanvasItem* lower = makeRect(40.0, lowerFill, lowerFill, 1.0);
        CanvasItem* upper =
            makeRect(80.0, QColor(upperFill.red(), upperFill.green(), upperFill.blue(), 0), upperStroke, 6.0);
        lower->setSelected(true);
        upper->setSelected(true);
        const bool fallbackOk = BoolOps::apply(&fallbackScene, BoolOps::Operation::Union, QStringLiteral("联合"));
        const QImage rendered = fallbackScene.renderImage(QRectF(0.0, 0.0, 300.0, 200.0), 1.0);
        const QColor overlapPixel = rendered.pixelColor(90, 100);
        ctx.check(fallbackOk && overlapPixel.alpha() == 0, QStringLiteral("donor 填充透明时重叠区保持透明"),
                  QStringLiteral("ok=%1 pixel=%2 alpha=%3")
                      .arg(fallbackOk ? 1 : 0)
                      .arg(overlapPixel.name())
                      .arg(overlapPixel.alpha()));
        const QColor internalEdge = rendered.pixelColor(81, 100); // 上层左边框，落在下层内部
        ctx.check(internalEdge != upperStroke, QStringLiteral("重叠区内部的交界轮廓被填充色同化"),
                  QStringLiteral("pixel=%1").arg(internalEdge.name()));
    }
    {
        // 整组作为一个参与者：组内两个方块先并成一条轮廓再参与相减，
        // 不会因为最高优先级组员被挖空就把整组结果判成空。
        QUndoStack groupBoolUndo;
        CanvasScene groupBoolScene;
        groupBoolScene.setUndoStack(&groupBoolUndo);
        groupBoolScene.setMode(ToolMode::Edit);
        groupBoolScene.applyCanvasSize(QSizeF(300.0, 200.0));
        auto makeRect = [&groupBoolScene](qreal x, const QColor& color) {
            auto* item = new ShapeItem();
            item->setShapeType(ShapeType::Rectangle);
            item->setPos(QPointF(x, 100.0));
            item->setContentSize(QSizeF(40.0, 40.0));
            item->setStyle(color, color, 2.0);
            item->setFill(color, FillStyle::Solid);
            groupBoolScene.addItemsWithUndo({item}, QStringLiteral("Add grouped bool item"));
            return item;
        };
        CanvasItem* outside = makeRect(120.0, QColor(40, 90, 200));
        CanvasItem* groupLow = makeRect(10.0, QColor(220, 60, 60));
        CanvasItem* groupHigh = makeRect(100.0, QColor(30, 160, 90));
        groupLow->setSelected(true);
        groupHigh->setSelected(true);
        groupBoolScene.groupSelectedItems();
        groupBoolScene.clearSelection();
        groupBoolScene.selectGroupOfItem(groupLow);
        outside->setSelected(true);

        const bool groupedSubtract =
            BoolOps::apply(&groupBoolScene, BoolOps::Operation::Subtract, QStringLiteral("相减"));
        const QList<CanvasItem*> groupedSelection = groupBoolScene.selectedCanvasItems();
        auto* groupedResult =
            groupedSelection.isEmpty() ? nullptr : dynamic_cast<CompositeShapeItem*>(groupedSelection.constFirst());
        ctx.check(groupedSubtract && groupedResult && groupedResult->outlineShape().boundingRect().width() > 100.0,
                  QStringLiteral("整组作为一个参与者参与合并"),
                  QStringLiteral("ok=%1 width=%2")
                      .arg(groupedSubtract ? 1 : 0)
                      .arg(groupedResult ? groupedResult->outlineShape().boundingRect().width() : -1.0));
        ctx.check(groupedResult && !groupedResult->layers().isEmpty() &&
                      groupedResult->layers().constFirst().fill == QColor(30, 160, 90),
                  QStringLiteral("整组结果采用组内最高优先级图元的样式"));
    }
    {
        // 合并图元仍然能从菜单、右键菜单和编辑图元卡进入，但主工具栏不再放合并图元按钮。
        MainWindow boolWindow;
        QToolButton* boolButton = boolWindow.findChild<QToolButton*>(QStringLiteral("BoolToolButton"));
        QMenu* boolMenu = boolWindow.findChild<QMenu*>(QStringLiteral("BoolMenu"));
        CanvasView* boolView = boolWindow.findChild<CanvasView*>();
        ctx.check(boolMenu && boolMenu->actions().size() == 5, QStringLiteral("编辑菜单提供 5 项合并图元操作"),
                  boolMenu ? QString::number(boolMenu->actions().size()) : QString());
        ctx.check(!boolButton, QStringLiteral("工具栏不再显示合并图元按钮"));
        ctx.check(boolView && boolView->contextMenuGroupCount() >= 1, QStringLiteral("画布右键菜单挂上了合并图元分组"),
                  boolView ? QString::number(boolView->contextMenuGroupCount()) : QString());
        bool iconsOk = boolMenu && boolMenu->actions().size() == 5;
        if (iconsOk) {
            for (QAction* action : boolMenu->actions()) {
                if (action->icon().isNull()) {
                    iconsOk = false;
                    break;
                }
            }
        }
        ctx.check(iconsOk, QStringLiteral("合并图元五项操作都带参照 PPT 的图标"));
        // 合并图元主入口（菜单项）图标必须与 5 项子操作不同，否则看起来像同一项操作。
        bool morphIconDistinct = boolMenu && !boolMenu->icon().isNull();
        if (morphIconDistinct) {
            for (QAction* action : boolMenu->actions()) {
                if (action->icon().cacheKey() == boolMenu->icon().cacheKey()) {
                    morphIconDistinct = false;
                    break;
                }
            }
        }
        ctx.check(morphIconDistinct, QStringLiteral("合并图元主入口图标与五项子操作图标不同"));
        QAction* heightUp = boolWindow.findChild<QAction*>(QStringLiteral("HeightUpAction"));
        QAction* heightDown = boolWindow.findChild<QAction*>(QStringLiteral("HeightDownAction"));
        QMenu* shapeMenu = boolWindow.findChild<QMenu*>(QStringLiteral("ShapeMenu"));
        ctx.check(heightUp && heightDown && !heightUp->icon().isNull() && !heightDown->icon().isNull() && boolMenu &&
                      !boolMenu->icon().isNull() && shapeMenu && !shapeMenu->icon().isNull(),
                  QStringLiteral("菜单栏为已有图标的同类功能补齐图标"));
        const QString polyText = uiText(QStringLiteral("Polygon"));
        const QString curveText = uiText(QStringLiteral("Curve"));
        const bool named = polyText != QStringLiteral("Polygon") && curveText != QStringLiteral("Curve");
        ctx.check(named, QStringLiteral("多边形的类型名有中文译文"), polyText + QStringLiteral(" / ") + curveText);
    }
    {
        // 拆分结果必须在块间保留交界线，不能渲染成联合的样子。
        const QColor fillA(200, 40, 40);
        const QColor strokeA(250, 200, 40);
        const QColor fillB(40, 90, 200);
        const QColor strokeB(40, 200, 120);
        auto renderOp = [&](BoolOps::Operation op, QUndoStack* undo, CanvasScene* scene) {
            scene->setUndoStack(undo);
            scene->setMode(ToolMode::Edit);
            scene->applyCanvasSize(QSizeF(300.0, 200.0));
            auto makeRect = [scene](qreal x, const QColor& fill, const QColor& stroke) {
                auto* item = new ShapeItem();
                item->setShapeType(ShapeType::Rectangle);
                item->setPos(QPointF(x, 40.0));
                item->setContentSize(QSizeF(60.0, 60.0));
                item->setStyle(stroke, fill, 2.0);
                item->setFill(fill, FillStyle::Solid);
                scene->addItemsWithUndo({item}, QStringLiteral("Add split item"));
                return item;
            };
            CanvasItem* lower = makeRect(20.0, fillA, strokeA);
            CanvasItem* upper = makeRect(50.0, fillB, strokeB);
            lower->setSelected(true);
            upper->setSelected(true);
            return BoolOps::apply(scene, op, QStringLiteral("split"));
        };
        QUndoStack unionSplitUndo;
        QUndoStack fragmentUndo;
        CanvasScene unionSplitScene;
        CanvasScene fragmentScene;
        const bool unionSplitOk = renderOp(BoolOps::Operation::Union, &unionSplitUndo, &unionSplitScene);
        const bool fragmentOk = renderOp(BoolOps::Operation::Fragment, &fragmentUndo, &fragmentScene);
        const QImage unionImage = unionSplitScene.renderImage(QRectF(0.0, 0.0, 300.0, 200.0), 1.0);
        const QImage fragmentImage = fragmentScene.renderImage(QRectF(0.0, 0.0, 300.0, 200.0), 1.0);
        int differingPixels = 0;
        for (int y = 40; y < 100; ++y) {
            for (int x = 20; x < 110; ++x) {
                if (unionImage.pixelColor(x, y) != fragmentImage.pixelColor(x, y)) {
                    ++differingPixels;
                }
            }
        }
        ctx.check(unionSplitOk && fragmentOk && differingPixels > 10,
                  QStringLiteral("拆分在块间保留内部交界线，与联合结果不同"),
                  QStringLiteral("diff=%1").arg(differingPixels));
    }
}
