#include "ImageFill.h"
#include "PolygonItem.h"

#include <QJsonArray>
#include <QJsonObject>

// 多边形的序列化单独成文件：路径元素表与图层快照都和绘制无关，
// 只在这里与画布文件的 JSON 结构对接。
QJsonArray PolygonItem::pathToJson(const QPainterPath& path)
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

QPainterPath PolygonItem::pathFromJson(const QJsonArray& elements)
{
    QPainterPath path;
    QList<QPointF> curvePoints;
    auto flushCurve = [&path, &curvePoints]() {
        if (curvePoints.size() >= 3) {
            path.cubicTo(curvePoints.at(0), curvePoints.at(1), curvePoints.at(2));
        } else {
            // 段不足三个控制点（文件被截断或手改）：降级为折线段接上，
            // 不整段丢弃，否则曲线开头或结尾会凭空缺一块。
            for (const QPointF& point : curvePoints) {
                path.lineTo(point);
            }
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

QJsonObject PolygonItem::toJson() const
{
    QJsonObject json = baseJson();
    QJsonArray layers;
    for (const Layer& layer : m_layers) {
        QJsonObject entry;
        entry.insert(QStringLiteral("path"), pathToJson(layer.path));
        entry.insert(QStringLiteral("fill"), layer.fill.name(QColor::HexArgb));
        entry.insert(QStringLiteral("stroke"), layer.stroke.name(QColor::HexArgb));
        entry.insert(QStringLiteral("width"), layer.strokeWidth);
        entry.insert(QStringLiteral("filled"), layer.filled);
        entry.insert(QStringLiteral("role"), static_cast<int>(layer.role));
        layers.append(entry);
    }
    json.insert(QStringLiteral("layers"), layers);
    json.insert(QStringLiteral("fillStyle"), static_cast<int>(m_fillStyle));
    json.insert(QStringLiteral("strokeStyle"), static_cast<int>(m_strokeStyle));
    json.insert(QStringLiteral("designWidth"), m_designSize.width());
    json.insert(QStringLiteral("designHeight"), m_designSize.height());
    const QJsonObject fillImageJson = ImageFill::toJson(m_fillImage, fillImageMode());
    if (!fillImageJson.isEmpty()) {
        json.insert(QStringLiteral("fillImage"), fillImageJson);
    }
    return json;
}

void PolygonItem::fromJson(const QJsonObject& json)
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
        const int role = entry.value(QStringLiteral("role")).toInt(static_cast<int>(LayerRole::FILLED_AREA));
        if (role >= static_cast<int>(LayerRole::FILLED_AREA) && role <= static_cast<int>(LayerRole::TEXT)) {
            layer.role = static_cast<LayerRole>(role);
        }
        m_layers.append(layer);
    }
    const int fillStyle = json.value(QStringLiteral("fillStyle")).toInt(static_cast<int>(FillStyle::SOLID));
    m_fillStyle = fillStyle >= static_cast<int>(FillStyle::NONE) && fillStyle <= static_cast<int>(FillStyle::DOTS)
                      ? static_cast<FillStyle>(fillStyle)
                      : FillStyle::SOLID;
    const int strokeStyle = json.value(QStringLiteral("strokeStyle")).toInt(static_cast<int>(Qt::SolidLine));
    m_strokeStyle = strokeStyle >= static_cast<int>(Qt::NoPen) && strokeStyle <= static_cast<int>(Qt::CustomDashLine)
                        ? static_cast<Qt::PenStyle>(strokeStyle)
                        : Qt::SolidLine;
    m_designSize = QSizeF(qMax(1.0, json.value(QStringLiteral("designWidth")).toDouble(1.0)),
                          qMax(1.0, json.value(QStringLiteral("designHeight")).toDouble(1.0)));
    setContentSize(m_designSize);
    if (json.contains(QStringLiteral("fillImage"))) {
        m_fillImage = ImageFill::pixmapFromJson(json.value(QStringLiteral("fillImage")).toObject(), &m_fillImageMode);
    } else {
        m_fillImage = QPixmap();
        m_fillImageMode = ImageFill::Mode::STRETCH;
    }
}
