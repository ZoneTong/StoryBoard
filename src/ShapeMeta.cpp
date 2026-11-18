#include "ShapeMeta.h"

QList<ShapeType> ShapeMeta::allTypes()
{
    // 需求 11：新增 8 种；需求 13：花括号改为曲线，不再列入常见图形。
    return {ShapeType::Rectangle, ShapeType::Ellipse,     ShapeType::Triangle, ShapeType::Diamond, ShapeType::Star,
            ShapeType::Arrow,     ShapeType::DoubleArrow, ShapeType::Line,     ShapeType::Callout, ShapeType::Sector,
            ShapeType::Arc,       ShapeType::Star4,       ShapeType::Hexagram, ShapeType::Heart,   ShapeType::SineWave,
            ShapeType::FlagWave,  ShapeType::Scroll,      ShapeType::Cube,     ShapeType::Cylinder};
}

QString ShapeMeta::typeToString(ShapeType type)
{
    switch (type) {
    case ShapeType::Rectangle:
        return QStringLiteral("rectangle");
    case ShapeType::Ellipse:
        return QStringLiteral("ellipse");
    case ShapeType::Triangle:
        return QStringLiteral("triangle");
    case ShapeType::Diamond:
        return QStringLiteral("diamond");
    case ShapeType::Star:
        return QStringLiteral("star");
    case ShapeType::Arrow:
        return QStringLiteral("arrow");
    case ShapeType::DoubleArrow:
        return QStringLiteral("double_arrow");
    case ShapeType::Line:
        return QStringLiteral("line");
    case ShapeType::Callout:
        return QStringLiteral("callout");
    case ShapeType::Sector:
        return QStringLiteral("sector");
    case ShapeType::Arc:
        return QStringLiteral("arc");
    case ShapeType::Star4:
        return QStringLiteral("star4");
    case ShapeType::Hexagram:
        return QStringLiteral("hexagram");
    case ShapeType::Heart:
        return QStringLiteral("heart");
    case ShapeType::SineWave:
        return QStringLiteral("sine_wave");
    case ShapeType::FlagWave:
        return QStringLiteral("flag_wave");
    case ShapeType::Scroll:
        // 需求 3：卷形旗帜替换为卷轴；旧文件的 flag_curl / banner 兼容读入。
        return QStringLiteral("scroll");
    case ShapeType::Cube:
        return QStringLiteral("cube");
    case ShapeType::Cylinder:
        return QStringLiteral("cylinder");
    }
    return QStringLiteral("rectangle");
}

ShapeType ShapeMeta::typeFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("rounded_rectangle") || normalized == QStringLiteral("roundedrect") ||
        normalized == QStringLiteral("roundrect")) {
        // 需求 5：圆角矩形已并入矩形，旧字符串统一读成矩形（由 ShapeItem 记为自动圆角态）。
        return ShapeType::Rectangle;
    }
    if (normalized == QStringLiteral("ellipse") || normalized == QStringLiteral("circle") ||
        normalized == QStringLiteral("oval")) {
        return ShapeType::Ellipse;
    }
    if (normalized == QStringLiteral("triangle")) {
        return ShapeType::Triangle;
    }
    if (normalized == QStringLiteral("diamond") || normalized == QStringLiteral("rhombus")) {
        return ShapeType::Diamond;
    }
    if (normalized == QStringLiteral("star")) {
        return ShapeType::Star;
    }
    if (normalized == QStringLiteral("arrow")) {
        return ShapeType::Arrow;
    }
    if (normalized == QStringLiteral("double_arrow") || normalized == QStringLiteral("doublearrow") ||
        normalized == QStringLiteral("bidirectional")) {
        return ShapeType::DoubleArrow;
    }
    if (normalized == QStringLiteral("line")) {
        return ShapeType::Line;
    }
    if (normalized == QStringLiteral("callout") || normalized == QStringLiteral("bubble") ||
        normalized == QStringLiteral("speech")) {
        return ShapeType::Callout;
    }
    if (normalized == QStringLiteral("sector") || normalized == QStringLiteral("pie")) {
        return ShapeType::Sector;
    }
    if (normalized == QStringLiteral("arc") || normalized == QStringLiteral("curve")) {
        return ShapeType::Arc;
    }
    if (normalized == QStringLiteral("star4") || normalized == QStringLiteral("four_point_star") ||
        normalized == QStringLiteral("fourpointstar")) {
        return ShapeType::Star4;
    }
    if (normalized == QStringLiteral("hexagram") || normalized == QStringLiteral("david_star") ||
        normalized == QStringLiteral("six_point_star")) {
        return ShapeType::Hexagram;
    }
    if (normalized == QStringLiteral("heart") || normalized == QStringLiteral("love")) {
        return ShapeType::Heart;
    }
    if (normalized == QStringLiteral("sine_wave") || normalized == QStringLiteral("sine") ||
        normalized == QStringLiteral("wave")) {
        return ShapeType::SineWave;
    }
    if (normalized == QStringLiteral("flag_wave") || normalized == QStringLiteral("wavy_flag")) {
        return ShapeType::FlagWave;
    }
    if (normalized == QStringLiteral("scroll") || normalized == QStringLiteral("flag_curl") ||
        normalized == QStringLiteral("banner")) {
        return ShapeType::Scroll;
    }
    if (normalized == QStringLiteral("cube")) {
        return ShapeType::Cube;
    }
    if (normalized == QStringLiteral("cylinder")) {
        return ShapeType::Cylinder;
    }
    if (normalized == QStringLiteral("rect") || normalized == QStringLiteral("rectangle")) {
        return ShapeType::Rectangle;
    }
    return ShapeType::Rectangle;
}

