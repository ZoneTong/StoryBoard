#include "StyleController.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "CanvasView.h"
#include "Commands.h"
#include "CompositeShapeItem.h"
#include "CurveItem.h"
#include "IconFactory.h"
#include "ItemStyleVisuals.h"
#include "MainWindow.h"
#include "Settings.h"
#include "ShapeItem.h"

#include <QColorDialog>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QToolButton>
#include <QtMath>


StyleController::StyleController(MainWindow* window) : m_window(window) {}

QIcon StyleController::fillColorIcon(const QColor& color, int logicalSize) const
{
    return ItemStyleVisuals::fillColorIcon(color, logicalSize);
}

QIcon StyleController::strokeColorIcon(const QColor& color, int logicalSize) const
{
    return ItemStyleVisuals::borderColorIcon(color, logicalSize);
}

QIcon StyleController::fillStyleIcon(FillStyle style, const QColor& color, int logicalSize) const
{
    return ItemStyleVisuals::fillStyleIcon(style, color, logicalSize);
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
    if (!m_window->m_scene || !m_window->m_undo) {
        return;
    }

    const QColor initial =
        m_window->m_brushColor.alpha() < 8 ? QColor(QStringLiteral("#1E293B")) : m_window->m_brushColor;
    const QColor color = QColorDialog::getColor(initial, m_window, MainWindow::tr("Choose border color"),
                                                QColorDialog::ShowAlphaChannel);
    if (!color.isValid()) {
        return;
    }

    setBrushColor(color, true);
    const int applied = applyStrokeStyleToSelection(color, 0, true, false, MainWindow::tr("Change border color"));
    if (applied == 0) {
        m_window->statusBar()->showMessage(
            MainWindow::tr("Border color set; it will be used for newly inserted shapes and strokes"), 4000);
    }
}

void StyleController::applyStrokeColorToSelection(const QColor& color)
{
    if (color.isValid()) {
        applyStrokeStyleToSelection(color, 0, true, false, MainWindow::tr("Change border color"));
    }
}

void StyleController::applyStrokeWidthToSelection(qreal width)
{
    if (width > 0) {
        applyStrokeStyleToSelection(QColor(), width, false, true, MainWindow::tr("Change border width"));
    }
}

void StyleController::setStrokeStyle(Qt::PenStyle style)
{
    m_window->m_strokeStyle = style;
    updateStrokeStyleActions();
    applyPenStyleToSelection(style, MainWindow::tr("Change border style"));
}

int StyleController::applyPenStyleToSelection(Qt::PenStyle style, const QString& text)
{
    if (!m_window->m_scene || !m_window->m_undo) {
        return 0;
    }

    QList<CanvasItem*> items;
    QList<Qt::PenStyle> before;
    QList<Qt::PenStyle> after;
    const QList<CanvasItem*> selected = m_window->m_scene->selectedCanvasItems();
    for (CanvasItem* item : selected) {
        Qt::PenStyle current = Qt::SolidLine;
        if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
            current = shape->strokeStyle();
        } else if (auto* stroke = dynamic_cast<CurveItem*>(item)) {
            current = stroke->strokeStyle();
        } else if (auto* composite = dynamic_cast<CompositeShapeItem*>(item)) {
            current = composite->strokeStyle();
        } else {
            continue;
        }
        items.append(item);
        before.append(current);
        after.append(style);
    }

    if (items.isEmpty()) {
        m_window->statusBar()->showMessage(
            MainWindow::tr("Border style set to \"%1\"; it will be used for newly inserted items")
                .arg(ItemStyleVisuals::borderStyleName(style)),
            4000);
        return 0;
    }

    if (before == after) {
        m_window->statusBar()->showMessage(MainWindow::tr("The border style did not change"), 3000);
        return items.size();
    }

    m_window->m_undo->push(new ItemPenStyleCommand(m_window->m_scene, items, before, after, text));
    m_window->statusBar()->showMessage(MainWindow::tr("Applied border style to %1 item(s)").arg(items.size()), 4000);
    return items.size();
}

