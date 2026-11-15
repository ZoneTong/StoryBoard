#include "CompositeShapeItem.h"

#include "I18n.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPathStroker>
#include <QTransform>

CompositeShapeItem::CompositeShapeItem(const QList<Layer>& layers) : CanvasItem(), m_layers(layers)
{
    updateBoundsFromLayers();
}

QString CompositeShapeItem::displayName() const
{
    return uiText(QStringLiteral("Polygon"));
}

QString CompositeShapeItem::typeDisplayName() const
{
    return uiText(QStringLiteral("Polygon"));
}

void CompositeShapeItem::updateBoundsFromLayers()
{
    QRectF bounds;
    for (const Layer& layer : m_layers) {
        const QRectF rect = layer.path.boundingRect();
        if (rect.isNull()) {
            continue;
        }
        bounds = bounds.isNull() ? rect : bounds.united(rect);
    }
    if (bounds.isNull()) {
        m_designSize = QSizeF(1.0, 1.0);
        setContentSize(QSizeF(1.0, 1.0));
        return;
    }
    // 路径统一挪到 (0,0) 起点，绘制时只按内容尺寸缩放。
    if (!qFuzzyIsNull(bounds.left()) || !qFuzzyIsNull(bounds.top())) {
        for (Layer& layer : m_layers) {
            layer.path.translate(-bounds.left(), -bounds.top());
        }
    }
    m_designSize = QSizeF(qMax(1.0, bounds.width()), qMax(1.0, bounds.height()));
    setContentSize(m_designSize);
    setPos(bounds.topLeft());
}

QTransform CompositeShapeItem::layerTransform() const
{
    QTransform transform;
    const QRectF rect = contentRect();
    if (rect.width() <= 0.0 || rect.height() <= 0.0 || m_designSize.width() <= 0.0 || m_designSize.height() <= 0.0) {
        return transform;
    }
    transform.scale(rect.width() / m_designSize.width(), rect.height() / m_designSize.height());
    return transform;
}

QPainterPath CompositeShapeItem::paintedShape() const
{
    QPainterPath shape;
    for (const Layer& layer : m_layers) {
        if (layer.filled) {
            shape.addPath(layer.path);
        }
    }
    return layerTransform().map(shape);
}

QPainterPath CompositeShapeItem::outlineShape() const
{
    QPainterPath shape;
    for (const Layer& layer : m_layers) {
        shape.addPath(layer.path);
    }
    return layerTransform().map(shape);
}

void CompositeShapeItem::paintContent(QPainter* painter)
{
    if (m_layers.isEmpty()) {
        return;
    }

    painter->save();
    painter->setTransform(layerTransform(), true);
    const int count = m_layers.size();

    // 填充取色：优先级从低到高逐层绘制，但只有“填充可见”的高层才遮挡低层。
    // 高层填充完全透明时不参与遮挡，重叠区自然落到次优先级图元的颜色，以此类推。
    for (int i = 0; i < count; ++i) {
        const Layer& layer = m_layers.at(i);
        QPainterPath maskedByHigher;
        for (int j = i + 1; j < count; ++j) {
            const Layer& higher = m_layers.at(j);
            if (higher.filled && higher.fill.alpha() > 0) {
                maskedByHigher = maskedByHigher.isEmpty() ? higher.path : maskedByHigher.united(higher.path);
            }
        }
        if (!layer.filled || layer.fill.alpha() <= 0) {
            continue;
        }
        const QPainterPath visible = maskedByHigher.isEmpty() ? layer.path : layer.path.subtracted(maskedByHigher);
        if (visible.isEmpty()) {
            continue;
        }
        painter->setPen(Qt::NoPen);
        painter->setBrush(layer.fill);
        painter->drawPath(visible);
    }

    // 描边：只保留多边形整体外轮廓（含孔洞边界）上的部分。
    // 落在图形内部的交界轮廓不再绘制，由填充色盖住（无填充时透出相邻像素）。
    bool overlapping = false;
    for (int i = 0; i < count && !overlapping; ++i) {
        for (int j = i + 1; j < count; ++j) {
            const QRectF bound = m_layers.at(i).path.boundingRect().intersected(m_layers.at(j).path.boundingRect());
            if (bound.isNull()) {
                continue;
            }
            // 拆分结果的各块可能共边，共边会生成零面积的退化交集；
            // 只有真正有面积的重叠才算“重叠层”，否则会把块间交界线误裁掉。
            const QRectF common = m_layers.at(i).path.intersected(m_layers.at(j).path).boundingRect();
            if (common.width() * common.height() > 1.0) {
                overlapping = true;
                break;
            }
        }
    }

    qreal maxWidth = 0.0;
    for (const Layer& layer : m_layers) {
        if (layer.stroke.alpha() > 0) {
            maxWidth = qMax(maxWidth, layer.strokeWidth);
        }
    }

    if (overlapping && maxWidth > 0.0) {
        // 用整体并集的外轮廓做一条描边带，把各层描边裁到这条带上：
        // 外轮廓按贡献它的图元颜色描边，内部交界线被裁掉。
        QPainterPath merged;
        for (const Layer& layer : m_layers) {
            merged = merged.isEmpty() ? layer.path : merged.united(layer.path);
        }
        QPainterPathStroker stroker;
        stroker.setJoinStyle(Qt::MiterJoin);
        stroker.setWidth(qMax(2.0, maxWidth + 2.0));
        const QPainterPath band = stroker.createStroke(merged);
        painter->setClipPath(band, Qt::IntersectClip);
    }

    for (const Layer& layer : m_layers) {
        if (layer.stroke.alpha() <= 0 || layer.strokeWidth <= 0.0) {
            continue;
        }
        QPen pen(layer.stroke, layer.strokeWidth);
        pen.setJoinStyle(Qt::MiterJoin);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(layer.path);
    }

    painter->restore();
}

