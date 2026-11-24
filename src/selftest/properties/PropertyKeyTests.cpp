#include "items/properties/ItemProperties.h"
#include "items/properties/ItemPropertyCatalog.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "items/shapes/ShapeFactory.h"
#include "scene/CanvasScene.h"
#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"
#include "ui/panels/properties/PropertyCommitService.h"
#include "ui/panels/properties/PropertyRowSpec.h"

#include <QDoubleSpinBox>
#include <QSet>
#include <QUndoStack>

void selftest::RunPropertyKeyTests(Context& ctx)
{
    const PropertyKey opacity(ItemProperty::OPACITY);
    ctx.Check(opacity.IsCommon() && opacity.Id() == static_cast<int>(ItemProperty::OPACITY),
              QStringLiteral("公共枚举映射为固定属性键"));

    const PropertyKey first = PropertyKey::FromStorageKey(QStringLiteral("triangle.test.apexOffset"));
    const PropertyKey second = PropertyKey::FromStorageKey(QStringLiteral("triangle.test.apexOffset"));
    ctx.Check(first == second && !first.IsCommon() && first.Id() >= 256,
              QStringLiteral("动态属性键按稳定字符串分配且可复取"), QString::number(first.Id()));

    const ItemPropertyDescriptor* descriptor =
        ItemPropertyCatalog::RegisterDynamic(first, QStringLiteral("测试顶点偏移"), ItemPropertyGroup::VALUE,
                                             ItemPropertyKind::REAL, 0.0, 100.0, 1.0, 1, QStringLiteral(" px"));
    const PropertyRowSpec* dynamicRow = PropertyRowSpecs::For(first);
    ctx.Check(descriptor && descriptor->key == first && dynamicRow && dynamicRow->Matches(ItemPropertySet{first}) &&
                  dynamicRow->Label() == QStringLiteral("测试顶点偏移"),
              QStringLiteral("动态属性可注册描述并自动取得行描述"));

    ItemPropertySet bulk;
    for (int index = 0; index < 70; ++index) {
        bulk.Insert(PropertyKey::FromStorageKey(QStringLiteral("test.bulk.%1").arg(index)));
    }
    bulk.Insert(ItemProperty::OPACITY);
    ctx.Check(bulk.Size() == 71 && bulk.Contains(ItemProperty::OPACITY) &&
                  bulk.Contains(PropertyKey::FromStorageKey(QStringLiteral("test.bulk.69"))),
              QStringLiteral("属性集合不再受 64 位上限约束"), QString::number(bulk.Size()));

    ShapeAbstractItem* triangle = ShapeFactory::Create(ShapeType::TRIANGLE);
    static qreal callbackValue = 5.0;
    const PropertyKey callbackKey = PropertyKey::FromStorageKey(QStringLiteral("triangle.test.callback"));
    ItemPropertyCatalog::RegisterDynamic(
        callbackKey, QStringLiteral("回调顶点偏移"), ItemPropertyGroup::VALUE, ItemPropertyKind::REAL, 0.0, 100.0, 1.0,
        1, QStringLiteral(" px"), [](const CanvasItem*) { return QVariant(callbackValue); },
        [](CanvasItem*, const QVariant& value) { callbackValue = value.toDouble(); });
    triangle->RegisterDynamicProperty(callbackKey, callbackValue);
    triangle->SetPropertyValueByKey(callbackKey, 9.0);
    ctx.Check(qAbs(triangle->PropertyValueByKey(callbackKey).toDouble() - 9.0) < 0.001,
              QStringLiteral("动态属性可通过描述符读写回调存取"));
    triangle->RegisterDynamicProperty(first, 8.0);
    ctx.Check(triangle->Properties().Contains(first) &&
                  qAbs(triangle->PropertyValueByKey(first).toDouble() - 8.0) < 0.001,
              QStringLiteral("三角形可注册测试参数且不改属性枚举"));
    triangle->SetPropertyValueByKey(first, 11.0);
    const QMap<PropertyKey, QVariant> snapshot = PropertyCommitService::Snapshot(triangle);
    ctx.Check(snapshot.contains(first) && qAbs(snapshot.value(first).toDouble() - 11.0) < 0.001,
              QStringLiteral("属性快照按属性键收录三角形测试参数"));
    ctx.Check(ItemPropertyCatalog::Find(first) != nullptr,
              QStringLiteral("动态属性键能查到描述符，编辑面板据此生成通用参数行"));
    delete triangle;
}