void StyleController::updateStrokeStyleActions()
{
    for (QAction* action : m_window->m_strokeStyleActions) {
        if (!action) {
            continue;
        }
        const Qt::PenStyle style = static_cast<Qt::PenStyle>(action->data().toInt());
        action->setChecked(style == m_window->m_strokeStyle);
        action->setIcon(ItemStyleVisuals::borderStyleIcon(style, 16));
        action->setToolTip(ItemStyleVisuals::borderStyleName(style));
    }
    if (m_window->m_strokeStyleButton) {
        m_window->m_strokeStyleButton->setIcon(
            ItemStyleVisuals::borderStyleIcon(m_window->m_strokeStyle, MainWindow::kToolBarIconSize));
        m_window->m_strokeStyleButton->setToolTip(MainWindow::tr("Border style: %1 (click to choose)")
                                                      .arg(ItemStyleVisuals::borderStyleName(m_window->m_strokeStyle)));
    }
    if (m_window->m_strokeStyleMenu) {
        m_window->m_strokeStyleMenu->menuAction()->setIcon(
            ItemStyleVisuals::borderStyleIcon(m_window->m_strokeStyle, 16));
    }
}

int StyleController::applyStrokeStyleToSelection(const QColor& color, qreal width, bool changeColor, bool changeWidth,
                                                 const QString& text)
{
    if (!m_window->m_scene || !m_window->m_undo) {
        return 0;
    }

    QList<CanvasItem*> items;
    const QList<CanvasItem*> selected = m_window->m_scene->selectedCanvasItems();
    for (CanvasItem* item : selected) {
        if (dynamic_cast<ShapeItem*>(item) || dynamic_cast<CurveItem*>(item) ||
            dynamic_cast<CompositeShapeItem*>(item)) {
            items.append(item);
        }
    }
    if (items.isEmpty()) {
        return 0;
    }

    QList<QColor> beforeColors;
    QList<qreal> beforeWidths;
    QList<QColor> afterColors;
    QList<qreal> afterWidths;
    beforeColors.reserve(items.size());
    beforeWidths.reserve(items.size());
    afterColors.reserve(items.size());
    afterWidths.reserve(items.size());
    for (CanvasItem* item : items) {
        QColor beforeColor;
        qreal beforeWidth = 0;
        QColor afterColor;
        qreal afterWidth = 0;
        if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
            beforeColor = shape->strokeColor();
            beforeWidth = shape->strokeWidth();
            afterColor = changeColor ? color : beforeColor;
            afterWidth = changeWidth ? width : beforeWidth;
            shape->setStyle(afterColor, shape->fillColor(), afterWidth);
        } else if (auto* stroke = dynamic_cast<CurveItem*>(item)) {
            beforeColor = stroke->strokeColor();
            beforeWidth = stroke->strokeWidth();
            afterColor = changeColor ? color : beforeColor;
            afterWidth = changeWidth ? width : beforeWidth;
            stroke->setStyle(afterColor, afterWidth);
        } else if (auto* composite = dynamic_cast<CompositeShapeItem*>(item)) {
            beforeColor = composite->strokeColor();
            beforeWidth = composite->strokeWidth();
            afterColor = changeColor ? color : beforeColor;
            afterWidth = changeWidth ? width : beforeWidth;
            composite->setStroke(afterColor, afterWidth);
        }
        beforeColors.append(beforeColor);
        beforeWidths.append(beforeWidth);
        afterColors.append(afterColor);
        afterWidths.append(afterWidth);
    }

    if (beforeColors == afterColors && beforeWidths == afterWidths) {
        m_window->statusBar()->showMessage(MainWindow::tr("The border style did not change"), 3000);
        return items.size();
    }

    m_window->m_undo->push(new ItemStrokeStyleCommand(m_window->m_scene, items, beforeColors, beforeWidths, afterColors,
                                                      afterWidths, text));
    m_window->statusBar()->showMessage(MainWindow::tr("Applied border style to %1 item(s)").arg(items.size()), 4000);
    return items.size();
}

