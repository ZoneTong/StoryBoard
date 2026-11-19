#pragma once

#include "CurveItem.h"
#include "ShapeItem.h"

#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QRectF>

// 图元绘制单元：填充纹理中心线几何、图案填充与描边/纯色落笔。只接收图元当前
// 的属性快照，不持有 QGraphicsItem 状态，便于缩略图与正式图元复用同一套画法。
class ShapePainter {
  public:
    // 填充纹理的中心线几何：绘制与遮挡计算共用这一份，保证看到的和“挡住别人的”一致。
    static QPainterPath centerLines(FillStyle style, const QRectF& rect);
    static bool isPatternFill(FillStyle style);

    static void paintPatternFill(QPainter* painter, const QPainterPath& clip, const QRectF& rect, const QColor& color,
                                 FillStyle style);

    // 按当前属性把轮廓画到 painter 上；save/restore、抗锯齿与透明度在方法内部完成。
    static void paintShape(QPainter* painter, const QPainterPath& path, const QRectF& rect, ShapeType type,
                           const QColor& strokeColor, const QColor& fillColor, qreal strokeWidth,
                           Qt::PenStyle strokeStyle, FillStyle fillStyle, qreal opacity);

    // 工具按钮缩略图：按类型取默认轮廓，纯描边类型只落笔，其余叠加半透明填充。
    static QIcon makeIcon(ShapeType type, const QColor& color, int logicalSize);
    // 曲线族缩略图：直线 / 弧线 / 正弦按各自几何画一条开放描线（与插入后的形态一致）。
    static QIcon makeCurveIcon(CurveKind kind, const QColor& color, int logicalSize);
};
