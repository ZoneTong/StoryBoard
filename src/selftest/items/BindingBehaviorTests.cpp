#include "items/lines/StraightLineItem.h"
#include "items/shapes/ShapeFactory.h"
#include "items/text/TextItem.h"
#include "scene/CanvasScene.h"
#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"

#include <QColor>
#include <QJsonObject>
#include <QPointF>
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
        binding.insert(QStringLiteral("focus"), 0.0);
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
        text->SetTextFormat(QStringLiteral("容器文字"), text->TextFont(), QColor(QStringLiteral("#1E293B")));
        text->SetContainerUid(rect->Uid());
        text->setPos(QPointF(0.0, 0.0));
        scene.AddItemsWithUndo({text}, QStringLiteral("Add text"));

        scene.SyncContainerTexts();
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
    }
}
