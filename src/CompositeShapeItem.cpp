#include "CompositeShapeItem.h"

#include "I18n.h"
#include "ShapePainter.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPathStroker>
#include <QTransform>

// 把路径里每个子路径显式闭合：布尔运算（交集 / 拆块）产生的路径与序列化
// 重建的路径都不带 closeSubpath 标记，描边时收尾一条边会缺失（需求 11）。
QPainterPath CompositeShapeItem::closedSubpaths(const QPainterPath& path)
{
    QPainterPath closed;
    bool hasDraw = false;
    for (int i = 0; i < path.elementCount(); ++i) {
        const QPainterPath::Element element = path.elementAt(i);
        switch (element.type) {
        case QPainterPath::MoveToElement:
            if (hasDraw) {
                closed.closeSubpath();
            }
            closed.moveTo(element);
            hasDraw = false;
            break;
        case QPainterPath::LineToElement:
            closed.lineTo(element);
            hasDraw = true;
            break;
        case QPainterPath::CurveToElement:
            // 曲线段占三个元素：c1 在 i，c2 / 终点紧随其后。
            closed.cubicTo(element, path.elementAt(i + 1), path.elementAt(i + 2));
            i += 2;
            hasDraw = true;
            break;
        default:
            break;
        }
    }
    if (hasDraw) {
        closed.closeSubpath();
    }
    return closed;
}

CompositeShapeItem::CompositeShapeItem(const QList<Layer>& layers, FillStyle fillStyle, Qt::PenStyle strokeStyle)
    : CanvasItem(), m_layers(layers), m_fillStyle(fillStyle), m_strokeStyle(strokeStyle)
{
    updateBoundsFromLayers();
}

QColor CompositeShapeItem::fillColor() const
{
    for (const Layer& layer : m_layers) {
        if (layer.role == LayerRole::FilledArea && layer.filled) {
            return layer.fill;
        }
    }
    for (const Layer& layer : m_layers) {
        if (layer.filled) {
            return layer.fill;
        }
    }
    return QColor();
}

QColor CompositeShapeItem::strokeColor() const
{
    for (const Layer& layer : m_layers) {
        if (layer.role == LayerRole::FilledArea && layer.strokeWidth > 0.0) {
            return layer.stroke;
        }
    }
    for (const Layer& layer : m_layers) {
        // 纯直线 / 曲线合并没有面积层，线宽带的填充色就是描线色。
        if (layer.role == LayerRole::StrokeBand) {
            return layer.fill;
        }
    }
    return fillColor();
}

qreal CompositeShapeItem::strokeWidth() const
{
    for (const Layer& layer : m_layers) {
        if (layer.role == LayerRole::FilledArea && layer.strokeWidth > 0.0) {
            return layer.strokeWidth;
        }
    }
    return 0.0;
}

void CompositeShapeItem::setFill(const QColor& color, FillStyle style)
{
    if (!color.isValid()) {
        return;
    }
    m_fillStyle = style;
    for (Layer& layer : m_layers) {
        // 文字字形跟多边形填充色一起变化；线宽带属于描线，不在这里改。
        if (layer.role == LayerRole::FilledArea || layer.role == LayerRole::Text) {
            layer.fill = color;
        }
    }
    update();
}

void CompositeShapeItem::setFillStyle(FillStyle style)
{
    m_fillStyle = style;
    update();
}

void CompositeShapeItem::setStroke(const QColor& color, qreal width)
{
    if (!color.isValid()) {
        return;
    }
    const qreal effectiveWidth = qMax(0.0, width);
    for (Layer& layer : m_layers) {
        if (layer.role == LayerRole::FilledArea) {
            layer.stroke = color;
            if (effectiveWidth > 0.0) {
                layer.strokeWidth = effectiveWidth;
            }
        } else if (layer.role == LayerRole::StrokeBand) {
            // 零面积图元的线宽由自身几何决定，换色只改填充，避免再次描边加粗。
            layer.fill = color;
        }
    }
    update();
}

void CompositeShapeItem::setFillImage(const QPixmap& image, ImageFill::Mode mode)
{
    if (image.isNull()) {
        clearFillImage();
        return;
    }
    m_fillImage = image;
    m_fillImageMode = mode;
    update();
}

