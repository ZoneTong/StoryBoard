#include "IconMergeIcons.h"

#include <QPainter>
#include <QPainterPath>

namespace {

void drawMerge(QPainter& painter, IconId id)
{
    // PPT「合并形状」图标语言：两个交叠的圆，用实心区域表示运算结果。
    QPainterPath left;
    left.addEllipse(QRectF(2.5, 4.5, 13.0, 13.0));
    QPainterPath right;
    right.addEllipse(QRectF(8.5, 4.5, 13.0, 13.0));
    const QPainterPath merged = left.united(right);
    const QPainterPath lens = left.intersected(right);
    const QColor color = painter.pen().color();

    auto fillSolid = [&painter, &color](const QPainterPath& path) {
        painter.setBrush(color);
        painter.setPen(QPen(color, 1.0));
        painter.drawPath(path);
    };
    auto outline = [&painter, &color](const QPainterPath& path, qreal width) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
    };

    if (id == IconId::MergeUnion) {
        fillSolid(merged);
    } else if (id == IconId::MergeCombine) {
        fillSolid(merged.subtracted(lens));
    } else if (id == IconId::MergeFragment) {
        outline(left, 1.5);
        outline(right, 1.5);
        outline(lens, 1.2);
    } else if (id == IconId::MergeIntersect) {
        fillSolid(lens);
    } else {
        fillSolid(left.subtracted(right));
        outline(right, 1.4);
    }
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(color, 1.9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
}

constexpr struct IconEntry {
    IconId id;
    void (*draw)(QPainter&, IconId);
} kEntries[] = {
    {IconId::MergeUnion, drawMerge},     {IconId::MergeCombine, drawMerge},  {IconId::MergeFragment, drawMerge},
    {IconId::MergeIntersect, drawMerge}, {IconId::MergeSubtract, drawMerge},
};

} // namespace

bool paintIconMerge(QPainter& painter, IconId id)
{
    for (const IconEntry& entry : kEntries) {
        if (entry.id == id) {
            entry.draw(painter, id);
            return true;
        }
    }
    return false;
}
