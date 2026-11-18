#include "CanvasItem.h"
#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CompositeShapeItem.h"
#include "CurveItem.h"
#include "LineSplitOps.h"
#include "ShapeItem.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"

#include <QPainterPath>
#include <QPointF>
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
        ctx.check(!LineSplitOps::supports(&scene), QStringLiteral("没有切刀时线条分割不可用"));

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
        ctx.check(LineSplitOps::apply(&scene, QStringLiteral("线条分割")), QStringLiteral("直线可以切分矩形"));
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
        ctx.check(LineSplitOps::apply(&scene, QStringLiteral("线条分割")), QStringLiteral("曲线可以切分椭圆"));

        int fragments = 0;
        for (CanvasItem* item : scene.canvasItems()) {
            if (dynamic_cast<CompositeShapeItem*>(item)) {
                ++fragments;
            }
        }
        ctx.check(fragments == 2, QStringLiteral("椭圆被曲线切成两块"), QStringLiteral("fragments=%1").arg(fragments));
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
        ctx.check(!LineSplitOps::apply(&scene, QStringLiteral("线条分割")),
                  QStringLiteral("切刀不与填充图形相交时返回失败"));
        ctx.check(itemCount(scene) == before && undo.count() == undoCount, QStringLiteral("切空时不产生图元与撤销记录"),
                  QStringLiteral("items=%1 undo=%2").arg(itemCount(scene)).arg(undo.count()));
    }
}