void CompositeShapeItem::clearFillImage()
{
    if (m_fillImage.isNull()) {
        return;
    }
    m_fillImage = QPixmap();
    update();
}

void CompositeShapeItem::setStrokeStyle(Qt::PenStyle style)
{
    m_strokeStyle = style;
    update();
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

QPainterPath CompositeShapeItem::mapLayerPath(const QPainterPath& localPath) const
{
    return layerTransform().map(localPath);
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
            // 高层为不透明填充时直接覆盖下层即可；若先把下层挖掉再填，会在
            // 两层的抗锯齿边缘露出画布底色，形成文字 / 图形边上的白色重影。
            if (higher.filled && higher.fill.alpha() > 0 && higher.fill.alpha() < 255) {
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
        if (layer.role == LayerRole::FilledArea && m_fillStyle == FillStyle::None) {
            continue;
        }
        painter->setPen(Qt::NoPen);
        if (layer.role == LayerRole::FilledArea && ShapePainter::isPatternFill(m_fillStyle)) {
            ShapePainter::paintPatternFill(painter, visible, visible.boundingRect(), layer.fill, m_fillStyle);
        } else {
            painter->setBrush(layer.fill);
            painter->drawPath(visible);
        }
    }

    // 需求 15：图片填充只铺在有面积层围成的区域内，孔洞与描线带外自然留空。
    if (!m_fillImage.isNull()) {
        QPainterPath area;
        qreal borderWidth = 0.0;
        for (const Layer& layer : m_layers) {
            if (layer.role != LayerRole::FilledArea || !layer.filled) {
                continue;
            }
            area = area.isEmpty() ? layer.path : area.united(layer.path);
            borderWidth = qMax(borderWidth, layer.strokeWidth);
        }
        if (!area.isEmpty()) {
            if (borderWidth > 0.0) {
                QPainterPathStroker stroker;
                stroker.setJoinStyle(Qt::MiterJoin);
                stroker.setWidth(borderWidth);
                area = area.subtracted(stroker.createStroke(area));
            }
            ImageFill::paint(painter, m_fillImage, m_fillImageMode, QRectF(QPointF(0.0, 0.0), m_designSize), area);
        }
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
        QPen pen(layer.stroke, layer.strokeWidth, m_strokeStyle);
        pen.setJoinStyle(Qt::MiterJoin);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        // 需求 11：有面积层与文字字形必须按闭合轮廓描边（收尾边不能缺）；
        // 线宽带本身是开放笔画，保持原样。
        painter->drawPath(layer.role == LayerRole::StrokeBand ? layer.path : closedSubpaths(layer.path));
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
        entry.insert(QStringLiteral("role"), static_cast<int>(layer.role));
        layers.append(entry);
    }
    json.insert(QStringLiteral("layers"), layers);
    json.insert(QStringLiteral("fillStyle"), static_cast<int>(m_fillStyle));
    json.insert(QStringLiteral("strokeStyle"), static_cast<int>(m_strokeStyle));
    json.insert(QStringLiteral("designWidth"), m_designSize.width());
    json.insert(QStringLiteral("designHeight"), m_designSize.height());
    const QJsonObject fillImageJson = ImageFill::toJson(m_fillImage, m_fillImageMode);
    if (!fillImageJson.isEmpty()) {
        json.insert(QStringLiteral("fillImage"), fillImageJson);
    }
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
        const int role = entry.value(QStringLiteral("role")).toInt(static_cast<int>(LayerRole::FilledArea));
        if (role >= static_cast<int>(LayerRole::FilledArea) && role <= static_cast<int>(LayerRole::Text)) {
            layer.role = static_cast<LayerRole>(role);
        }
        m_layers.append(layer);
    }
    const int fillStyle = json.value(QStringLiteral("fillStyle")).toInt(static_cast<int>(FillStyle::Solid));
    m_fillStyle = fillStyle >= static_cast<int>(FillStyle::None) && fillStyle <= static_cast<int>(FillStyle::Honeycomb)
                      ? static_cast<FillStyle>(fillStyle)
                      : FillStyle::Solid;
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
        m_fillImageMode = ImageFill::Mode::Stretch;
    }
}