QJsonArray CompositeShapeItem::pathToJson(const QPainterPath& path)
{
    QJsonArray elements;
    for (int i = 0; i < path.elementCount(); ++i) {
        const QPainterPath::Element element = path.elementAt(i);
        QJsonObject entry;
        switch (element.type) {
        case QPainterPath::MoveToElement:
            entry.insert(QStringLiteral("t"), QStringLiteral("m"));
            break;
        case QPainterPath::LineToElement:
            entry.insert(QStringLiteral("t"), QStringLiteral("l"));
            break;
        default:
            entry.insert(QStringLiteral("t"), QStringLiteral("c"));
            break;
        }
        entry.insert(QStringLiteral("x"), element.x);
        entry.insert(QStringLiteral("y"), element.y);
        elements.append(entry);
    }
    return elements;
}

QPainterPath CompositeShapeItem::pathFromJson(const QJsonArray& elements)
{
    QPainterPath path;
    QList<QPointF> curvePoints;
    auto flushCurve = [&path, &curvePoints]() {
        if (curvePoints.size() >= 3) {
            path.cubicTo(curvePoints.at(0), curvePoints.at(1), curvePoints.at(2));
        }
        curvePoints.clear();
    };
    for (const QJsonValue& value : elements) {
        const QJsonObject entry = value.toObject();
        const QString type = entry.value(QStringLiteral("t")).toString();
        const QPointF point(entry.value(QStringLiteral("x")).toDouble(), entry.value(QStringLiteral("y")).toDouble());
        if (type == QStringLiteral("m")) {
            flushCurve();
            path.moveTo(point);
        } else if (type == QStringLiteral("l")) {
            flushCurve();
            path.lineTo(point);
        } else {
            curvePoints.append(point);
            if (curvePoints.size() == 3) {
                flushCurve();
            }
        }
    }
    flushCurve();
    return path;
}

QJsonObject CompositeShapeItem::toJson() const
{
    QJsonObject json = baseJson();
    json.insert(QStringLiteral("kind"), kind());
    QJsonArray layers;
    for (const Layer& layer : m_layers) {
        QJsonObject entry;
        entry.insert(QStringLiteral("path"), pathToJson(layer.path));
        entry.insert(QStringLiteral("fill"), layer.fill.name(QColor::HexArgb));
        entry.insert(QStringLiteral("stroke"), layer.stroke.name(QColor::HexArgb));
        entry.insert(QStringLiteral("width"), layer.strokeWidth);
        entry.insert(QStringLiteral("filled"), layer.filled);
        layers.append(entry);
    }
    json.insert(QStringLiteral("layers"), layers);
    json.insert(QStringLiteral("designWidth"), m_designSize.width());
    json.insert(QStringLiteral("designHeight"), m_designSize.height());
    return json;
}

void CompositeShapeItem::fromJson(const QJsonObject& json)
{
    applyBaseJson(json);
    m_layers.clear();
    const QJsonArray layers = json.value(QStringLiteral("layers")).toArray();
    for (const QJsonValue& value : layers) {
        const QJsonObject entry = value.toObject();
        Layer layer;
        layer.path = pathFromJson(entry.value(QStringLiteral("path")).toArray());
        layer.fill = QColor(entry.value(QStringLiteral("fill")).toString());
        layer.stroke = QColor(entry.value(QStringLiteral("stroke")).toString());
        layer.strokeWidth = entry.value(QStringLiteral("width")).toDouble(1.0);
        layer.filled = entry.value(QStringLiteral("filled")).toBool(true);
        m_layers.append(layer);
    }
    m_designSize = QSizeF(qMax(1.0, json.value(QStringLiteral("designWidth")).toDouble(1.0)),
                          qMax(1.0, json.value(QStringLiteral("designHeight")).toDouble(1.0)));
    setContentSize(m_designSize);
}
