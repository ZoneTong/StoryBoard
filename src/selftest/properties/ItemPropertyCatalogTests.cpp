#include "items/properties/ItemPropertyCatalog.h"
#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"

#include <QSet>
#include <QString>

void selftest::RunItemPropertyCatalogTests(Context& ctx)
{
    const QList<ItemPropertyDescriptor*>& descriptors = ItemPropertyCatalog::All();
    ctx.Check(descriptors.size() == 30, QStringLiteral("属性描述符覆盖全部 ItemProperty"),
              QString::number(descriptors.size()));

    QSet<QString> keys;
    bool uniqueKeys = true;
    bool labelsPresent = true;
    bool rangesValid = true;
    bool derivedCorrect = true;
    bool callbacksPresent = true;
    for (const ItemPropertyDescriptor* descriptor : descriptors) {
        const QString key = descriptor->storageKey;
        if (key.isEmpty() || keys.contains(key)) {
            uniqueKeys = false;
        }
        keys.insert(key);
        if (ItemPropertyCatalog::Label(descriptor->key).isEmpty()) {
            labelsPresent = false;
        }
        if (descriptor->kind == ItemPropertyKind::INTEGER || descriptor->kind == ItemPropertyKind::REAL) {
            if (descriptor->maximum < descriptor->minimum || descriptor->step <= 0.0) {
                rangesValid = false;
            }
        }
        const bool expectedDerived = descriptor->key == ItemProperty::MAJOR_AXIS ||
                                     descriptor->key == ItemProperty::MINOR_AXIS ||
                                     descriptor->key == ItemProperty::CENTER;
        if (descriptor->derived != expectedDerived) {
            derivedCorrect = false;
        }
        if (!descriptor->reader || !descriptor->writer) {
            callbacksPresent = false;
        }
        if (ItemPropertyCatalog::Find(descriptor->key) != descriptor) {
            labelsPresent = false;
        }
    }

    ctx.Check(uniqueKeys, QStringLiteral("属性描述符 key 非空且唯一"));
    ctx.Check(labelsPresent, QStringLiteral("每个属性都有标签且可反查"));
    ctx.Check(rangesValid, QStringLiteral("数值属性的范围与步长有效"));
    ctx.Check(derivedCorrect, QStringLiteral("派生属性只标记长短半轴与中心点"));
    ctx.Check(callbacksPresent, QStringLiteral("全部属性描述符均具备读写回调"));

    // 中文标签必须走属性窗上下文，手工维护的 storyboard_zh.ts 才不会因改上下文丢译文。
    ctx.Check(ItemPropertyCatalog::Label(ItemProperty::NAME) == QStringLiteral("图元名称"),
              QStringLiteral("属性描述符标签装载中文译文"), ItemPropertyCatalog::Label(ItemProperty::NAME));
    ctx.Check(ItemPropertyCatalog::Key(ItemProperty::CENTER) == QStringLiteral("center"),
              QStringLiteral("属性 key 可供 C3b 迁移使用"), ItemPropertyCatalog::Key(ItemProperty::CENTER));
}
