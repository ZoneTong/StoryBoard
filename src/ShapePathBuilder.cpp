#include "ShapePathBuilder.h"

#include <QTransform>
#include <QtMath>

namespace {

QRectF normalizedRect(const QRectF& rect)
{
    return rect.normalized();
}

// 底角换算成"高 / 底边"系数：cot(α)。角度先做安全钳制，两角和不超过 175°。
qreal cotDegrees(qreal degrees)
{
    const qreal clamped = qBound<qreal>(1.0, degrees, 174.0);
    return 1.0 / qTan(qDegreesToRadians(clamped));
}

QPainterPath trianglePathWithAngles(const QRectF& rect, qreal angle1, qreal angle2)
{
    const QRectF r = rect.normalized();
    QPainterPath path;
    if (r.width() <= 0.0 || r.height() <= 0.0) {
        return path;
    }
    const qreal a1 = qBound<qreal>(1.0, angle1, 174.0);
    const qreal a2 = qBound<qreal>(1.0, angle2, 175.0 - a1);
    // 两侧水平投影之和应等于宽度；尺寸与角度暂时不自洽时按比例压缩，顶点不会跑出矩形。
    const qreal fromLeft = r.height() * cotDegrees(a1);
    const qreal fromRight = r.height() * cotDegrees(a2);
    const qreal sum = fromLeft + fromRight;
    const qreal apexOffset = sum > r.width() && sum > 0.0 ? qBound<qreal>(0.0, fromLeft * r.width() / sum, r.width())
                                                          : qBound<qreal>(0.0, fromLeft, r.width());
    path.moveTo(r.left() + apexOffset, r.top());
    path.lineTo(r.right(), r.bottom());
    path.lineTo(r.left(), r.bottom());
    path.closeSubpath();
    return path;
}

QPainterPath starPath(const QRectF& rect)
{
    const QRectF r = normalizedRect(rect);
    const QPointF center = r.center();
    const qreal rx = r.width() / 2.0;
    const qreal ry = r.height() / 2.0;
    if (rx <= 0.0 || ry <= 0.0) {
        return QPainterPath();
    }

    QPainterPath path;
    constexpr int points = 10;
    for (int i = 0; i < points; ++i) {
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

QPainterPath calloutPath(const QRectF& rect)
{
    const QRectF r = normalizedRect(rect);
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

// 需求 11 的 8 种新图形。全部在内容矩形内构造；需要“轮廓 + 内部棱线 / 旗杆”的
// 形状用零面积子路径承载细线：填充不受影响，描线时一并描出。
QPainterPath fourPointStarPath(const QRectF& rect)
{
    const QRectF r = normalizedRect(rect);
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

QPainterPath hexagramPath(const QRectF& rect)
{
    const QRectF r = normalizedRect(rect);
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

QPainterPath heartPath(const QRectF& rect)
{
    const QRectF r = normalizedRect(rect);
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

QPainterPath sineWavePath(const QRectF& rect)
{
    const QRectF r = normalizedRect(rect);
    if (r.width() <= 0.0 || r.height() <= 0.0) {
        return QPainterPath();
    }

    // 需求：一个完整周期（0…2π），振幅取内容高度的 20%，与 64px 基准下的可辨识度一致。
    constexpr int steps = 64;
    const qreal amplitude = r.height() * 0.20;
    const qreal cy = r.center().y();
    QPainterPath path;
    for (int i = 0; i <= steps; ++i) {
        const qreal t = qreal(i) / qreal(steps);
        const QPointF point(r.left() + t * r.width(), cy - qSin(t * 2.0 * M_PI) * amplitude);
        if (i == 0) {
            path.moveTo(point);
        } else {
            path.lineTo(point);
        }
    }
    return path;
}

QPainterPath flagWavePath(const QRectF& rect)
{
    const QRectF r = normalizedRect(rect);
    if (r.width() <= 0.0 || r.height() <= 0.0) {
        return QPainterPath();
    }

    const qreal x0 = r.left() + r.width() * 0.16;
    const qreal topBase = r.top() + r.height() * 0.10;
    const qreal flagHeight = r.height() * 0.46;
    const qreal amplitude = r.height() * 0.07;
    constexpr int steps = 24;

    QPainterPath path;
    path.moveTo(x0, topBase);
    for (int i = 1; i <= steps; ++i) {
        const qreal t = qreal(i) / qreal(steps);
        path.lineTo(x0 + t * (r.right() - x0), topBase + qSin(t * 3.0 * M_PI) * amplitude);
    }
    for (int i = steps; i >= 0; --i) {
        const qreal t = qreal(i) / qreal(steps);
        path.lineTo(x0 + t * (r.right() - x0), topBase + flagHeight + qSin(t * 3.0 * M_PI) * amplitude);
    }
    path.closeSubpath();

    // 需求：去掉左边旗杆竖线，只保留波形旗面。
    return path;
}

QPainterPath scrollPath(const QRectF& rect)
{
    const QRectF r = normalizedRect(rect);
    if (r.width() <= 0.0 || r.height() <= 0.0) {
        return QPainterPath();
    }

    // 卷轴：左右两根卷筒 + 中间展开的纸面，纸面与卷筒同为可填充区域。
    const qreal rollW = r.width() * 0.14;
    const qreal rollH = r.height() * 0.84;
    const qreal rollTop = r.top() + (r.height() - rollH) / 2.0;

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

QPainterPath cubePath(const QRectF& rect)
{
    const QRectF r = normalizedRect(rect);
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

QPainterPath cylinderPath(const QRectF& rect)
{
    const QRectF r = normalizedRect(rect);
    if (r.width() <= 0.0 || r.height() <= 0.0) {
        return QPainterPath();
    }

    const qreal ry = qMin(r.height() * 0.16, r.width() * 0.30);
    const QRectF topEllipse(r.left(), r.top(), r.width(), ry * 2.0);
    const QRectF bottomEllipse(r.left(), r.bottom() - ry * 2.0, r.width(), ry * 2.0);
    const QRectF body(r.left(), r.top() + ry, r.width(), r.height() - ry * 2.0);

    QPainterPath topPath;
    topPath.addEllipse(topEllipse);
    QPainterPath bodyPath;
    bodyPath.addRect(body);
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

} // namespace

QPainterPath ShapePathBuilder::bracePath(const QRectF& rect)
{
    // 需求 13：花括号不再是 ShapeType，而是曲线图元的采样源；这里公开成静态助手，
    // 插入（采样构点）与图标绘制共用同一份几何。
    const QRectF r = normalizedRect(rect);
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

QPainterPath ShapePathBuilder::mapUnitPath(const QPainterPath& unitPath, const QRectF& rect)
{
    const QRectF r = normalizedRect(rect);
    if (r.width() <= 0.0 || r.height() <= 0.0) {
        return QPainterPath();
    }
    QTransform transform;
    transform.translate(r.left(), r.top());
    transform.scale(r.width(), r.height());
    return transform.map(unitPath);
}

QPainterPath ShapePathBuilder::pathFor(ShapeType type, const QRectF& rect, qreal cornerRadius)
{
    return pathFor(type, rect, cornerRadius, 30.0, 120.0);
}

QPainterPath ShapePathBuilder::pathFor(ShapeType type, const QRectF& rect, qreal cornerRadius, qreal startAngle,
                                       qreal sweepAngle)
{
    const QRectF r = normalizedRect(rect);
    if (r.width() <= 0.0 || r.height() <= 0.0) {
        return QPainterPath();
    }

    // 需求 6：扇形 / 弧线的椭圆外接框就是内容矩形，长半轴 = 宽/2、短半轴 = 高/2，
    // 不再维护独立半径；想画扁弧直接改图元尺寸即可。
    auto arcRect = [&r]() { return r; };
    auto arcSweep = [sweepAngle]() {
        const qreal sweep = qBound<qreal>(-360.0, sweepAngle, 360.0);
        // 扫过角为 0 时画不出可见图形，退回默认 120°，避免插入后“什么都没有”。
        return qFuzzyIsNull(sweep) ? 120.0 : sweep;
    };

    QPainterPath path;
    switch (type) {
    case ShapeType::Rectangle: {
        // 圆角：正数按 px（上限短边一半）；-2 为自动态（短边 16%，旧圆角矩形）；
        // 其余 <= 0 保持直角。
        if (cornerRadius > 0.0) {
            const qreal radius = qMin(cornerRadius, qMin(r.width(), r.height()) / 2.0);
            path.addRoundedRect(r, radius, radius);
        } else if (cornerRadius <= -2.0) {
            const qreal radius = qMin(r.width(), r.height()) * 0.16;
            path.addRoundedRect(r, radius, radius);
        } else {
            path.addRect(r);
        }
        break;
    }
    case ShapeType::Ellipse:
        path.addEllipse(r);
        break;
    case ShapeType::Triangle:
        // 默认按 60°/60° 的等腰三角形生成；具体图元用带角度参数的重载。
        path = trianglePathWithAngles(r, 60.0, 60.0);
        break;
    case ShapeType::Diamond:
        path.moveTo(r.center().x(), r.top());
        path.lineTo(r.right(), r.center().y());
        path.lineTo(r.center().x(), r.bottom());
        path.lineTo(r.left(), r.center().y());
        path.closeSubpath();
        break;
    case ShapeType::Star:
        path = starPath(r);
        break;
    case ShapeType::Arrow: {
        const qreal headLength = qMin(r.width() * 0.45, qMax(r.height() * 0.75, r.width() * 0.22));
        const qreal shaftHalf = qMin(r.height() * 0.17, headLength * 0.45);
        const qreal cy = r.center().y();
        const qreal headBase = r.right() - headLength;
        path.moveTo(r.left(), cy - shaftHalf);
        path.lineTo(headBase, cy - shaftHalf);
        path.lineTo(headBase, r.top());
        path.lineTo(r.right(), cy);
        path.lineTo(headBase, r.bottom());
        path.lineTo(headBase, cy + shaftHalf);
        path.lineTo(r.left(), cy + shaftHalf);
        path.closeSubpath();
        break;
    }
    case ShapeType::DoubleArrow: {
        const qreal headLength =
            qMin(r.width() * 0.45, qMin(r.width() * 0.30, qMax(r.height() * 0.70, r.width() * 0.16)));
        const qreal shaftHalf = qMin(r.height() * 0.17, headLength * 0.45);
        const qreal cy = r.center().y();
        const qreal leftBase = r.left() + headLength;
        const qreal rightBase = r.right() - headLength;
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
        break;
    }
    case ShapeType::Line:
        path.moveTo(r.left(), r.center().y());
        path.lineTo(r.right(), r.center().y());
        break;
    case ShapeType::Callout:
        path = calloutPath(r);
        break;
    case ShapeType::Sector:
        path.arcMoveTo(arcRect(), startAngle);
        path.arcTo(arcRect(), startAngle, arcSweep());
        path.lineTo(r.center());
        path.closeSubpath();
        break;
    case ShapeType::Arc:
        path.arcMoveTo(arcRect(), startAngle);
        path.arcTo(arcRect(), startAngle, arcSweep());
        break;
    case ShapeType::Star4:
        path = fourPointStarPath(r);
        break;
    case ShapeType::Hexagram:
        path = hexagramPath(r);
        break;
    case ShapeType::Heart:
        path = heartPath(r);
        break;
    case ShapeType::SineWave:
        path = sineWavePath(r);
        break;
    case ShapeType::FlagWave:
        path = flagWavePath(r);
        break;
    case ShapeType::Scroll:
        path = scrollPath(r);
        break;
    case ShapeType::Cube:
        path = cubePath(r);
        break;
    case ShapeType::Cylinder:
        path = cylinderPath(r);
        break;
    }
    return path;
}

QPainterPath ShapePathBuilder::trianglePath(const QRectF& rect, qreal bottomAngle1, qreal bottomAngle2)
{
    return trianglePathWithAngles(rect, bottomAngle1, bottomAngle2);
}
