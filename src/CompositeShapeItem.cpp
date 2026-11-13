#include "CompositeShapeItem.h"

#include "I18n.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QPainter>
#include <QTransform>

CompositeShapeItem::CompositeShapeItem(const QList<Layer>& layers) : CanvasItem(), m_layers(layers)
{
    updateBoundsFromLayers();
}

QString CompositeShapeItem::displayName() const
{
    return uiText(QStringLiteral("Boolean shape"));
}

QString CompositeShapeItem::typeDisplayName() const
{
    return uiText(QStringLiteral("Boolean shape"));
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
    painter->save();
    painter->setTransform(layerTransform(), true);
    // 按优先级从低到高逐层绘制，每层只画“不被更高层盖住”的那部分：
    // 重叠区因此整块显示最高优先级图元的填充色，重叠区里的下层轮廓也被裁掉，
    // 边界上的轮廓颜色同样是高层图元的描边色。
    const int count = m_layers.size();
    for (int i = 0; i < count; ++i) {
        const Layer& layer = m_layers.at(i);
        QPainterPath above;
        for (int j = i + 1; j < count; ++j) {
            above = above.isEmpty() ? m_layers.at(j).path : above.united(m_layers.at(j).path);
        }
        const QPainterPath visible = above.isEmpty() ? layer.path : layer.path.subtracted(above);
        if (visible.isEmpty()) {
            continue;
        }
        if (layer.filled && layer.fill.alpha() > 0) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(layer.fill);
            painter->drawPath(visible);
        }
        if (layer.stroke.alpha() > 0 && layer.strokeWidth > 0.0) {
            QPen pen(layer.stroke, layer.strokeWidth);
            pen.setJoinStyle(Qt::MiterJoin);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(visible);
            // 顶层图元整圈描边画在最后，重叠边界处也取它的轮廓颜色。
            if (i == count - 1) {
                painter->drawPath(layer.path);
            }
        }
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
