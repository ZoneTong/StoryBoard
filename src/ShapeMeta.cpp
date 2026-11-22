#include "ShapeMeta.h"

QList<ShapeType> ShapeMeta::AllTypes()
{
    // 需求 7：菜单与工具栏「常见图形」共用这一份常用顺序表（唯一顺序源）。
    // 需求 9：菱形紧跟平行四边形，方便和立方体三个面归在一处认图。
    return {ShapeType::RECTANGLE, ShapeType::ELLIPSE,  ShapeType::ARROW,         ShapeType::DOUBLE_ARROW,
            ShapeType::TRIANGLE,  ShapeType::CALLOUT,  ShapeType::PARALLELOGRAM, ShapeType::DIAMOND,
            ShapeType::SECTOR,    ShapeType::CUBE,     ShapeType::CYLINDER,      ShapeType::STAR4,
            ShapeType::STAR,      ShapeType::HEXAGRAM, ShapeType::HEART,         ShapeType::FLAG_WAVE,
            ShapeType::SCROLL};
}

QString ShapeMeta::TypeToString(ShapeType type)
{
    switch (type) {
    case ShapeType::RECTANGLE:
        return QStringLiteral("rectangle");
    case ShapeType::ELLIPSE:
        return QStringLiteral("ellipse");
    case ShapeType::TRIANGLE:
        return QStringLiteral("triangle");
    case ShapeType::STAR:
        return QStringLiteral("star");
    case ShapeType::ARROW:
        return QStringLiteral("arrow");
    case ShapeType::DOUBLE_ARROW:
        return QStringLiteral("double_arrow");

    case ShapeType::CALLOUT:
        return QStringLiteral("callout");
    case ShapeType::SECTOR:
        return QStringLiteral("sector");

    case ShapeType::STAR4:
        return QStringLiteral("star4");
    case ShapeType::HEXAGRAM:
        return QStringLiteral("hexagram");
    case ShapeType::HEART:
        return QStringLiteral("heart");

    case ShapeType::FLAG_WAVE:
        return QStringLiteral("flag_wave");
    case ShapeType::SCROLL:
        // 需求 3：卷形旗帜替换为卷轴；旧文件的 flag_curl / banner 兼容读入。
        return QStringLiteral("scroll");
    case ShapeType::CUBE:
        return QStringLiteral("cube");
    case ShapeType::CYLINDER:
        return QStringLiteral("cylinder");
    case ShapeType::PARALLELOGRAM:
        return QStringLiteral("parallelogram");
    case ShapeType::DIAMOND:
        return QStringLiteral("diamond");
    }
    return QStringLiteral("rectangle");
}

ShapeType ShapeMeta::TypeFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("rounded_rectangle") || normalized == QStringLiteral("roundedrect") ||
        normalized == QStringLiteral("roundrect")) {
        // 需求 5：圆角矩形已并入矩形，旧字符串统一读成矩形（由 ShapeAbstractItem 记为自动圆角态）。
        return ShapeType::RECTANGLE;
    }
    if (normalized == QStringLiteral("ellipse") || normalized == QStringLiteral("circle") ||
        normalized == QStringLiteral("oval")) {
        return ShapeType::ELLIPSE;
    }
    if (normalized == QStringLiteral("triangle")) {
        return ShapeType::TRIANGLE;
    }
    if (normalized == QStringLiteral("diamond") || normalized == QStringLiteral("rhombus")) {
        // 菱形已是独立类型（DiamondItem）；旧文件里写成 parallelogram + 45° 旋转的预置
        // 仍是平行四边形，按自身字段读回，不受这里影响。
        return ShapeType::DIAMOND;
    }
    if (normalized == QStringLiteral("star")) {
        return ShapeType::STAR;
    }
    if (normalized == QStringLiteral("arrow")) {
        return ShapeType::ARROW;
    }
    if (normalized == QStringLiteral("double_arrow") || normalized == QStringLiteral("doublearrow") ||
        normalized == QStringLiteral("bidirectional")) {
        return ShapeType::DOUBLE_ARROW;
    }

    if (normalized == QStringLiteral("callout") || normalized == QStringLiteral("bubble") ||
        normalized == QStringLiteral("speech")) {
        return ShapeType::CALLOUT;
    }
    if (normalized == QStringLiteral("sector") || normalized == QStringLiteral("pie")) {
        return ShapeType::SECTOR;
    }

    if (normalized == QStringLiteral("star4") || normalized == QStringLiteral("four_point_star") ||
        normalized == QStringLiteral("fourpointstar")) {
        return ShapeType::STAR4;
    }
    if (normalized == QStringLiteral("hexagram") || normalized == QStringLiteral("david_star") ||
        normalized == QStringLiteral("six_point_star")) {
        return ShapeType::HEXAGRAM;
    }
    if (normalized == QStringLiteral("heart") || normalized == QStringLiteral("love")) {
        return ShapeType::HEART;
    }

    if (normalized == QStringLiteral("flag_wave") || normalized == QStringLiteral("wavy_flag")) {
        return ShapeType::FLAG_WAVE;
    }
    if (normalized == QStringLiteral("scroll") || normalized == QStringLiteral("flag_curl") ||
        normalized == QStringLiteral("banner")) {
        return ShapeType::SCROLL;
    }
    if (normalized == QStringLiteral("cube")) {
        return ShapeType::CUBE;
    }
    if (normalized == QStringLiteral("cylinder")) {
        return ShapeType::CYLINDER;
    }
    if (normalized == QStringLiteral("parallelogram") || normalized == QStringLiteral("rhomboid")) {
        return ShapeType::PARALLELOGRAM;
    }
    if (normalized == QStringLiteral("rect") || normalized == QStringLiteral("rectangle")) {
        return ShapeType::RECTANGLE;
    }
    return ShapeType::RECTANGLE;
}

bool ShapeMeta::IsFilled(ShapeType type)
{
    switch (type) {
    case ShapeType::RECTANGLE:
    case ShapeType::ELLIPSE:
    case ShapeType::TRIANGLE:
    case ShapeType::STAR:
    case ShapeType::ARROW:
    case ShapeType::DOUBLE_ARROW:
    case ShapeType::CALLOUT:
    case ShapeType::SECTOR:
    case ShapeType::STAR4:
    case ShapeType::HEXAGRAM:
    case ShapeType::HEART:
    case ShapeType::FLAG_WAVE:
    case ShapeType::SCROLL:
    case ShapeType::CUBE:
    case ShapeType::CYLINDER:
    case ShapeType::PARALLELOGRAM:
    case ShapeType::DIAMOND:
        return true;
    }
    return false;
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
