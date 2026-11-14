#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "scene/SceneConstants.h"

#include <QImage>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QUndoStack>
#include <QtMath>

QRectF CanvasScene::canvasRect() const
{
    return QRectF(m_canvasOrigin, m_canvasSize);
}

void CanvasScene::applyCanvasRect(const QRectF& rect)
{
    const QRectF normalized = rect.normalized();
    const QSizeF bounded(qBound(scene_constants::kMinCanvasSide, normalized.width(), scene_constants::kMaxCanvasSide),
                         qBound(scene_constants::kMinCanvasSide, normalized.height(), scene_constants::kMaxCanvasSide));
    const QPointF origin(normalized.left(), normalized.top());
    if (qFuzzyCompare(bounded.width(), m_canvasSize.width()) &&
        qFuzzyCompare(bounded.height(), m_canvasSize.height()) && origin == m_canvasOrigin) {
        return;
    }
    m_canvasOrigin = origin;
    m_canvasSize = bounded;
    updateSceneExtent();
    update();
    emit canvasSizeChanged(m_canvasSize);
}

void CanvasScene::applyCanvasSize(const QSizeF& size)
{
    applyCanvasRect(QRectF(m_canvasOrigin, size));
}

void CanvasScene::setCanvasSizeWithUndo(const QSizeF& size)
{
    setCanvasRectWithUndo(QRectF(m_canvasOrigin, size));
}

void CanvasScene::setCanvasRectWithUndo(const QRectF& rect)
{
    const QRectF before = canvasRect();
    applyCanvasRect(rect);
    commitCanvasRect(before);
}

void CanvasScene::commitCanvasRect(const QRectF& before)
{
    const QRectF after = canvasRect();
    if (before == after) {
        return;
    }
    if (m_undoStack) {
        m_undoStack->push(new CanvasSizeCommand(this, before, after, tr("Resize canvas")));
    }
}

void CanvasScene::applyBackground(const BackgroundState& background)
{
    m_background = background;
    update();
    emit backgroundChanged();
}

void CanvasScene::replaceBackground(const BackgroundState& background, const QString& text)
{
    if (background == m_background) {
        return;
    }

    if (m_undoStack) {
        m_undoStack->push(
            new BackgroundCommand(this, m_background, background, text.isEmpty() ? tr("Change background") : text));
    } else {
        applyBackground(background);
    }
}

void CanvasScene::setShowGrid(bool enabled)
{
    if (m_showGrid == enabled) {
        return;
    }
    m_showGrid = enabled;
    update();
}

QRectF CanvasScene::contentRect() const
{
    QRectF rect;
    bool hasItem = false;
    const QList<CanvasItem*> items = canvasItems();
    for (CanvasItem* item : items) {
        if (!item->isVisible()) {
            continue;
        }
        const QRectF itemRect = item->sceneContentRect();
        if (itemRect.isEmpty()) {
            continue;
        }
        if (!hasItem) {
            rect = itemRect;
            hasItem = true;
        } else {
            rect = rect.united(itemRect);
        }
    }
    return hasItem ? rect : QRectF();
}

void CanvasScene::updateSceneExtent()
{
    QRectF extent = canvasRect();
    const QRectF content = contentRect();
    if (!content.isEmpty()) {
        extent = extent.united(content);
    }
    extent = extent.adjusted(-400.0, -400.0, 400.0, 400.0);
    if (sceneRect() != extent) {
        setSceneRect(extent);
    }
}

QImage CanvasScene::renderImage(const QRectF& area, qreal scale)
{
    if (area.isEmpty() || scale <= 0.0) {
        return QImage();
    }

    QSizeF pixelSize(area.width() * scale, area.height() * scale);
    constexpr qreal kMaxSide = 12000.0;
    if (pixelSize.width() > kMaxSide || pixelSize.height() > kMaxSide) {
        const qreal factor = qMin(kMaxSide / pixelSize.width(), kMaxSide / pixelSize.height());
        pixelSize *= factor;
    }

    const QSize finalSize(qMax(1, qCeil(pixelSize.width())), qMax(1, qCeil(pixelSize.height())));
    QImage image(finalSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);

    m_exporting = true;
    render(&painter, QRectF(0.0, 0.0, finalSize.width(), finalSize.height()), area, Qt::IgnoreAspectRatio);
    m_exporting = false;

    painter.end();
    return image;
}

