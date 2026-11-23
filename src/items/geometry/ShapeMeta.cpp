#include "items/geometry/ShapeMeta.h"

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
    return {FillStyle::NONE,           FillStyle::SOLID,      FillStyle::HACHURE,   FillStyle::REVERSE_HACHURE,
            FillStyle::CROSS_HATCH,    FillStyle::GRID_HATCH, FillStyle::HONEYCOMB, FillStyle::HORIZONTAL_HATCH,
            FillStyle::VERTICAL_HATCH, FillStyle::BRICK,      FillStyle::DOTS};
}

QString ShapeMeta::StyleToString(FillStyle style)
{
    switch (style) {
    case FillStyle::NONE:
        return QStringLiteral("none");
    case FillStyle::SOLID:
        return QStringLiteral("solid");
    case FillStyle::HACHURE:
        return QStringLiteral("hachure");
    case FillStyle::REVERSE_HACHURE:
        return QStringLiteral("reverse_hatch");
    case FillStyle::CROSS_HATCH:
        return QStringLiteral("cross_hatch");
    case FillStyle::GRID_HATCH:
        return QStringLiteral("grid_hatch");
    case FillStyle::HONEYCOMB:
        return QStringLiteral("honeycomb");
    case FillStyle::HORIZONTAL_HATCH:
        return QStringLiteral("horizontal_lines");
    case FillStyle::VERTICAL_HATCH:
        return QStringLiteral("vertical_lines");
    case FillStyle::BRICK:
        return QStringLiteral("brick");
    case FillStyle::DOTS:
        return QStringLiteral("dots");
    }
    return QStringLiteral("solid");
}

FillStyle ShapeMeta::StyleFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("none") || normalized == QStringLiteral("transparent")) {
        return FillStyle::NONE;
    }
    if (normalized == QStringLiteral("hachure") || normalized == QStringLiteral("hatch") ||
        normalized == QStringLiteral("diagonal")) {
        return FillStyle::HACHURE;
    }
    if (normalized == QStringLiteral("reverse_hatch") || normalized == QStringLiteral("reverse_hachure") ||
        normalized == QStringLiteral("reverse") || normalized == QStringLiteral("backslash")) {
        return FillStyle::REVERSE_HACHURE;
    }
    if (normalized == QStringLiteral("cross_hatch") || normalized == QStringLiteral("crosshatch") ||
        normalized == QStringLiteral("cross") || normalized == QStringLiteral("zigzag")) {
        return FillStyle::CROSS_HATCH;
    }
    if (normalized == QStringLiteral("grid_hatch") || normalized == QStringLiteral("grid") ||
        normalized == QStringLiteral("cross_grid") || normalized == QStringLiteral("orthogonal")) {
        return FillStyle::GRID_HATCH;
    }
    if (normalized == QStringLiteral("honeycomb") || normalized == QStringLiteral("hexagon") ||
        normalized == QStringLiteral("hex")) {
        return FillStyle::HONEYCOMB;
    }
    if (normalized == QStringLiteral("horizontal_lines") || normalized == QStringLiteral("horizontal")) {
        return FillStyle::HORIZONTAL_HATCH;
    }
    if (normalized == QStringLiteral("vertical_lines") || normalized == QStringLiteral("vertical")) {
        return FillStyle::VERTICAL_HATCH;
    }
    if (normalized == QStringLiteral("brick") || normalized == QStringLiteral("brickwall")) {
        return FillStyle::BRICK;
    }
    if (normalized == QStringLiteral("dots") || normalized == QStringLiteral("dot") ||
        normalized == QStringLiteral("point")) {
        return FillStyle::DOTS;
    }
    if (normalized == QStringLiteral("solid") || normalized == QStringLiteral("color")) {
        return FillStyle::SOLID;
    }
    return FillStyle::SOLID;
}
