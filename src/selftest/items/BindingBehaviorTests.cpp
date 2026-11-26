#include "items/lines/StraightLineItem.h"
#include "items/shapes/ShapeFactory.h"
#include "items/text/TextItem.h"
#include "scene/CanvasScene.h"
#include "scene/Commands.h"
#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"

#include <QColor>
#include <QFontMetricsF>
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
        const qreal focus = CanvasScene::FocusForPoint(targetRect, edgePoint);
        const QPointF restored = CanvasScene::PointForFocus(targetRect, focus);
        ctx.Check(QLineF(restored, edgePoint).length() < 1.0 &&
                      CanvasScene::AttachmentPoint(rect, QPointF(500.0, 210.0), 4.0, focus).x() <
                          targetRect.left(),
                  QStringLiteral("轮廓 focus 与场景点可互算并用于绑定落点"),
                  QStringLiteral("focus=%1 point=(%2,%3)")
                      .arg(focus)
                      .arg(restored.x())
                      .arg(restored.y()));

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
}
