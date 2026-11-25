#include "items/geometry/ShapePathBuilder.h"

#include "items/geometry/EllipseGeometry.h"
#include "items/geometry/TriangleGeometry.h"

#include <QTransform>
#include <QtMath>

namespace {

QRectF NormalizedRect(const QRectF& rect)
{
    return rect.normalized();
}

QPainterPath ParallelogramPathWithAngle(const QRectF& rect, qreal bottomLeftAngle)
{
    const QRectF r = rect.normalized();
    QPainterPath path;
    if (r.width() <= 0.0 || r.height() <= 0.0) {
        return path;
    }
    const qreal angle = qBound<qreal>(5.0, bottomLeftAngle, 175.0);
    const qreal cotValue = 1.0 / qTan(qDegreesToRadians(angle));
    // 上边相对底边的水平偏移；留 1px 底边，避免钝角时把底边压没。
    qreal shift = qAbs(r.height() * cotValue);
    shift = qMin(shift, qMax<qreal>(0.0, r.width() - 1.0));
    if (cotValue >= 0.0) {
        // 锐角：上边向右移，底边贴着左下角。
        path.moveTo(r.left(), r.bottom());
        path.lineTo(r.right() - shift, r.bottom());
        path.lineTo(r.right(), r.top());
        path.lineTo(r.left() + shift, r.top());
    } else {
        // 钝角：上边向左移，底边贴着右下角。
        path.moveTo(r.left() + shift, r.bottom());
        path.lineTo(r.right(), r.bottom());
        path.lineTo(r.right() - shift, r.top());
        path.lineTo(r.left(), r.top());
    }
    path.closeSubpath();
    return path;
}


// 需求 11 的 8 种新图形。全部在内容矩形内构造；需要“轮廓 + 内部棱线 / 旗杆”的
// 形状用零面积子路径承载细线：填充不受影响，描线时一并描出。


} // namespace
QPainterPath ShapePathBuilder::StarPath(const QRectF& rect)
{
    const QRectF r = NormalizedRect(rect);
    const QPointF center = r.center();
    const qreal rx = r.width() / 2.0;
    const qreal ry = r.height() / 2.0;
    if (rx <= 0.0 || ry <= 0.0) {
        return QPainterPath();
    }

    QPainterPath path;
    constexpr int POINTS = 10;
    for (int i = 0; i < POINTS; ++i) {
        const qreal angle = -M_PI / 2.0 + i * M_PI / 5.0;
        const qreal factor = (i % 2 == 0) ? 1.0 : 0.42;
        const QPointF point(center.x() + qCos(angle) * rx * factor, center.y() + qSin(angle) * ry * factor);
        if (i == 0) {
            path.moveTo(point);
        } else {
            path.lineTo(point);
        }
    }
    path.closeSubpath();
    return path;
}
QPainterPath ShapePathBuilder::CalloutPath(const QRectF& rect)
{
    const QRectF r = NormalizedRect(rect);
    const qreal w = r.width();
    const qreal h = r.height();
    if (w <= 0.0 || h <= 0.0) {
        return QPainterPath();
    }

    const QRectF body(r.left(), r.top(), w, h * 0.78);
    const qreal radius = qMin(w, h) * 0.16;
    QPainterPath bodyPath;
    bodyPath.addRoundedRect(body, radius, radius);

    QPainterPath tail;
    tail.moveTo(r.left() + w * 0.20, body.bottom() - 0.5);
    tail.lineTo(r.left() + w * 0.12, r.bottom());
    tail.lineTo(r.left() + w * 0.38, body.bottom() - 0.5);
    tail.closeSubpath();

    return bodyPath.united(tail);
}
QPainterPath ShapePathBuilder::FourPointStarPath(const QRectF& rect)
{
    const QRectF r = NormalizedRect(rect);
    const QPointF center = r.center();
    const qreal rx = r.width() / 2.0;
    const qreal ry = r.height() / 2.0;
    if (rx <= 0.0 || ry <= 0.0) {
        return QPainterPath();
    }

    QPainterPath path;
    for (int i = 0; i < 8; ++i) {
        const qreal angle = -M_PI / 2.0 + i * M_PI / 4.0;
        const qreal factor = (i % 2 == 0) ? 1.0 : 0.35;
        const QPointF point(center.x() + qCos(angle) * rx * factor, center.y() + qSin(angle) * ry * factor);
        if (i == 0) {
            path.moveTo(point);
        } else {
            path.lineTo(point);
        }
    }
    path.closeSubpath();
    return path;
}
QPainterPath ShapePathBuilder::HexagramPath(const QRectF& rect)
{
    const QRectF r = NormalizedRect(rect);
    const QPointF center = r.center();
    const qreal rx = r.width() / 2.0;
    const qreal ry = r.height() / 2.0;
    if (rx <= 0.0 || ry <= 0.0) {
        return QPainterPath();
    }

    auto triangleAt = [&](qreal startDegrees) {
        QPainterPath triangle;
        for (int i = 0; i < 3; ++i) {
            const qreal angle = qDegreesToRadians(startDegrees + i * 120.0);
            const QPointF point(center.x() + qCos(angle) * rx, center.y() + qSin(angle) * ry);
            if (i == 0) {
                triangle.moveTo(point);
            } else {
                triangle.lineTo(point);
            }
        }
        triangle.closeSubpath();
        return triangle;
    };

    return triangleAt(-90.0).united(triangleAt(90.0));
}
QPainterPath ShapePathBuilder::HexagonPath(const QRectF& rect)
{
    const QRectF r = NormalizedRect(rect);
    if (r.width() <= 0.0 || r.height() <= 0.0) {
        return QPainterPath();
    }
    const qreal w = r.width();
    const qreal h = r.height();
    const qreal left = r.left();
    const qreal top = r.top();
    // 平顶：上下两条水平边各占半宽，左右顶点在中线上；六条边等长时高/宽 = √3/2。
    QPainterPath path;
    path.moveTo(left + w * 0.25, top);
    path.lineTo(left + w * 0.75, top);
    path.lineTo(left + w, top + h * 0.5);
    path.lineTo(left + w * 0.75, top + h);
    path.lineTo(left + w * 0.25, top + h);
    path.lineTo(left, top + h * 0.5);
    path.closeSubpath();
    return path;
}

