#include "BoolOps.h"
#include "CanvasScene.h"
#include "CanvasView.h"
#include "MainWindow.h"
#include "MergeRegions.h"
#include "PathRegions.h"
#include "PolygonItem.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"

#include <QAction>
#include <QImage>
#include <QMenu>
#include <QPainterPath>
#include <QToolButton>
#include <QUndoStack>
void selftest::RunBoolOpsSemanticTests(Context& ctx)
{
    // 布尔运算语义边界：组合拆分 / 联合取色 / 凹多边形 / 复合图层参与者 / 入口图标 / 拆分交界线。
    {
        // 组合（挖掉重叠）与拆分（切成互不重叠的多块）：
        // 两个 60×60 矩形横向错开 30，重叠区 30×60。
        auto runOperation = [](BoolOps::Operation operation, int& layerCount, qreal& area, int& itemCount) {
            QUndoStack undo;
            CanvasScene scene;
            scene.SetUndoStack(&undo);
            scene.SetMode(ToolMode::EDIT);
            scene.ApplyCanvasSize(QSizeF(400.0, 300.0));
            auto makeRect = [&scene](qreal x, const QColor& color) {
                auto* item = CreateShapeItem(ShapeType::RECTANGLE);
                item->setPos(QPointF(x, 40.0));
                item->SetContentSize(QSizeF(60.0, 60.0));
                item->SetStyle(color, color.lighter(160), 1.0);
                scene.AddItemsWithUndo({item}, QStringLiteral("Add bool item"));
                return item;
            };
            CanvasItem* lower = makeRect(20.0, QColor(200, 40, 40));
            CanvasItem* upper = makeRect(50.0, QColor(40, 90, 200));
            lower->setSelected(true);
            upper->setSelected(true);
            const bool ok = BoolOps::Apply(&scene, operation, QStringLiteral("bool"));
            layerCount = 0;
            area = 0.0;
            const QList<CanvasItem*> selected = scene.SelectedCanvasItems();
            itemCount = ok ? selected.size() : 0;
            for (CanvasItem* item : selected) {
                if (auto* composite = dynamic_cast<PolygonItem*>(item)) {
                    layerCount += composite->LayerCount();
                    if (area <= 0.0) {
                        area = composite->OutlineShape().boundingRect().width();
                    }
                }
            }
            return ok;
        };
        int combineLayers = 0;
        qreal combineWidth = 0.0;
        int combineItems = 0;
        const bool combineOk = runOperation(BoolOps::Operation::XOR, combineLayers, combineWidth, combineItems);
        ctx.Check(
            combineOk && combineLayers == 1 && combineItems == 1,
            QStringLiteral("异或运算生成单个单层结果，重叠区被挖空"),
            QStringLiteral("ok=%1 layers=%2 items=%3").arg(combineOk ? 1 : 0).arg(combineLayers).arg(combineItems));

        int fragmentLayers = 0;
        qreal fragmentWidth = 0.0;
        int fragmentItems = 0;
        const bool fragmentOk =
            runOperation(BoolOps::Operation::FRAGMENT, fragmentLayers, fragmentWidth, fragmentItems);
        ctx.Check(
            fragmentOk && fragmentItems == 3 && fragmentLayers == 3,
            QStringLiteral("拆分把重叠区切成三个可分别选中的多边形"),
            QStringLiteral("ok=%1 items=%2 layers=%3").arg(fragmentOk ? 1 : 0).arg(fragmentItems).arg(fragmentLayers));
    }
    {
        // 联合的重叠区必须整块取高优先级图元的颜色（含轮廓色），不留低层的描边。
        QUndoStack unionUndo;
        CanvasScene unionScene;
        unionScene.SetUndoStack(&unionUndo);
        unionScene.SetMode(ToolMode::EDIT);
        unionScene.ApplyCanvasSize(QSizeF(300.0, 200.0));
        const QColor lowerFill(30, 160, 90);
        const QColor lowerStroke(200, 40, 40);
        const QColor upperFill(40, 90, 200);
        const QColor upperStroke(40, 90, 200);
        auto makeRect = [&unionScene](qreal x, const QColor& fill, const QColor& stroke) {
            auto* item = CreateShapeItem(ShapeType::RECTANGLE);
            item->setPos(QPointF(x, 60.0));
            item->SetContentSize(QSizeF(80.0, 80.0));
            item->SetStyle(stroke, fill, 6.0);
            item->SetFill(fill, FillStyle::SOLID);
            unionScene.AddItemsWithUndo({item}, QStringLiteral("Add union item"));
            return item;
        };
        CanvasItem* lower = makeRect(40.0, lowerFill, lowerStroke);
        CanvasItem* upper = makeRect(80.0, upperFill, upperStroke);
        lower->setSelected(true);
        upper->setSelected(true);
        const bool unionOk = BoolOps::Apply(&unionScene, BoolOps::Operation::UNION, QStringLiteral("联合"));
        const QImage rendered = unionScene.RenderImage(QRectF(0.0, 0.0, 300.0, 200.0), 1.0);
        const QColor overlapPixel = rendered.pixelColor(90, 100); // 两块都覆盖
        const QColor lowerPixel = rendered.pixelColor(50, 100);   // 只有下层
        const QColor upperEdge = rendered.pixelColor(80, 100);    // 上层左边框，落在下层区域内
        ctx.Check(unionOk && overlapPixel == upperFill, QStringLiteral("联合后重叠区按高优先级图元的填充色显示"),
                  QStringLiteral("ok=%1 pixel=%2 expected=%3")
                      .arg(unionOk ? 1 : 0)
                      .arg(overlapPixel.name())
                      .arg(upperFill.name()));
        ctx.Check(lowerPixel == upperFill, QStringLiteral("联合后整体改用高优先级图元的填充色"),
                  QStringLiteral("pixel=%1 expected=%2").arg(lowerPixel.name()).arg(upperFill.name()));
        ctx.Check(upperEdge == upperFill || upperEdge == upperStroke,
                  QStringLiteral("重叠区内的轮廓改取高优先级图元的颜色"),
                  QStringLiteral("pixel=%1").arg(upperEdge.name()));
    }
    {
        // 平行四边形与五角星都是凹多边形：联合必须按完整轮廓取并集，不能只取可见的
        // 描线带；同时覆盖无填充 / 有填充组合和五种布尔方式。
        struct PairResult {
            bool ok = false;
            QImage image;
            QPainterPath path;
            QPointF overlapPoint;
            int items = 0;
            int layers = 0;
            FillStyle fillStyle = FillStyle::NONE;
            qreal strokeWidth = 0.0;
        };
        const QPointF diamondCenter(170.0, 160.0);
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
            scene.SetUndoStack(&undo);
            scene.SetMode(ToolMode::EDIT);
            scene.ApplyCanvasSize(QSizeF(420.0, 320.0));
            scene.SetShowGrid(false);
            CanvasBackground background;
            background.type = BackgroundType::COLOR;
            background.color = QColor(Qt::white);
            scene.SetCanvasBackground(background);

            auto makeShape = [&scene](ShapeType type, QPointF pos, const QColor& fill, bool filled) {
                auto* shape = CreateShapeItem(type);
                shape->setPos(pos);
                shape->SetContentSize(QSizeF(180.0, 180.0));
                shape->SetStyle(QColor(20, 20, 20), filled ? fill : QColor(Qt::transparent), 8.0);
                shape->SetFill(filled ? fill : QColor(Qt::transparent), filled ? FillStyle::SOLID : FillStyle::NONE);
                scene.AddItemsWithUndo({shape}, QStringLiteral("Add pair shape"));
                return shape;
            };
            auto* lower = makeShape(ShapeType::PARALLELOGRAM, QPointF(100.0, 70.0), diamondFill, diamondFilled);
            auto* upper = makeShape(ShapeType::STAR, QPointF(180.0, 60.0), starFill, starFilled);
            const QPainterPath lowerOutline = lower->mapToScene(lower->OutlineShape());
            const QPainterPath upperOutline = upper->mapToScene(upper->OutlineShape());
            result.overlapPoint = interiorPoint(lowerOutline.intersected(upperOutline));
            lower->setSelected(true);
            upper->setSelected(true);
            result.ok = BoolOps::Apply(&scene, operation, QStringLiteral("Pair operation"));
            result.image = scene.RenderImage(QRectF(0.0, 0.0, 420.0, 320.0), 1.0);

            const QList<CanvasItem*> selected = scene.SelectedCanvasItems();
            result.items = selected.size();
            for (CanvasItem* item : selected) {
                if (auto* composite = dynamic_cast<PolygonItem*>(item)) {
                    const QPainterPath outline = composite->mapToScene(composite->OutlineShape());
                    result.path = result.path.isEmpty() ? outline : result.path.united(outline);
                    result.layers += composite->LayerCount();
                    result.fillStyle = composite->fillStyle();
                    result.strokeWidth = composite->StrokeWidth();
                }
            }
            return result;
        };

        const PairResult unionBoth = runPair(BoolOps::Operation::UNION, true, true);
        ctx.Check(unionBoth.ok && unionBoth.path.contains(diamondCenter) && unionBoth.path.contains(starCenter) &&
                      unionBoth.image.pixelColor(diamondCenter.toPoint()) == starFill &&
                      unionBoth.image.pixelColor(starCenter.toPoint()) == starFill,
                  QStringLiteral("平行四边形与五角星联合保留完整轮廓并统一顶层填充"),
                  QStringLiteral("ok=%1 diamond=%2 star=%3")
                      .arg(unionBoth.ok ? 1 : 0)
                      .arg(unionBoth.path.contains(diamondCenter) ? 1 : 0)
                      .arg(unionBoth.path.contains(starCenter) ? 1 : 0));

        const PairResult hollowDiamondUnion = runPair(BoolOps::Operation::UNION, false, true);
        ctx.Check(hollowDiamondUnion.ok && hollowDiamondUnion.path.contains(diamondCenter) &&
                      hollowDiamondUnion.image.pixelColor(diamondCenter.toPoint()) == starFill,
                  QStringLiteral("无填充平行四边形与填充五角星联合仍按完整轮廓取并集"),
                  QStringLiteral("ok=%1 contains=%2 pixel=%3")
                      .arg(hollowDiamondUnion.ok ? 1 : 0)
                      .arg(hollowDiamondUnion.path.contains(diamondCenter) ? 1 : 0)
                      .arg(hollowDiamondUnion.image.pixelColor(diamondCenter.toPoint()).name()));

        const PairResult hollowUnion = runPair(BoolOps::Operation::UNION, false, false);
        ctx.Check(hollowUnion.ok && hollowUnion.path.contains(diamondCenter) && hollowUnion.path.contains(starCenter) &&
                      hollowUnion.layers == 1 && hollowUnion.fillStyle == FillStyle::NONE &&
                      hollowUnion.strokeWidth > 0.0,
                  QStringLiteral("两个无填充图形联合得到单层闭合轮廓，不保留内部交界"),
                  QStringLiteral("ok=%1 layers=%2 contains=%3,%4")
                      .arg(hollowUnion.ok ? 1 : 0)
                      .arg(hollowUnion.layers)
                      .arg(hollowUnion.path.contains(diamondCenter) ? 1 : 0)
                      .arg(hollowUnion.path.contains(starCenter) ? 1 : 0));

        const PairResult combine = runPair(BoolOps::Operation::XOR, true, true);
        ctx.Check(combine.ok && combine.path.contains(diamondCenter) && combine.path.contains(starCenter) &&
                      !combine.path.contains(combine.overlapPoint),
                  QStringLiteral("平行四边形与五角星异或后重叠区被挖空"),
                  QStringLiteral("ok=%1 overlap=%2")
                      .arg(combine.ok ? 1 : 0)
                      .arg(combine.path.contains(combine.overlapPoint) ? 1 : 0));

        const PairResult intersect = runPair(BoolOps::Operation::INTERSECT, true, true);
        ctx.Check(intersect.ok && intersect.path.contains(intersect.overlapPoint) &&
                      !intersect.path.contains(diamondCenter) && !intersect.path.contains(starCenter),
                  QStringLiteral("平行四边形与五角星相交只保留重叠区"),
                  QStringLiteral("ok=%1 overlap=%2 diamond=%3 star=%4")
                      .arg(intersect.ok ? 1 : 0)
                      .arg(intersect.path.contains(intersect.overlapPoint) ? 1 : 0)
                      .arg(intersect.path.contains(diamondCenter) ? 1 : 0)
                      .arg(intersect.path.contains(starCenter) ? 1 : 0));

        const PairResult subtract = runPair(BoolOps::Operation::SUBTRACT, true, true);
        ctx.Check(subtract.ok && subtract.path.contains(diamondCenter) && !subtract.path.contains(starCenter) &&
                      !subtract.path.contains(subtract.overlapPoint),
                  QStringLiteral("平行四边形与五角星剪除以先选中的平行四边形为底，挖掉五角星"),
                  QStringLiteral("ok=%1 diamond=%2 star=%3 overlap=%4")
                      .arg(subtract.ok ? 1 : 0)
                      .arg(subtract.path.contains(diamondCenter) ? 1 : 0)
                      .arg(subtract.path.contains(starCenter) ? 1 : 0)
                      .arg(subtract.path.contains(subtract.overlapPoint) ? 1 : 0));

        const PairResult fragment = runPair(BoolOps::Operation::FRAGMENT, true, true);
        ctx.Check(fragment.ok && fragment.items == 3 && fragment.layers == 3 && fragment.path.contains(diamondCenter) &&
                      fragment.path.contains(starCenter),
                  QStringLiteral("平行四边形与五角星拆分得到三块可分别选中的多边形"),
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
        compositeScene.SetUndoStack(&compositeUndo);
        compositeScene.SetMode(ToolMode::EDIT);
        compositeScene.ApplyCanvasSize(QSizeF(460.0, 320.0));
        compositeScene.SetShowGrid(false);
        CanvasBackground compositeBackground;
        compositeBackground.type = BackgroundType::COLOR;
        compositeBackground.color = QColor(Qt::white);
        compositeScene.SetCanvasBackground(compositeBackground);

        auto makeShape = [&compositeScene](ShapeType type, QPointF pos, QColor fill) {
            auto* shape = CreateShapeItem(type);
            shape->setPos(pos);
            shape->SetContentSize(QSizeF(140.0, 140.0));
            shape->SetStyle(fill, fill, 4.0);
            shape->SetFill(fill, FillStyle::SOLID);
            compositeScene.AddItemsWithUndo({shape}, QStringLiteral("Add composite source"));
            return shape;
        };
        auto* rectangle = makeShape(ShapeType::RECTANGLE, QPointF(40.0, 60.0), QColor(30, 160, 90));
        auto* diamond = makeShape(ShapeType::PARALLELOGRAM, QPointF(100.0, 100.0), QColor(230, 140, 40));
        rectangle->setSelected(true);
        diamond->setSelected(true);
        const bool firstUnion =
            BoolOps::Apply(&compositeScene, BoolOps::Operation::UNION, QStringLiteral("First union"));
        const QList<CanvasItem*> firstSelection = compositeScene.SelectedCanvasItems();
        auto* polygon = firstSelection.isEmpty() ? nullptr : dynamic_cast<PolygonItem*>(firstSelection.constFirst());
        auto* star = makeShape(ShapeType::STAR, QPointF(260.0, 90.0), QColor(40, 90, 200));
        if (polygon) {
            polygon->setSelected(true);
        }
        star->setSelected(true);
        const bool secondUnion =
            BoolOps::Apply(&compositeScene, BoolOps::Operation::UNION, QStringLiteral("Second union"));
        const QList<CanvasItem*> secondSelection = compositeScene.SelectedCanvasItems();
        auto* result = secondSelection.isEmpty() ? nullptr : secondSelection.constFirst();
        const QPainterPath resultPath = result ? result->mapToScene(result->OutlineShape()) : QPainterPath();
        ctx.Check(firstUnion && polygon && secondUnion && result && resultPath.contains(QPointF(80.0, 100.0)) &&
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
        fallbackScene.SetUndoStack(&fallbackUndo);
        fallbackScene.SetMode(ToolMode::EDIT);
        fallbackScene.ApplyCanvasSize(QSizeF(300.0, 200.0));
        const QColor lowerFill(30, 160, 90);
        const QColor upperFill(40, 90, 200);
        const QColor upperStroke(220, 40, 40);
        auto makeRect = [&fallbackScene](qreal x, const QColor& fill, const QColor& stroke, qreal width) {
            auto* item = CreateShapeItem(ShapeType::RECTANGLE);
            item->setPos(QPointF(x, 60.0));
            item->SetContentSize(QSizeF(80.0, 80.0));
            item->SetStyle(stroke, fill, width);
            item->SetFill(fill, FillStyle::SOLID);
            fallbackScene.AddItemsWithUndo({item}, QStringLiteral("Add fallback item"));
            return item;
        };
        CanvasItem* lower = makeRect(40.0, lowerFill, lowerFill, 1.0);
        CanvasItem* upper =
            makeRect(80.0, QColor(upperFill.red(), upperFill.green(), upperFill.blue(), 0), upperStroke, 6.0);
        lower->setSelected(true);
        upper->setSelected(true);
        const bool fallbackOk = BoolOps::Apply(&fallbackScene, BoolOps::Operation::UNION, QStringLiteral("联合"));
        const QImage rendered = fallbackScene.RenderImage(QRectF(0.0, 0.0, 300.0, 200.0), 1.0);
        const QColor overlapPixel = rendered.pixelColor(90, 100);
        ctx.Check(fallbackOk && overlapPixel.alpha() == 0, QStringLiteral("donor 填充透明时重叠区保持透明"),
                  QStringLiteral("ok=%1 pixel=%2 alpha=%3")
                      .arg(fallbackOk ? 1 : 0)
                      .arg(overlapPixel.name())
                      .arg(overlapPixel.alpha()));
        const QColor internalEdge = rendered.pixelColor(81, 100); // 上层左边框，落在下层内部
        ctx.Check(internalEdge != upperStroke, QStringLiteral("重叠区内部的交界轮廓被填充色同化"),
                  QStringLiteral("pixel=%1").arg(internalEdge.name()));
    }
    {
        // 整组作为一个参与者：组内两个方块先并成一条轮廓再参与相减，
        // 不会因为最高优先级组员被挖空就把整组结果判成空。
        QUndoStack groupBoolUndo;
        CanvasScene groupBoolScene;
        groupBoolScene.SetUndoStack(&groupBoolUndo);
        groupBoolScene.SetMode(ToolMode::EDIT);
        groupBoolScene.ApplyCanvasSize(QSizeF(300.0, 200.0));
        auto makeRect = [&groupBoolScene](qreal x, const QColor& color) {
            auto* item = CreateShapeItem(ShapeType::RECTANGLE);
            item->setPos(QPointF(x, 100.0));
            item->SetContentSize(QSizeF(40.0, 40.0));
            item->SetStyle(color, color, 2.0);
            item->SetFill(color, FillStyle::SOLID);
            groupBoolScene.AddItemsWithUndo({item}, QStringLiteral("Add grouped bool item"));
            return item;
        };
        CanvasItem* outside = makeRect(120.0, QColor(40, 90, 200));
        CanvasItem* groupLow = makeRect(10.0, QColor(220, 60, 60));
        CanvasItem* groupHigh = makeRect(100.0, QColor(30, 160, 90));
        groupLow->setSelected(true);
        groupHigh->setSelected(true);
        groupBoolScene.GroupSelectedItems();
        groupBoolScene.clearSelection();
        groupBoolScene.SelectGroupOfItem(groupLow);
        outside->setSelected(true);

        const bool groupedSubtract =
            BoolOps::Apply(&groupBoolScene, BoolOps::Operation::SUBTRACT, QStringLiteral("相减"));
        const QList<CanvasItem*> groupedSelection = groupBoolScene.SelectedCanvasItems();
        QRectF groupedExtent;
        for (CanvasItem* item : std::as_const(groupedSelection)) {
            groupedExtent =
                groupedExtent.isNull() ? item->sceneBoundingRect() : groupedExtent.united(item->sceneBoundingRect());
        }
        auto* groupedResult =
            groupedSelection.isEmpty() ? nullptr : dynamic_cast<PolygonItem*>(groupedSelection.constFirst());
        ctx.Check(groupedSubtract && groupedResult && groupedExtent.width() > 100.0,
                  QStringLiteral("整组作为一个参与者参与合并"),
                  QStringLiteral("ok=%1 width=%2 pieces=%3")
                      .arg(groupedSubtract ? 1 : 0)
                      .arg(groupedExtent.width())
                      .arg(groupedSelection.size()));
        ctx.Check(groupedResult && !groupedResult->Layers().isEmpty() &&
                      groupedResult->Layers().constFirst().fill == QColor(30, 160, 90),
                  QStringLiteral("整组结果采用组内最高优先级图元的样式"));
    }
    {
        // 合并图元仍然能从菜单、右键菜单和编辑面板进入，但主工具栏不再放合并图元按钮。
        MainWindow boolWindow;
        QToolButton* boolButton = boolWindow.findChild<QToolButton*>(QStringLiteral("BoolToolButton"));
        QMenu* boolMenu = boolWindow.findChild<QMenu*>(QStringLiteral("BoolMenu"));
        CanvasView* boolView = boolWindow.findChild<CanvasView*>();
        ctx.Check(boolMenu && boolMenu->actions().size() == 6, QStringLiteral("编辑菜单提供 6 项合并图元操作"),
                  boolMenu ? QString::number(boolMenu->actions().size()) : QString());
        ctx.Check(!boolButton, QStringLiteral("工具栏不再显示合并图元按钮"));
        ctx.Check(boolView && boolView->ContextMenuGroupCount() >= 1, QStringLiteral("画布右键菜单挂上了合并图元分组"),
                  boolView ? QString::number(boolView->ContextMenuGroupCount()) : QString());
        bool iconsOk = boolMenu && boolMenu->actions().size() == 6;
        if (iconsOk) {
            for (QAction* action : boolMenu->actions()) {
                if (action->icon().isNull()) {
                    iconsOk = false;
                    break;
                }
            }
        }
        ctx.Check(iconsOk, QStringLiteral("合并图元六项操作都带参照 PPT 的图标"));
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
        ctx.Check(morphIconDistinct, QStringLiteral("合并图元主入口图标与六项子操作图标不同"));
        QAction* heightUp = boolWindow.findChild<QAction*>(QStringLiteral("HeightUpAction"));
        QAction* heightDown = boolWindow.findChild<QAction*>(QStringLiteral("HeightDownAction"));
        QMenu* shapeMenu = boolWindow.findChild<QMenu*>(QStringLiteral("ShapeMenu"));
        ctx.Check(heightUp && heightDown && !heightUp->icon().isNull() && !heightDown->icon().isNull() && boolMenu &&
                      !boolMenu->icon().isNull() && shapeMenu && !shapeMenu->icon().isNull(),
                  QStringLiteral("菜单栏为已有图标的同类功能补齐图标"));
        const QString polyText = UiText(QStringLiteral("Polygon"));
        const QString curveText = UiText(QStringLiteral("Curve"));
        const bool named = polyText != QStringLiteral("Polygon") && curveText != QStringLiteral("Curve");
        ctx.Check(named, QStringLiteral("多边形的类型名有中文译文"), polyText + QStringLiteral(" / ") + curveText);
    }
    {
        // 拆分结果必须在块间保留交界线，不能渲染成联合的样子。
        const QColor fillA(200, 40, 40);
        const QColor strokeA(250, 200, 40);
        const QColor fillB(40, 90, 200);
        const QColor strokeB(40, 200, 120);
        auto renderOp = [&](BoolOps::Operation op, QUndoStack* undo, CanvasScene* scene) {
            scene->SetUndoStack(undo);
            scene->SetMode(ToolMode::EDIT);
            scene->ApplyCanvasSize(QSizeF(300.0, 200.0));
            auto makeRect = [scene](qreal x, const QColor& fill, const QColor& stroke) {
                auto* item = CreateShapeItem(ShapeType::RECTANGLE);
                item->setPos(QPointF(x, 40.0));
                item->SetContentSize(QSizeF(60.0, 60.0));
                item->SetStyle(stroke, fill, 2.0);
                item->SetFill(fill, FillStyle::SOLID);
                scene->AddItemsWithUndo({item}, QStringLiteral("Add split item"));
                return item;
            };
            CanvasItem* lower = makeRect(20.0, fillA, strokeA);
            CanvasItem* upper = makeRect(50.0, fillB, strokeB);
            lower->setSelected(true);
            upper->setSelected(true);
            return BoolOps::Apply(scene, op, QStringLiteral("split"));
        };
        QUndoStack unionSplitUndo;
        QUndoStack fragmentUndo;
        CanvasScene unionSplitScene;
        CanvasScene fragmentScene;
        const bool unionSplitOk = renderOp(BoolOps::Operation::UNION, &unionSplitUndo, &unionSplitScene);
        const bool fragmentOk = renderOp(BoolOps::Operation::FRAGMENT, &fragmentUndo, &fragmentScene);
        const QImage unionImage = unionSplitScene.RenderImage(QRectF(0.0, 0.0, 300.0, 200.0), 1.0);
        const QImage fragmentImage = fragmentScene.RenderImage(QRectF(0.0, 0.0, 300.0, 200.0), 1.0);
        int differingPixels = 0;
        for (int y = 40; y < 100; ++y) {
            for (int x = 20; x < 110; ++x) {
                if (unionImage.pixelColor(x, y) != fragmentImage.pixelColor(x, y)) {
                    ++differingPixels;
                }
            }
        }
        ctx.Check(unionSplitOk && fragmentOk && differingPixels > 10,
                  QStringLiteral("拆分在块间保留内部交界线，与联合结果不同"),
                  QStringLiteral("diff=%1").arg(differingPixels));
    }
    {
        // §5.1/§5.2：拆分连续块全覆盖；角点接触不算连续。
        auto rectPath = [](qreal x, qreal y, qreal w, qreal h) {
            QPainterPath path;
            path.addRect(QRectF(x, y, w, h));
            return path;
        };
        // 两块互不相连的矩形：拆成两块，并集面积等于两块面积之和，不丢面积。
        const QPainterPath disjoint = rectPath(0, 0, 40, 40).united(rectPath(100, 0, 40, 40));
        const QList<QPainterPath> disjointPieces = PathRegions::SplitDisjoint(disjoint);
        qreal disjointArea = 0.0;
        for (const QPainterPath& piece : disjointPieces) {
            disjointArea += PathRegions::Area(piece);
        }
        ctx.Check(disjointPieces.size() == 2 && qAbs(disjointArea - 3200.0) < 1.0,
                  QStringLiteral("互不相连区域拆成两块且全覆盖"),
                  QStringLiteral("pieces=%1 area=%2").arg(disjointPieces.size()).arg(disjointArea, 0, 'f', 2));

        // 仅角点接触的两个正方形：不算连续，保持两块独立。
        const QPainterPath cornerTouch = rectPath(0, 0, 30, 30).united(rectPath(30, 30, 30, 30));
        const QList<QPainterPath> cornerPieces = PathRegions::SplitDisjoint(cornerTouch);
        ctx.Check(cornerPieces.size() == 2 &&
                      !PathRegions::RegionsContinuous(rectPath(0, 0, 30, 30), rectPath(30, 30, 30, 30)),
                  QStringLiteral("角点接触不算连续，保持两块独立"),
                  QStringLiteral("pieces=%1").arg(cornerPieces.size()));

        // 共享一条边的两个矩形：判为连续。
        ctx.Check(PathRegions::RegionsContinuous(rectPath(0, 0, 30, 30), rectPath(30, 0, 30, 30)),
                  QStringLiteral("共享边的两块判为连续"));

        // 带孔洞的矩形：孔洞保留在同一块内，不被拆成独立块。
        QPainterPath withHole;
        withHole.addRect(QRectF(0, 0, 60, 60));
        withHole.addRect(QRectF(20, 20, 20, 20));
        withHole.setFillRule(Qt::OddEvenFill);
        const QList<QPainterPath> holePieces = PathRegions::SplitDisjoint(withHole);
        ctx.Check(holePieces.size() == 1 && qAbs(PathRegions::Area(holePieces.constFirst()) - 3200.0) < 1.0,
                  QStringLiteral("孔洞保留在同一连续块内"),
                  QStringLiteral("pieces=%1 area=%2")
                      .arg(holePieces.size())
                      .arg(holePieces.isEmpty() ? 0.0 : PathRegions::Area(holePieces.constFirst()), 0, 'f', 2));
    }
    {
        // §5.3：极小碎块先吸收后删除；D2 豁免块不进入该流程。
        auto makeBlock = [](const QPainterPath& path, int source = 0, bool exempt = false) {
            MergeRegions::RegionBlock block;
            block.path = path;
            block.area = PathRegions::Area(path);
            block.sourceIndex = source;
            block.exempt = exempt;
            return block;
        };
        auto rectPath = [](qreal x, qreal y, qreal w, qreal h) {
            QPainterPath path;
            path.addRect(QRectF(x, y, w, h));
            return path;
        };
        // 大宿主 + 共边小碎块（3×3=9 < 16）：碎块被吸收，结果一块，面积不丢。
        QList<MergeRegions::RegionBlock> absorbed;
        absorbed.append(makeBlock(rectPath(0, 0, 40, 40)));
        absorbed.append(makeBlock(rectPath(40, 0, 3, 3)));
        absorbed = MergeRegions::AbsorbTinyFragments(absorbed);
        qreal absorbedArea = 0.0;
        for (const MergeRegions::RegionBlock& block : absorbed) {
            absorbedArea += block.area;
        }
        ctx.Check(absorbed.size() == 1 && qAbs(absorbedArea - 1609.0) < 1.0, QStringLiteral("共边极小碎块先并入宿主"),
                  QStringLiteral("blocks=%1 area=%2").arg(absorbed.size()).arg(absorbedArea, 0, 'f', 2));

        // 孤立小碎块（无合法宿主）：删除，宿主保留。
        QList<MergeRegions::RegionBlock> orphan;
        orphan.append(makeBlock(rectPath(0, 0, 40, 40)));
        orphan.append(makeBlock(rectPath(200, 200, 3, 3)));
        orphan = MergeRegions::AbsorbTinyFragments(orphan);
        ctx.Check(orphan.size() == 1 && PathRegions::Area(orphan.constFirst().path) > 16.0,
                  QStringLiteral("无宿主的极小碎块删除，宿主保留"), QStringLiteral("blocks=%1").arg(orphan.size()));

        // D2 豁免块：即使小于阈值也原样保留。
        QList<MergeRegions::RegionBlock> exempt;
        exempt.append(makeBlock(rectPath(0, 0, 40, 40)));
        exempt.append(makeBlock(rectPath(200, 200, 3, 3), 0, true));
        exempt = MergeRegions::AbsorbTinyFragments(exempt);
        ctx.Check(exempt.size() == 2, QStringLiteral("原输入小图元豁免碎块规则"));
    }
    {
        // §5.2：允许掩膜校验——最终填充落在掩膜外即不通过，提交前拦截。
        QPainterPath mask;
        mask.addRect(QRectF(0, 0, 60, 60));
        QPainterPath inside;
        inside.addRect(QRectF(10, 10, 20, 20));
        QPainterPath outside;
        outside.addRect(QRectF(70, 10, 20, 20));
        ctx.Check(MergeRegions::FillWithinMask(inside, mask), QStringLiteral("掩膜内填充通过允许区域校验"));
        ctx.Check(!MergeRegions::FillWithinMask(outside, mask), QStringLiteral("超出掩膜的填充不通过允许区域校验"));
    }
}
