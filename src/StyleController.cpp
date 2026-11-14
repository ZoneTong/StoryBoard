#include "StyleController.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "CanvasView.h"
#include "Commands.h"
#include "IconFactory.h"
#include "MainWindow.h"
#include "Settings.h"
#include "ShapeItem.h"

#include <QAction>
#include <QActionGroup>
#include <QColorDialog>
#include <QComboBox>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QToolButton>
#include <QtMath>

namespace {

QColor fillIconColor(const QColor& color)
{
    QColor fill = color.isValid() ? color : QColor(96, 132, 255);
    if (fill.alpha() < 150) {
        fill.setAlpha(150);
    }
    return fill;
}

QIcon makeFillStyleIcon(FillStyle style, const QColor& color, int logicalSize)
{
    constexpr qreal scale = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * scale), static_cast<int>(logicalSize * scale));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(scale);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(logicalSize / 24.0, logicalSize / 24.0);

    const QRectF box(3.0, 4.0, 18.0, 16.0);
    const QColor ink(58, 64, 80);
    const QColor fill = fillIconColor(color);

    QPainterPath clipPath;
    clipPath.addRoundedRect(box.adjusted(1.0, 1.0, -1.0, -1.0), 2.5, 2.5);
    painter.save();
    painter.setClipPath(clipPath);

    if (style != FillStyle::None) {
        QColor background = fill;
        background.setAlpha(style == FillStyle::Solid ? 255 : 70);
        painter.fillRect(box, background);
    }

    switch (style) {
    case FillStyle::None: {
        painter.setPen(QPen(ink, 2.2, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(5.5, 18.5), QPointF(18.5, 5.5));
        break;
    }
    case FillStyle::Solid:
        break;
    case FillStyle::Hachure:
    case FillStyle::ReverseHachure:
    case FillStyle::CrossHatch: {
        painter.setPen(QPen(ink, 1.5));
        const bool forward = style != FillStyle::ReverseHachure;
        const bool backward = style != FillStyle::Hachure;
        for (qreal offset = -box.height(); offset < box.width() + box.height(); offset += 4.2) {
            if (forward) {
                painter.drawLine(QPointF(box.left() + offset, box.bottom()),
                                 QPointF(box.left() + offset + box.height(), box.top()));
            }
            if (backward) {
                painter.drawLine(QPointF(box.left() + offset, box.top()),
                                 QPointF(box.left() + offset + box.height(), box.bottom()));
            }
        }
        break;
    }
    case FillStyle::GridHatch: {
        painter.setPen(QPen(ink, 1.4));
        for (qreal x = box.left() + 4.0; x < box.right(); x += 4.5) {
            painter.drawLine(QPointF(x, box.top()), QPointF(x, box.bottom()));
        }
        for (qreal y = box.top() + 4.0; y < box.bottom(); y += 4.5) {
            painter.drawLine(QPointF(box.left(), y), QPointF(box.right(), y));
        }
        break;
    }
    case FillStyle::Honeycomb: {
        painter.setPen(QPen(ink, 1.2));
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                const QPointF center(box.left() + 4.0 + column * 5.5 + (row % 2 ? 2.75 : 0.0),
                                     box.top() + 3.5 + row * 5.0);
                QPolygonF hexagon;
                for (int i = 0; i < 6; ++i) {
                    const qreal angle = qDegreesToRadians(60.0 * i - 30.0);
                    hexagon << center + QPointF(2.8 * qCos(angle), 2.8 * qSin(angle));
                }
                painter.drawPolygon(hexagon);
            }
        }
        break;
    }
    }

    painter.restore();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(ink, 1.8));
    painter.drawRoundedRect(box, 3.0, 3.0);
    painter.end();

    return QIcon(pixmap);
}

QIcon makeFillColorIcon(const QColor& color, int logicalSize)
{
    constexpr qreal scale = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * scale), static_cast<int>(logicalSize * scale));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(scale);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(logicalSize / 24.0, logicalSize / 24.0);

    const QColor ink(58, 64, 80);
    const QColor fill = fillIconColor(color);

    QPainterPath bucket;
    bucket.moveTo(5.0, 9.0);
    bucket.lineTo(8.0, 20.0);
    bucket.lineTo(17.0, 20.0);
    bucket.lineTo(20.0, 9.0);
    bucket.closeSubpath();

    painter.setPen(QPen(ink, 1.6));
    painter.setBrush(fill);
    painter.drawPath(bucket);

    QPainterPath handle;
    handle.moveTo(7.0, 9.0);
    handle.cubicTo(7.0, 2.5, 18.0, 2.5, 18.0, 9.0);
    painter.setPen(QPen(ink, 1.6));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(handle);

    painter.setPen(QPen(ink, 1.9));
    painter.drawLine(QPointF(4.0, 9.0), QPointF(21.0, 9.0));
    painter.setPen(QPen(QColor(255, 255, 255, 180), 1.0));
    painter.drawLine(QPointF(9.5, 12.5), QPointF(11.0, 18.0));
    painter.end();

    return QIcon(pixmap);
}

