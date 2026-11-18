#include "CanvasItem.h"
#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CompositeShapeItem.h"
#include "CurveItem.h"
#include "ItemDockPanel.h"
#include "LineSplitOps.h"
#include "MainWindow.h"
#include "ShapeItem.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"

#include <QApplication>
#include <QPainterPath>
#include <QPointF>
#include <QToolButton>
#include <QUndoStack>
#include <QVector>
#include <QtMath>

namespace {

// 场景内所有图元的数量（切刀保留，碎片与被切图元都会被计数）。
int itemCount(const CanvasScene& scene)
{
    return scene.canvasItemCount();
}

} // namespace

void selftest::runLineSplitTests(Context& ctx)
{
    // 参与者规则：≥1 个填充图元 + 恰好 1 条线类切刀。
    {
        CanvasScene scene;
        scene.applyCanvasSize(QSizeF(800.0, 600.0));
        scene.setMode(ToolMode::Edit);
        QUndoStack undo;
        scene.setUndoStack(&undo);

        auto* rect = new ShapeItem(ShapeType::Rectangle);
        rect->setContentSize(QSizeF(180.0, 120.0));
        scene.addItemsWithUndo({rect}, QStringLiteral("Add rect"));

        scene.clearSelection();
        rect->setSelected(true);
        ctx.check(!LineSplitOps::supports(&scene), QStringLiteral("没有切刀时分割不可用"));

        auto* knife = new ShapeItem(ShapeType::Line);
        knife->setContentSize(QSizeF(200.0, 10.0));
        scene.addItemsWithUndo({knife}, QStringLiteral("Add knife"));

        scene.clearSelection();
        rect->setSelected(true);
        knife->setSelected(true);
        ctx.check(LineSplitOps::supports(&scene), QStringLiteral("填充图元加一条直线切刀时可用"));

        auto* secondRect = new ShapeItem(ShapeType::Rectangle);
        secondRect->setContentSize(QSizeF(80.0, 80.0));
        scene.addItemsWithUndo({secondRect}, QStringLiteral("Add second rect"));
        scene.clearSelection();
        rect->setSelected(true);
        secondRect->setSelected(true);
        ctx.check(!LineSplitOps::supports(&scene), QStringLiteral("只有填充图元没有切刀时不可用"));
        secondRect->setSelected(false);
    }

    // 直线切矩形：上下两块多边形，切刀保留，一次撤销还原。
    {
        CanvasScene scene;
        scene.applyCanvasSize(QSizeF(800.0, 600.0));
        scene.setMode(ToolMode::Edit);
        QUndoStack undo;
        scene.setUndoStack(&undo);

        auto* rect = new ShapeItem(ShapeType::Rectangle);
        rect->setContentSize(QSizeF(180.0, 120.0));
        rect->setPos(QPointF(110.0, 150.0));
        rect->setFill(QColor(QStringLiteral("#2563EB")), FillStyle::Solid);
        scene.addItemsWithUndo({rect}, QStringLiteral("Add rect"));
        auto* knife = new ShapeItem(ShapeType::Line);
        knife->setContentSize(QSizeF(200.0, 10.0));
        knife->setPos(QPointF(100.0, 200.0));
        scene.addItemsWithUndo({knife}, QStringLiteral("Add knife"));

        scene.clearSelection();
        rect->setSelected(true);
        knife->setSelected(true);
        const int before = itemCount(scene);
        ctx.check(LineSplitOps::apply(&scene, QStringLiteral("分割")), QStringLiteral("直线可以切分矩形"));
        const int after = itemCount(scene);
        ctx.check(after == before + 1, QStringLiteral("切分后切刀保留并多出两块碎片"),
                  QStringLiteral("before=%1 after=%2").arg(before).arg(after));

        // 碎片样式沿用被切图元，且刚好两块、互不重叠。
        QList<CompositeShapeItem*> fragments;
        int knifeCount = 0;
        for (CanvasItem* item : scene.canvasItems()) {
            if (auto* composite = dynamic_cast<CompositeShapeItem*>(item)) {
                fragments.append(composite);
            } else if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
                if (shape->shapeType() == ShapeType::Line) {
                    ++knifeCount;
                }
            }
        }
        ctx.check(fragments.size() == 2 && knifeCount == 1, QStringLiteral("矩形被切成两块且直线切刀还在"),
                  QStringLiteral("fragments=%1 knives=%2").arg(fragments.size()).arg(knifeCount));
        qreal areaSum = 0.0;
        bool stylesOk = fragments.size() == 2;
        for (const CompositeShapeItem* fragment : fragments) {
            const CompositeShapeItem::Layer& layer = fragment->layers().constFirst();
            stylesOk = stylesOk && layer.fill == QColor(QStringLiteral("#2563EB")) &&
                       layer.role == CompositeShapeItem::LayerRole::FilledArea &&
                       fragment->fillStyle() == FillStyle::Solid;
            const QRectF bounds = layer.path.boundingRect();
            areaSum += bounds.width() * bounds.height();
        }
        ctx.check(stylesOk, QStringLiteral("碎片沿用被切图元的填充色与填充样式"));
        ctx.check(qAbs(areaSum - 180.0 * 120.0) < 8.0, QStringLiteral("两块碎片面积之和等于原矩形"),
                  QStringLiteral("area=%1").arg(areaSum));
        ctx.check(fragments.at(0)->isSelected() && fragments.at(1)->isSelected(),
                  QStringLiteral("切分后碎片处于选中状态"));

        // 需求 11：碎片轮廓按闭合子路径描边，收尾边不能缺。
        bool closureOk = fragments.size() == 2;
        for (const CompositeShapeItem* fragment : fragments) {
            const QPainterPath& piece = fragment->layers().constFirst().path;
            const QPainterPath closed = CompositeShapeItem::closedSubpaths(piece);
            closureOk = closureOk && closed.elementCount() >= piece.elementCount();
            // 闭合后的路径终点回到子路径起点：首元素与最后一个绘制元素重合。
            const QPointF first = closed.elementAt(0);
            const QPointF last = closed.elementAt(closed.elementCount() - 1);
            closureOk = closureOk && qFuzzyCompare(first.x(), last.x()) && qFuzzyCompare(first.y(), last.y());
        }
        ctx.check(closureOk, QStringLiteral("分割碎片的每条子路径都显式闭合"));

        // 需求 12：选中填充图元 + 切刀时，编辑图元面板也展开分割按钮（6 项都可见）。
        {
            MainWindow window;
            window.show();
            QApplication::processEvents();
            window.insertShape(ShapeType::Rectangle);
            window.insertShape(ShapeType::Line);
            CanvasScene* mainScene = window.findChild<CanvasScene*>();
            QList<CanvasItem*> items = mainScene->canvasItems();
            mainScene->clearSelection();
            for (CanvasItem* item : items) {
                item->setSelected(true);
            }
            mainScene->update();
            if (auto* dockPanel = window.findChild<ItemDockPanel*>()) {
                dockPanel->refresh();
                QApplication::processEvents();
                const QList<QToolButton*> morphButtons =
                    dockPanel->findChildren<QToolButton*>(QStringLiteral("ItemMorphButton"));
                int visibleMorph = 0;
                for (const QToolButton* button : morphButtons) {
                    if (button->isVisible()) {
                        ++visibleMorph;
                    }
                }
                // 矩形 + 直线只有分割可用：6 颗按钮里只有最后一颗（分割）可见。
                ctx.check(morphButtons.size() == 6 && visibleMorph == 1 && morphButtons.last()->isVisible(),
                          QStringLiteral("选中填充图元与切刀时面板展开分割按钮"),
                          QStringLiteral("total=%1 visible=%2").arg(morphButtons.size()).arg(visibleMorph));
            } else {
                ctx.check(false, QStringLiteral("编辑图元面板存在"));
            }
        }

        undo.undo();
        ctx.check(itemCount(scene) == before, QStringLiteral("撤销一次还原为矩形加切刀"));
        undo.redo();
        ctx.check(itemCount(scene) == after, QStringLiteral("重做恢复切分结果"));
    }

    // 曲线切椭圆：中心线延伸后同样分成两块。
    {
        CanvasScene scene;
        scene.applyCanvasSize(QSizeF(800.0, 600.0));
        scene.setMode(ToolMode::Edit);
        QUndoStack undo;
        scene.setUndoStack(&undo);

        auto* ellipse = new ShapeItem(ShapeType::Ellipse);
        ellipse->setContentSize(QSizeF(160.0, 120.0));
        ellipse->setPos(QPointF(120.0, 180.0));
        scene.addItemsWithUndo({ellipse}, QStringLiteral("Add ellipse"));
        auto* curve = new CurveItem();
        curve->setStrokePoints({QPointF(100.0, 240.0), QPointF(280.0, 240.0)}, QColor(QStringLiteral("#334155")), 3.0);
        scene.addItemsWithUndo({curve}, QStringLiteral("Add curve"));

        scene.clearSelection();
        ellipse->setSelected(true);
        curve->setSelected(true);
        ctx.check(LineSplitOps::supports(&scene), QStringLiteral("曲线作为切刀时可用"));
        ctx.check(LineSplitOps::apply(&scene, QStringLiteral("分割")), QStringLiteral("曲线可以切分椭圆"));

        int fragments = 0;
        for (CanvasItem* item : scene.canvasItems()) {
            if (dynamic_cast<CompositeShapeItem*>(item)) {
                ++fragments;
            }
        }
        ctx.check(fragments == 2, QStringLiteral("椭圆被曲线切成两块"), QStringLiteral("fragments=%1").arg(fragments));
    }

    // 需求 21：弯曲曲线按完整轨迹切分，切口贴合曲线走向而不是端点连线。
    {
        CanvasScene scene;
        scene.applyCanvasSize(QSizeF(800.0, 600.0));
        scene.setMode(ToolMode::Edit);
        QUndoStack undo;
        scene.setUndoStack(&undo);

        auto* rect = new ShapeItem(ShapeType::Rectangle);
        rect->setContentSize(QSizeF(200.0, 200.0));
        rect->setPos(QPointF(110.0, 150.0));
        scene.addItemsWithUndo({rect}, QStringLiteral("Add rect"));
        // 下凹的曲线：从矩形左缘进入、中部下探到 y≈300、右缘出去；
        // 端点连线（y≈200 的水平线）与轨迹明显不同。
        auto* curve = new CurveItem();
        curve->setStrokePoints({QPointF(80.0, 200.0), QPointF(200.0, 300.0), QPointF(320.0, 200.0)},
                               QColor(QStringLiteral("#334155")), 4.0);
        scene.addItemsWithUndo({curve}, QStringLiteral("Add curve"));

        scene.clearSelection();
        rect->setSelected(true);
        curve->setSelected(true);
        ctx.check(LineSplitOps::supports(&scene), QStringLiteral("弯曲曲线作为切刀时可用"));
        ctx.check(LineSplitOps::apply(&scene, QStringLiteral("分割")), QStringLiteral("弯曲曲线可以按轨迹切分矩形"));

        int fragments = 0;
        bool upperFound = false;
        bool lowerFound = false;
        for (CanvasItem* item : scene.canvasItems()) {
            if (auto* fragment = dynamic_cast<CompositeShapeItem*>(item)) {
                ++fragments;
                const QPainterPath painted =
                    fragment->mapToScene(fragment->mapLayerPath(fragment->layers().constFirst().path));
                upperFound = upperFound || painted.contains(QPointF(200.0, 160.0));
                lowerFound = lowerFound || painted.contains(QPointF(200.0, 340.0));
            }
        }
        ctx.check(fragments == 2, QStringLiteral("矩形被弯曲曲线切成两块"),
                  QStringLiteral("fragments=%1").arg(fragments));
        ctx.check(upperFound && lowerFound, QStringLiteral("切口贴合曲线轨迹（上/下两块沿曲线分布）"),
                  QStringLiteral("upper=%1 lower=%2").arg(upperFound).arg(lowerFound));
    }

    // 需求 21：曲线刀线未穿过填充图形（整条落在内部）时不切分。
    {
        CanvasScene scene;
        scene.applyCanvasSize(QSizeF(800.0, 600.0));
        scene.setMode(ToolMode::Edit);
        QUndoStack undo;
        scene.setUndoStack(&undo);

        auto* rect = new ShapeItem(ShapeType::Rectangle);
        rect->setContentSize(QSizeF(200.0, 200.0));
        rect->setPos(QPointF(110.0, 150.0));
        scene.addItemsWithUndo({rect}, QStringLiteral("Add rect"));
        auto* curve = new CurveItem();
        curve->setStrokePoints({QPointF(150.0, 200.0), QPointF(200.0, 260.0), QPointF(250.0, 200.0)},
                               QColor(QStringLiteral("#334155")), 4.0);
        scene.addItemsWithUndo({curve}, QStringLiteral("Add curve"));

        scene.clearSelection();
        rect->setSelected(true);
        curve->setSelected(true);
        ctx.check(!LineSplitOps::apply(&scene, QStringLiteral("分割")), QStringLiteral("内部曲线不切分填充图形"));
        bool originalKept = false;
        for (CanvasItem* item : scene.canvasItems()) {
            if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
                originalKept = originalKept || shape == rect;
            }
        }
        ctx.check(originalKept, QStringLiteral("未切到时原图元保留"));
    }

    // 刀线没碰到填充图形：不入栈、场景不变。
    {
        CanvasScene scene;
        scene.applyCanvasSize(QSizeF(800.0, 600.0));
        scene.setMode(ToolMode::Edit);
        QUndoStack undo;
        scene.setUndoStack(&undo);

        auto* rect = new ShapeItem(ShapeType::Rectangle);
        rect->setContentSize(QSizeF(120.0, 90.0));
        rect->setPos(QPointF(110.0, 150.0));
        scene.addItemsWithUndo({rect}, QStringLiteral("Add rect"));
        auto* knife = new ShapeItem(ShapeType::Line);
        knife->setContentSize(QSizeF(160.0, 10.0));
        knife->setPos(QPointF(90.0, 300.0));
        scene.addItemsWithUndo({knife}, QStringLiteral("Add knife"));

        scene.clearSelection();
        rect->setSelected(true);
        knife->setSelected(true);
        const int before = itemCount(scene);
        const int undoCount = undo.count();
        ctx.check(!LineSplitOps::apply(&scene, QStringLiteral("分割")),
                  QStringLiteral("切刀不与填充图形相交时返回失败"));
        ctx.check(itemCount(scene) == before && undo.count() == undoCount, QStringLiteral("切空时不产生图元与撤销记录"),
                  QStringLiteral("items=%1 undo=%2").arg(itemCount(scene)).arg(undo.count()));
    }
}
