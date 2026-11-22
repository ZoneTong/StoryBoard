#include "items/shapes/HexagonItem.h"

#include "items/geometry/ShapePathBuilder.h"

HexagonItem::HexagonItem()
{
    SetContentSize(DefaultContentSize());
}

QSizeF HexagonItem::DefaultContentSize() const
{
    // 平顶正六边形的默认比例：宽 = 2 格（128），高 ≈ 宽 × √3/2 ≈ 111。
    return QSizeF(128.0, 111.0);
}

QPainterPath HexagonItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::HexagonPath(rect);
}
