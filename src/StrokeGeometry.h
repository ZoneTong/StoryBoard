#pragma once

#include <QColor>
#include <QPainterPath>
#include <QSizeF>
#include <Qt>

class QPainter;

// 笔画的几何与命中：按内容尺寸把路径映射到当前大小，生成描边遮挡区、
// 加粗命中区，并负责把笔画画到画布上。笔画数据仍由 CurveItem 持有。
namespace StrokeGeometry {

// 路径按 baseSize -> contentSize 的比例映射到当前大小。
QPainterPath outline(const QPainterPath& path, const QSizeF& baseSize, const QSizeF& contentSize);

// 真正涂满画布的遮挡区：中心线上按平均缩放描出笔触宽度。
QPainterPath Painted(const QPainterPath& outline, const QSizeF& baseSize, const QSizeF& contentSize, qreal penWidth);

// 命中测试用的加粗路径（不含图元手柄区域，手柄由图元自身补上）。
QPainterPath HitScaled(const QPainterPath& path, const QSizeF& baseSize, const QSizeF& contentSize, qreal penWidth,
                       qreal viewScale);

// 按内容尺寸缩放后描边渲染。
void render(QPainter* painter, const QPainterPath& path, const QSizeF& baseSize, const QSizeF& contentSize,
            const QColor& color, qreal penWidth, Qt::PenStyle style = Qt::SolidLine);

} // namespace StrokeGeometry