QPainterPath ShapePathBuilder::HeartPath(const QRectF& rect)
{
    const QRectF r = NormalizedRect(rect);
    const qreal w = r.width();
    const qreal h = r.height();
    if (w <= 0.0 || h <= 0.0) {
        return QPainterPath();
    }

    const qreal cx = r.center().x();
    QPainterPath path;
    path.moveTo(cx, r.top() + h * 0.32);
    path.cubicTo(cx - w * 0.22, r.top() + h * 0.02, r.left(), r.top() + h * 0.08, r.left(), r.top() + h * 0.38);
    path.cubicTo(r.left(), r.top() + h * 0.66, cx - w * 0.20, r.bottom() - h * 0.06, cx, r.bottom());
    path.cubicTo(cx + w * 0.20, r.bottom() - h * 0.06, r.right(), r.top() + h * 0.66, r.right(), r.top() + h * 0.38);
    path.cubicTo(r.right(), r.top() + h * 0.08, cx + w * 0.22, r.top() + h * 0.02, cx, r.top() + h * 0.32);
    path.closeSubpath();
    return path;
}
QPainterPath ShapePathBuilder::FlagWavePath(const QRectF& rect)
{
    const QRectF r = NormalizedRect(rect);
    if (r.width() <= 0.0 || r.height() <= 0.0) {
        return QPainterPath();
    }

    // 需求 17：旗面横向占满整个内容宽度（插入后宽度就是两棋盘格 64px）。
    // 需求 22：纵向也撑满内容高度——上下边随波动正好触及内容矩形上下缘，
    // 选择框就是路径的最小包围盒。
    const qreal amplitude = r.height() * 0.07;
    const qreal flagHeight = r.height() - amplitude * 2.0;
    const qreal topBase = r.top() + amplitude;
    constexpr int STEPS = 24;

    QPainterPath path;
    path.moveTo(r.left(), topBase);
    for (int i = 1; i <= STEPS; ++i) {
        const qreal t = qreal(i) / qreal(STEPS);
        path.lineTo(r.left() + t * r.width(), topBase + qSin(t * 3.0 * M_PI) * amplitude);
    }
    for (int i = STEPS; i >= 0; --i) {
        const qreal t = qreal(i) / qreal(STEPS);
        path.lineTo(r.left() + t * r.width(), topBase + flagHeight + qSin(t * 3.0 * M_PI) * amplitude);
    }
    path.closeSubpath();

    // 需求：去掉左边旗杆竖线，只保留波形旗面。
    return path;
}
QPainterPath ShapePathBuilder::ScrollPath(const QRectF& rect)
{
    const QRectF r = NormalizedRect(rect);
    if (r.width() <= 0.0 || r.height() <= 0.0) {
        return QPainterPath();
    }

    // 卷轴：左右两根卷筒 + 中间展开的纸面，纸面与卷筒同为可填充区域。
    // 需求 22：卷筒纵向撑满内容高度，路径包围盒与内容矩形一致。
    const qreal rollW = r.width() * 0.14;
    const qreal rollH = r.height();
    const qreal rollTop = r.top();

    QPainterPath leftRoll;
    leftRoll.addRoundedRect(QRectF(r.left(), rollTop, rollW, rollH), rollW / 2.0, rollW / 2.0);
    QPainterPath rightRoll;
    rightRoll.addRoundedRect(QRectF(r.right() - rollW, rollTop, rollW, rollH), rollW / 2.0, rollW / 2.0);

    const qreal paperTop = rollTop + rollH * 0.10;
    const qreal paperH = rollH * 0.80;
    QPainterPath paper;
    paper.addRect(QRectF(r.left() + rollW * 0.72, paperTop, r.width() - rollW * 1.44, paperH));

    return leftRoll.united(paper).united(rightRoll);
}
QPainterPath ShapePathBuilder::CubePath(const QRectF& rect)
{
    const QRectF r = NormalizedRect(rect);
    if (r.width() <= 0.0 || r.height() <= 0.0) {
        return QPainterPath();
    }

    // 等轴测立方体：六边形轮廓 + 中心到三条棱的“Y”形内部棱线。
    const QPointF top(r.center().x(), r.top());
    const QPointF upperRight(r.right(), r.top() + r.height() * 0.25);
    const QPointF lowerRight(r.right(), r.bottom() - r.height() * 0.25);
    const QPointF bottom(r.center().x(), r.bottom());
    const QPointF lowerLeft(r.left(), r.bottom() - r.height() * 0.25);
    const QPointF upperLeft(r.left(), r.top() + r.height() * 0.25);
    const QPointF center = r.center();

    QPainterPath path;
    path.moveTo(top);
    path.lineTo(upperRight);
    path.lineTo(lowerRight);
    path.lineTo(bottom);
    path.lineTo(lowerLeft);
    path.lineTo(upperLeft);
    path.closeSubpath();

    path.moveTo(center);
    path.lineTo(upperRight);
    path.moveTo(center);
    path.lineTo(bottom);
    path.moveTo(center);
    path.lineTo(upperLeft);
    return path;
}
QPainterPath ShapePathBuilder::CylinderPath(const QRectF& rect)
{
    const QRectF r = NormalizedRect(rect);
    if (r.width() <= 0.0 || r.height() <= 0.0) {
        return QPainterPath();
    }

    const qreal ry = qMin(r.height() * 0.16, r.width() * 0.30);
    const QRectF topEllipse(r.left(), r.top(), r.width(), ry * 2.0);
    const QRectF bottomEllipse(r.left(), r.bottom() - ry * 2.0, r.width(), ry * 2.0);

    QPainterPath topPath;
    topPath.addEllipse(topEllipse);
    // 柱身顶边取顶椭圆的下半弧：加矩形会把椭圆中线的弦也画出来，看起来像一条直径。
    QPainterPath bodyPath;
    bodyPath.arcMoveTo(topEllipse, 180.0);
    bodyPath.arcTo(topEllipse, 180.0, -180.0);
    bodyPath.lineTo(bottomEllipse.right(), bottomEllipse.center().y());
    bodyPath.arcTo(bottomEllipse, 0.0, -180.0);
    bodyPath.closeSubpath();
    QPainterPath bottomPath;
    bottomPath.addEllipse(bottomEllipse);
    QPainterPath result = topPath.united(bodyPath).united(bottomPath);

    // 底面朝前的半弧：零面积线段，表现柱体的下半圈。
    QPainterPath frontArc;
    frontArc.arcMoveTo(bottomEllipse, 180.0);
    frontArc.arcTo(bottomEllipse, 180.0, 180.0);
    result.addPath(frontArc);
    return result;
}

