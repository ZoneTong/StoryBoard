#include "items/shapes/ShapeRegistry.h"
#include "scene/CanvasScene.h"
#include "scene/Commands.h"
#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"
#include "ui/panels/properties/PropertyCommitService.h"
#include "ui/panels/properties/PropertyRowSpec.h"

#include <QPointF>
#include <QSizeF>
#include <QUndoStack>

void selftest::RunPropertyRowSpecTests(Context& ctx)
{
    const QList<PropertyRowSpec>& specs = PropertyRowSpecs::All();
    ctx.Check(specs.size() == 30, QStringLiteral("行描述覆盖全部属性"), QString::number(specs.size()));

    bool everyPropertyResolvable = true;
    for (const ItemPropertyDescriptor& descriptor : ItemPropertyCatalog::All()) {
        if (!PropertyRowSpecs::For(descriptor.property)) {
            everyPropertyResolvable = false;
        }
    }
    ctx.Check(everyPropertyResolvable, QStringLiteral("每个属性都能反查到行描述"));

    const PropertyRowSpec* angle = PropertyRowSpecs::For(ItemProperty::BOTTOM_ANGLE1);
    ctx.Check(angle && angle->Matches(ItemPropertySet{ItemProperty::PARALLELOGRAM_ANGLE}) &&
                  !angle->Matches(ItemPropertySet{ItemProperty::STROKE_COLOR}),
              QStringLiteral("内角行同时接受三角形底角与平行四边形内角"));

    CanvasScene scene;
    QUndoStack undo;
    scene.SetUndoStack(&undo);
    scene.ApplyCanvasSize(QSizeF(800.0, 600.0));
    scene.SetMode(ToolMode::EDIT);

    auto* rect = ShapeRegistry::Create(ShapeType::RECTANGLE);
    rect->SetContentSize(QSizeF(90.0, 70.0));
    rect->setPos(QPointF(80.0, 90.0));
    scene.AddItemsWithUndo({rect}, QStringLiteral("Add test rect"));

    const qreal originalOpacity = rect->ItemOpacity();
    QMap<ItemProperty, QVariant> before = PropertyCommitService::Snapshot(rect);
    QMap<ItemProperty, QVariant> after = before;
    after.insert(ItemProperty::OPACITY, 0.35);
    PropertyCommitService::Push(&scene, {rect}, {before}, {after}, QStringLiteral("Change opacity"));

    ctx.Check(qAbs(rect->ItemOpacity() - 0.35) < 0.01, QStringLiteral("属性提交服务写入一条属性映射"),
              QString::number(rect->ItemOpacity()));
    undo.undo();
    ctx.Check(qAbs(rect->ItemOpacity() - originalOpacity) < 0.01, QStringLiteral("属性提交服务可用一次撤销恢复"),
              QString::number(rect->ItemOpacity()));
}
