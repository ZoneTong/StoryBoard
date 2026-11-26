
#include "canvas/controllers/CanvasItemInteraction.h"
#include "items/lines/CurveItem.h"
#include "items/lines/StraightLineItem.h"
#include "items/shapes/ShapeFactory.h"
#include "items/text/TextItem.h"
#include "scene/CanvasScene.h"
#include "scene/Commands.h"
#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"

#include <QApplication>
#include <QColor>
#include <QFontMetricsF>
#include <QGraphicsSceneMouseEvent>
#include <QJsonObject>
#include <QLineF>
#include <QPointF>
#include <QStringList>
#include <QUndoStack>
#include <QVariant>

void selftest::RunBindingBehaviorTests(Context& ctx)
{
    // 箭头端点跟随：目标矩形移动后，绑定的起点仍贴在矩形左边缘并留 gap。
    {
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        scene.SetMode(ToolMode::EDIT);
        QUndoStack undo;
        scene.SetUndoStack(&undo);

        auto* rect = ShapeFactory::Create(ShapeType::RECTANGLE);
        rect->SetContentSize(QSizeF(160.0, 120.0));
        rect->setPos(QPointF(200.0, 150.0));
        scene.AddItemsWithUndo({rect}, QStringLiteral("Add rect"));

        auto* line = new StraightLineItem();
        line->SetEndpoints(QPointF(0.0, 210.0), QPointF(180.0, 210.0), QColor(QStringLiteral("#1E293B")), 2.0);
        QJsonObject binding;
        binding.insert(QStringLiteral("elementId"), rect->Uid());
        binding.insert(QStringLiteral("gap"), 4.0);
        line->SetStartBinding(binding);
        scene.AddItemsWithUndo({line}, QStringLiteral("Add arrow"));

        scene.SyncArrowBindings();
        QPointF start = line->mapToScene(line->StrokePoints().first());
        ctx.Check(qAbs(start.x() - 196.0) < 1.0 && qAbs(start.y() - 210.0) < 1.0,
                  QStringLiteral("绑定箭头起点贴住目标左边缘并留 gap"),
                  QStringLiteral("start=(%1,%2)").arg(start.x()).arg(start.y()));

        rect->setPos(QPointF(300.0, 150.0));
        scene.SyncArrowBindings();
        start = line->mapToScene(line->StrokePoints().first());
        ctx.Check(qAbs(start.x() - 296.0) < 1.0 && qAbs(start.y() - 210.0) < 1.0,
                  QStringLiteral("目标移动后绑定箭头端点跟随"),
                  QStringLiteral("start=(%1,%2)").arg(start.x()).arg(start.y()));

        ctx.Check(scene.BindableShapeAt(QPointF(350.0, 210.0), 12.0) == rect &&
                      scene.BindableShapeAt(QPointF(290.0, 210.0), 12.0) == rect,
                  QStringLiteral("图形内与 12px 贴边内都可吸附"),
                  QStringLiteral("inside/outside"));
        ctx.Check(scene.BindableShapeAt(QPointF(150.0, 210.0), 12.0) == nullptr,
                  QStringLiteral("离边缘超过 12px 不吸附"), QStringLiteral("far"));

        const QRectF targetRect = rect->mapRectToScene(rect->ContentRect());
        const QPointF edgePoint(targetRect.left(), 210.0);
        const QPointF edgeAttached = CanvasScene::AttachmentPoint(rect, edgePoint, 4.0);
        const qreal focus = CanvasScene::OutlineFocusForPoint(rect, edgeAttached);
        const QPointF restored = CanvasScene::AttachmentPoint(rect, QPointF(500.0, 210.0), 4.0, focus);
        ctx.Check(QLineF(restored, edgeAttached).length() < 1.0 && edgeAttached.x() < targetRect.left(),
                  QStringLiteral("轮廓 focus 与场景点可互算并用于绑定落点"),
                  QStringLiteral("focus=%1 point=(%2,%3)")
                      .arg(focus)
                      .arg(restored.x())
                      .arg(restored.y()));

        // 右下方 / 左下方斜向靠近：贴到真实轮廓上离落点最近的角点，落点不能跑到矩形外面。
        const QPointF bottomRight = CanvasScene::AttachmentPoint(rect, QPointF(500.0, 320.0), 4.0);
        const QPointF bottomLeft = CanvasScene::AttachmentPoint(rect, QPointF(200.0, 320.0), 4.0);
        const QPointF outBottomRight = targetRect.bottomRight() + QPointF(0.8, 0.6) * 4.0;
        const QPointF outBottomLeft = targetRect.bottomLeft() + QPointF(-0.8, 0.6) * 4.0;
        ctx.Check(QLineF(bottomRight, outBottomRight).length() < 1.0 &&
                      QLineF(bottomLeft, outBottomLeft).length() < 1.0,
                  QStringLiteral("斜向靠近矩形时端点吸附到四角"),
                  QStringLiteral("br=(%1,%2) bl=(%3,%4)")
                      .arg(bottomRight.x())
                      .arg(bottomRight.y())
                      .arg(bottomLeft.x())
                      .arg(bottomLeft.y()));

        // 其他图形按真实轮廓吸附：菱形顶点、椭圆弧上的点都不再用外接矩形边中点。
        auto* diamond = ShapeFactory::Create(ShapeType::DIAMOND);
        diamond->SetContentSize(QSizeF(160.0, 120.0));
        diamond->setPos(QPointF(560.0, 150.0));
        const QPointF diamondCorner = CanvasScene::AttachmentPoint(diamond, QPointF(750.0, 210.0), 4.0);
        const QRectF diamondRect = diamond->mapRectToScene(diamond->ContentRect());
        ctx.Check(qAbs(diamondCorner.x() - (diamondRect.right() + 4.0)) < 1.0 &&
                      qAbs(diamondCorner.y() - diamondRect.center().y()) < 1.0,
                  QStringLiteral("菱形吸附到侧向顶点而不是外接矩形边中点"),
                  QStringLiteral("corner=(%1,%2) right=%3")
                      .arg(diamondCorner.x())
                      .arg(diamondCorner.y())
                      .arg(diamondRect.right()));

        auto* ellipse = ShapeFactory::Create(ShapeType::ELLIPSE);
        ellipse->SetContentSize(QSizeF(160.0, 120.0));
        ellipse->setPos(QPointF(560.0, 330.0));
        const QPointF ellipseEdge = CanvasScene::AttachmentPoint(ellipse, QPointF(760.0, 390.0), 4.0);
        const QRectF ellipseRect = ellipse->mapRectToScene(ellipse->ContentRect());
        ctx.Check(
            qAbs(ellipseEdge.x() - (ellipseRect.right() + 4.0)) < 1.0 &&
                qAbs(ellipseEdge.y() - ellipseRect.center().y()) < 1.0,
            QStringLiteral("椭圆吸附到弧线轮廓最右点"),
            QStringLiteral("edge=(%1,%2) right=%3").arg(ellipseEdge.x()).arg(ellipseEdge.y()).arg(ellipseRect.right()));

        // 特征点附近自动校准：矩形边中点 8px 内吸到中点；偏离中点的普通边位置保持原投影；
        // 靠近角点的边位置校准到角点。
        const QPointF nearMiddle = CanvasScene::AttachmentPoint(rect, QPointF(388.0, 330.0), 4.0);
        const QPointF plainEdge = CanvasScene::AttachmentPoint(rect, QPointF(410.0, 330.0), 4.0);
        const QPointF plainBase(410.0, targetRect.bottom());
        const QPointF plainOutward = plainBase - targetRect.center();
        const QPointF plainExpected =
            plainBase +
            plainOutward / qSqrt(plainOutward.x() * plainOutward.x() + plainOutward.y() * plainOutward.y()) * 4.0;
        const QPointF nearCorner = CanvasScene::AttachmentPoint(rect, QPointF(452.0, 330.0), 4.0);
        const QPointF bottomMiddle(targetRect.center().x(), targetRect.bottom() + 4.0);
        ctx.Check(QLineF(nearMiddle, bottomMiddle).length() < 1.0 && QLineF(plainEdge, plainExpected).length() < 1.0 &&
                      QLineF(nearCorner, outBottomRight).length() < 1.0,
                  QStringLiteral("边中点附近校准中点、普通边位置保持、角点附近校准角点"),
                  QStringLiteral("nearMiddle=(%1,%2) plain=(%3,%4) corner=(%5,%6)")
                      .arg(nearMiddle.x())
                      .arg(nearMiddle.y())
                      .arg(plainEdge.x())
                      .arg(plainEdge.y())
                      .arg(nearCorner.x())
                      .arg(nearCorner.y()));
        const QPointF ellipsePlain = CanvasScene::AttachmentPoint(ellipse, QPointF(755.0, 335.0), 4.0);
        ctx.Check(qAbs(ellipsePlain.y() - ellipseRect.center().y()) > 5.0 && ellipsePlain.x() < ellipseRect.right() &&
                      ellipsePlain.x() > ellipseRect.center().x(),
                  QStringLiteral("椭圆远离极点时吸附任意弧线位置"),
                  QStringLiteral("plain=(%1,%2) centerY=%3")
                      .arg(ellipsePlain.x())
                      .arg(ellipsePlain.y())
                      .arg(ellipseRect.center().y()));
        delete diamond;
        delete ellipse;

        scene.clearSelection();
        rect->setSelected(true);
        scene.DeleteSelected();
        ctx.Check(scene.CanvasItemCount() == 1 && line->StartBinding().isEmpty(),
                  QStringLiteral("删除目标图形时清空箭头绑定并保留箭头"),
                  QStringLiteral("count=%1 binding=%2")
                      .arg(scene.CanvasItemCount())
                      .arg(line->StartBinding().isEmpty() ? QStringLiteral("empty") : QStringLiteral("set")));
        undo.undo();
        ctx.Check(scene.CanvasItemCount() == 2 && !line->StartBinding().isEmpty(),
                  QStringLiteral("撤销删除后图形与绑定一起恢复"),
                  QStringLiteral("count=%1 binding=%2")
                      .arg(scene.CanvasItemCount())
                      .arg(line->StartBinding().isEmpty() ? QStringLiteral("empty") : QStringLiteral("set")));
    }

    // 撤销 / 重做目标移动后，绑定箭头由场景在栈位置变化时重算，不会停在旧位置。
    {
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        scene.SetMode(ToolMode::EDIT);
        QUndoStack undo;
        scene.SetUndoStack(&undo);

        auto* rect = ShapeFactory::Create(ShapeType::RECTANGLE);
        rect->SetContentSize(QSizeF(160.0, 120.0));
        rect->setPos(QPointF(200.0, 150.0));
        scene.AddItemsWithUndo({rect}, QStringLiteral("Add rect"));

        auto* line = new StraightLineItem();
        line->SetEndpoints(QPointF(0.0, 210.0), QPointF(180.0, 210.0), QColor(QStringLiteral("#1E293B")), 2.0);
        QJsonObject binding;
        binding.insert(QStringLiteral("elementId"), rect->Uid());
        binding.insert(QStringLiteral("gap"), 4.0);
        line->SetStartBinding(binding);
        scene.AddItemsWithUndo({line}, QStringLiteral("Add arrow"));
        scene.SyncArrowBindings();

        const QSizeF size = rect->ContentRect().size();
        ItemPlacementList before;
        before.append(ItemPlacement{rect, QPointF(200.0, 150.0), size});
        rect->setPos(QPointF(300.0, 150.0));
        ItemPlacementList after;
        after.append(ItemPlacement{rect, QPointF(300.0, 150.0), size});
        undo.push(new GeometryCommand(&scene, before, after, QStringLiteral("Move rect")));

        const QPointF moved = line->mapToScene(line->StrokePoints().first());
        ctx.Check(qAbs(moved.x() - 296.0) < 1.0, QStringLiteral("移动目标后箭头贴住新位置"),
                  QStringLiteral("startX=%1").arg(moved.x()));

        undo.undo();
        const QPointF restored = line->mapToScene(line->StrokePoints().first());
        ctx.Check(qAbs(restored.x() - 196.0) < 1.0, QStringLiteral("撤销移动后箭头重新贴合旧位置"),
                  QStringLiteral("startX=%1").arg(restored.x()));

        undo.redo();
        const QPointF redone = line->mapToScene(line->StrokePoints().first());
        ctx.Check(qAbs(redone.x() - 296.0) < 1.0, QStringLiteral("重做移动后箭头再次跟随"),
                  QStringLiteral("startX=%1").arg(redone.x()));
    }

    // 端点句柄：拖离目标超过 12px 解绑，拖回目标边缘重新绑定。
    {
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        scene.SetMode(ToolMode::EDIT);
        QUndoStack undo;
        scene.SetUndoStack(&undo);

        auto* rect = ShapeFactory::Create(ShapeType::RECTANGLE);
        rect->SetContentSize(QSizeF(160.0, 120.0));
        rect->setPos(QPointF(300.0, 150.0));
        scene.AddItemsWithUndo({rect}, QStringLiteral("Add rect"));

        auto* line = new StraightLineItem();
        line->SetEndpoints(QPointF(0.0, 210.0), QPointF(280.0, 210.0), QColor(QStringLiteral("#1E293B")), 2.0);
        QJsonObject binding;
        binding.insert(QStringLiteral("elementId"), rect->Uid());
        binding.insert(QStringLiteral("gap"), 4.0);
        line->SetStartBinding(binding);
        scene.AddItemsWithUndo({line}, QStringLiteral("Add arrow"));

        ctx.Check(line->CustomHandles().size() == 2, QStringLiteral("直线提供两个端点句柄"),
                  QStringLiteral("count=%1").arg(line->CustomHandles().size()));
        line->BeginCustomHandleDrag(CanvasItem::CUSTOM_0, QPointF(100.0, 100.0));
        line->ApplyCustomHandleDrag(CanvasItem::CUSTOM_0, QPointF(100.0, 100.0));
        ctx.Check(line->StartBinding().isEmpty(), QStringLiteral("端点拖离图形后解除绑定"),
                  QStringLiteral("binding=%1").arg(line->StartBinding().isEmpty() ? QStringLiteral("empty")
                                                                                : QStringLiteral("set")));

        line->ApplyCustomHandleDrag(CanvasItem::CUSTOM_0, QPointF(290.0, 210.0));
        const QPointF attached = line->mapToScene(line->StrokePoints().first());
        ctx.Check(!line->StartBinding().isEmpty() && qAbs(attached.x() - 296.0) < 1.0,
                  QStringLiteral("端点拖回图形边缘重新绑定并贴边"),
                  QStringLiteral("x=%1 binding=%2")
                      .arg(attached.x())
                      .arg(line->StartBinding().isEmpty() ? QStringLiteral("empty") : QStringLiteral("set")));
    }

    // 直线非端点部分命中时只整体移动，不出现缩放手柄，也不会改两端间距。
    {
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        scene.SetMode(ToolMode::EDIT);
        QUndoStack undo;
        scene.SetUndoStack(&undo);

        auto* line = new StraightLineItem();
        line->SetEndpoints(QPointF(100.0, 100.0), QPointF(300.0, 100.0), QColor(QStringLiteral("#1E293B")), 2.0);
        scene.AddItemsWithUndo({line}, QStringLiteral("Add line"));
        line->setSelected(true);

        const QPointF middle(200.0, 100.0);
        const QPointF posBefore = line->pos();
        const qreal lengthBefore =
            QLineF(line->mapToScene(line->StrokePoints().first()), line->mapToScene(line->StrokePoints().constLast()))
                .length();
        bool endpointOnly = true;
        for (const HandleSpec& spec : line->CustomHandles()) {
            endpointOnly = endpointOnly && QLineF(spec.position, middle).length() > 1.0;
        }
        ctx.Check(endpointOnly && !line->HasSelectionFrame(), QStringLiteral("直线中段没有可拖句柄也不显示外接框"),
                  QStringLiteral("endpointOnly=%1 framed=%2").arg(endpointOnly).arg(line->HasSelectionFrame()));

        CanvasItemInteraction drag(line);
        drag.BeginMove(middle);
        drag.ApplyMove(middle + QPointF(40.0, 30.0));
        drag.FinishInteraction();
        const qreal lengthAfter =
            QLineF(line->mapToScene(line->StrokePoints().first()), line->mapToScene(line->StrokePoints().constLast()))
                .length();
        ctx.Check(qAbs(lengthAfter - lengthBefore) < 0.5 && qAbs(line->pos().x() - posBefore.x() - 40.0) < 0.5 &&
                      qAbs(line->pos().y() - posBefore.y() - 30.0) < 0.5,
                  QStringLiteral("拖直线中段只整体移动、长度不变"),
                  QStringLiteral("length=%1->%2 pos=(%3,%4)->(%5,%6)")
                      .arg(lengthBefore)
                      .arg(lengthAfter)
                      .arg(posBefore.x())
                      .arg(posBefore.y())
                      .arg(line->pos().x())
                      .arg(line->pos().y()));
    }

    // 曲线非端点、非拐点部分命中时只整体移动，不拉伸曲线长度。
    {
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        scene.SetMode(ToolMode::EDIT);
        QUndoStack undo;
        scene.SetUndoStack(&undo);

        auto* curve = new CurveItem();
        curve->SetStrokePoints(
            {QPointF(40.0, 320.0), QPointF(150.0, 300.0), QPointF(260.0, 320.0), QPointF(340.0, 360.0)},
            QColor(QStringLiteral("#1E293B")), 2.0);
        scene.AddItemsWithUndo({curve}, QStringLiteral("Add curve"));
        curve->setSelected(true);
        ctx.Check(
            curve->CustomHandles().size() == 2, QStringLiteral("平滑曲线只保留首尾端点手柄"),
            QStringLiteral("handles=%1 knots=%2").arg(curve->CustomHandles().size()).arg(curve->StrokePoints().size()));

        const QPointF middle =
            (curve->mapToScene(curve->StrokePoints().at(1)) + curve->mapToScene(curve->StrokePoints().at(2))) / 2.0;
        const QPointF posBefore = curve->pos();
        auto polylineLength = [&curve]() {
            const QVector<QPointF> points = curve->StrokePoints();
            qreal length = 0.0;
            for (int i = 1; i < points.size(); ++i) {
                length += QLineF(curve->mapToScene(points.at(i - 1)), curve->mapToScene(points.at(i))).length();
            }
            return length;
        };
        const qreal lengthBefore = polylineLength();
        bool knotFree = true;
        for (const HandleSpec& spec : curve->CustomHandles()) {
            knotFree = knotFree && QLineF(spec.position, curve->mapFromScene(middle)).length() > 1.0;
        }
        ctx.Check(knotFree && !curve->HasSelectionFrame(), QStringLiteral("曲线非端点、非拐点部分没有可拖句柄"),
                  QStringLiteral("knotFree=%1 framed=%2").arg(knotFree).arg(curve->HasSelectionFrame()));

        CanvasItemInteraction drag(curve);
        drag.BeginMove(middle);
        drag.ApplyMove(middle + QPointF(40.0, 30.0));
        drag.FinishInteraction();
        const qreal lengthAfter = polylineLength();
        ctx.Check(qAbs(lengthAfter - lengthBefore) < 0.5 && qAbs(curve->pos().x() - posBefore.x() - 40.0) < 0.5 &&
                      qAbs(curve->pos().y() - posBefore.y() - 30.0) < 0.5,
                  QStringLiteral("拖曲线非端点、非拐点部分只整体移动、长度不变"),
                  QStringLiteral("length=%1->%2 pos=(%3,%4)->(%5,%6)")
                      .arg(lengthBefore)
                      .arg(lengthAfter)
                      .arg(posBefore.x())
                      .arg(posBefore.y())
                      .arg(curve->pos().x())
                      .arg(curve->pos().y()));
    }

    // 曲线两端：首尾句柄拖到图形边缘就吸附并绑定，目标移动后端点跟随，拖离则解绑。
    // 绑定只对图形生效：文字不参与吸附。
    {
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        scene.SetMode(ToolMode::EDIT);
        QUndoStack undo;
        scene.SetUndoStack(&undo);

        auto* rect = ShapeFactory::Create(ShapeType::RECTANGLE);
        rect->SetContentSize(QSizeF(160.0, 120.0));
        rect->setPos(QPointF(300.0, 150.0));
        scene.AddItemsWithUndo({rect}, QStringLiteral("Add rect"));

        auto* curve = new CurveItem();
        curve->SetStrokePoints(
            {QPointF(20.0, 300.0), QPointF(140.0, 250.0), QPointF(240.0, 330.0), QPointF(280.0, 300.0)},
            QColor(QStringLiteral("#1E293B")), 2.0);
        scene.AddItemsWithUndo({curve}, QStringLiteral("Add curve"));

        const int knotCount = curve->StrokePoints().size();
        ctx.Check(curve->CustomHandles().size() == knotCount && !curve->HasSelectionFrame(),
                  QStringLiteral("曲线不画选中框，端点与拐点各有一个调整方块"),
                  QStringLiteral("count=%1 knots=%2").arg(curve->CustomHandles().size()).arg(knotCount));

        const QPointF middleKnot = curve->mapToScene(curve->StrokePoints().at(1));
        const QPointF movedKnot = middleKnot + QPointF(30.0, -20.0);
        curve->BeginCustomHandleDrag(CanvasItem::CUSTOM_1 + 1, middleKnot);
        curve->ApplyCustomHandleDrag(CanvasItem::CUSTOM_1 + 1, movedKnot);
        const QPointF knotAfter = curve->mapToScene(curve->StrokePoints().at(1));
        ctx.Check(curve->StrokePoints().size() == knotCount && QLineF(knotAfter, movedKnot).length() < 1.0,
                  QStringLiteral("拖动中间拐点只改形状、不改节点数量"),
                  QStringLiteral("count=%1 knot=(%2,%3)")
                      .arg(curve->StrokePoints().size())
                      .arg(knotAfter.x())
                      .arg(knotAfter.y()));

        curve->BeginCustomHandleDrag(CanvasItem::CUSTOM_1, QPointF(280.0, 300.0));
        curve->ApplyCustomHandleDrag(CanvasItem::CUSTOM_1, QPointF(300.0, 210.0));
        const QPointF attached = curve->mapToScene(curve->StrokePoints().constLast());
        ctx.Check(curve->EndBinding().value(QStringLiteral("elementId")).toString() == rect->Uid() &&
                      qAbs(attached.x() - 296.0) < 1.0,
                  QStringLiteral("曲线端点拖到图形边缘后绑定并贴边"),
                  QStringLiteral("x=%1 binding=%2")
                      .arg(attached.x())
                      .arg(curve->EndBinding().isEmpty() ? QStringLiteral("empty") : QStringLiteral("set")));

        rect->setPos(QPointF(400.0, 150.0));
        scene.SyncArrowBindings();
        const QPointF followed = curve->mapToScene(curve->StrokePoints().constLast());
        ctx.Check(qAbs(followed.x() - 396.0) < 1.0, QStringLiteral("目标移动后曲线端点跟随"),
                  QStringLiteral("x=%1").arg(followed.x()));

        // 已绑定的曲线：拖中段不能让端点被拉回而中间控制点继续平移，长度保持不变。
        curve->setSelected(true);
        const QVector<QPointF> beforePoints = curve->StrokePoints();
        qreal boundLengthBefore = 0.0;
        for (int i = 1; i < beforePoints.size(); ++i) {
            boundLengthBefore +=
                QLineF(curve->mapToScene(beforePoints.at(i - 1)), curve->mapToScene(beforePoints.at(i))).length();
        }
        const QPointF bodyMiddle =
            (curve->mapToScene(beforePoints.at(1)) + curve->mapToScene(beforePoints.at(2))) / 2.0;
        const QPointF boundPosBefore = curve->pos();
        CanvasItemInteraction bodyDrag(curve);
        bodyDrag.BeginMove(bodyMiddle);
        bodyDrag.ApplyMove(bodyMiddle + QPointF(50.0, 35.0));
        bodyDrag.FinishInteraction();
        scene.SyncArrowBindings();
        const QVector<QPointF> afterPoints = curve->StrokePoints();
        qreal boundLengthAfter = 0.0;
        for (int i = 1; i < afterPoints.size(); ++i) {
            boundLengthAfter +=
                QLineF(curve->mapToScene(afterPoints.at(i - 1)), curve->mapToScene(afterPoints.at(i))).length();
        }
        ctx.Check(qAbs(boundLengthAfter - boundLengthBefore) < 0.5 && curve->pos() == boundPosBefore,
                  QStringLiteral("拖已绑定曲线中段不拉伸长度"),
                  QStringLiteral("length=%1->%2 pos=(%3,%4)->(%5,%6)")
                      .arg(boundLengthBefore)
                      .arg(boundLengthAfter)
                      .arg(boundPosBefore.x())
                      .arg(boundPosBefore.y())
                      .arg(curve->pos().x())
                      .arg(curve->pos().y()));

        curve->ApplyCustomHandleDrag(CanvasItem::CUSTOM_1, QPointF(120.0, 420.0));
        ctx.Check(curve->EndBinding().isEmpty(), QStringLiteral("曲线端点拖离图形后解除绑定"),
                  QStringLiteral("binding=%1")
                      .arg(curve->EndBinding().isEmpty() ? QStringLiteral("empty") : QStringLiteral("set")));

        auto* text = new TextItem();
        text->SetTextFormat(QStringLiteral("Text"), text->TextFont(), QColor(QStringLiteral("#1E293B")));
        text->setPos(QPointF(60.0, 60.0));
        scene.AddItemsWithUndo({text}, QStringLiteral("Add text"));
        const QRectF textRect = text->mapRectToScene(text->ContentRect());
        ctx.Check(scene.BindableShapeAt(textRect.center(), 12.0) == nullptr, QStringLiteral("文字图元不作为绑定目标"),
                  QStringLiteral("center=%1,%2").arg(textRect.center().x()).arg(textRect.center().y()));
    }

    // 容器文字：容器移动 / 缩放后文字回到容器中心。
    {
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        scene.SetMode(ToolMode::EDIT);
        QUndoStack undo;
        scene.SetUndoStack(&undo);

        auto* rect = ShapeFactory::Create(ShapeType::RECTANGLE);
        rect->SetContentSize(QSizeF(200.0, 120.0));
        rect->setPos(QPointF(100.0, 80.0));
        scene.AddItemsWithUndo({rect}, QStringLiteral("Add container"));

        auto* text = new TextItem();
        text->SetTextFormat(QStringLiteral("容器文字换行测试：这是一段需要按容器宽度折行的长文字。"),
                            text->TextFont(), QColor(QStringLiteral("#1E293B")));
        text->SetContainerUid(rect->Uid());
        text->setPos(QPointF(0.0, 0.0));
        scene.AddItemsWithUndo({text}, QStringLiteral("Add text"));

        scene.SyncContainerTexts();
        const QStringList wrapped = text->WrappedLines(text->ContentSize().width());
        ctx.Check(wrapped.size() > 1 && qAbs(text->ContentSize().width() - 200.0) < 1.0,
                  QStringLiteral("容器文字按容器宽度折行"),
                  QStringLiteral("lines=%1 width=%2").arg(wrapped.size()).arg(text->ContentSize().width()));
        const QFontMetricsF metrics(text->TextFont());
        const qreal expectedHeight = metrics.lineSpacing() * wrapped.size() + 10.0;
        ctx.Check(qAbs(text->ContentSize().height() - expectedHeight) < 0.5,
                  QStringLiteral("容器文字上下内边距为 Excalidraw 的 5px"),
                  QStringLiteral("height=%1 expected=%2")
                      .arg(text->ContentSize().height())
                      .arg(expectedHeight));
        const QPointF textCenter = text->mapToScene(text->ContentRect().center());
        const QRectF container = rect->mapRectToScene(rect->ContentRect());
        ctx.Check(qAbs(textCenter.x() - container.center().x()) < 1.0 &&
                      qAbs(textCenter.y() - container.center().y()) < 1.0,
                  QStringLiteral("容器文字落在容器中心"),
                  QStringLiteral("text=(%1,%2) container=(%3,%4)")
                      .arg(textCenter.x())
                      .arg(textCenter.y())
                      .arg(container.center().x())
                      .arg(container.center().y()));

        rect->SetContentSize(QSizeF(300.0, 220.0));
        scene.SyncContainerTexts();
        const QPointF movedCenter = text->mapToScene(text->ContentRect().center());
        const QRectF resized = rect->mapRectToScene(rect->ContentRect());
        ctx.Check(qAbs(movedCenter.x() - resized.center().x()) < 1.0 &&
                      qAbs(movedCenter.y() - resized.center().y()) < 1.0,
                  QStringLiteral("容器缩放后文字仍居中"),
                  QStringLiteral("text=(%1,%2) container=(%3,%4)")
                      .arg(movedCenter.x())
                      .arg(movedCenter.y())
                      .arg(resized.center().x())
                      .arg(resized.center().y()));

        scene.clearSelection();
        rect->setSelected(true);
        scene.DeleteSelected();
        ctx.Check(scene.CanvasItemCount() == 0, QStringLiteral("删除容器时一起删掉容器文字"),
                  QStringLiteral("count=%1").arg(scene.CanvasItemCount()));
    }

    // 文字归属：解绑保留当前位置与尺寸，撤销回到容器内的排版。
    {
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        scene.SetMode(ToolMode::EDIT);
        QUndoStack undo;
        scene.SetUndoStack(&undo);

        auto* rect = ShapeFactory::Create(ShapeType::RECTANGLE);
        rect->SetContentSize(QSizeF(200.0, 120.0));
        rect->setPos(QPointF(100.0, 80.0));
        auto* text = new TextItem();
        text->SetTextFormat(QStringLiteral("Attached text"), text->TextFont(), QColor(QStringLiteral("#1E293B")));
        text->SetContainerUid(rect->Uid());
        scene.AddItemsWithUndo({rect, text}, QStringLiteral("Add container and text"));
        scene.SyncContainerTexts();

        const QString boundUid = text->ContainerUid();
        const QPointF boundPos = text->pos();
        const QSizeF boundSize = text->ContentSize();
        ctx.Check(scene.SetTextContainer(text, QString(), QStringLiteral("Unbind text")) &&
                      text->ContainerUid().isEmpty(),
                  QStringLiteral("解绑文字清空 containerId"), QStringLiteral("uid=%1").arg(text->ContainerUid()));
        ctx.Check(text->pos() == boundPos && text->ContentSize() == boundSize,
                  QStringLiteral("解绑保留文字当前位置与尺寸"),
                  QStringLiteral("pos=(%1,%2) size=(%3,%4)")
                      .arg(text->pos().x())
                      .arg(text->pos().y())
                      .arg(text->ContentSize().width())
                      .arg(text->ContentSize().height()));
        undo.undo();
        ctx.Check(text->ContainerUid() == boundUid, QStringLiteral("撤销解绑恢复文字归属"),
                  QStringLiteral("uid=%1").arg(text->ContainerUid()));
    }

    // 拖进图形：文字中心命中矩形时绑定并按容器宽度折行，撤销回到自由文字的位置。
    {
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        scene.SetMode(ToolMode::EDIT);
        QUndoStack undo;
        scene.SetUndoStack(&undo);

        auto* rect = ShapeFactory::Create(ShapeType::RECTANGLE);
        rect->SetContentSize(QSizeF(200.0, 120.0));
        rect->setPos(QPointF(100.0, 80.0));
        auto* text = new TextItem();
        text->SetTextFormat(QStringLiteral("Free text"), text->TextFont(), QColor(QStringLiteral("#1E293B")));
        scene.AddItemsWithUndo({rect, text}, QStringLiteral("Add rect and free text"));

        // 把文字中心对准矩形中心，模拟拖拽落入图形内部。
        const QPointF rectCenter = rect->mapRectToScene(rect->ContentRect()).center();
        const QSizeF textSize = text->ContentSize();
        text->setPos(rectCenter - QPointF(textSize.width() / 2.0, textSize.height() / 2.0));
        ctx.Check(scene.ContainerShapeForText(text) == rect, QStringLiteral("文字中心落在图形内时识别到容器"));
        const TextItemState beforeState = text->State();
        const QString rectUid = rect->Uid();
        ctx.Check(scene.SetTextContainer(text, rectUid, QString(), beforeState, QStringLiteral("Bind text")) &&
                      text->ContainerUid() == rectUid,
                  QStringLiteral("拖动文字落入图形后绑定"), QStringLiteral("uid=%1").arg(text->ContainerUid()));
        const QRectF containerRect = rect->mapRectToScene(rect->ContentRect());
        ctx.Check(qAbs(text->ContentSize().width() - containerRect.width()) < 1.0,
                  QStringLiteral("绑定后文字按容器宽度折行"),
                  QStringLiteral("w=%1 container=%2").arg(text->ContentSize().width()).arg(containerRect.width()));
        undo.undo();
        ctx.Check(text->ContainerUid().isEmpty() && text->pos() == beforeState.pos,
                  QStringLiteral("撤销绑定回到自由文字的位置与尺寸"),
                  QStringLiteral("uid=%1 pos=(%2,%3)")
                      .arg(text->ContainerUid())
                      .arg(text->pos().x())
                      .arg(text->pos().y()));
    }

    // 换类型：原图元带容器文字时，文字要跟着新图元的 uid，撤销再指回旧 uid。
    {
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        scene.SetMode(ToolMode::EDIT);
        QUndoStack undo;
        scene.SetUndoStack(&undo);

        auto* sector = ShapeFactory::Create(ShapeType::SECTOR);
        sector->SetContentSize(QSizeF(180.0, 180.0));
        sector->setPos(QPointF(100.0, 100.0));
        auto* text = new TextItem();
        text->SetTextFormat(QStringLiteral("Sector text"), text->TextFont(), QColor(QStringLiteral("#1E293B")));
        text->SetContainerUid(sector->Uid());
        scene.AddItemsWithUndo({sector, text}, QStringLiteral("Add sector and text"));
        scene.SyncContainerTexts();

        const QString oldUid = sector->Uid();
        CanvasItem* arc = scene.ConvertItemType(sector, QStringLiteral("arc"));
        const QString newUid = arc ? arc->Uid() : QString();
        ctx.Check(arc && !newUid.isEmpty() && newUid != oldUid && text->ContainerUid() == newUid,
                  QStringLiteral("换类型后容器文字改指新图元"),
                  QStringLiteral("old=%1 new=%2 text=%3").arg(oldUid, newUid, text->ContainerUid()));
        undo.undo();
        ctx.Check(text->ContainerUid() == oldUid, QStringLiteral("撤销换类型后文字指回旧图元"),
                  QStringLiteral("text=%1 old=%2").arg(text->ContainerUid(), oldUid));
    }

    // 已绑定的文字拖回原容器内：松手立刻拉回容器中心，不产生位移撤销记录。
    {
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        scene.SetMode(ToolMode::EDIT);
        QUndoStack undo;
        scene.SetUndoStack(&undo);

        auto* rect = ShapeFactory::Create(ShapeType::RECTANGLE);
        rect->SetContentSize(QSizeF(200.0, 120.0));
        rect->setPos(QPointF(100.0, 80.0));
        auto* text = new TextItem();
        text->SetTextFormat(QStringLiteral("Container text"), text->TextFont(), QColor(QStringLiteral("#1E293B")));
        text->SetContainerUid(rect->Uid());
        scene.AddItemsWithUndo({rect, text}, QStringLiteral("Add container and text"));
        scene.SyncContainerTexts();

        CanvasView view(&scene);
        text->setSelected(true);
        CanvasItemInteraction drag(text);
        const QPointF startCenter = text->mapToScene(text->ContentRect().center());
        const int indexBefore = undo.index();
        drag.BeginMove(startCenter);
        drag.ApplyMove(startCenter + QPointF(26.0, 18.0));
        drag.FinishInteraction();
        const QRectF containerRect = rect->mapRectToScene(rect->ContentRect());
        const QPointF endCenter = text->mapToScene(text->ContentRect().center());
        ctx.Check(qAbs(endCenter.x() - containerRect.center().x()) < 1.0 &&
                      qAbs(endCenter.y() - containerRect.center().y()) < 1.0,
                  QStringLiteral("拖回原容器松手拉回容器中心"),
                  QStringLiteral("center=(%1,%2) expected=(%3,%4)")
                      .arg(endCenter.x())
                      .arg(endCenter.y())
                      .arg(containerRect.center().x())
                      .arg(containerRect.center().y()));
        ctx.Check(undo.index() == indexBefore, QStringLiteral("拖回原容器不留位移撤销记录"),
                  QStringLiteral("index=%1->%2").arg(indexBefore).arg(undo.index()));
    }

    // 交互路径：拖住容器文字往外拖时，移动中的容器同步不再把它拉回，松手后解绑。
    {
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        scene.SetMode(ToolMode::EDIT);
        QUndoStack undo;
        scene.SetUndoStack(&undo);

        auto* rect = ShapeFactory::Create(ShapeType::RECTANGLE);
        rect->SetContentSize(QSizeF(200.0, 120.0));
        rect->setPos(QPointF(100.0, 80.0));
        auto* text = new TextItem();
        text->SetTextFormat(QStringLiteral("Drag me"), text->TextFont(), QColor(QStringLiteral("#1E293B")));
        text->SetContainerUid(rect->Uid());
        scene.AddItemsWithUndo({rect, text}, QStringLiteral("Add container and text"));
        scene.SyncContainerTexts();

        const QPointF pressScene = text->mapToScene(text->ContentRect().center());
        QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
        press.setScenePos(pressScene);
        press.setButton(Qt::LeftButton);
        press.setButtons(Qt::LeftButton);
        QApplication::sendEvent(&scene, &press);
        ctx.Check(scene.mouseGrabberItem() == text, QStringLiteral("按下容器文字后由文字接收拖动"));

        const QPointF dragScene = pressScene + QPointF(0.0, 260.0);
        QGraphicsSceneMouseEvent move(QEvent::GraphicsSceneMouseMove);
        move.setScenePos(dragScene);
        move.setButtons(Qt::LeftButton);
        QApplication::sendEvent(&scene, &move);
        // 视图在拖动过程中每帧都会同步容器文字，这里模拟同一步。
        scene.SyncContainerTexts();
        const QPointF centerDuringDrag = text->mapToScene(text->ContentRect().center());
        ctx.Check(centerDuringDrag.y() > 200.0, QStringLiteral("拖动容器文字时不被拉回容器"),
                  QStringLiteral("centerY=%1").arg(centerDuringDrag.y()));

        QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
        release.setScenePos(dragScene);
        release.setButton(Qt::LeftButton);
        QApplication::sendEvent(&scene, &release);
        ctx.Check(text->ContainerUid().isEmpty(), QStringLiteral("拖出容器松手后自动解绑"),
                  QStringLiteral("uid=%1").arg(text->ContainerUid()));
    }

    // 交互路径：自由文字拖进图形内部松手后绑定，并按容器宽度折行。
    {
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        scene.SetMode(ToolMode::EDIT);
        QUndoStack undo;
        scene.SetUndoStack(&undo);

        auto* rect = ShapeFactory::Create(ShapeType::RECTANGLE);
        rect->SetContentSize(QSizeF(200.0, 120.0));
        rect->setPos(QPointF(100.0, 80.0));
        auto* text = new TextItem();
        text->SetTextFormat(QStringLiteral("Loose text"), text->TextFont(), QColor(QStringLiteral("#1E293B")));
        text->setPos(QPointF(480.0, 380.0));
        scene.AddItemsWithUndo({rect, text}, QStringLiteral("Add rect and loose text"));

        const QPointF pressScene = text->mapToScene(text->ContentRect().center());
        QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
        press.setScenePos(pressScene);
        press.setButton(Qt::LeftButton);
        press.setButtons(Qt::LeftButton);
        QApplication::sendEvent(&scene, &press);

        const QPointF dropScene = rect->mapRectToScene(rect->ContentRect()).center();
        QGraphicsSceneMouseEvent move(QEvent::GraphicsSceneMouseMove);
        move.setScenePos(dropScene);
        move.setButtons(Qt::LeftButton);
        QApplication::sendEvent(&scene, &move);
        QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
        release.setScenePos(dropScene);
        release.setButton(Qt::LeftButton);
        QApplication::sendEvent(&scene, &release);

        ctx.Check(text->ContainerUid() == rect->Uid(), QStringLiteral("自由文字拖进图形松手后绑定"),
                  QStringLiteral("uid=%1 rect=%2").arg(text->ContainerUid(), rect->Uid()));
        const QRectF containerRect = rect->mapRectToScene(rect->ContentRect());
        const QPointF endCenter = text->mapToScene(text->ContentRect().center());
        ctx.Check(qAbs(endCenter.x() - containerRect.center().x()) < 1.0 &&
                      qAbs(text->ContentSize().width() - containerRect.width()) < 1.0,
                  QStringLiteral("绑定后文字居中并按容器宽度折行"),
                  QStringLiteral("centerX=%1 w=%2 containerW=%3")
                      .arg(endCenter.x())
                      .arg(text->ContentSize().width())
                      .arg(containerRect.width()));
    }

    // 直线箭头绑定图形后，图形移动时端点跟随，笔迹仍保持笔直（控制柄按新弦重排）。
    {
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(900.0, 700.0));
        scene.SetMode(ToolMode::EDIT);
        QUndoStack undo;
        scene.SetUndoStack(&undo);

        auto* rect = ShapeFactory::Create(ShapeType::RECTANGLE);
        rect->SetContentSize(QSizeF(120.0, 100.0));
        rect->setPos(QPointF(300.0, 100.0));
        auto* line = new StraightLineItem();
        line->SetEndpoints(QPointF(100.0, 150.0), QPointF(420.0, 150.0), QColor(QStringLiteral("#334155")), 3.0);
        line->SetArrowStyles(ArrowStyle::NONE, ArrowStyle::OPEN_ARROW);
        scene.AddItemsWithUndo({rect, line}, QStringLiteral("Add bound line arrow"));
        line->DragEndpointWithBinding(false, QPointF(420.0, 150.0));
        ctx.Check(line->EndBinding().value(QStringLiteral("elementId")).toString() == rect->Uid(),
                  QStringLiteral("直线箭头尾端绑定到矩形"));

        rect->setPos(QPointF(520.0, 320.0));
        scene.SyncArrowBindings();
        const QVector<CurveSegment> segments = line->geometryState().segments;
        bool straight = segments.size() == 1;
        if (straight) {
            const CurveSegment& segment = segments.constFirst();
            const QPointF chord = segment.p1 - segment.p0;
            const qreal length = qSqrt(chord.x() * chord.x() + chord.y() * chord.y());
            auto deviation = [&](const QPointF& point) {
                return qAbs((point.x() - segment.p0.x()) * chord.y() -
                            (point.y() - segment.p0.y()) * chord.x()) /
                       qMax<qreal>(1.0, length);
            };
            const QPointF mid = (segment.p0 + 3.0 * segment.c1 + 3.0 * segment.c2 + segment.p1) / 8.0;
            straight = length > 0.0 && deviation(segment.c1) < 0.01 && deviation(segment.c2) < 0.01 &&
                       deviation(mid) < 0.01;
        }
        ctx.Check(straight, QStringLiteral("图形移动后直线箭头笔迹不弯曲"),
                  QStringLiteral("segments=%1").arg(segments.size()));
    }

    // 两端都绑定的直线：走线可切直连 / 折线；折线只沿横纵轴，移动图形后仍然成立。
    {
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(1000.0, 800.0));
        scene.SetMode(ToolMode::EDIT);
        QUndoStack undo;
        scene.SetUndoStack(&undo);

        auto* rectA = ShapeFactory::Create(ShapeType::RECTANGLE);
        rectA->SetContentSize(QSizeF(120.0, 100.0));
        rectA->setPos(QPointF(100.0, 100.0));
        auto* rectB = ShapeFactory::Create(ShapeType::RECTANGLE);
        rectB->SetContentSize(QSizeF(120.0, 100.0));
        rectB->setPos(QPointF(600.0, 400.0));
        auto* line = new StraightLineItem();
        line->SetEndpoints(QPointF(230.0, 160.0), QPointF(580.0, 430.0), QColor(QStringLiteral("#334155")), 3.0);
        line->SetArrowStyles(ArrowStyle::NONE, ArrowStyle::OPEN_ARROW);
        scene.AddItemsWithUndo({rectA, rectB, line}, QStringLiteral("Add two-end bound line"));

        line->DragEndpointWithBinding(true, QPointF(230.0, 160.0));
        line->DragEndpointWithBinding(false, QPointF(595.0, 430.0));
        const bool bothBound = !line->StartBinding().isEmpty() && !line->EndBinding().isEmpty();
        QVector<QPointF> points = line->StrokePoints();
        const bool diagonal = points.size() == 2 && qAbs(points.at(1).x() - points.at(0).x()) > 1.0 &&
                              qAbs(points.at(1).y() - points.at(0).y()) > 1.0;
        ctx.Check(bothBound && line->LineRoute() == StraightLineItem::RouteMode::DIRECT && diagonal,
                  QStringLiteral("两端绑定的直线默认直连：两点斜线相连"),
                  QStringLiteral("bound=%1 points=%2").arg(bothBound).arg(points.size()));

        ctx.Check(scene.SetStraightLineRoute(line, 1, QStringLiteral("route")), QStringLiteral("两端绑定时可切到折线"),
                  QStringLiteral("route=1"));
        points = line->StrokePoints();
        bool orthogonal = points.size() >= 2;
        for (int i = 1; orthogonal && i < points.size(); ++i) {
            const QPointF delta = points.at(i) - points.at(i - 1);
            orthogonal = qAbs(delta.x()) < 0.01 || qAbs(delta.y()) < 0.01;
        }
        const QRectF rectBScene = rectB->mapRectToScene(rectB->ContentRect());
        const bool landsOnB = line->mapToScene(points.constLast()).x() < rectBScene.left() &&
                              line->mapToScene(points.constLast()).x() > rectBScene.left() - 12.0;
        ctx.Check(orthogonal && landsOnB, QStringLiteral("折线只走横纵轴且终点贴在目标边缘"),
                  QStringLiteral("points=%1 last=(%2,%3)")
                      .arg(points.size())
                      .arg(line->mapToScene(points.constLast()).x())
                      .arg(line->mapToScene(points.constLast()).y()));

        undo.undo();
        ctx.Check(line->LineRoute() == StraightLineItem::RouteMode::DIRECT && line->StrokePoints().size() == 2,
                  QStringLiteral("撤销回到直连两点"), QStringLiteral("points=%1").arg(line->StrokePoints().size()));
        undo.redo();
        ctx.Check(line->LineRoute() == StraightLineItem::RouteMode::ORTHOGONAL && line->StrokePoints().size() >= 2,
                  QStringLiteral("重做回到折线"), QStringLiteral("points=%1").arg(line->StrokePoints().size()));

        rectB->setPos(QPointF(600.0, 520.0));
        scene.SyncArrowBindings();
        points = line->StrokePoints();
        bool stillOrthogonal = points.size() >= 2;
        for (int i = 1; stillOrthogonal && i < points.size(); ++i) {
            const QPointF delta = points.at(i) - points.at(i - 1);
            stillOrthogonal = qAbs(delta.x()) < 0.01 || qAbs(delta.y()) < 0.01;
        }
        ctx.Check(stillOrthogonal && line->LineRoute() == StraightLineItem::RouteMode::ORTHOGONAL,
                  QStringLiteral("移动绑定图形后转折仍只走横纵轴"), QStringLiteral("points=%1").arg(points.size()));

        // 拐点手柄：横向拖动只改转折轴的 x，纵坐标仍由首尾吸附点决定。
        const QPointF bendBefore = line->mapToScene(line->StrokePoints().at(1));
        const QPointF bendTarget = bendBefore + QPointF(40.0, 25.0);
        line->BeginCustomHandleDrag(CanvasItem::CUSTOM_1 + 1, bendBefore);
        line->ApplyCustomHandleDrag(CanvasItem::CUSTOM_1 + 1, bendTarget);
        points = line->StrokePoints();
        bool axisAligned = points.size() >= 2;
        bool bendFollowed = false;
        for (int i = 1; axisAligned && i < points.size(); ++i) {
            const QPointF delta = points.at(i) - points.at(i - 1);
            axisAligned = qAbs(delta.x()) < 0.01 || qAbs(delta.y()) < 0.01;
            bendFollowed = bendFollowed || qAbs(line->mapToScene(points.at(i)).x() - bendTarget.x()) < 1.0;
        }
        ctx.Check(axisAligned && bendFollowed && points.size() >= 3,
                  QStringLiteral("拖动转折拐点只沿横轴移动且保持横纵轴"),
                  QStringLiteral("points=%1").arg(points.size()));

        ctx.Check(scene.SetStraightLineRoute(line, 0, QStringLiteral("route")), QStringLiteral("可切回直连"),
                  QStringLiteral("route=0"));
        ctx.Check(line->LineRoute() == StraightLineItem::RouteMode::DIRECT && line->StrokePoints().size() == 2,
                  QStringLiteral("切回直连只剩两个端点"), QStringLiteral("points=%1").arg(line->StrokePoints().size()));
    }
}
