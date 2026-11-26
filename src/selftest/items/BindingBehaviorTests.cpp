#include "items/lines/CurveItem.h"
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

        // 右下方 / 左下方斜向靠近：先命中哪条边就贴到该边中点，落点不能跑到矩形外面。
        const QPointF bottomRight = CanvasScene::AttachmentPoint(rect, QPointF(500.0, 320.0), 4.0);
        const QPointF bottomLeft = CanvasScene::AttachmentPoint(rect, QPointF(200.0, 320.0), 4.0);
        const qreal midX = targetRect.center().x();
        const qreal midY = targetRect.center().y();
        ctx.Check(qAbs(bottomRight.x() - midX) < 1.0 && qAbs(bottomRight.y() - targetRect.bottom() - 4.0) < 1.0 &&
                      qAbs(bottomLeft.x() - targetRect.left() + 4.0) < 1.0 && qAbs(bottomLeft.y() - midY) < 1.0,
                  QStringLiteral("斜向右下 / 左下靠近时端点吸附到所命中的边中点"),
                  QStringLiteral("br=(%1,%2) bl=(%3,%4) mid=(%5,%6)")
                      .arg(bottomRight.x())
                      .arg(bottomRight.y())
                      .arg(bottomLeft.x())
                      .arg(bottomLeft.y())
                      .arg(midX)
                      .arg(midY));

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

        ctx.Check(curve->CustomHandles().size() == 2, QStringLiteral("曲线提供首尾两个端点句柄"),
                  QStringLiteral("count=%1").arg(curve->CustomHandles().size()));

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
}
