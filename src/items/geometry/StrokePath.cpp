#include "items/geometry/StrokePath.h"

#include <QLineF>

namespace StrokePath {

QPainterPath Build(const QVector<QPointF>& points, bool dot, bool linearPath, qreal penWidth)
{
    QPainterPath path;
    if (points.isEmpty()) {
        return path;
    }

    if (dot || points.size() == 1) {
        const QPointF center = points.constFirst();
        const qreal radius = penWidth / 2.0;
        path.addEllipse(center, radius, radius);
        return path;
    }

    path.moveTo(points.constFirst());
    if (linearPath) {
        for (int i = 1; i < points.size(); ++i) {
            path.lineTo(points.at(i));
        }
    } else if (points.size() == 2) {
        path.lineTo(points.constLast());
    } else {
        for (int i = 1; i < points.size() - 1; ++i) {
            const QPointF middle = (points.at(i) + points.at(i + 1)) / 2.0;
            path.quadTo(points.at(i), middle);
        }
        path.lineTo(points.constLast());
    }
    return path;
}

qreal Length(const QVector<QPointF>& points)
{
    qreal length = 0.0;
    for (int i = 1; i < points.size(); ++i) {
        length += QLineF(points.at(i - 1), points.at(i)).length();
    }
    return length;
}

} // namespace StrokePath