QPainterPath ShapePathBuilder::BracePath(const QRectF& rect)
{
    // 需求 13：花括号不再是 ShapeType，而是曲线图元的采样源；这里公开成静态助手，
    // 插入（采样构点）与图标绘制共用同一份几何。
    const QRectF r = NormalizedRect(rect);
    const qreal w = r.width();
    const qreal h = r.height();
    if (w <= 0.0 || h <= 0.0) {
        return QPainterPath();
    }

    const qreal xOuter = r.left() + w * 0.82;
    const qreal xMid = r.left() + w * 0.60;
    const qreal xTip = r.left() + w * 0.16;

    QPainterPath path;
    path.moveTo(xOuter, r.top());
    path.cubicTo(xMid, r.top() + h * 0.025, xMid, r.top() + h * 0.14, xMid, r.top() + h * 0.30);
    path.cubicTo(xMid, r.top() + h * 0.43, xTip, r.top() + h * 0.42, xTip, r.center().y());
    path.cubicTo(xTip, r.top() + h * 0.58, xMid, r.top() + h * 0.57, xMid, r.top() + h * 0.70);
    path.cubicTo(xMid, r.top() + h * 0.86, xMid, r.bottom() - h * 0.025, xOuter, r.bottom());
    return path;
}

QPainterPath ShapePathBuilder::MapUnitPath(const QPainterPath& unitPath, const QRectF& rect)
{
    const QRectF r = NormalizedRect(rect);
    if (r.width() <= 0.0 || r.height() <= 0.0) {
        return QPainterPath();
    }
    QTransform transform;
    transform.translate(r.left(), r.top());
    transform.scale(r.width(), r.height());
    return transform.map(unitPath);
}

