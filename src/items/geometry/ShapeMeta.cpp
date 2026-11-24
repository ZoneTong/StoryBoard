#include "items/geometry/ShapeMeta.h"

#include "items/properties/FillStyleMeta.h"
#include "items/shapes/ShapeFactory.h"

#include <algorithm>

namespace {

// 描述器里的兼容字符串以空格分隔，逐词与规范化后的输入比对。
bool MatchesAliases(const char* aliases, const QString& normalized)
{
    if (!aliases) {
        return false;
    }
    const QStringList tokens = QString::fromLatin1(aliases).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    return tokens.contains(normalized);
}

} // namespace

QList<ShapeType> ShapeMeta::AllTypes()
{
    // 需求 7：菜单与工具栏「常见图形」的顺位来自描述器 menuOrder（唯一顺序源）。
    QList<ShapeType> types;
    for (const ShapeFactory::Entry& entry : ShapeFactory::Entries()) {
        if (entry.descriptor.menuOrder >= 0) {
            types.append(entry.descriptor.type);
        }
    }
    std::sort(types.begin(), types.end(), [](ShapeType a, ShapeType b) {
        return ShapeFactory::Find(a)->menuOrder < ShapeFactory::Find(b)->menuOrder;
    });
    return types;
}

QString ShapeMeta::TypeToString(ShapeType type)
{
    const ShapeFactory::Descriptor* descriptor = ShapeFactory::Find(type);
    return descriptor ? QString::fromLatin1(descriptor->json) : QStringLiteral("rectangle");
}

ShapeType ShapeMeta::TypeFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    for (const ShapeFactory::Entry& entry : ShapeFactory::Entries()) {
        const ShapeFactory::Descriptor& descriptor = entry.descriptor;
        if (normalized == QString::fromLatin1(descriptor.json) || MatchesAliases(descriptor.aliases, normalized)) {
            return descriptor.type;
        }
    }
    return ShapeType::RECTANGLE;
}

bool ShapeMeta::IsFilled(ShapeType type)
{
    const ShapeFactory::Descriptor* descriptor = ShapeFactory::Find(type);
    return descriptor && descriptor->filled;
}

bool ShapeMeta::SupportsCenter(ShapeType type)
{
    // §3.4 中心点：描述器里显式标记的对称图形才上报这一行。
    const ShapeFactory::Descriptor* descriptor = ShapeFactory::Find(type);
    return descriptor && descriptor->supportsCenter;
}

QList<FillStyle> ShapeMeta::AllStyles()
{
    return FillStyleMeta::Styles();
}

QString ShapeMeta::StyleToString(FillStyle style)
{
    return FillStyleMeta::ToString(style);
}

FillStyle ShapeMeta::StyleFromString(const QString& value)
{
    return FillStyleMeta::FromString(value);
}
