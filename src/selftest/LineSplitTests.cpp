#include "CanvasItem.h"
#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CanvasView.h"
#include "CompositeShapeItem.h"
#include "CurveItem.h"
#include "ItemDockPanel.h"
#include "LineSplitOps.h"
#include "MainWindow.h"
#include "PathRegions.h"
#include "ShapeItem.h"
#include "ShapePathBuilder.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"
#include "selftest/SelftestHelpers.h"

#include <QAction>
#include <QApplication>
#include <QPainterPath>
#include <QPointF>
#include <QToolButton>
#include <QUndoStack>
#include <QVector>
#include <QtMath>
#include <algorithm>
#include <limits>

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

    // 需求 23：真机流程复现——画笔曲线 + 矩形双选时，分割动作必须使能。
    {
        MainWindow window;
        window.show();
        QApplication::processEvents();
        window.insertShape(ShapeType::Rectangle);
        CanvasView* view = window.view();
        // 画一条横穿矩形本体的画笔曲线（起终点在矩形两侧之外）。
        const QRectF rectBounds = window.scene()->canvasItems().constFirst()->sceneContentRect();
        const QPointF crossStart = rectBounds.center() + QPointF(-rectBounds.width(), 0.0);
        view->setMode(ToolMode::Brush);
        const QPoint brushStart = view->mapFromScene(crossStart);
        sendMouseEvent(view->viewport(), QEvent::MouseButtonPress, brushStart, Qt::LeftButton, Qt::LeftButton);
        for (int i = 1; i <= 8; ++i) {
            const QPoint point(brushStart.x() + i * (rectBounds.width() / 4.0),
                               brushStart.y() + (i % 2 == 0 ? 12 : -10));
            sendMouseEvent(view->viewport(), QEvent::MouseMove, point, Qt::NoButton, Qt::LeftButton);
        }
        const QPoint brushEnd(brushStart.x() + int(rectBounds.width() * 2.0), brushStart.y() + 6);
        sendMouseEvent(view->viewport(), QEvent::MouseButtonRelease, brushEnd, Qt::LeftButton, Qt::NoButton);
        QApplication::processEvents();
        view->setMode(ToolMode::Edit);
        QApplication::processEvents();

        CanvasScene* mainScene = window.findChild<CanvasScene*>();
        mainScene->clearSelection();
        QApplication::processEvents();
        // 按真机操作路径选择：先点选矩形（取角落，避开横穿曲线的命中容差带），
        // 再 Ctrl+点选画笔曲线的顶点，两个图元必须同时处于选中状态。
        const QPoint rectClick = view->mapFromScene(QPointF(rectBounds.left() + 12.0, rectBounds.top() + 12.0));
        sendMouseEvent(view->viewport(), QEvent::MouseButtonPress, rectClick, Qt::LeftButton, Qt::LeftButton);
        sendMouseEvent(view->viewport(), QEvent::MouseButtonRelease, rectClick, Qt::LeftButton, Qt::NoButton);
        QApplication::processEvents();
        {
            const QList<CanvasItem*> selected = mainScene->selectedCanvasItems();
            ctx.check(selected.size() == 1 && dynamic_cast<ShapeItem*>(selected.value(0)) != nullptr,
                      QStringLiteral("点选矩形内部仅选中矩形"), QStringLiteral("size=%1").arg(selected.size()));
        }
        const QPointF curveScenePos = QPointF(crossStart.x() + rectBounds.width() / 2.0, crossStart.y() - 10.0);
        const QPoint curveClick = view->mapFromScene(curveScenePos);
        sendMouseEvent(view->viewport(), QEvent::MouseButtonPress, curveClick, Qt::LeftButton, Qt::LeftButton,
                       Qt::ControlModifier);
        QApplication::processEvents();
        {
            const QList<CanvasItem*> pressed = mainScene->selectedCanvasItems();
            ctx.check(pressed.size() == 2, QStringLiteral("Ctrl 按下即同时选中矩形与曲线"),
                      QStringLiteral("pressed=%1").arg(pressed.size()));
        }
        sendMouseEvent(view->viewport(), QEvent::MouseButtonRelease, curveClick, Qt::LeftButton, Qt::NoButton,
                       Qt::ControlModifier);
        QApplication::processEvents();
        {
            const QList<CanvasItem*> selected = mainScene->selectedCanvasItems();
            const bool hasShape = std::any_of(selected.cbegin(), selected.cend(), [](CanvasItem* item) {
                return dynamic_cast<ShapeItem*>(item) != nullptr;
            });
            const bool hasCurve = std::any_of(selected.cbegin(), selected.cend(), [](CanvasItem* item) {
                return dynamic_cast<CurveItem*>(item) != nullptr;
            });
            ctx.check(selected.size() == 2 && hasShape && hasCurve, QStringLiteral("松开后矩形与画笔曲线同时选中"),
                      QStringLiteral("size=%1").arg(selected.size()));
        }
        ctx.check(LineSplitOps::supports(mainScene), QStringLiteral("矩形加画笔曲线双选时分割判定可用"),
                  QStringLiteral("items=%1").arg(mainScene->canvasItemCount()));
        QAction* splitAction = nullptr;
        for (QAction* action : window.findChildren<QAction*>()) {
            if (action->property("lineSplitAction").toBool()) {
                splitAction = action;
            }
        }
        ctx.check(splitAction && splitAction->isEnabled(), QStringLiteral("分割动作在双选后使能"),
                  splitAction ? QStringLiteral("enabled=%1").arg(splitAction->isEnabled()) : QStringLiteral("missing"));
        // 再验证整条链路能真正执行切分：画笔曲线必须穿过矩形本体。
        ctx.check(LineSplitOps::apply(mainScene, QStringLiteral("分割")), QStringLiteral("画笔曲线可以切分矩形"));
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

    // 需求 25：左花括号分割波形旗帜时出现多余的奇怪色块。复现两种真机摆放：
    // A. 括号比旗面高、尖端探出旗面左缘；B. 按默认插入尺寸 42×128 与旗面
    // 128×86 中心层叠（真机默认插入即此摆放）。统计每块碎片的包围盒。
    const struct Arrangement {
        const char* label;
        QRectF flagRect;
        QRectF braceRect;
    } arrangements[] = {
        {"A-tip-outside", QRectF(300.0, 260.0, 128.0, 86.0), QRectF(280.0, 220.0, 110.0, 170.0)},
        {"B-default-cascade", QRectF(722.0, 487.0, 128.0, 86.0), QRectF(747.0, 448.0, 42.0, 128.0)},
    };
    for (const Arrangement& arrangement : arrangements) {
        CanvasScene scene;
        scene.applyCanvasSize(QSizeF(1536.0, 1024.0));
        scene.setMode(ToolMode::Edit);
        QUndoStack undo;
        scene.setUndoStack(&undo);

        auto* flag = new ShapeItem(ShapeType::FlagWave);
        flag->setContentSize(arrangement.flagRect.size());
        flag->setPos(arrangement.flagRect.topLeft());
        scene.addItemsWithUndo({flag}, QStringLiteral("Add flag"));

        auto* brace = new CurveItem();
        {
            const QPainterPath braceShape = ShapePathBuilder::bracePath(
                QRectF(0.0, 0.0, arrangement.braceRect.width(), arrangement.braceRect.height()));
            QVector<QPointF> points;
            points.reserve(49);
            for (int i = 0; i <= 48; ++i) {
                points.append(arrangement.braceRect.topLeft() + braceShape.pointAtPercent(qreal(i) / qreal(48)));
            }
            brace->setStrokePoints(points, QColor(QStringLiteral("#334155")), 4.0);
        }
        scene.addItemsWithUndo({brace}, QStringLiteral("Add brace"));

        scene.clearSelection();
        flag->setSelected(true);
        brace->setSelected(true);
        ctx.check(LineSplitOps::supports(&scene), QStringLiteral("波形旗加花括号双选时分割可用"));
        ctx.check(LineSplitOps::apply(&scene, QStringLiteral("分割")), QStringLiteral("花括号可以切分波形旗"));

        // 需求 25：布尔减法在条带与波面近切线处会留下面积趋近 0 的退化碎屑，
        // 之前单独成图元渲染成孤立的“奇怪色块”。修复后所有碎片都是真实碎块。
        int fragments = 0;
        qreal minArea = std::numeric_limits<qreal>::max();
        QString geometry;
        for (CanvasItem* item : scene.canvasItems()) {
            if (auto* fragment = dynamic_cast<CompositeShapeItem*>(item)) {
                ++fragments;
                const QPainterPath painted =
                    fragment->mapToScene(fragment->mapLayerPath(fragment->layers().constFirst().path));
                const qreal fillArea = PathRegions::area(painted);
                minArea = qMin(minArea, fillArea);
                const QRectF bounds = painted.boundingRect().normalized();
                geometry += QStringLiteral("[area=%1 bbox=%2x%3]")
                                .arg(QString::number(fillArea, 'f', 1))
                                .arg(QString::number(bounds.width(), 'f', 1))
                                .arg(QString::number(bounds.height(), 'f', 1));
            }
        }
        ctx.check(fragments >= 2, QStringLiteral("需求25-%1：花括号切出真实碎块").arg(arrangement.label),
                  QStringLiteral("fragments=%1 minArea=%2 %3").arg(fragments).arg(minArea, 0, 'f', 1).arg(geometry));
        ctx.check(minArea >= 16.0, QStringLiteral("需求25-%1：不再产生退化色块碎片").arg(arrangement.label),
                  QStringLiteral("minArea=%1").arg(minArea, 0, 'f', 1));
    }
}
