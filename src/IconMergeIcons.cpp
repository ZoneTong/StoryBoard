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

    if (id == IconId::MERGE_UNION) {
        fillSolid(merged);
    } else if (id == IconId::MERGE_COMBINE) {
        fillSolid(merged.subtracted(lens));
    } else if (id == IconId::MERGE_FRAGMENT) {
        outline(left, 1.5);
        outline(right, 1.5);
        outline(lens, 1.2);
    } else if (id == IconId::MERGE_INTERSECT) {
        fillSolid(lens);
    } else if (id == IconId::MERGE_LINE_SPLIT) {
        // 线条分割：一条斜刀线把图形切开，只实心刀线一侧的结果。
        QPainterPath knifeLine;
        knifeLine.moveTo(2.0, 21.0);
        knifeLine.lineTo(22.0, 3.0);
        QPainterPath belowKnife;
        belowKnife.moveTo(2.0, 21.0);
        belowKnife.lineTo(22.0, 3.0);
        belowKnife.lineTo(26.0, 7.5);
        belowKnife.lineTo(26.0, 26.0);
        belowKnife.lineTo(-2.0, 26.0);
        belowKnife.closeSubpath();
        painter.save();
        painter.setClipPath(belowKnife);
        fillSolid(left);
        painter.restore();
        outline(right, 1.4);
        painter.setPen(QPen(color, 1.9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(knifeLine);
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
} ENTRIES[] = {
    {IconId::MERGE_UNION, drawMerge},     {IconId::MERGE_COMBINE, drawMerge},  {IconId::MERGE_FRAGMENT, drawMerge},
    {IconId::MERGE_INTERSECT, drawMerge}, {IconId::MERGE_SUBTRACT, drawMerge}, {IconId::MERGE_LINE_SPLIT, drawMerge},
};

} // namespace

bool paintIconMerge(QPainter& painter, IconId id)
{
    for (const IconEntry& entry : ENTRIES) {
        if (entry.id == id) {
            entry.draw(painter, id);
            return true;
        }
    }
    return false;
}
