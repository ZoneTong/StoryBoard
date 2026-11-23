#include "items/properties/ItemProperties.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "items/shapes/ShapeRegistry.h"
#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"
#include "ui/panels/properties/PropertyCommitService.h"

#include <QSet>

void selftest::RunPropertyKeyTests(Context& ctx)
{
    const PropertyKey opacity(ItemProperty::OPACITY);
    ctx.Check(opacity.IsCommon() && opacity.Id() == static_cast<int>(ItemProperty::OPACITY),
              QStringLiteral("公共枚举映射为固定属性键"));

    const PropertyKey first = PropertyKey::FromStorageKey(QStringLiteral("triangle.test.apexOffset"));
    const PropertyKey second = PropertyKey::FromStorageKey(QStringLiteral("triangle.test.apexOffset"));
    ctx.Check(first == second && !first.IsCommon() && first.Id() >= 256,
              QStringLiteral("动态属性键按稳定字符串分配且可复取"), QString::number(first.Id()));

    ItemPropertySet bulk;
    for (int index = 0; index < 70; ++index) {
        bulk.Insert(PropertyKey::FromStorageKey(QStringLiteral("test.bulk.%1").arg(index)));
    }
    bulk.Insert(ItemProperty::OPACITY);
    ctx.Check(bulk.Size() == 71 && bulk.Contains(ItemProperty::OPACITY) &&
                  bulk.Contains(PropertyKey::FromStorageKey(QStringLiteral("test.bulk.69"))),
              QStringLiteral("属性集合不再受 64 位上限约束"), QString::number(bulk.Size()));

    ShapeAbstractItem* triangle = ShapeRegistry::Create(ShapeType::TRIANGLE);
    triangle->RegisterDynamicProperty(first, 8.0);
    ctx.Check(triangle->Properties().Contains(first) &&
                  qAbs(triangle->PropertyValueByKey(first).toDouble() - 8.0) < 0.001,
              QStringLiteral("三角形可注册测试参数且不改属性枚举"));
    triangle->SetPropertyValueByKey(first, 11.0);
    const QMap<PropertyKey, QVariant> snapshot = PropertyCommitService::Snapshot(triangle);
    ctx.Check(snapshot.contains(first) && qAbs(snapshot.value(first).toDouble() - 11.0) < 0.001,
              QStringLiteral("属性快照按属性键收录三角形测试参数"));
    delete triangle;
}