QPainterPath ShapePathBuilder::RoundedRectPath(const QRectF& rect, qreal cornerRadius)
{
    const QRectF r = NormalizedRect(rect);
    if (r.width() <= 0.0 || r.height() <= 0.0) {
        return QPainterPath();
    }
    QPainterPath path;
    // 圆角：正数按 px（上限短边一半），0 表示直角。
    if (cornerRadius > 0.0) {
        const qreal radius = qMin(cornerRadius, qMin(r.width(), r.height()) / 2.0);
        path.addRoundedRect(r, radius, radius);
    } else {
        path.addRect(r);
    }
    return path;
}

QPainterPath ShapePathBuilder::EllipsePath(const QRectF& rect)
{
    const QRectF r = NormalizedRect(rect);
    if (r.width() <= 0.0 || r.height() <= 0.0) {
        return QPainterPath();
    }
    // 椭圆 / 扇形 / 弧线共用 EllipseGeometry，角度语义与扫过角兜底只有一份实现。
    const EllipseGeometry ellipseShape(r);
    return ellipseShape.Ellipse();
}

QPainterPath ShapePathBuilder::SectorPath(const QRectF& rect, qreal startAngle, qreal sweepAngle)
{
    const QRectF r = NormalizedRect(rect);
    if (r.width() <= 0.0 || r.height() <= 0.0) {
        return QPainterPath();
    }
    const EllipseGeometry ellipseShape(r);
    return ellipseShape.Sector(startAngle, sweepAngle);
}

QPainterPath ShapePathBuilder::ArrowPath(const QRectF& rect)
{
    const QRectF r = NormalizedRect(rect);
    if (r.width() <= 0.0 || r.height() <= 0.0) {
        return QPainterPath();
    }
    const qreal headLength = qMin(r.width() * 0.45, qMax(r.height() * 0.75, r.width() * 0.22));
    const qreal shaftHalf = qMin(r.height() * 0.17, headLength * 0.45);
    const qreal cy = r.center().y();
    const qreal headBase = r.right() - headLength;
    QPainterPath path;
    path.moveTo(r.left(), cy - shaftHalf);
    path.lineTo(headBase, cy - shaftHalf);
    path.lineTo(headBase, r.top());
    path.lineTo(r.right(), cy);
    path.lineTo(headBase, r.bottom());
    path.lineTo(headBase, cy + shaftHalf);
    path.lineTo(r.left(), cy + shaftHalf);
    path.closeSubpath();
    return path;
}