void StyleController::applyFillToSelection()
{
    if (!m_window->m_scene || !m_window->m_undo) {
        return;
    }

    QList<ShapeItem*> shapes;
    QList<CompositeShapeItem*> composites;
    int outlineOnlyCount = 0;
    const QList<CanvasItem*> selected = m_window->m_scene->selectedCanvasItems();
    for (CanvasItem* item : selected) {
        if (auto* composite = dynamic_cast<CompositeShapeItem*>(item)) {
            composites.append(composite);
            continue;
        }
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

    if (shapes.isEmpty() && composites.isEmpty()) {
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
    QList<QColor> beforeCompositeColors;
    QList<FillStyle> beforeCompositeStyles;
    QList<QColor> afterCompositeColors;
    QList<FillStyle> afterCompositeStyles;
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
    for (CompositeShapeItem* composite : composites) {
        beforeCompositeColors.append(composite->fillColor());
        beforeCompositeStyles.append(composite->fillStyle());
        QColor fill = m_window->m_fillColor.isValid() ? m_window->m_fillColor : composite->fillColor();
        if (fill.alpha() == 0) {
            fill = composite->strokeColor();
        }
        composite->setFill(fill, m_window->m_fillStyle);
        afterCompositeColors.append(composite->fillColor());
        afterCompositeStyles.append(composite->fillStyle());
    }

    if (before == after && beforeCompositeColors == afterCompositeColors &&
        beforeCompositeStyles == afterCompositeStyles) {
        m_window->statusBar()->showMessage(MainWindow::tr("The fill style did not change"), 3000);
        return;
    }

    const bool mixed = !shapes.isEmpty() && !composites.isEmpty();
    if (mixed) {
        m_window->m_undo->beginMacro(MainWindow::tr("Change fill style"));
    }
    if (!shapes.isEmpty()) {
        m_window->m_undo->push(
            new ShapeStyleCommand(m_window->m_scene, shapes, before, after, MainWindow::tr("Change fill style")));
    }
    if (!composites.isEmpty()) {
        QList<CanvasItem*> compositeItems;
        for (CompositeShapeItem* composite : composites) {
            compositeItems.append(composite);
        }
        m_window->m_undo->push(new CompositeFillStyleCommand(
            m_window->m_scene, compositeItems, beforeCompositeColors, beforeCompositeStyles, afterCompositeColors,
            afterCompositeStyles, MainWindow::tr("Change fill style")));
    }
    if (mixed) {
        m_window->m_undo->endMacro();
    }
    m_window->statusBar()->showMessage(MainWindow::tr("Applied \"%2\" to %1 shape(s)")
                                           .arg(shapes.size() + composites.size())
                                           .arg(fillStyleName(m_window->m_fillStyle)),
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
    if (m_window->m_fillHorizontalAct) {
        m_window->m_fillHorizontalAct->setChecked(m_window->m_fillStyle == FillStyle::HorizontalHatch);
    }
    if (m_window->m_fillVerticalAct) {
        m_window->m_fillVerticalAct->setChecked(m_window->m_fillStyle == FillStyle::VerticalHatch);
    }
    if (m_window->m_fillBrickAct) {
        m_window->m_fillBrickAct->setChecked(m_window->m_fillStyle == FillStyle::Brick);
    }
    if (m_window->m_fillDotsAct) {
        m_window->m_fillDotsAct->setChecked(m_window->m_fillStyle == FillStyle::Dots);
    }

    const QColor swatch = m_window->m_fillColor.isValid() ? m_window->m_fillColor : m_window->m_brushColor;
    m_window->m_fillButton->setIcon(
        ItemStyleVisuals::fillStyleIcon(m_window->m_fillStyle, swatch, MainWindow::kToolBarIconSize));
    if (m_window->m_fillNoneAct) {
        m_window->m_fillNoneAct->setIcon(ItemStyleVisuals::fillStyleIcon(FillStyle::None, swatch, 16));
    }
    if (m_window->m_fillSolidAct) {
        m_window->m_fillSolidAct->setIcon(ItemStyleVisuals::fillStyleIcon(FillStyle::Solid, swatch, 16));
    }
    if (m_window->m_fillHachureAct) {
        m_window->m_fillHachureAct->setIcon(ItemStyleVisuals::fillStyleIcon(FillStyle::Hachure, swatch, 16));
    }
    if (m_window->m_fillReverseAct) {
        m_window->m_fillReverseAct->setIcon(ItemStyleVisuals::fillStyleIcon(FillStyle::ReverseHachure, swatch, 16));
    }
    if (m_window->m_fillCrossAct) {
        m_window->m_fillCrossAct->setIcon(ItemStyleVisuals::fillStyleIcon(FillStyle::CrossHatch, swatch, 16));
    }
    if (m_window->m_fillGridAct) {
        m_window->m_fillGridAct->setIcon(ItemStyleVisuals::fillStyleIcon(FillStyle::GridHatch, swatch, 16));
    }
    if (m_window->m_fillHoneycombAct) {
        m_window->m_fillHoneycombAct->setIcon(ItemStyleVisuals::fillStyleIcon(FillStyle::Honeycomb, swatch, 16));
    }
    if (m_window->m_fillHorizontalAct) {
        m_window->m_fillHorizontalAct->setIcon(ItemStyleVisuals::fillStyleIcon(FillStyle::HorizontalHatch, swatch, 16));
    }
    if (m_window->m_fillVerticalAct) {
        m_window->m_fillVerticalAct->setIcon(ItemStyleVisuals::fillStyleIcon(FillStyle::VerticalHatch, swatch, 16));
    }
    if (m_window->m_fillBrickAct) {
        m_window->m_fillBrickAct->setIcon(ItemStyleVisuals::fillStyleIcon(FillStyle::Brick, swatch, 16));
    }
    if (m_window->m_fillDotsAct) {
        m_window->m_fillDotsAct->setIcon(ItemStyleVisuals::fillStyleIcon(FillStyle::Dots, swatch, 16));
    }
    const QString colorText = m_window->m_fillColor.isValid() ? m_window->m_fillColor.name(QColor::HexArgb).toUpper()
                                                              : MainWindow::tr("Follow brush color");
    m_window->m_fillButton->setToolTip(
        MainWindow::tr("Fill: %1 · %2 (click to choose)").arg(fillStyleName(m_window->m_fillStyle), colorText));
    if (m_window->m_fillColorAct) {
        m_window->m_fillColorAct->setIcon(fillColorIcon(swatch, 16));
    }
    if (m_window->m_fillColorButton) {
        // 工具栏按钮的色板图标单独按工具栏尺寸绘制，菜单项保持 16px。
        m_window->m_fillColorButton->setIcon(fillColorIcon(swatch, MainWindow::kToolBarIconSize));
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
    if (m_window->m_strokeColorAct) {
        m_window->m_strokeColorAct->setIcon(strokeColorIcon(color, 16));
    }
    if (m_window->m_strokeColorButton) {
        m_window->m_strokeColorButton->setIcon(strokeColorIcon(color, MainWindow::kToolBarIconSize));
        m_window->m_strokeColorButton->setToolTip(
            MainWindow::tr("Border color: %1 (click to choose)").arg(color.name(QColor::HexArgb).toUpper()));
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
    for (QSpinBox* spin : {m_window->m_widthSpin}) {
        if (spin) {
            const QSignalBlocker blocker(spin);
            spin->setValue(width);
        }
    }
}

void StyleController::setStrokeSmoothingUi(qreal tolerance)
{
    tolerance = qBound<qreal>(0.0, tolerance, 20.0);
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