QSizeF ShapeMeta::defaultSize(ShapeType type)
{
    // 统一基准（需求 16）：棋盘格 1 格 = 64px，基准边长 = 2 格 = 128px。
    // 所有常见图形都由 128 派生：正方形 / 圆直接 128×128，其它按原比例放大一倍。
    switch (type) {
    case ShapeType::Rectangle:
        // 插入即为 2 格的正方形，需要长方形再拖控制点拉伸。
        return QSizeF(128.0, 128.0);
    case ShapeType::Ellipse:
        // 插入即为标准圆：直径 = 基准边长 128。
        return QSizeF(128.0, 128.0);
    case ShapeType::Triangle:
        // 正三角形：底 128，高 = √3/2 × 128。
        return QSizeF(128.0, 111.0);
    case ShapeType::Diamond:
        return QSizeF(128.0, 100.0);
    case ShapeType::Star:
        return QSizeF(128.0, 128.0);
    case ShapeType::Arrow:
        return QSizeF(128.0, 46.0);
    case ShapeType::DoubleArrow:
        return QSizeF(128.0, 40.0);
    case ShapeType::Line:
        // 线长 = 基准边长 128，格高取半格 64，避免默认高度大于长度。
        return QSizeF(128.0, 64.0);
    case ShapeType::Callout:
        return QSizeF(128.0, 86.0);
    case ShapeType::Sector:
        return QSizeF(128.0, 128.0);
    case ShapeType::Arc:
        return QSizeF(128.0, 94.0);
    case ShapeType::Star4:
        return QSizeF(128.0, 128.0);
    case ShapeType::Hexagram:
        return QSizeF(128.0, 112.0);
    case ShapeType::Heart:
        return QSizeF(128.0, 112.0);
    case ShapeType::SineWave:
        // 波线：一个完整周期，振幅 20% 高。
        return QSizeF(128.0, 40.0);
    case ShapeType::FlagWave:
        return QSizeF(128.0, 86.0);
    case ShapeType::Scroll:
        // 卷轴：横长比例，左右卷筒占宽约 14%。
        return QSizeF(128.0, 96.0);
    case ShapeType::Cube:
        return QSizeF(128.0, 128.0);
    case ShapeType::Cylinder:
        return QSizeF(104.0, 128.0);
    }
    return QSizeF(128.0, 128.0);
}

bool ShapeMeta::isFilled(ShapeType type)
{
    switch (type) {
    case ShapeType::Rectangle:
    case ShapeType::Ellipse:
    case ShapeType::Triangle:
    case ShapeType::Diamond:
    case ShapeType::Star:
    case ShapeType::Arrow:
    case ShapeType::DoubleArrow:
    case ShapeType::Callout:
    case ShapeType::Sector:
    case ShapeType::Star4:
    case ShapeType::Hexagram:
    case ShapeType::Heart:
    case ShapeType::FlagWave:
    case ShapeType::Scroll:
    case ShapeType::Cube:
    case ShapeType::Cylinder:
        return true;
    case ShapeType::Line:
    case ShapeType::Arc:
    case ShapeType::SineWave:
        return false;
    }
    return false;
}

bool ShapeMeta::isStrokeOnly(ShapeType type)
{
    // 正弦波形是零面积波线，与直线、弧线同属纯描边。
    return type == ShapeType::Line || type == ShapeType::Arc || type == ShapeType::SineWave;
}

QList<FillStyle> ShapeMeta::allStyles()
{
    return {FillStyle::None,       FillStyle::Solid,     FillStyle::Hachure,  FillStyle::ReverseHachure,
            FillStyle::CrossHatch, FillStyle::GridHatch, FillStyle::Honeycomb};
}

QString ShapeMeta::styleToString(FillStyle style)
{
    switch (style) {
    case FillStyle::None:
        return QStringLiteral("none");
    case FillStyle::Solid:
        return QStringLiteral("solid");
    case FillStyle::Hachure:
        return QStringLiteral("hachure");
    case FillStyle::ReverseHachure:
        return QStringLiteral("reverse_hatch");
    case FillStyle::CrossHatch:
        return QStringLiteral("cross_hatch");
    case FillStyle::GridHatch:
        return QStringLiteral("grid_hatch");
    case FillStyle::Honeycomb:
        return QStringLiteral("honeycomb");
    }
    return QStringLiteral("solid");
}

FillStyle ShapeMeta::styleFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("none") || normalized == QStringLiteral("transparent")) {
        return FillStyle::None;
    }
    if (normalized == QStringLiteral("hachure") || normalized == QStringLiteral("hatch") ||
        normalized == QStringLiteral("diagonal")) {
        return FillStyle::Hachure;
    }
    if (normalized == QStringLiteral("reverse_hatch") || normalized == QStringLiteral("reverse_hachure") ||
        normalized == QStringLiteral("reverse") || normalized == QStringLiteral("backslash")) {
        return FillStyle::ReverseHachure;
    }
    if (normalized == QStringLiteral("cross_hatch") || normalized == QStringLiteral("crosshatch") ||
        normalized == QStringLiteral("cross") || normalized == QStringLiteral("zigzag")) {
        return FillStyle::CrossHatch;
    }
    if (normalized == QStringLiteral("grid_hatch") || normalized == QStringLiteral("grid") ||
        normalized == QStringLiteral("cross_grid") || normalized == QStringLiteral("orthogonal")) {
        return FillStyle::GridHatch;
    }
    if (normalized == QStringLiteral("honeycomb") || normalized == QStringLiteral("hexagon") ||
        normalized == QStringLiteral("hex")) {
        return FillStyle::Honeycomb;
    }
    if (normalized == QStringLiteral("solid") || normalized == QStringLiteral("color")) {
        return FillStyle::Solid;
    }
    return FillStyle::Solid;
}
