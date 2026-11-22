#include "EllipseAxis.h"

qreal EllipseAxis::Major(const QSizeF& size)
{
    return qMax(size.width(), size.height()) / 2.0;
}

qreal EllipseAxis::Minor(const QSizeF& size)
{
    return qMin(size.width(), size.height()) / 2.0;
}

EllipseAxis::ResizeResult EllipseAxis::Apply(bool majorAxis, qreal value, const QPointF& pos, const QSizeF& size)
{
    const bool widthIsMajor = size.width() >= size.height();
    qreal major = widthIsMajor ? size.width() / 2.0 : size.height() / 2.0;
    qreal minor = widthIsMajor ? size.height() / 2.0 : size.width() / 2.0;
    if (majorAxis) {
        major = value;
        if (major < minor) {
            minor = major;
        }
    } else {
        minor = value;
        if (minor > major) {
            major = minor;
        }
    }
    const QSizeF newSize(widthIsMajor ? major * 2.0 : minor * 2.0, widthIsMajor ? minor * 2.0 : major * 2.0);
    // 中心不动：内容框变大 / 变小时左上都反向补偿。
    const QPointF center = pos + QPointF(size.width() / 2.0, size.height() / 2.0);
    const QPointF newPos(center.x() - newSize.width() / 2.0, center.y() - newSize.height() / 2.0);
    return ResizeResult{newPos, newSize};
}
