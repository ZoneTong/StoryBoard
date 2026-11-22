#include "app/I18n.h"
#include "app/Settings.h"
#include "canvas/CanvasView.h"
#include "io/CustomShapeTemplates.h"
#include "items/CanvasTypes.h"
#include "items/lines/CurveItem.h"
#include "items/lines/StraightLineItem.h"
#include "items/polygon/PolygonItem.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "items/text/TextItem.h"
#include "lineart/LineArtConverter.h"
#include "lineart/LineArtDialog.h"
#include "lineart/LineArtPresets.h"
#include "ops/Alignment.h"
#include "ops/BoolOps.h"
#include "ops/MergeRegions.h"
#include "ops/PathRegions.h"
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
void selftest::RunGroupBoolOpsTests(Context& ctx)
{
    // 分组层级与布尔运算。组可以再被组包住：单击组内图元选中整棵组树。
    {
        QUndoStack groupUndo;
        CanvasScene groupScene;
        groupScene.SetUndoStack(&groupUndo);
        groupScene.SetMode(ToolMode::EDIT);
        groupScene.ApplyCanvasSize(QSizeF(400.0, 300.0));
        auto makeRect = [&groupScene](qreal x, qreal y) {
            auto* item = CreateShapeItem(ShapeType::RECTANGLE);
            item->setPos(QPointF(x, y));
            item->SetContentSize(QSizeF(40.0, 40.0));
            item->SetStyle(QColor(30, 41, 59), QColor(30, 41, 59, 36), 1.0);
            item->SetFill(QColor(30, 41, 59, 36), FillStyle::SOLID);
            groupScene.AddItemsWithUndo({item}, QStringLiteral("Add group item"));
            return item;
        };
        CanvasItem* first = makeRect(10.0, 10.0);
        CanvasItem* second = makeRect(20.0, 20.0);
        CanvasItem* third = makeRect(30.0, 30.0);
        first->setSelected(true);
        second->setSelected(true);
        const int innerGroup = groupScene.GroupSelectedItems();
        ctx.Check(innerGroup == 2 && first->GroupId() > 0 && groupScene.ItemsInGroup(first->GroupId()).size() == 2,
                  QStringLiteral("两个同层图元可以组合成一组"), QStringLiteral("grouped=%1").arg(innerGroup));

        third->setSelected(true);
        groupScene.GroupSelectedItems();
        const int outerGroup = first->GroupId() > 0 ? groupScene.OutermostGroup(first->GroupId()) : 0;
        ctx.Check(
            outerGroup > 0 && groupScene.ItemsInGroupTree(outerGroup).size() == 3,
            QStringLiteral("再和其他图元组合时形成外层组，覆盖整棵组树"),
            QStringLiteral("outer=%1 members=%2").arg(outerGroup).arg(groupScene.ItemsInGroupTree(outerGroup).size()));

        groupScene.clearSelection();
        const bool selectedWholeTree = groupScene.SelectGroupOfItem(second);
        ctx.Check(selectedWholeTree && groupScene.SelectedCanvasItems().size() == 3,
                  QStringLiteral("单击组内图元时选中整棵组树"),
                  QStringLiteral("selected=%1").arg(groupScene.SelectedCanvasItems().size()));
        // 撤销分组后成员回到散图元状态。
        groupScene.UndoStack()->undo();
        ctx.Check(third->GroupId() == 0 && groupScene.GroupParents().isEmpty(),
                  QStringLiteral("撤销外层分组还原组层级"));
    }
    {
        QUndoStack boolUndo;
        CanvasScene boolScene;
        boolScene.SetUndoStack(&boolUndo);
        boolScene.SetMode(ToolMode::EDIT);
        boolScene.ApplyCanvasSize(QSizeF(400.0, 300.0));
        auto makeRect = [&boolScene](qreal x, const QColor& color) {
            auto* item = CreateShapeItem(ShapeType::RECTANGLE);
            item->setPos(QPointF(x, 40.0));
            item->SetContentSize(QSizeF(60.0, 60.0));
            item->SetStyle(color, color.lighter(160), 1.0);
            boolScene.AddItemsWithUndo({item}, QStringLiteral("Add bool item"));
            return item;
        };
        CanvasItem* lower = makeRect(20.0, QColor(200, 40, 40));
        CanvasItem* upper = makeRect(50.0, QColor(40, 90, 200));
        lower->setSelected(true);
        upper->setSelected(true);
        ctx.Check(BoolOps::Supports(&boolScene), QStringLiteral("两个图元可以进行合并"), QStringLiteral("items=2"));
        const bool applied = BoolOps::Apply(&boolScene, BoolOps::Operation::UNION, QStringLiteral("合并"));
        const QList<CanvasItem*> selected = boolScene.SelectedCanvasItems();
        auto* composite = selected.isEmpty() ? nullptr : dynamic_cast<PolygonItem*>(selected.constFirst());
        ctx.Check(applied && composite && composite->LayerCount() == 1,
                  QStringLiteral("联合运算生成单层 donor 样式多边形"),
                  QStringLiteral("ok=%1 layers=%2").arg(applied ? 1 : 0).arg(composite ? composite->LayerCount() : -1));

        // 3 个及以上图元的合并：五项运算都要真正落到结果，不能只处理前两个。
        auto addRect = [](CanvasScene& scene, qreal x, qreal y, qreal w, qreal h) {
            auto* item = CreateShapeItem(ShapeType::RECTANGLE);
            item->setPos(QPointF(x, y));
            item->SetContentSize(QSizeF(w, h));
            item->SetStyle(QColor(20, 20, 20), QColor(200, 60, 60), 2.0);
            item->SetFill(QColor(200, 60, 60), FillStyle::SOLID);
            scene.AddItemsWithUndo({item}, QStringLiteral("Add rect"));
            return item;
        };
        {
            // 联合：3 个互相重叠的矩形合成一个图元。
            QUndoStack undo;
            CanvasScene scene;
            scene.SetUndoStack(&undo);
            scene.SetMode(ToolMode::EDIT);
            scene.ApplyCanvasSize(QSizeF(600.0, 400.0));
            CanvasItem* a = addRect(scene, 20, 20, 80, 60);
            CanvasItem* b = addRect(scene, 60, 50, 80, 60);
            CanvasItem* c = addRect(scene, 100, 20, 80, 60);
            a->setSelected(true);
            b->setSelected(true);
            c->setSelected(true);
            const bool ok = BoolOps::Apply(&scene, BoolOps::Operation::UNION, QStringLiteral("合并"));
            const QList<CanvasItem*> sel = scene.SelectedCanvasItems();
            const bool covers =
                sel.size() == 1 && sel.constFirst()->sceneBoundingRect().contains(QRectF(20, 20, 160, 90));
            ctx.Check(ok && sel.size() == 1 && scene.CanvasItemCount() == 1 && covers,
                      QStringLiteral("3 个图元联合成功并覆盖全部原始范围"),
                      QStringLiteral("ok=%1 selected=%2 items=%3 covers=%4")
                          .arg(ok ? 1 : 0)
                          .arg(sel.size())
                          .arg(scene.CanvasItemCount())
                          .arg(covers ? 1 : 0));
        }
        {
            // 联合：4 个图元（3 个及以上含 4 个的情形）。
            QUndoStack undo;
            CanvasScene scene;
            scene.SetUndoStack(&undo);
            scene.SetMode(ToolMode::EDIT);
            scene.ApplyCanvasSize(QSizeF(600.0, 400.0));
            CanvasItem* a = addRect(scene, 20, 20, 70, 50);
            CanvasItem* b = addRect(scene, 50, 40, 70, 50);
            CanvasItem* c = addRect(scene, 80, 20, 70, 50);
            CanvasItem* d = addRect(scene, 60, 80, 70, 50);
            a->setSelected(true);
            b->setSelected(true);
            c->setSelected(true);
            d->setSelected(true);
            const bool ok = BoolOps::Apply(&scene, BoolOps::Operation::UNION, QStringLiteral("合并"));
            ctx.Check(ok && scene.SelectedCanvasItems().size() == 1 && scene.CanvasItemCount() == 1,
                      QStringLiteral("4 个图元联合成功"),
                      QStringLiteral("ok=%1 selected=%2 items=%3")
                          .arg(ok ? 1 : 0)
                          .arg(scene.SelectedCanvasItems().size())
                          .arg(scene.CanvasItemCount()));
        }
        {
            // 异或：3 个矩形，并集减重叠；新语义按连续块拆分：左右顶部共边相连成一块，底部横条单独一块。
            QUndoStack undo;
            CanvasScene scene;
            scene.SetUndoStack(&undo);
            scene.SetMode(ToolMode::EDIT);
            scene.ApplyCanvasSize(QSizeF(600.0, 400.0));
            CanvasItem* a = addRect(scene, 20, 20, 80, 60);
            CanvasItem* b = addRect(scene, 60, 50, 80, 60);
            CanvasItem* c = addRect(scene, 100, 20, 80, 60);
            a->setSelected(true);
            b->setSelected(true);
            c->setSelected(true);
            const bool ok = BoolOps::Apply(&scene, BoolOps::Operation::XOR, QStringLiteral("异或"));
            ctx.Check(ok && scene.SelectedCanvasItems().size() == 2 && scene.CanvasItemCount() == 2,
                      QStringLiteral("3 个图元异或成功"),
                      QStringLiteral("ok=%1 selected=%2 items=%3")
                          .arg(ok ? 1 : 0)
                          .arg(scene.SelectedCanvasItems().size())
                          .arg(scene.CanvasItemCount()));
        }
        {
            // 相交：3 个矩形取公共重叠区。
            QUndoStack undo;
            CanvasScene scene;
            scene.SetUndoStack(&undo);
            scene.SetMode(ToolMode::EDIT);
            scene.ApplyCanvasSize(QSizeF(600.0, 400.0));
            CanvasItem* a = addRect(scene, 20, 20, 100, 80);
            CanvasItem* b = addRect(scene, 60, 40, 100, 80);
            CanvasItem* c = addRect(scene, 80, 20, 100, 80);
            a->setSelected(true);
            b->setSelected(true);
            c->setSelected(true);
            const bool ok = BoolOps::Apply(&scene, BoolOps::Operation::INTERSECT, QStringLiteral("相交"));
            const QList<CanvasItem*> sel = scene.SelectedCanvasItems();
            ctx.Check(ok && sel.size() == 1 && sel.constFirst()->sceneBoundingRect().width() <= 160.0,
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
            scene.SetUndoStack(&undo);
            scene.SetMode(ToolMode::EDIT);
            scene.ApplyCanvasSize(QSizeF(600.0, 400.0));
            CanvasItem* base = addRect(scene, 20, 20, 240, 180);
            CanvasItem* cut1 = addRect(scene, 60, 60, 80, 60);
            CanvasItem* cut2 = addRect(scene, 150, 90, 80, 60);
            base->setSelected(true);
            cut1->setSelected(true);
            cut2->setSelected(true);
            const bool ok = BoolOps::Apply(&scene, BoolOps::Operation::SUBTRACT, QStringLiteral("剪除"));
            ctx.Check(ok && scene.SelectedCanvasItems().size() == 1 && scene.CanvasItemCount() == 1,
                      QStringLiteral("3 个图元剪除成功（首个选中为底）"),
                      QStringLiteral("ok=%1 selected=%2 items=%3")
                          .arg(ok ? 1 : 0)
                          .arg(scene.SelectedCanvasItems().size())
                          .arg(scene.CanvasItemCount()));
        }
        {
            // 剪除把底图切成两半时，每一块都要成为独立图元，而不是一个图元里藏着两块。
            QUndoStack undo;
            CanvasScene scene;
            scene.SetUndoStack(&undo);
            scene.SetMode(ToolMode::EDIT);
            scene.ApplyCanvasSize(QSizeF(600.0, 400.0));
            CanvasItem* base = addRect(scene, 20, 20, 300, 120);
            CanvasItem* cutter = addRect(scene, 150, 10, 40, 140);
            base->setSelected(true);
            cutter->setSelected(true);
            const bool ok = BoolOps::Apply(&scene, BoolOps::Operation::SUBTRACT, QStringLiteral("剪除"));
            const QList<CanvasItem*> pieces = scene.SelectedCanvasItems();
            bool eachNarrower = pieces.size() == 2;
            for (CanvasItem* piece : std::as_const(pieces)) {
                if (piece->sceneBoundingRect().width() >= 250.0) {
                    eachNarrower = false;
                }
            }
            ctx.Check(ok && pieces.size() == 2 && scene.CanvasItemCount() == 2 && eachNarrower,
                      QStringLiteral("剪除把底图切成互不相连的两块，各自成为独立图元"),
                      QStringLiteral("ok=%1 pieces=%2 items=%3")
                          .arg(ok ? 1 : 0)
                          .arg(pieces.size())
                          .arg(scene.CanvasItemCount()));
        }
        {
            // 拆分：3 个填充图元互相重叠，拆成多块且总数守恒（原 3 个消失）。
            QUndoStack undo;
            CanvasScene scene;
            scene.SetUndoStack(&undo);
            scene.SetMode(ToolMode::EDIT);
            scene.ApplyCanvasSize(QSizeF(600.0, 400.0));
            CanvasItem* a = addRect(scene, 20, 20, 80, 60);
            CanvasItem* b = addRect(scene, 60, 50, 80, 60);
            CanvasItem* c = addRect(scene, 100, 20, 80, 60);
            a->setSelected(true);
            b->setSelected(true);
            c->setSelected(true);
            const bool ok = BoolOps::Apply(&scene, BoolOps::Operation::FRAGMENT, QStringLiteral("拆分"));
            const int pieces = scene.SelectedCanvasItems().size();
            ctx.Check(
                ok && pieces >= 2 && pieces <= 8 && scene.CanvasItemCount() == pieces,
                QStringLiteral("3 个图元拆分成功且分块数在 2–8 之间"),
                QStringLiteral("ok=%1 pieces=%2 items=%3").arg(ok ? 1 : 0).arg(pieces).arg(scene.CanvasItemCount()));
        }

        // 零面积描线不作为挖除与分块边界：直线横穿平行四边形后，组合 / 剪除 / 拆分
        // 都不能在平行四边形上留细缝，交集条带按线宽带渲染而不是套描线的面积层。
        {
            // 线条类图元（直线 / 曲线 / 括号 / 弧 / 纯描线多边形）不参与合并：
            // 只选中一条直线与一个平行四边形时，参与者只有平行四边形一个，合并直接失败，
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
                scene.SetUndoStack(&undo);
                scene.SetMode(ToolMode::EDIT);
                scene.ApplyCanvasSize(QSizeF(320.0, 220.0));
                CanvasBackground background;
                background.type = BackgroundType::COLOR;
                background.color = QColor(Qt::white);
                scene.SetCanvasBackground(background);
                auto* diamond = CreateShapeItem(ShapeType::PARALLELOGRAM);
                diamond->setPos(QPointF(70.0, 40.0));
                diamond->SetContentSize(QSizeF(160.0, 120.0));
                diamond->SetStyle(QColor(20, 20, 20), QColor(200, 40, 40), 4.0);
                diamond->SetFill(QColor(200, 40, 40), FillStyle::SOLID);
                auto* line = new StraightLineItem();
                line->SetEndpoints(QPointF(30.0, 100.0), QPointF(280.0, 100.0), QColor(30, 90, 200), 6.0);
                scene.AddItemsWithUndo({line, diamond}, QStringLiteral("Add line skip"));
                line->setSelected(true);
                diamond->setSelected(true);
                result.applied = BoolOps::Apply(&scene, operation, QStringLiteral("Line skip"));
                result.items = scene.CanvasItems().size();
                for (CanvasItem* item : scene.CanvasItems()) {
                    if (dynamic_cast<PolygonItem*>(item)) {
                        ++result.composites;
                    }
                }
                const QImage rendered = scene.RenderImage(QRectF(0.0, 0.0, 320.0, 220.0), 1.0);
                result.diamondTop = rendered.pixelColor(150, 60);
                result.slit = rendered.pixelColor(150, 100);
                return result;
            };
            const QColor diamondFill(200, 40, 40);
            const QList<QPair<BoolOps::Operation, QString>> lineOps = {
                {BoolOps::Operation::UNION, QStringLiteral("联合")},
                {BoolOps::Operation::XOR, QStringLiteral("异或")},
                {BoolOps::Operation::INTERSECT, QStringLiteral("相交")},
                {BoolOps::Operation::SUBTRACT, QStringLiteral("剪除")},
                {BoolOps::Operation::FRAGMENT, QStringLiteral("拆分")},
            };
            for (const auto& entry : lineOps) {
                const LineSkip result = runLineSkip(entry.first);
                ctx.Check(!result.applied && result.items == 2 && result.composites == 0 &&
                              result.diamondTop == diamondFill && result.slit == diamondFill,
                          QStringLiteral("%1：直线不参与合并，画布保持原样").arg(entry.second),
                          QStringLiteral("applied=%1 items=%2 composites=%3 top=%4 slit=%5")
                              .arg(result.applied ? 1 : 0)
                              .arg(result.items)
                              .arg(result.composites)
                              .arg(result.diamondTop.name())
                              .arg(result.slit.name()));
            }

            // 细窄交集降级为线宽带：平行四边形与五角星边角相错形成的窄透镜交集只填充
            // 不描边，不会被描线吞成黑块。
            QUndoStack lensUndo;
            CanvasScene lensScene;
            lensScene.SetUndoStack(&lensUndo);
            lensScene.SetMode(ToolMode::EDIT);
            lensScene.ApplyCanvasSize(QSizeF(420.0, 320.0));
            CanvasBackground lensBackground;
            lensBackground.type = BackgroundType::COLOR;
            lensBackground.color = QColor(Qt::white);
            lensScene.SetCanvasBackground(lensBackground);
            auto* lensDiamond = CreateShapeItem(ShapeType::PARALLELOGRAM);
            lensDiamond->setPos(QPointF(60.0, 70.0));
            lensDiamond->SetContentSize(QSizeF(180.0, 180.0));
            auto* lensStar = CreateShapeItem(ShapeType::STAR);
            lensStar->setPos(QPointF(180.0, 60.0));
            lensStar->SetContentSize(QSizeF(180.0, 180.0));
            lensScene.AddItemsWithUndo({lensDiamond, lensStar}, QStringLiteral("Add lens pair"));
            lensDiamond->setSelected(true);
            lensStar->setSelected(true);
            const bool lensOk =
                BoolOps::Apply(&lensScene, BoolOps::Operation::INTERSECT, QStringLiteral("Lens intersect"));
            int lensLayers = 0;
            int lensStroked = 0;
            for (CanvasItem* item : lensScene.SelectedCanvasItems()) {
                if (auto* composite = dynamic_cast<PolygonItem*>(item)) {
                    lensLayers += composite->LayerCount();
                    for (const PolygonItem::Layer& layer : composite->Layers()) {
                        if (layer.strokeWidth > 0.0) {
                            ++lensStroked;
                        }
                    }
                }
            }
            ctx.Check(
                lensOk && lensLayers == 1 && lensStroked == 0, QStringLiteral("细窄交集区域降级为线宽带只填充不描边"),
                QStringLiteral("ok=%1 layers=%2 stroked=%3").arg(lensOk ? 1 : 0).arg(lensLayers).arg(lensStroked));
        }

        // 有填充空间的图元优先于零面积图元：黑色文字叠在矩形上方（height 更大），
        // 合并 donor 仍取矩形的填充与描线样式。
        {
            QUndoStack undo;
            CanvasScene scene;
            scene.SetUndoStack(&undo);
            scene.SetMode(ToolMode::EDIT);
            scene.ApplyCanvasSize(QSizeF(320.0, 220.0));
            auto* rect = CreateShapeItem(ShapeType::RECTANGLE);
            rect->setPos(QPointF(60.0, 50.0));
            rect->SetContentSize(QSizeF(160.0, 120.0));
            rect->SetStyle(QColor(20, 20, 20), QColor(255, 255, 255), 4.0);
            rect->SetFill(QColor(200, 40, 40), FillStyle::SOLID);
            auto* text = new TextItem();
            text->SetTextFormat(QStringLiteral("Ag文"), QFont(QStringLiteral("Microsoft YaHei"), 24), QColor(0, 0, 0));
            text->SetContentSize(QSizeF(150.0, 50.0));
            text->setPos(QPointF(65.0, 85.0));
            scene.AddItemsWithUndo({rect, text}, QStringLiteral("Add rect and text"));
            rect->setSelected(true);
            text->setSelected(true);
            const bool merged = BoolOps::Apply(&scene, BoolOps::Operation::UNION, QStringLiteral("合并"));
            const QList<CanvasItem*> mergedItems = scene.SelectedCanvasItems();
            auto* shell = mergedItems.isEmpty() ? nullptr : dynamic_cast<PolygonItem*>(mergedItems.constFirst());
            ctx.Check(merged && shell && shell->FillColor() == QColor(200, 40, 40),
                      QStringLiteral("合并优先采用有填充空间图元的样式（矩形>文字）"),
                      QStringLiteral("ok=%1 fill=%2")
                          .arg(merged ? 1 : 0)
                          .arg(shell ? shell->FillColor().name() : QStringLiteral("none")));
        }

        // 文字参与合并：只填字形、不描边；不透明层直接覆盖，避免抗锯齿边缘露出白色重影。
        {
            QUndoStack undo;
            CanvasScene scene;
            scene.SetUndoStack(&undo);
            scene.SetMode(ToolMode::EDIT);
            scene.ApplyCanvasSize(QSizeF(320.0, 220.0));
            auto* diamond = CreateShapeItem(ShapeType::PARALLELOGRAM);
            diamond->setPos(QPointF(60.0, 30.0));
            diamond->SetContentSize(QSizeF(180.0, 140.0));
            diamond->SetStyle(QColor(20, 20, 20), QColor(255, 255, 255), 4.0);
            diamond->SetFill(QColor(255, 255, 255), FillStyle::SOLID);
            auto* text = new TextItem();
            text->SetTextFormat(QStringLiteral("Ag文"), QFont(QStringLiteral("Microsoft YaHei"), 24), QColor(0, 0, 0));
            text->SetContentSize(QSizeF(150.0, 50.0));
            text->setPos(QPointF(75.0, 80.0));
            scene.AddItemsWithUndo({diamond, text}, QStringLiteral("Add diamond and text"));
            diamond->setSelected(true);
            text->setSelected(true);
            const bool textMerged = BoolOps::Apply(&scene, BoolOps::Operation::UNION, QStringLiteral("合并"));
            const QList<CanvasItem*> textSelected = scene.SelectedCanvasItems();
            auto* textShell = textSelected.isEmpty() ? nullptr : dynamic_cast<PolygonItem*>(textSelected.constFirst());

            int inkPixels = -1;
            bool textLayerFound = false;
            if (textShell) {
                const QImage rendered = scene.RenderImage(QRectF(0.0, 0.0, 320.0, 220.0), 1.0);
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
                for (const PolygonItem::Layer& layer : textShell->Layers()) {
                    if (qFuzzyIsNull(layer.strokeWidth) && layer.fill == QColor(0, 0, 0)) {
                        textLayerFound = true;
                        break;
                    }
                }
            }
            // donor 换成有填充空间的平行四边形（白底）后，字形缝隙露出白底属合法；
            // 需要守住的是两条：黑色字形在合并后仍可见（遮挡语义：合并前在上层
            // 的文字合并后仍后画），以及文字层零描边（donor 白色描线不在字形外
            // 形成重影）。inkPixels > 0 即字形可见。
            ctx.Check(textMerged && textShell && textLayerFound && inkPixels > 0,
                      QStringLiteral("黑色文字合并后字形保持可见且无描边重影"),
                      QStringLiteral("ok=%1 textLayer=%2 ink=%3 layers=%4")
                          .arg(textMerged ? 1 : 0)
                          .arg(textLayerFound ? 1 : 0)
                          .arg(inkPixels)
                          .arg(textShell ? textShell->LayerCount() : -1));
        }
    }

    // §5.3/§5.5：宿主只在同一来源结果内挑选，分组内不跨子图元吸收碎块。
    {
        auto rectPath = [](qreal x, qreal y, qreal w, qreal h) {
            QPainterPath path;
            path.addRect(QRectF(x, y, w, h));
            return path;
        };
        auto makeBlock = [](const QPainterPath& path, int source) {
            MergeRegions::RegionBlock block;
            block.path = path;
            block.area = PathRegions::Area(path);
            block.sourceIndex = source;
            return block;
        };
        QList<MergeRegions::RegionBlock> blocks;
        blocks.append(makeBlock(rectPath(0, 0, 40, 40), 0));   // 来源 0 宿主
        blocks.append(makeBlock(rectPath(40, 0, 3, 3), 0));    // 来源 0 小碎块，共边可吸收
        blocks.append(makeBlock(rectPath(200, 200, 3, 3), 1)); // 来源 1 孤立小碎块，不跨来源并入
        blocks = MergeRegions::AbsorbTinyFragments(blocks);
        // 来源 0 碎块并入宿主；来源 1 碎块找不到同来源宿主被删除，结果只剩来源 0 宿主一块。
        ctx.Check(blocks.size() == 1 && blocks.constFirst().sourceIndex == 0,
                  QStringLiteral("碎块不跨来源结果并入宿主"), QStringLiteral("blocks=%1").arg(blocks.size()));
    }
}