QImage CanvasScene::renderLayerImage(const QRectF& area, qreal scale, int layerId)
{
    if (!hasLayer(layerId)) {
        return QImage();
    }

    const QList<CanvasItem*> items = canvasItems();
    QList<bool> previousVisibility;
    previousVisibility.reserve(items.size());
    for (CanvasItem* item : items) {
        previousVisibility.append(item->isVisible());
        item->setVisible(item->layerId() == layerId && isLayerVisible(layerId));
    }

    QImage image = renderImage(area, scale);

    for (int i = 0; i < items.size() && i < previousVisibility.size(); ++i) {
        items.at(i)->setVisible(previousVisibility.at(i));
    }
    return image;
}

void CanvasScene::drawBackground(QPainter* painter, const QRectF& rect)
{
    const QRectF canvas = canvasRect();
    const QRectF area = rect.intersected(canvas);
    if (area.isEmpty()) {
        return;
    }

    painter->save();
    painter->setClipRect(rect);
    drawCanvasBackground(painter, area, !m_exporting && m_showGrid);
    painter->restore();

    if (!m_exporting) {
        QPen borderPen(QColor(15, 23, 42, 55));
        borderPen.setCosmetic(true);
        borderPen.setWidthF(1.0);
        painter->setPen(borderPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(canvas.adjusted(0.5, 0.5, -0.5, -0.5));
    }
}

void CanvasScene::drawCanvasBackground(QPainter* painter, const QRectF& area, bool includeGrid) const
{
    const QRectF canvas = canvasRect();

    // 默认背景层为无色（None）：不铺任何底色，画布区域保持纯透明；
    // 背景层隐藏时同样不绘制底，仅在设置了纯色或背景图时才覆盖画布。
    if (isLayerVisible(BackgroundLayerId)) {
        switch (m_background.type) {
        case BackgroundType::None:
            break;
        case BackgroundType::Color:
            painter->fillRect(canvas, m_background.color);
            break;
        case BackgroundType::Image: {
            const QPixmap& pixmap = m_background.image;
            if (pixmap.isNull()) {
                break;
            }
            if (m_background.mode == BackgroundMode::Stretch) {
                painter->drawPixmap(canvas, pixmap, QRectF(pixmap.rect()));
            } else if (m_background.mode == BackgroundMode::Tile) {
                painter->drawTiledPixmap(canvas, pixmap);
            } else {
                const qreal factor = qMin(canvas.width() / pixmap.width(), canvas.height() / pixmap.height());
                QSizeF size(pixmap.width() * factor, pixmap.height() * factor);
                QRectF target(QPointF(0.0, 0.0), size);
                target.moveCenter(canvas.center());
                painter->drawPixmap(target, pixmap, QRectF(pixmap.rect()));
            }
            break;
        }
        }
    }

    if (!includeGrid) {
        return;
    }

    const qreal zoom = painter->worldTransform().m11();
    if (zoom < 0.18) {
        return;
    }
    const bool drawMinor = zoom >= 0.32;
    constexpr qreal step = 40.0;

    QPen minorPen(QColor(15, 23, 42, 13));
    minorPen.setCosmetic(true);
    minorPen.setWidthF(1.0);
    QPen majorPen(QColor(15, 23, 42, 30));
    majorPen.setCosmetic(true);
    majorPen.setWidthF(1.0);

    painter->save();
    painter->setClipRect(area);
    const int columns = qCeil(canvas.width() / step);
    for (int i = 0; i <= columns; ++i) {
        const bool major = (i % 5) == 0;
        if (!major && !drawMinor) {
            continue;
        }
        const qreal x = canvas.left() + i * step;
        painter->setPen(major ? majorPen : minorPen);
        painter->drawLine(QPointF(x, canvas.top()), QPointF(x, canvas.bottom()));
    }
    const int rows = qCeil(canvas.height() / step);
    for (int i = 0; i <= rows; ++i) {
        const bool major = (i % 5) == 0;
        if (!major && !drawMinor) {
            continue;
        }
        const qreal y = canvas.top() + i * step;
        painter->setPen(major ? majorPen : minorPen);
        painter->drawLine(QPointF(canvas.left(), y), QPointF(canvas.right(), y));
    }
    painter->restore();
}
