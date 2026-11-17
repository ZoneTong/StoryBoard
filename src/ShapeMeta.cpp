#include "ShapeMeta.h"

QList<ShapeType> ShapeMeta::allTypes()
{
    return {ShapeType::Rectangle,  ShapeType::RoundedRectangle,
            ShapeType::Ellipse,    ShapeType::Triangle,
            ShapeType::Diamond,    ShapeType::Star,
            ShapeType::Arrow,      ShapeType::DoubleArrow,
            ShapeType::Line,       ShapeType::BraceLeft,
            ShapeType::BraceRight, ShapeType::Callout,
            ShapeType::Sector,     ShapeType::Arc};
}

QString ShapeMeta::typeToString(ShapeType type)
{
    switch (type) {
    case ShapeType::Rectangle:
        return QStringLiteral("rectangle");
    case ShapeType::RoundedRectangle:
        return QStringLiteral("rounded_rectangle");
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
    case ShapeType::BraceLeft:
        return QStringLiteral("brace_left");
    case ShapeType::BraceRight:
        return QStringLiteral("brace_right");
    case ShapeType::Callout:
        return QStringLiteral("callout");
    case ShapeType::Sector:
        return QStringLiteral("sector");
    case ShapeType::Arc:
        return QStringLiteral("arc");
    }
    return QStringLiteral("rectangle");
}

ShapeType ShapeMeta::typeFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("rounded_rectangle") || normalized == QStringLiteral("roundedrect") ||
        normalized == QStringLiteral("roundrect")) {
        return ShapeType::RoundedRectangle;
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
    if (normalized == QStringLiteral("brace_left") || normalized == QStringLiteral("leftbrace") ||
        normalized == QStringLiteral("braceleft")) {
        return ShapeType::BraceLeft;
    }
    if (normalized == QStringLiteral("brace_right") || normalized == QStringLiteral("rightbrace") ||
        normalized == QStringLiteral("braceright")) {
        return ShapeType::BraceRight;
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
    if (normalized == QStringLiteral("rect") || normalized == QStringLiteral("rectangle")) {
        return ShapeType::Rectangle;
    }
    return ShapeType::Rectangle;
}

QSizeF ShapeMeta::defaultSize(ShapeType type)
{
    // 统一基准：1 格 = 32px，基准边长 = 2 格 = 64px。
    // 所有常见图形的默认尺寸由 64 派生，长边或基准边为 64，圆 / 正方形直接 64×64。
    switch (type) {
    case ShapeType::Rectangle:
    case ShapeType::RoundedRectangle:
        // 插入即为 2 格的正方形，需要长方形再拖控制点拉伸。
        return QSizeF(64.0, 64.0);
    case ShapeType::Ellipse:
        // 插入即为标准圆：直径 = 基准边长 64。
        return QSizeF(64.0, 64.0);
    case ShapeType::Triangle:
        // 正三角形：底 64，高 = √3/2 × 64。
        return QSizeF(64.0, 55.0);
    case ShapeType::Diamond:
        return QSizeF(64.0, 50.0);
    case ShapeType::Star:
        return QSizeF(64.0, 64.0);
    case ShapeType::Arrow:
        return QSizeF(64.0, 23.0);
    case ShapeType::DoubleArrow:
        return QSizeF(64.0, 20.0);
    case ShapeType::Line:
        // 线长 = 基准边长 64，格高取半格 32，避免默认高度大于长度。
        return QSizeF(64.0, 32.0);
    case ShapeType::BraceLeft:
    case ShapeType::BraceRight:
        return QSizeF(21.0, 64.0);
    case ShapeType::Callout:
        return QSizeF(64.0, 43.0);
    case ShapeType::Sector:
        return QSizeF(64.0, 64.0);
    case ShapeType::Arc:
        return QSizeF(64.0, 47.0);
    }
    return QSizeF(64.0, 64.0);
}

bool ShapeMeta::isFilled(ShapeType type)
{
    switch (type) {
    case ShapeType::Rectangle:
    case ShapeType::RoundedRectangle:
    case ShapeType::Ellipse:
    case ShapeType::Triangle:
    case ShapeType::Diamond:
    case ShapeType::Star:
    case ShapeType::Arrow:
    case ShapeType::DoubleArrow:
    case ShapeType::Callout:
    case ShapeType::Sector:
        return true;
    case ShapeType::Line:
    case ShapeType::BraceLeft:
    case ShapeType::BraceRight:
    case ShapeType::Arc:
        return false;
    }
    return false;
}

bool ShapeMeta::isStrokeOnly(ShapeType type)
{
    return type == ShapeType::Line || type == ShapeType::BraceLeft || type == ShapeType::BraceRight ||
           type == ShapeType::Arc;
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