QPainterPath ShapePathBuilder::DoubleArrowPath(const QRectF& rect)
{
    const QRectF r = NormalizedRect(rect);
    if (r.width() <= 0.0 || r.height() <= 0.0) {
        return QPainterPath();
    }
    const qreal headLength = qMin(r.width() * 0.45, qMin(r.width() * 0.30, qMax(r.height() * 0.70, r.width() * 0.16)));
    const qreal shaftHalf = qMin(r.height() * 0.17, headLength * 0.45);
    const qreal cy = r.center().y();
    const qreal leftBase = r.left() + headLength;
    const qreal rightBase = r.right() - headLength;
    QPainterPath path;
    path.moveTo(r.left(), cy);
    path.lineTo(leftBase, r.top());
    path.lineTo(leftBase, cy - shaftHalf);
    path.lineTo(rightBase, cy - shaftHalf);
    path.lineTo(rightBase, r.top());
    path.lineTo(r.right(), cy);
    path.lineTo(rightBase, r.bottom());
    path.lineTo(rightBase, cy + shaftHalf);
    path.lineTo(leftBase, cy + shaftHalf);
    path.lineTo(leftBase, r.bottom());
    path.closeSubpath();
    return path;
}

QPainterPath ShapePathBuilder::PathFor(ShapeType type, const QRectF& rect, qreal cornerRadius)
{
    return PathFor(type, rect, cornerRadius, 30.0, 120.0);
}

QPainterPath ShapePathBuilder::PathFor(ShapeType type, const QRectF& rect, qreal cornerRadius, qreal startAngle,
                                       qreal sweepAngle)
{
    switch (type) {
    case ShapeType::RECTANGLE:
        return RoundedRectPath(rect, cornerRadius);
    case ShapeType::ELLIPSE:
        return EllipsePath(rect);
    case ShapeType::TRIANGLE:
        // 默认按 60°/60° 的等腰三角形生成；具体图元用带角度参数的重载。
        return TrianglePath(rect, 60.0, 60.0);
    case ShapeType::HEXAGON:
        return HexagonPath(rect);
    case ShapeType::STAR:
        return StarPath(rect);
    case ShapeType::ARROW:
        return ArrowPath(rect);
    case ShapeType::DOUBLE_ARROW:
        return DoubleArrowPath(rect);
    case ShapeType::CALLOUT:
        return CalloutPath(rect);
    case ShapeType::SECTOR:
        return SectorPath(rect, startAngle, sweepAngle);
    case ShapeType::STAR4:
        return FourPointStarPath(rect);
    case ShapeType::HEXAGRAM:
        return HexagramPath(rect);
    case ShapeType::HEART:
        return HeartPath(rect);
    case ShapeType::FLAG_WAVE:
        return FlagWavePath(rect);
    case ShapeType::SCROLL:
        return ScrollPath(rect);
    case ShapeType::CUBE:
        return CubePath(rect);
    case ShapeType::CYLINDER:
        return CylinderPath(rect);
    case ShapeType::PARALLELOGRAM:
        // 默认按 60° 生成；具体图元用带角度参数的重载。
        return ParallelogramPath(rect, 60.0);
    case ShapeType::DIAMOND:
        return DiamondPath(rect);
    case ShapeType::POLYGON:
        // 多边形（合并产物）没有内置轮廓，由 PolygonItem 自己按图层提供路径。
        break;
    }
    return QPainterPath();
}
QPainterPath ShapePathBuilder::TrianglePath(const QRectF& rect, qreal bottomAngle1, qreal bottomAngle2)
{
    return TriangleGeometry::Path(rect, bottomAngle1, bottomAngle2);
}

QPainterPath ShapePathBuilder::ParallelogramPath(const QRectF& rect, qreal bottomLeftAngle)
{
    return ParallelogramPathWithAngle(rect, bottomLeftAngle);
}

QPainterPath ShapePathBuilder::DiamondPath(const QRectF& rect)
{
    const QRectF r = NormalizedRect(rect);
    QPainterPath path;
    if (r.width() <= 0.0 || r.height() <= 0.0) {
        return path;
    }
    // 四个顶点取矩形四边中点：两条对角线互相垂直平分，四条边自然恒等。
    path.moveTo(r.center().x(), r.top());
    path.lineTo(r.right(), r.center().y());
    path.lineTo(r.center().x(), r.bottom());
    path.lineTo(r.left(), r.center().y());
    path.closeSubpath();
    return path;
}
