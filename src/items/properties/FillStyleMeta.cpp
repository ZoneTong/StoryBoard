#include "items/properties/FillStyleMeta.h"

namespace FillStyleMeta {

namespace {

const QList<Entry>& BuildRegistry()
{
    static const QList<Entry> REGISTRY = {
        {FillStyle::NONE, QStringLiteral("none"), QStringLiteral("No fill"), false,
         {QStringLiteral("transparent")}},
        {FillStyle::SOLID, QStringLiteral("solid"), QStringLiteral("Solid"), false,
         {QStringLiteral("color")}},
        {FillStyle::HACHURE, QStringLiteral("hachure"), QStringLiteral("Hatch"), true,
         {QStringLiteral("hatch"), QStringLiteral("diagonal"), QStringLiteral("horizontal_lines"),
          QStringLiteral("horizontal"), QStringLiteral("vertical_lines"), QStringLiteral("vertical")}},
        {FillStyle::REVERSE_HACHURE, QStringLiteral("reverse_hatch"), QStringLiteral("Reverse hatch"), true,
         {QStringLiteral("reverse_hachure"), QStringLiteral("reverse"), QStringLiteral("backslash")}},
        {FillStyle::CROSS_HATCH, QStringLiteral("cross_hatch"), QStringLiteral("Cross hatch"), true,
         {QStringLiteral("crosshatch"), QStringLiteral("cross"), QStringLiteral("zigzag")}},
        {FillStyle::GRID_HATCH, QStringLiteral("grid_hatch"), QStringLiteral("Grid hatch"), true,
         {QStringLiteral("grid"), QStringLiteral("cross_grid"), QStringLiteral("orthogonal")}},
        {FillStyle::HONEYCOMB, QStringLiteral("honeycomb"), QStringLiteral("Hexagon"), true,
         {QStringLiteral("hexagon"), QStringLiteral("hex")}},
        {FillStyle::BRICK, QStringLiteral("brick"), QStringLiteral("Brick"), true,
         {QStringLiteral("brickwall")}},
        {FillStyle::DOTS, QStringLiteral("dots"), QStringLiteral("Dots"), true,
         {QStringLiteral("dot"), QStringLiteral("point")}},
    };
    return REGISTRY;
}

const Entry* FindByStyle(FillStyle style)
{
    for (const Entry& entry : Registry()) {
        if (entry.style == style) {
            return &entry;
        }
    }
    return nullptr;
}

} // namespace

const QList<Entry>& Registry()
{
    return BuildRegistry();
}

QList<FillStyle> Styles()
{
    QList<FillStyle> styles;
    for (const Entry& entry : Registry()) {
        styles.append(entry.style);
    }
    return styles;
}

QString ToString(FillStyle style)
{
    const Entry* entry = FindByStyle(style);
    return entry ? entry->key : QStringLiteral("solid");
}

FillStyle FromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    for (const Entry& entry : Registry()) {
        if (entry.key == normalized || entry.aliases.contains(normalized)) {
            return entry.style;
        }
    }
    return FillStyle::SOLID;
}

bool IsPattern(FillStyle style)
{
    const Entry* entry = FindByStyle(style);
    return entry && entry->isPattern;
}

QString NameSource(FillStyle style)
{
    const Entry* entry = FindByStyle(style);
    return entry ? entry->nameSource : QStringLiteral("Solid");
}

} // namespace FillStyleMeta
