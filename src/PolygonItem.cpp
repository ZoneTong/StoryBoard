#include "PolygonItem.h"

#include "I18n.h"
#include "ShapePainter.h"

#include <QPainter>
#include <QPainterPathStroker>
#include <QTransform>

// 把路径里每个子路径显式闭合：布尔运算（交集 / 拆块）产生的路径与序列化
// 重建的路径都不带 closeSubpath 标记，描边时收尾一条边会缺失（需求 11）。
QPainterPath PolygonItem::closedSubpaths(const QPainterPath& path)
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

PolygonItem::PolygonItem(const QList<Layer>& layers, FillStyle fillStyle, Qt::PenStyle strokeStyle)
    : ShapeItem(), m_layers(layers), m_fillStyle(fillStyle), m_strokeStyle(strokeStyle)
{
    updateBoundsFromLayers();
}

QColor PolygonItem::fillColor() const
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

QColor PolygonItem::strokeColor() const
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

qreal PolygonItem::strokeWidth() const
{
    for (const Layer& layer : m_layers) {
        if (layer.role == LayerRole::FilledArea && layer.strokeWidth > 0.0) {
            return layer.strokeWidth;
        }
    }
    return 0.0;
}

void PolygonItem::setFill(const QColor& color, FillStyle style)
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

void PolygonItem::setFillStyle(FillStyle style)
{
    m_fillStyle = style;
    update();
}

void PolygonItem::setStroke(const QColor& color, qreal width)
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

void PolygonItem::setFillImage(const QPixmap& image, ImageFill::Mode mode)
{
    if (image.isNull()) {
        clearFillImage();
        return;
    }
    m_fillImage = image;
    m_fillImageMode = mode;
    update();
}

void PolygonItem::clearFillImage()
{
    if (m_fillImage.isNull()) {
        return;
    }
    m_fillImage = QPixmap();
    update();
}

void PolygonItem::setStrokeStyle(Qt::PenStyle style)
{
    m_strokeStyle = style;
    update();
}

QString PolygonItem::displayName() const
{
    return uiText(QStringLiteral("Polygon"));
}

QString PolygonItem::typeDisplayName() const
{
    return uiText(QStringLiteral("Polygon"));
}

void PolygonItem::updateBoundsFromLayers()
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

QTransform PolygonItem::layerTransform() const
{
    QTransform transform;
    const QRectF rect = contentRect();
    if (rect.width() <= 0.0 || rect.height() <= 0.0 || m_designSize.width() <= 0.0 || m_designSize.height() <= 0.0) {
        return transform;
    }
    transform.scale(rect.width() / m_designSize.width(), rect.height() / m_designSize.height());
    return transform;
}

QPainterPath PolygonItem::mapLayerPath(const QPainterPath& localPath) const
{
    return layerTransform().map(localPath);
}

QPainterPath PolygonItem::paintedShape() const
{
    QPainterPath shape;
    for (const Layer& layer : m_layers) {
        if (layer.filled) {
            shape.addPath(layer.path);
        }
    }
    return layerTransform().map(shape);
}

QPainterPath PolygonItem::outlineShape() const
{
    QPainterPath shape;
    for (const Layer& layer : m_layers) {
        shape.addPath(layer.path);
    }
    return layerTransform().map(shape);
}

QPainterPath PolygonItem::shape() const
{
    // 命中范围沿用画布图元的规则（内容矩形 + 手柄），不用单一形状类型的轮廓。
    return CanvasItem::shape();
}

QPainterPath PolygonItem::currentPath(const QRectF& rect) const
{
    QPainterPath unionPath;
    for (const Layer& layer : m_layers) {
        unionPath.addPath(layer.path);
    }
    const QRectF content = contentRect();
    if (content.isEmpty() || m_designSize.isEmpty() || rect == content) {
        return layerTransform().map(unionPath);
    }
    QTransform transform;
    transform.translate(rect.center().x(), rect.center().y());
    transform.scale(rect.width() / content.width(), rect.height() / content.height());
    transform.translate(-content.center().x(), -content.center().y());
    return transform.map(layerTransform().map(unionPath));
}

void PolygonItem::setStyle(const QColor& strokeColor, const QColor& fillColor, qreal strokeWidth)
{
    setStroke(strokeColor, strokeWidth);
    setFill(fillColor, fillStyle());
}

ShapeItemState PolygonItem::state() const
{
    ShapeItemState result;
    result.strokeColor = strokeColor();
    result.fillColor = fillColor();
    result.fillStyle = m_fillStyle;
    result.strokeWidth = strokeWidth();
    result.strokeStyle = m_strokeStyle;
    result.opacity = itemOpacity();
    result.hasFillImage = hasFillImage();
    result.fillImage = m_fillImage;
    result.fillImageMode = m_fillImageMode;
    result.pos = pos();
    result.size = contentSize();
    return result;
}

void PolygonItem::applyState(const ShapeItemState& state)
{
    m_fillStyle = state.fillStyle;
    m_strokeStyle = state.strokeStyle;
    m_fillImage = state.hasFillImage ? state.fillImage : QPixmap();
    m_fillImageMode = state.fillImageMode;
    setItemOpacity(state.opacity);
    setPos(state.pos);
    setContentSize(state.size.isEmpty() ? contentSize() : state.size);
    setFill(state.fillColor, state.fillStyle);
    setStroke(state.strokeColor, state.strokeWidth);
    setStrokeStyle(state.strokeStyle);
    update();
}

void PolygonItem::paintContent(QPainter* painter)
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

ShapeLayer PolygonItem::shapeLayer(int index) const
{
    if (index < 0 || index >= m_layers.size()) {
        return ShapeLayer();
    }
    const Layer& layer = m_layers.at(index);
    ShapeLayer view;
    view.path = layer.path;
    view.fill = layer.fill;
    view.stroke = layer.stroke;
    view.strokeWidth = layer.strokeWidth;
    view.filled = layer.filled;
    switch (layer.role) {
    case LayerRole::StrokeBand:
        view.role = ShapeLayerRole::STROKE_BAND;
        break;
    case LayerRole::Text:
        view.role = ShapeLayerRole::TEXT;
        break;
    case LayerRole::FilledArea:
    default:
        view.role = ShapeLayerRole::FILL_AREA;
        break;
    }
    return view;
}

QPainterPath PolygonItem::mapShapeLayerPath(const ShapeLayer& layer) const
{
    return layerTransform().map(layer.path);
}
