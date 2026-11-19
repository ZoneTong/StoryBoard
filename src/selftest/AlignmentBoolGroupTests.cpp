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
#include "ItemDockPanel.h"
#include "ItemPropertiesDialog.h"
#include "LayerPanel.h"
#include "LineArtConverter.h"
#include "LineArtDialog.h"
#include "LineArtPresets.h"
#include "LineItem.h"
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
        // 功能键只看绑定键生效；副键不整组断言，改键窗口会兜底冲突提示。
        ctx.check(redoAction && redoAction->shortcut() == QKeySequence(QStringLiteral("Ctrl+Y")),
                  QStringLiteral("重做绑定 Ctrl+Y"), redoAction ? redoAction->shortcut().toString() : QString());
        ctx.check(zoomInAction && zoomInAction->shortcut() == QKeySequence(QStringLiteral("Ctrl+=")),
                  QStringLiteral("放大绑定 Ctrl+="), zoomInAction ? zoomInAction->shortcut().toString() : QString());
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

        // 3 个及以上图元的合并：五项运算都要真正落到结果，不能只处理前两个。
        auto addRect = [](CanvasScene& scene, qreal x, qreal y, qreal w, qreal h) {
            auto* item = new ShapeItem();
            item->setShapeType(ShapeType::Rectangle);
            item->setPos(QPointF(x, y));
            item->setContentSize(QSizeF(w, h));
            item->setStyle(QColor(20, 20, 20), QColor(200, 60, 60), 2.0);
            item->setFill(QColor(200, 60, 60), FillStyle::Solid);
            scene.addItemsWithUndo({item}, QStringLiteral("Add rect"));
            return item;
        };
        {
            // 联合：3 个互相重叠的矩形合成一个图元。
            QUndoStack undo;
            CanvasScene scene;
            scene.setUndoStack(&undo);
            scene.setMode(ToolMode::Edit);
            scene.applyCanvasSize(QSizeF(600.0, 400.0));
            CanvasItem* a = addRect(scene, 20, 20, 80, 60);
            CanvasItem* b = addRect(scene, 60, 50, 80, 60);
            CanvasItem* c = addRect(scene, 100, 20, 80, 60);
            a->setSelected(true);
            b->setSelected(true);
            c->setSelected(true);
            const bool ok = BoolOps::apply(&scene, BoolOps::Operation::Union, QStringLiteral("合并"));
            const QList<CanvasItem*> sel = scene.selectedCanvasItems();
            const bool covers =
                sel.size() == 1 && sel.constFirst()->sceneBoundingRect().contains(QRectF(20, 20, 160, 90));
            ctx.check(ok && sel.size() == 1 && scene.canvasItemCount() == 1 && covers,
                      QStringLiteral("3 个图元联合成功并覆盖全部原始范围"),
                      QStringLiteral("ok=%1 selected=%2 items=%3 covers=%4")
                          .arg(ok ? 1 : 0)
                          .arg(sel.size())
                          .arg(scene.canvasItemCount())
                          .arg(covers ? 1 : 0));
        }
        {
            // 联合：4 个图元（3 个及以上含 4 个的情形）。
            QUndoStack undo;
            CanvasScene scene;
            scene.setUndoStack(&undo);
            scene.setMode(ToolMode::Edit);
            scene.applyCanvasSize(QSizeF(600.0, 400.0));
            CanvasItem* a = addRect(scene, 20, 20, 70, 50);
            CanvasItem* b = addRect(scene, 50, 40, 70, 50);
            CanvasItem* c = addRect(scene, 80, 20, 70, 50);
            CanvasItem* d = addRect(scene, 60, 80, 70, 50);
            a->setSelected(true);
            b->setSelected(true);
            c->setSelected(true);
            d->setSelected(true);
            const bool ok = BoolOps::apply(&scene, BoolOps::Operation::Union, QStringLiteral("合并"));
            ctx.check(ok && scene.selectedCanvasItems().size() == 1 && scene.canvasItemCount() == 1,
                      QStringLiteral("4 个图元联合成功"),
                      QStringLiteral("ok=%1 selected=%2 items=%3")
                          .arg(ok ? 1 : 0)
                          .arg(scene.selectedCanvasItems().size())
                          .arg(scene.canvasItemCount()));
        }
        {
            // 异或：3 个矩形，并集减重叠，结果仍是单个图元。
            QUndoStack undo;
            CanvasScene scene;
            scene.setUndoStack(&undo);
            scene.setMode(ToolMode::Edit);
            scene.applyCanvasSize(QSizeF(600.0, 400.0));
            CanvasItem* a = addRect(scene, 20, 20, 80, 60);
            CanvasItem* b = addRect(scene, 60, 50, 80, 60);
            CanvasItem* c = addRect(scene, 100, 20, 80, 60);
            a->setSelected(true);
            b->setSelected(true);
            c->setSelected(true);
            const bool ok = BoolOps::apply(&scene, BoolOps::Operation::Xor, QStringLiteral("异或"));
            ctx.check(ok && scene.selectedCanvasItems().size() == 1 && scene.canvasItemCount() == 1,
                      QStringLiteral("3 个图元异或成功"),
                      QStringLiteral("ok=%1 selected=%2 items=%3")
                          .arg(ok ? 1 : 0)
                          .arg(scene.selectedCanvasItems().size())
                          .arg(scene.canvasItemCount()));
        }
        {
            // 相交：3 个矩形取公共重叠区。
            QUndoStack undo;
            CanvasScene scene;
            scene.setUndoStack(&undo);
            scene.setMode(ToolMode::Edit);
            scene.applyCanvasSize(QSizeF(600.0, 400.0));
            CanvasItem* a = addRect(scene, 20, 20, 100, 80);
            CanvasItem* b = addRect(scene, 60, 40, 100, 80);
            CanvasItem* c = addRect(scene, 80, 20, 100, 80);
            a->setSelected(true);
            b->setSelected(true);
            c->setSelected(true);
            const bool ok = BoolOps::apply(&scene, BoolOps::Operation::Intersect, QStringLiteral("相交"));
            const QList<CanvasItem*> sel = scene.selectedCanvasItems();
            ctx.check(ok && sel.size() == 1 && sel.constFirst()->sceneBoundingRect().width() <= 160.0,
                      QStringLiteral("3 个图元相交成功"),
                      QStringLiteral("ok=%1 selected=%2 width=%3")
                          .arg(ok ? 1 : 0)
                          .arg(sel.size())
                          .arg(sel.isEmpty() ? -1.0 : sel.constFirst()->sceneBoundingRect().width()));
        }
        {
            // 剪除：首个选中的大矩形为底，另外两个矩形从它里面挖掉。
            QUndoStack undo;
            CanvasScene scene;
            scene.setUndoStack(&undo);
            scene.setMode(ToolMode::Edit);
            scene.applyCanvasSize(QSizeF(600.0, 400.0));
            CanvasItem* base = addRect(scene, 20, 20, 240, 180);
            CanvasItem* cut1 = addRect(scene, 60, 60, 80, 60);
            CanvasItem* cut2 = addRect(scene, 150, 90, 80, 60);
            base->setSelected(true);
            cut1->setSelected(true);
            cut2->setSelected(true);
            const bool ok = BoolOps::apply(&scene, BoolOps::Operation::Subtract, QStringLiteral("剪除"));
            ctx.check(ok && scene.selectedCanvasItems().size() == 1 && scene.canvasItemCount() == 1,
                      QStringLiteral("3 个图元剪除成功（首个选中为底）"),
                      QStringLiteral("ok=%1 selected=%2 items=%3")
                          .arg(ok ? 1 : 0)
                          .arg(scene.selectedCanvasItems().size())
                          .arg(scene.canvasItemCount()));
        }
        {
            // 剪除把底图切成两半时，每一块都要成为独立图元，而不是一个图元里藏着两块。
            QUndoStack undo;
            CanvasScene scene;
            scene.setUndoStack(&undo);
            scene.setMode(ToolMode::Edit);
            scene.applyCanvasSize(QSizeF(600.0, 400.0));
            CanvasItem* base = addRect(scene, 20, 20, 300, 120);
            CanvasItem* cutter = addRect(scene, 150, 10, 40, 140);
            base->setSelected(true);
            cutter->setSelected(true);
            const bool ok = BoolOps::apply(&scene, BoolOps::Operation::Subtract, QStringLiteral("剪除"));
            const QList<CanvasItem*> pieces = scene.selectedCanvasItems();
            bool eachNarrower = pieces.size() == 2;
            for (CanvasItem* piece : std::as_const(pieces)) {
                if (piece->sceneBoundingRect().width() >= 250.0) {
                    eachNarrower = false;
                }
            }
            ctx.check(ok && pieces.size() == 2 && scene.canvasItemCount() == 2 && eachNarrower,
                      QStringLiteral("剪除把底图切成互不相连的两块，各自成为独立图元"),
                      QStringLiteral("ok=%1 pieces=%2 items=%3")
                          .arg(ok ? 1 : 0)
                          .arg(pieces.size())
                          .arg(scene.canvasItemCount()));
        }
        {
            // 拆分：3 个填充图元互相重叠，拆成多块且总数守恒（原 3 个消失）。
            QUndoStack undo;
            CanvasScene scene;
            scene.setUndoStack(&undo);
            scene.setMode(ToolMode::Edit);
            scene.applyCanvasSize(QSizeF(600.0, 400.0));
            CanvasItem* a = addRect(scene, 20, 20, 80, 60);
            CanvasItem* b = addRect(scene, 60, 50, 80, 60);
            CanvasItem* c = addRect(scene, 100, 20, 80, 60);
            a->setSelected(true);
            b->setSelected(true);
            c->setSelected(true);
            const bool ok = BoolOps::apply(&scene, BoolOps::Operation::Fragment, QStringLiteral("拆分"));
            const int pieces = scene.selectedCanvasItems().size();
            ctx.check(
                ok && pieces >= 2 && pieces <= 8 && scene.canvasItemCount() == pieces,
                QStringLiteral("3 个图元拆分成功且分块数在 2–8 之间"),
                QStringLiteral("ok=%1 pieces=%2 items=%3").arg(ok ? 1 : 0).arg(pieces).arg(scene.canvasItemCount()));
        }

        // 零面积描线不作为挖除与分块边界：直线横穿菱形后，组合 / 剪除 / 拆分
        // 都不能在菱形上留细缝，交集条带按线宽带渲染而不是套描线的面积层。
        {
            // 线条类图元（直线 / 曲线 / 括号 / 弧 / 纯描线多边形）不参与合并：
            // 只选中一条直线与一个菱形时，参与者只有菱形一个，合并直接失败，
            // 两个图元都保持原样留在画布上。
            struct LineSkip {
                bool applied = false;
                int composites = 0;
                int items = 0;
                QColor diamondTop;
                QColor slit;
            };
            auto runLineSkip = [](BoolOps::Operation operation) {
                LineSkip result;
                QUndoStack undo;
                CanvasScene scene;
                scene.setUndoStack(&undo);
                scene.setMode(ToolMode::Edit);
                scene.applyCanvasSize(QSizeF(320.0, 220.0));
                BackgroundState background;
                background.type = BackgroundType::Color;
                background.color = QColor(Qt::white);
                scene.applyBackground(background);
                auto* diamond = new ShapeItem();
                diamond->setShapeType(ShapeType::Diamond);
                diamond->setPos(QPointF(70.0, 40.0));
                diamond->setContentSize(QSizeF(160.0, 120.0));
                diamond->setStyle(QColor(20, 20, 20), QColor(200, 40, 40), 4.0);
                diamond->setFill(QColor(200, 40, 40), FillStyle::Solid);
                auto* line = new LineItem();
                line->setEndpoints(QPointF(30.0, 100.0), QPointF(280.0, 100.0), QColor(30, 90, 200), 6.0);
                scene.addItemsWithUndo({line, diamond}, QStringLiteral("Add line skip"));
                line->setSelected(true);
                diamond->setSelected(true);
                result.applied = BoolOps::apply(&scene, operation, QStringLiteral("Line skip"));
                result.items = scene.canvasItems().size();
                for (CanvasItem* item : scene.canvasItems()) {
                    if (dynamic_cast<CompositeShapeItem*>(item)) {
                        ++result.composites;
                    }
                }
                const QImage rendered = scene.renderImage(QRectF(0.0, 0.0, 320.0, 220.0), 1.0);
                result.diamondTop = rendered.pixelColor(150, 60);
                result.slit = rendered.pixelColor(150, 100);
                return result;
            };
            const QColor diamondFill(200, 40, 40);
            const QList<QPair<BoolOps::Operation, QString>> lineOps = {
                {BoolOps::Operation::Union, QStringLiteral("联合")},
                {BoolOps::Operation::Xor, QStringLiteral("异或")},
                {BoolOps::Operation::Intersect, QStringLiteral("相交")},
                {BoolOps::Operation::Subtract, QStringLiteral("剪除")},
                {BoolOps::Operation::Fragment, QStringLiteral("拆分")},
            };
            for (const auto& entry : lineOps) {
                const LineSkip result = runLineSkip(entry.first);
                ctx.check(!result.applied && result.items == 2 && result.composites == 0 &&
                              result.diamondTop == diamondFill && result.slit == diamondFill,
                          QStringLiteral("%1：直线不参与合并，画布保持原样").arg(entry.second),
                          QStringLiteral("applied=%1 items=%2 composites=%3 top=%4 slit=%5")
                              .arg(result.applied ? 1 : 0)
                              .arg(result.items)
                              .arg(result.composites)
                              .arg(result.diamondTop.name())
                              .arg(result.slit.name()));
            }

            // 细窄交集降级为线宽带：菱形与五角星边角相错形成的窄透镜交集只填充
            // 不描边，不会被描线吞成黑块。
            QUndoStack lensUndo;
            CanvasScene lensScene;
            lensScene.setUndoStack(&lensUndo);
            lensScene.setMode(ToolMode::Edit);
            lensScene.applyCanvasSize(QSizeF(420.0, 320.0));
            BackgroundState lensBackground;
            lensBackground.type = BackgroundType::Color;
            lensBackground.color = QColor(Qt::white);
            lensScene.applyBackground(lensBackground);
            auto* lensDiamond = new ShapeItem(ShapeType::Diamond);
            lensDiamond->setPos(QPointF(60.0, 70.0));
            lensDiamond->setContentSize(QSizeF(180.0, 180.0));
            auto* lensStar = new ShapeItem(ShapeType::Star);
            lensStar->setPos(QPointF(180.0, 60.0));
            lensStar->setContentSize(QSizeF(180.0, 180.0));
            lensScene.addItemsWithUndo({lensDiamond, lensStar}, QStringLiteral("Add lens pair"));
            lensDiamond->setSelected(true);
            lensStar->setSelected(true);
            const bool lensOk =
                BoolOps::apply(&lensScene, BoolOps::Operation::Intersect, QStringLiteral("Lens intersect"));
            int lensLayers = 0;
            int lensStroked = 0;
            for (CanvasItem* item : lensScene.selectedCanvasItems()) {
                if (auto* composite = dynamic_cast<CompositeShapeItem*>(item)) {
                    lensLayers += composite->layerCount();
                    for (const CompositeShapeItem::Layer& layer : composite->layers()) {
                        if (layer.strokeWidth > 0.0) {
                            ++lensStroked;
                        }
                    }
                }
            }
            ctx.check(
                lensOk && lensLayers == 1 && lensStroked == 0, QStringLiteral("细窄交集区域降级为线宽带只填充不描边"),
                QStringLiteral("ok=%1 layers=%2 stroked=%3").arg(lensOk ? 1 : 0).arg(lensLayers).arg(lensStroked));
        }

        // 有填充空间的图元优先于零面积图元：黑色文字叠在矩形上方（height 更大），
        // 合并 donor 仍取矩形的填充与描线样式。
        {
            QUndoStack undo;
            CanvasScene scene;
            scene.setUndoStack(&undo);
            scene.setMode(ToolMode::Edit);
            scene.applyCanvasSize(QSizeF(320.0, 220.0));
            auto* rect = new ShapeItem();
            rect->setShapeType(ShapeType::Rectangle);
            rect->setPos(QPointF(60.0, 50.0));
            rect->setContentSize(QSizeF(160.0, 120.0));
            rect->setStyle(QColor(20, 20, 20), QColor(255, 255, 255), 4.0);
            rect->setFill(QColor(200, 40, 40), FillStyle::Solid);
            auto* text = new TextItem();
            text->setTextFormat(QStringLiteral("Ag文"), QFont(QStringLiteral("Microsoft YaHei"), 24), QColor(0, 0, 0));
            text->setContentSize(QSizeF(150.0, 50.0));
            text->setPos(QPointF(65.0, 85.0));
            scene.addItemsWithUndo({rect, text}, QStringLiteral("Add rect and text"));
            rect->setSelected(true);
            text->setSelected(true);
            const bool merged = BoolOps::apply(&scene, BoolOps::Operation::Union, QStringLiteral("合并"));
            const QList<CanvasItem*> mergedItems = scene.selectedCanvasItems();
            auto* shell = mergedItems.isEmpty() ? nullptr : dynamic_cast<CompositeShapeItem*>(mergedItems.constFirst());
            ctx.check(merged && shell && shell->fillColor() == QColor(200, 40, 40),
                      QStringLiteral("合并优先采用有填充空间图元的样式（矩形>文字）"),
                      QStringLiteral("ok=%1 fill=%2")
                          .arg(merged ? 1 : 0)
                          .arg(shell ? shell->fillColor().name() : QStringLiteral("none")));
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

            int inkPixels = -1;
            bool textLayerFound = false;
            if (textShell) {
                const QImage rendered = scene.renderImage(QRectF(0.0, 0.0, 320.0, 220.0), 1.0);
                int count = 0;
                for (int y = 85; y < 125; ++y) {
                    for (int x = 100; x < 200; ++x) {
                        const QColor pixel = rendered.pixelColor(x, y);
                        if (pixel.red() < 60 && pixel.green() < 60 && pixel.blue() < 60) {
                            ++count;
                        }
                    }
                }
                inkPixels = count;
                for (const CompositeShapeItem::Layer& layer : textShell->layers()) {
                    if (qFuzzyIsNull(layer.strokeWidth) && layer.fill == QColor(0, 0, 0)) {
                        textLayerFound = true;
                        break;
                    }
                }
            }
            // donor 换成有填充空间的菱形（白底）后，字形缝隙露出白底属合法；
            // 需要守住的是两条：黑色字形在合并后仍可见（遮挡语义：合并前在上层
            // 的文字合并后仍后画），以及文字层零描边（donor 白色描线不在字形外
            // 形成重影）。inkPixels > 0 即字形可见。
            ctx.check(textMerged && textShell && textLayerFound && inkPixels > 0,
                      QStringLiteral("黑色文字合并后字形保持可见且无描边重影"),
                      QStringLiteral("ok=%1 textLayer=%2 ink=%3 layers=%4")
                          .arg(textMerged ? 1 : 0)
                          .arg(textLayerFound ? 1 : 0)
                          .arg(inkPixels)
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
        const bool combineOk = runOperation(BoolOps::Operation::Xor, combineLayers, combineWidth, combineItems);
        ctx.check(
            combineOk && combineLayers == 1 && combineItems == 1,
            QStringLiteral("异或运算生成单个单层结果，重叠区被挖空"),
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
        // 菱形与五角星都是凹多边形：联合必须按完整轮廓取并集，不能只取可见的
        // 描线带；同时覆盖无填充 / 有填充组合和五种布尔方式。
        struct PairResult {
            bool ok = false;
            QImage image;
            QPainterPath path;
            QPointF overlapPoint;
            int items = 0;
            int layers = 0;
            FillStyle fillStyle = FillStyle::None;
            qreal strokeWidth = 0.0;
        };
        const QPointF diamondCenter(150.0, 160.0);
        const QPointF starCenter(270.0, 150.0);
        const QColor diamondFill(30, 160, 90);
        const QColor starFill(40, 90, 200);

        auto interiorPoint = [](const QPainterPath& path) {
            const QRectF bounds = path.boundingRect();
            if (bounds.isEmpty()) {
                return bounds.center();
            }
            for (int y = 1; y < 20; ++y) {
                for (int x = 1; x < 20; ++x) {
                    const QPointF point(bounds.left() + bounds.width() * x / 20.0,
                                        bounds.top() + bounds.height() * y / 20.0);
                    if (path.contains(point)) {
                        return point;
                    }
                }
            }
            return bounds.center();
        };

        auto runPair = [&](BoolOps::Operation operation, bool diamondFilled, bool starFilled) {
            PairResult result;
            QUndoStack undo;
            CanvasScene scene;
            scene.setUndoStack(&undo);
            scene.setMode(ToolMode::Edit);
            scene.applyCanvasSize(QSizeF(420.0, 320.0));
            scene.setShowGrid(false);
            BackgroundState background;
            background.type = BackgroundType::Color;
            background.color = QColor(Qt::white);
            scene.applyBackground(background);

            auto makeShape = [&scene](ShapeType type, QPointF pos, const QColor& fill, bool filled) {
                auto* shape = new ShapeItem(type);
                shape->setPos(pos);
                shape->setContentSize(QSizeF(180.0, 180.0));
                shape->setStyle(QColor(20, 20, 20), filled ? fill : QColor(Qt::transparent), 8.0);
                shape->setFill(filled ? fill : QColor(Qt::transparent), filled ? FillStyle::Solid : FillStyle::None);
                scene.addItemsWithUndo({shape}, QStringLiteral("Add pair shape"));
                return shape;
            };
            auto* lower = makeShape(ShapeType::Diamond, QPointF(60.0, 70.0), diamondFill, diamondFilled);
            auto* upper = makeShape(ShapeType::Star, QPointF(180.0, 60.0), starFill, starFilled);
            const QPainterPath lowerOutline = lower->mapToScene(lower->outlineShape());
            const QPainterPath upperOutline = upper->mapToScene(upper->outlineShape());
            result.overlapPoint = interiorPoint(lowerOutline.intersected(upperOutline));
            lower->setSelected(true);
            upper->setSelected(true);
            result.ok = BoolOps::apply(&scene, operation, QStringLiteral("Pair operation"));
            result.image = scene.renderImage(QRectF(0.0, 0.0, 420.0, 320.0), 1.0);

            const QList<CanvasItem*> selected = scene.selectedCanvasItems();
            result.items = selected.size();
            for (CanvasItem* item : selected) {
                if (auto* composite = dynamic_cast<CompositeShapeItem*>(item)) {
                    const QPainterPath outline = composite->mapToScene(composite->outlineShape());
                    result.path = result.path.isEmpty() ? outline : result.path.united(outline);
                    result.layers += composite->layerCount();
                    result.fillStyle = composite->fillStyle();
                    result.strokeWidth = composite->strokeWidth();
                }
            }
            return result;
        };

        const PairResult unionBoth = runPair(BoolOps::Operation::Union, true, true);
        ctx.check(unionBoth.ok && unionBoth.path.contains(diamondCenter) && unionBoth.path.contains(starCenter) &&
                      unionBoth.image.pixelColor(diamondCenter.toPoint()) == starFill &&
                      unionBoth.image.pixelColor(starCenter.toPoint()) == starFill,
                  QStringLiteral("菱形与五角星联合保留完整轮廓并统一顶层填充"),
                  QStringLiteral("ok=%1 diamond=%2 star=%3")
                      .arg(unionBoth.ok ? 1 : 0)
                      .arg(unionBoth.path.contains(diamondCenter) ? 1 : 0)
                      .arg(unionBoth.path.contains(starCenter) ? 1 : 0));

        const PairResult hollowDiamondUnion = runPair(BoolOps::Operation::Union, false, true);
        ctx.check(hollowDiamondUnion.ok && hollowDiamondUnion.path.contains(diamondCenter) &&
                      hollowDiamondUnion.image.pixelColor(diamondCenter.toPoint()) == starFill,
                  QStringLiteral("无填充菱形与填充五角星联合仍按完整轮廓取并集"),
                  QStringLiteral("ok=%1 contains=%2 pixel=%3")
                      .arg(hollowDiamondUnion.ok ? 1 : 0)
                      .arg(hollowDiamondUnion.path.contains(diamondCenter) ? 1 : 0)
                      .arg(hollowDiamondUnion.image.pixelColor(diamondCenter.toPoint()).name()));

        const PairResult hollowUnion = runPair(BoolOps::Operation::Union, false, false);
        ctx.check(hollowUnion.ok && hollowUnion.path.contains(diamondCenter) && hollowUnion.path.contains(starCenter) &&
                      hollowUnion.layers == 1 && hollowUnion.fillStyle == FillStyle::None &&
                      hollowUnion.strokeWidth > 0.0,
                  QStringLiteral("两个无填充图形联合得到单层闭合轮廓，不保留内部交界"),
                  QStringLiteral("ok=%1 layers=%2 contains=%3,%4")
                      .arg(hollowUnion.ok ? 1 : 0)
                      .arg(hollowUnion.layers)
                      .arg(hollowUnion.path.contains(diamondCenter) ? 1 : 0)
                      .arg(hollowUnion.path.contains(starCenter) ? 1 : 0));

        const PairResult combine = runPair(BoolOps::Operation::Xor, true, true);
        ctx.check(combine.ok && combine.path.contains(diamondCenter) && combine.path.contains(starCenter) &&
                      !combine.path.contains(combine.overlapPoint),
                  QStringLiteral("菱形与五角星异或后重叠区被挖空"),
                  QStringLiteral("ok=%1 overlap=%2")
                      .arg(combine.ok ? 1 : 0)
                      .arg(combine.path.contains(combine.overlapPoint) ? 1 : 0));

        const PairResult intersect = runPair(BoolOps::Operation::Intersect, true, true);
        ctx.check(intersect.ok && intersect.path.contains(intersect.overlapPoint) &&
                      !intersect.path.contains(diamondCenter) && !intersect.path.contains(starCenter),
                  QStringLiteral("菱形与五角星相交只保留重叠区"),
                  QStringLiteral("ok=%1 overlap=%2 diamond=%3 star=%4")
                      .arg(intersect.ok ? 1 : 0)
                      .arg(intersect.path.contains(intersect.overlapPoint) ? 1 : 0)
                      .arg(intersect.path.contains(diamondCenter) ? 1 : 0)
                      .arg(intersect.path.contains(starCenter) ? 1 : 0));

        const PairResult subtract = runPair(BoolOps::Operation::Subtract, true, true);
        ctx.check(subtract.ok && subtract.path.contains(diamondCenter) && !subtract.path.contains(starCenter) &&
                      !subtract.path.contains(subtract.overlapPoint),
                  QStringLiteral("菱形与五角星剪除以先选中的菱形为底，挖掉五角星"),
                  QStringLiteral("ok=%1 diamond=%2 star=%3 overlap=%4")
                      .arg(subtract.ok ? 1 : 0)
                      .arg(subtract.path.contains(diamondCenter) ? 1 : 0)
                      .arg(subtract.path.contains(starCenter) ? 1 : 0)
                      .arg(subtract.path.contains(subtract.overlapPoint) ? 1 : 0));

        const PairResult fragment = runPair(BoolOps::Operation::Fragment, true, true);
        ctx.check(fragment.ok && fragment.items == 3 && fragment.layers == 3 && fragment.path.contains(diamondCenter) &&
                      fragment.path.contains(starCenter),
                  QStringLiteral("菱形与五角星拆分得到三块可分别选中的多边形"),
                  QStringLiteral("ok=%1 items=%2 layers=%3")
                      .arg(fragment.ok ? 1 : 0)
                      .arg(fragment.items)
                      .arg(fragment.layers));
    }
    {
        // 已生成的多边形再次参与联合：按层分解成填充轮廓后整块作为参与者，
        // 不能因为它是复合图层就把填充区域丢掉。
        QUndoStack compositeUndo;
        CanvasScene compositeScene;
        compositeScene.setUndoStack(&compositeUndo);
        compositeScene.setMode(ToolMode::Edit);
        compositeScene.applyCanvasSize(QSizeF(460.0, 320.0));
        compositeScene.setShowGrid(false);
        BackgroundState compositeBackground;
        compositeBackground.type = BackgroundType::Color;
        compositeBackground.color = QColor(Qt::white);
        compositeScene.applyBackground(compositeBackground);

        auto makeShape = [&compositeScene](ShapeType type, QPointF pos, QColor fill) {
            auto* shape = new ShapeItem(type);
            shape->setPos(pos);
            shape->setContentSize(QSizeF(140.0, 140.0));
            shape->setStyle(fill, fill, 4.0);
            shape->setFill(fill, FillStyle::Solid);
            compositeScene.addItemsWithUndo({shape}, QStringLiteral("Add composite source"));
            return shape;
        };
        auto* rectangle = makeShape(ShapeType::Rectangle, QPointF(40.0, 60.0), QColor(30, 160, 90));
        auto* diamond = makeShape(ShapeType::Diamond, QPointF(100.0, 100.0), QColor(230, 140, 40));
        rectangle->setSelected(true);
        diamond->setSelected(true);
        const bool firstUnion =
            BoolOps::apply(&compositeScene, BoolOps::Operation::Union, QStringLiteral("First union"));
        const QList<CanvasItem*> firstSelection = compositeScene.selectedCanvasItems();
        auto* polygon =
            firstSelection.isEmpty() ? nullptr : dynamic_cast<CompositeShapeItem*>(firstSelection.constFirst());
        auto* star = makeShape(ShapeType::Star, QPointF(260.0, 90.0), QColor(40, 90, 200));
        if (polygon) {
            polygon->setSelected(true);
        }
        star->setSelected(true);
        const bool secondUnion =
            BoolOps::apply(&compositeScene, BoolOps::Operation::Union, QStringLiteral("Second union"));
        const QList<CanvasItem*> secondSelection = compositeScene.selectedCanvasItems();
        auto* result = secondSelection.isEmpty() ? nullptr : secondSelection.constFirst();
        const QPainterPath resultPath = result ? result->mapToScene(result->outlineShape()) : QPainterPath();
        ctx.check(firstUnion && polygon && secondUnion && result && resultPath.contains(QPointF(80.0, 100.0)) &&
                      resultPath.contains(QPointF(330.0, 160.0)),
                  QStringLiteral("已生成的多边形再次参与联合仍保留原填充区域"),
                  QStringLiteral("first=%1 polygon=%2 second=%3 result=%4")
                      .arg(firstUnion ? 1 : 0)
                      .arg(polygon ? 1 : 0)
                      .arg(secondUnion ? 1 : 0)
                      .arg(result ? 1 : 0));
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
        QRectF groupedExtent;
        for (CanvasItem* item : std::as_const(groupedSelection)) {
            groupedExtent =
                groupedExtent.isNull() ? item->sceneBoundingRect() : groupedExtent.united(item->sceneBoundingRect());
        }
        auto* groupedResult =
            groupedSelection.isEmpty() ? nullptr : dynamic_cast<CompositeShapeItem*>(groupedSelection.constFirst());
        ctx.check(groupedSubtract && groupedResult && groupedExtent.width() > 100.0,
                  QStringLiteral("整组作为一个参与者参与合并"),
                  QStringLiteral("ok=%1 width=%2 pieces=%3")
                      .arg(groupedSubtract ? 1 : 0)
                      .arg(groupedExtent.width())
                      .arg(groupedSelection.size()));
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
        ctx.check(boolMenu && boolMenu->actions().size() == 6, QStringLiteral("编辑菜单提供 6 项合并图元操作"),
                  boolMenu ? QString::number(boolMenu->actions().size()) : QString());
        ctx.check(!boolButton, QStringLiteral("工具栏不再显示合并图元按钮"));
        ctx.check(boolView && boolView->contextMenuGroupCount() >= 1, QStringLiteral("画布右键菜单挂上了合并图元分组"),
                  boolView ? QString::number(boolView->contextMenuGroupCount()) : QString());
        bool iconsOk = boolMenu && boolMenu->actions().size() == 6;
        if (iconsOk) {
            for (QAction* action : boolMenu->actions()) {
                if (action->icon().isNull()) {
                    iconsOk = false;
                    break;
                }
            }
        }
        ctx.check(iconsOk, QStringLiteral("合并图元六项操作都带参照 PPT 的图标"));
        // 合并图元主入口（菜单项）图标必须与 6 项子操作不同，否则看起来像同一项操作。
        bool morphIconDistinct = boolMenu && !boolMenu->icon().isNull();
        if (morphIconDistinct) {
            for (QAction* action : boolMenu->actions()) {
                if (action->icon().cacheKey() == boolMenu->icon().cacheKey()) {
                    morphIconDistinct = false;
                    break;
                }
            }
        }
        ctx.check(morphIconDistinct, QStringLiteral("合并图元主入口图标与六项子操作图标不同"));
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