QIcon makeStrokeColorIcon(const QColor& color, int logicalSize)
{
    constexpr qreal scale = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * scale), static_cast<int>(logicalSize * scale));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(scale);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(logicalSize / 24.0, logicalSize / 24.0);

    const QColor ink(58, 64, 80);
    const QColor stroke = fillIconColor(color);
    const QRectF box(4.0, 4.0, 16.0, 16.0);

    painter.setPen(QPen(ink, 1.0));
    painter.setBrush(Qt::white);
    painter.drawRoundedRect(box, 3.0, 3.0);
    painter.setPen(QPen(stroke, 3.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(box.adjusted(2.0, 2.0, -2.0, -2.0), 2.2, 2.2);

    QPainterPath nib;
    nib.moveTo(14.5, 1.0);
    nib.lineTo(22.5, 9.0);
    nib.lineTo(18.5, 10.5);
    nib.lineTo(13.0, 5.0);
    nib.closeSubpath();
    painter.setPen(QPen(ink, 0.8));
    painter.setBrush(ink);
    painter.drawPath(nib);
    painter.end();

    return QIcon(pixmap);
}

} // namespace

StyleController::StyleController(MainWindow* window) : m_window(window) {}

QIcon StyleController::fillColorIcon(const QColor& color, int logicalSize) const
{
    return makeFillColorIcon(color, logicalSize);
}

QIcon StyleController::strokeColorIcon(const QColor& color, int logicalSize) const
{
    return makeStrokeColorIcon(color, logicalSize);
}

QIcon StyleController::fillStyleIcon(FillStyle style, const QColor& color, int logicalSize) const
{
    return makeFillStyleIcon(style, color, logicalSize);
}

void StyleController::setFillStyle(FillStyle style)
{
    m_window->m_fillStyle = style;
    updateFillActions();
    applyFillToSelection();
}

void StyleController::chooseFillColor()
{
    const QColor initial = m_window->m_fillColor.isValid() ? m_window->m_fillColor : QColor(59, 130, 246, 96);
    const QColor color =
        QColorDialog::getColor(initial, m_window, MainWindow::tr("Choose fill color"), QColorDialog::ShowAlphaChannel);
    if (!color.isValid()) {
        return;
    }
    m_window->m_fillColor = color;
    updateFillActions();
    applyFillToSelection();
}

void StyleController::chooseStrokeColor()
{
    const QColor initial =
        m_window->m_brushColor.alpha() < 8 ? QColor(QStringLiteral("#1E293B")) : m_window->m_brushColor;
    const QColor color = QColorDialog::getColor(initial, m_window, MainWindow::tr("Choose outline color"),
                                                QColorDialog::ShowAlphaChannel);
    if (!color.isValid()) {
        return;
    }

    setBrushColor(color, true);

    QList<ShapeItem*> shapes;
    const QList<CanvasItem*> selected = m_window->m_scene->selectedCanvasItems();
    for (CanvasItem* item : selected) {
        if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
            shapes.append(shape);
        }
    }

    if (shapes.isEmpty()) {
        m_window->statusBar()->showMessage(
            MainWindow::tr("Outline color set; it will be used for newly inserted shapes"), 4000);
        return;
    }

    QList<ShapeItemState> before;
    QList<ShapeItemState> after;
    before.reserve(shapes.size());
    after.reserve(shapes.size());
    for (ShapeItem* shape : shapes) {
        before.append(shape->state());
        shape->setStyle(color, shape->fillColor(), shape->strokeWidth());
        after.append(shape->state());
    }

    if (before == after) {
        m_window->statusBar()->showMessage(MainWindow::tr("The outline color did not change"), 3000);
        return;
    }

    m_window->m_undo->push(
        new ShapeStyleCommand(m_window->m_scene, shapes, before, after, MainWindow::tr("Change outline color")));
    m_window->statusBar()->showMessage(MainWindow::tr("Applied outline color to %1 shape(s)").arg(shapes.size()), 4000);
}

