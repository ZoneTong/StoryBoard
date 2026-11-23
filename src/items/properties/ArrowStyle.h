#pragma once

#include <QColor>
#include <QList>
#include <QPainterPath>
#include <QString>

class QPainter;

// 直线 / 直线箭头 / 曲线的两端箭头样式：无 / 实心三角 / 空心三角 / 开口箭头 / 圆点 / 竖杠。
// 箭头是描线的一部分：头长固定为 4 倍线宽、张角 90°，允许超出内容矩形，
// 但不改变图元尺寸，也不参与合并图元。
enum class ArrowStyle { NONE = 0, TRIANGLE_FILLED, TRIANGLE_HOLLOW, OPEN_ARROW, DOT, BAR };

QList<ArrowStyle> AllArrowStyles();
namespace ArrowStyles {

QString Name(ArrowStyle style);

} // namespace ArrowStyles
QString ArrowStyleToString(ArrowStyle style);
ArrowStyle ArrowStyleFromString(const QString& value);
bool ArrowStyleIsHead(ArrowStyle style);

// 以 tip 为尖端、toward 为线身上相邻的一点（由 toward 指向 tip 即箭头朝向），
// 生成箭头的轮廓：实心 / 空心三角与圆点、竖杠都有面积，开口箭头是两条零面积线段。
QPainterPath ArrowHeadPath(const QPointF& tip, const QPointF& toward, qreal strokeWidth, ArrowStyle style);
// 按样式直接画在 tip 处；实心三角用实心填充，空心三角只描边，其余按线宽画。
void PaintArrowHead(QPainter* painter, const QPointF& tip, const QPointF& toward, qreal strokeWidth,
                    const QColor& color, ArrowStyle style);
