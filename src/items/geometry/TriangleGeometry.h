#pragma once

#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QSizeF>

// 三角形纯几何单元：内容矩形是三角形的最小包围盒，底边 A/B 可以因钝角内缩到盒内。
// 两个底角是真相；底边、包围盒宽高与顶点坐标都由角度派生。
class TriangleGeometry {
  public:
    struct Vertices {
        QPointF a;
        QPointF b;
        QPointF c;
        qreal baseWidth = 0.0;
        qreal frameWidth = 0.0;
    };

    static void ClampAngles(qreal* angle1, qreal* angle2);
    static qreal AspectRatio(qreal angle1, qreal angle2);
    static Vertices Make(const QRectF& frame, qreal angle1, qreal angle2);
    // 保持旧底边长度不变，按新角度重算包围盒宽高。
    static QSizeF SizeForAngles(const QSizeF& oldSize, qreal oldAngle1, qreal oldAngle2, qreal angle1, qreal angle2);
    static QSizeF SizeForHeight(qreal height, qreal angle1, qreal angle2);
    static QPainterPath Path(const QRectF& frame, qreal angle1, qreal angle2);
};