void StyleController::applyFillToSelection()
{
    if (!m_window->m_scene || !m_window->m_undo) {
        return;
    }

    QList<ShapeItem*> shapes;
    int outlineOnlyCount = 0;
    const QList<CanvasItem*> selected = m_window->m_scene->selectedCanvasItems();
    for (CanvasItem* item : selected) {
        auto* shape = dynamic_cast<ShapeItem*>(item);
        if (!shape) {
            continue;
        }
        if (shapeTypeIsFilled(shape->shapeType())) {
            shapes.append(shape);
        } else {
            ++outlineOnlyCount;
        }
    }

    if (shapes.isEmpty()) {
        if (outlineOnlyCount > 0) {
            m_window->statusBar()->showMessage(
                MainWindow::tr("The selected shape is stroke-only (line, brace, etc.); fill is not supported"), 4000);
        } else {
            m_window->statusBar()->showMessage(
                MainWindow::tr("Fill set to \"%1\"; it will be used for newly inserted shapes")
                    .arg(fillStyleName(m_window->m_fillStyle)),
                4000);
        }
        return;
    }

    QList<ShapeItemState> before;
    QList<ShapeItemState> after;
    before.reserve(shapes.size());
    after.reserve(shapes.size());
    for (ShapeItem* shape : shapes) {
        before.append(shape->state());
        QColor fill = m_window->m_fillColor.isValid() ? m_window->m_fillColor : shape->fillColor();
        if (fill.alpha() == 0) {
            fill = shape->strokeColor();
        }
        shape->setFill(fill, m_window->m_fillStyle);
        after.append(shape->state());
    }

    if (before == after) {
        m_window->statusBar()->showMessage(MainWindow::tr("The fill style did not change"), 3000);
        return;
    }

    m_window->m_undo->push(
        new ShapeStyleCommand(m_window->m_scene, shapes, before, after, MainWindow::tr("Change fill style")));
    m_window->statusBar()->showMessage(
        MainWindow::tr("Applied \"%2\" to %1 shape(s)").arg(shapes.size()).arg(fillStyleName(m_window->m_fillStyle)),
        4000);
}

void StyleController::updateFillActions()
{
    if (!m_window->m_fillButton) {
        return;
    }

    if (m_window->m_fillNoneAct) {
        m_window->m_fillNoneAct->setChecked(m_window->m_fillStyle == FillStyle::None);
    }
    if (m_window->m_fillSolidAct) {
        m_window->m_fillSolidAct->setChecked(m_window->m_fillStyle == FillStyle::Solid);
    }
    if (m_window->m_fillHachureAct) {
        m_window->m_fillHachureAct->setChecked(m_window->m_fillStyle == FillStyle::Hachure);
    }
    if (m_window->m_fillReverseAct) {
        m_window->m_fillReverseAct->setChecked(m_window->m_fillStyle == FillStyle::ReverseHachure);
    }
    if (m_window->m_fillCrossAct) {
        m_window->m_fillCrossAct->setChecked(m_window->m_fillStyle == FillStyle::CrossHatch);
    }
    if (m_window->m_fillGridAct) {
        m_window->m_fillGridAct->setChecked(m_window->m_fillStyle == FillStyle::GridHatch);
    }
    if (m_window->m_fillHoneycombAct) {
        m_window->m_fillHoneycombAct->setChecked(m_window->m_fillStyle == FillStyle::Honeycomb);
    }

    const QColor swatch = m_window->m_fillColor.isValid() ? m_window->m_fillColor : m_window->m_brushColor;
    m_window->m_fillButton->setIcon(makeFillStyleIcon(m_window->m_fillStyle, swatch, MainWindow::kToolBarIconSize));
    if (m_window->m_fillNoneAct) {
        m_window->m_fillNoneAct->setIcon(makeFillStyleIcon(FillStyle::None, swatch, 16));
    }
    if (m_window->m_fillSolidAct) {
        m_window->m_fillSolidAct->setIcon(makeFillStyleIcon(FillStyle::Solid, swatch, 16));
    }
    if (m_window->m_fillHachureAct) {
        m_window->m_fillHachureAct->setIcon(makeFillStyleIcon(FillStyle::Hachure, swatch, 16));
    }
    if (m_window->m_fillReverseAct) {
        m_window->m_fillReverseAct->setIcon(makeFillStyleIcon(FillStyle::ReverseHachure, swatch, 16));
    }
    if (m_window->m_fillCrossAct) {
        m_window->m_fillCrossAct->setIcon(makeFillStyleIcon(FillStyle::CrossHatch, swatch, 16));
    }
    if (m_window->m_fillGridAct) {
        m_window->m_fillGridAct->setIcon(makeFillStyleIcon(FillStyle::GridHatch, swatch, 16));
    }
    if (m_window->m_fillHoneycombAct) {
        m_window->m_fillHoneycombAct->setIcon(makeFillStyleIcon(FillStyle::Honeycomb, swatch, 16));
    }
    const QString colorText = m_window->m_fillColor.isValid() ? m_window->m_fillColor.name(QColor::HexArgb).toUpper()
                                                              : MainWindow::tr("Follow brush color");
    m_window->m_fillButton->setToolTip(
        MainWindow::tr("Fill: %1 · %2 (click to choose)").arg(fillStyleName(m_window->m_fillStyle), colorText));
    if (m_window->m_fillColorAct) {
        m_window->m_fillColorAct->setIcon(makeFillColorIcon(swatch, 16));
    }
    if (m_window->m_fillColorButton) {
        // 工具栏按钮的色板图标单独按工具栏尺寸绘制，菜单项保持 16px。
        m_window->m_fillColorButton->setIcon(makeFillColorIcon(swatch, MainWindow::kToolBarIconSize));
        m_window->m_fillColorButton->setToolTip(MainWindow::tr("Fill color: %1 (click to choose)").arg(colorText));
    }
}

