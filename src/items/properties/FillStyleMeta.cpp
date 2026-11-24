#include "items/properties/FillStyleMeta.h"

namespace FillStyleMeta {

namespace {

const QList<Entry>& BuildRegistry()
{
    static const QList<Entry> REGISTRY = {
        {FillStyle::NONE,
         QStringLiteral("none"),
         QStringLiteral("No fill"),
         QStringLiteral("No fill"),
         false,
         {QStringLiteral("transparent")}},
        {FillStyle::SOLID,
         QStringLiteral("solid"),
         QStringLiteral("Solid"),
         QStringLiteral("Solid fill"),
         false,
         {QStringLiteral("color")}},
        {FillStyle::HACHURE,
         QStringLiteral("hachure"),
         QStringLiteral("Hatch"),
         QStringLiteral("Hatch fill"),
         true,
         {QStringLiteral("hatch"), QStringLiteral("diagonal"), QStringLiteral("horizontal_lines"),
          QStringLiteral("horizontal"), QStringLiteral("vertical_lines"), QStringLiteral("vertical")}},
        {FillStyle::REVERSE_HACHURE,
         QStringLiteral("reverse_hatch"),
         QStringLiteral("Reverse hatch"),
         QStringLiteral("Reverse hatch fill"),
         true,
         {QStringLiteral("reverse_hachure"), QStringLiteral("reverse"), QStringLiteral("backslash")}},
        {FillStyle::CROSS_HATCH,
         QStringLiteral("cross_hatch"),
         QStringLiteral("Cross hatch"),
         QStringLiteral("Cross hatch fill"),
         true,
         {QStringLiteral("crosshatch"), QStringLiteral("cross"), QStringLiteral("zigzag")}},
        {FillStyle::GRID_HATCH,
         QStringLiteral("grid_hatch"),
         QStringLiteral("Grid hatch"),
         QStringLiteral("Grid hatch fill"),
         true,
         {QStringLiteral("grid"), QStringLiteral("cross_grid"), QStringLiteral("orthogonal")}},
        {FillStyle::HONEYCOMB,
         QStringLiteral("honeycomb"),
         QStringLiteral("Hexagon"),
         QStringLiteral("Hexagon fill"),
         true,
         {QStringLiteral("hexagon"), QStringLiteral("hex")}},
        {FillStyle::BRICK,
         QStringLiteral("brick"),
         QStringLiteral("Brick"),
         QStringLiteral("Brick fill"),
         true,
         {QStringLiteral("brickwall")}},
        {FillStyle::DOTS,
         QStringLiteral("dots"),
         QStringLiteral("Dots"),
         QStringLiteral("Dots fill"),
         true,
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

QString ActionSource(FillStyle style)
{
    const Entry* entry = FindByStyle(style);
    return entry ? entry->actionSource : QStringLiteral("Solid fill");
}

} // namespace FillStyleMeta