void StyleController::setBrushColor(const QColor& color, bool remember)
{
    if (!color.isValid()) {
        return;
    }

    m_window->m_brushColor = color;
    m_window->m_view->setBrushColor(color);
    if (m_window->m_colorButton) {
        m_window->m_colorButton->setIcon(makeColorIcon(color, MainWindow::kToolBarIconSize));
        m_window->m_colorButton->setToolTip(
            MainWindow::tr("Brush color: %1 (click to choose)").arg(color.name(QColor::HexArgb).toUpper()));
    }
    if (m_window->m_brushColorAct) {
        m_window->m_brushColorAct->setIcon(makeColorIcon(color, 16));
    }
    if (m_window->m_strokeColorAct) {
        m_window->m_strokeColorAct->setIcon(makeStrokeColorIcon(color, 16));
    }
    if (m_window->m_strokeColorButton) {
        m_window->m_strokeColorButton->setIcon(makeStrokeColorIcon(color, MainWindow::kToolBarIconSize));
        m_window->m_strokeColorButton->setToolTip(
            MainWindow::tr("Outline color: %1 (click to choose)").arg(color.name(QColor::HexArgb).toUpper()));
    }
    if (m_window->m_fillButton) {
        updateFillActions();
    }
    if (remember) {
        m_window->m_settings.brushColor = color;
        m_window->m_settings.save();
    }
}

void StyleController::setBrushWidthUi(int width)
{
    width = qBound(1, width, 64);
    m_window->m_brushWidth = width;
    if (m_window->m_view) {
        m_window->m_view->setBrushWidth(width);
    }
    if (m_window->m_widthCombo) {
        int index = m_window->m_widthCombo->findData(width);
        if (index < 0) {
            m_window->m_widthCombo->addItem(QStringLiteral("%1 px").arg(width), width);
            index = m_window->m_widthCombo->count() - 1;
        }
        const QSignalBlocker blocker(m_window->m_widthCombo);
        m_window->m_widthCombo->setCurrentIndex(index);
    }
    if (m_window->m_brushWidthGroup) {
        for (QAction* action : m_window->m_brushWidthGroup->actions()) {
            if (action->data().toInt() == width) {
                const QSignalBlocker blocker(action);
                action->setChecked(true);
                break;
            }
        }
    }
}

void StyleController::setStrokeSmoothingUi(int tolerance)
{
    tolerance = qBound(0, tolerance, 30);
    m_window->m_settings.strokeSmoothing = tolerance;
    if (m_window->m_view) {
        m_window->m_view->setStrokeSmoothing(tolerance);
    }
    if (m_window->m_smoothingSpin) {
        const QSignalBlocker blocker(m_window->m_smoothingSpin);
        m_window->m_smoothingSpin->setValue(tolerance);
    }
}

void StyleController::chooseBrushColor()
{
    const QColor color = QColorDialog::getColor(m_window->m_brushColor, m_window, MainWindow::tr("Choose brush color"),
                                                QColorDialog::ShowAlphaChannel);
    if (color.isValid()) {
        setBrushColor(color, true);
    }
}
