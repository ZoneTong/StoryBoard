#include "ui/widgets/StyleController.h"

#include "app/Settings.h"
#include "canvas/CanvasView.h"
#include "items/CanvasItem.h"
#include "items/lines/LineAbstractItem.h"
#include "items/polygon/PolygonAccess.h"
#include "items/polygon/PolygonItem.h"
#include "items/properties/FillStyleMeta.h"
#include "items/properties/ItemStyleVisuals.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "scene/CanvasScene.h"
#include "scene/Commands.h"
#include "ui/icons/IconFactory.h"
#include "ui/mainwindow/MainWindow.h"

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

QIcon StyleController::FillColorIcon(const QColor& color, int logicalSize) const
{
    return ItemStyleVisuals::FillColorIcon(color, logicalSize);
}

QIcon StyleController::StrokeColorIcon(const QColor& color, int logicalSize) const
{
    return ItemStyleVisuals::BorderColorIcon(color, logicalSize);
}

QIcon StyleController::FillStyleIcon(FillStyle style, const QColor& color, int logicalSize) const
{
    return ItemStyleVisuals::FillStyleIcon(style, color, logicalSize);
}

void StyleController::SetFillStyle(FillStyle style)
{
    m_window->m_fillStyle = style;
    UpdateFillActions();
    ApplyFillToSelection();
}

void StyleController::ChooseFillColor()
{
    const QColor initial = m_window->m_fillColor.isValid() ? m_window->m_fillColor : QColor(59, 130, 246, 96);
    const QColor color =
        QColorDialog::getColor(initial, m_window, MainWindow::tr("Choose fill color"), QColorDialog::ShowAlphaChannel);
    if (!color.isValid()) {
        return;
    }
    m_window->m_fillColor = color;
    UpdateFillActions();
    ApplyFillToSelection();
}

void StyleController::ChooseStrokeColor()
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

    SetBrushColor(color, true);
    const int applied = ApplyStrokeStyleToSelection(color, 0, true, false, MainWindow::tr("Change border color"));
    if (applied == 0) {
        m_window->statusBar()->showMessage(
            MainWindow::tr("Border color set; it will be used for newly inserted shapes and strokes"), 4000);
    }
}

void StyleController::ApplyStrokeColorToSelection(const QColor& color)
{
    if (color.isValid()) {
        ApplyStrokeStyleToSelection(color, 0, true, false, MainWindow::tr("Change border color"));
    }
}

void StyleController::ApplyStrokeWidthToSelection(qreal width)
{
    if (width > 0) {
        ApplyStrokeStyleToSelection(QColor(), width, false, true, MainWindow::tr("Change border width"));
    }
}

void StyleController::SetStrokeStyle(Qt::PenStyle style)
{
    m_window->m_strokeStyle = style;
    UpdateStrokeStyleActions();
    ApplyPenStyleToSelection(style, MainWindow::tr("Change border style"));
}

int StyleController::ApplyPenStyleToSelection(Qt::PenStyle style, const QString& text)
{
    if (!m_window->m_scene || !m_window->m_undo) {
        return 0;
    }

    QList<CanvasItem*> items;
    QList<QMap<PropertyKey, QVariant>> before;
    QList<QMap<PropertyKey, QVariant>> after;
    const QList<CanvasItem*> selected = m_window->m_scene->SelectedCanvasItems();
    for (CanvasItem* item : selected) {
        if (!item->Properties().Contains(ItemProperty::STROKE_STYLE)) {
            continue;
        }
        QMap<PropertyKey, QVariant> beforeValues;
        beforeValues.insert(ItemProperty::STROKE_STYLE, item->PropertyValue(ItemProperty::STROKE_STYLE));
        QMap<PropertyKey, QVariant> afterValues = beforeValues;
        afterValues.insert(ItemProperty::STROKE_STYLE, static_cast<int>(style));
        items.append(item);
        before.append(beforeValues);
        after.append(afterValues);
    }

    if (items.isEmpty()) {
        m_window->statusBar()->showMessage(
            MainWindow::tr("Border style set to \"%1\"; it will be used for newly inserted items")
                .arg(ItemStyleVisuals::BorderStyleName(style)),
            4000);
        return 0;
    }

    if (before == after) {
        m_window->statusBar()->showMessage(MainWindow::tr("The border style did not change"), 3000);
        return items.size();
    }

    for (CanvasItem* item : items) {
        item->SetPropertyValue(ItemProperty::STROKE_STYLE, static_cast<int>(style));
    }
    m_window->m_undo->push(new ItemPropertyCommand(m_window->m_scene, items, before, after, text));
    m_window->statusBar()->showMessage(MainWindow::tr("Applied border style to %n Item(s)", nullptr, items.size()),
                                       4000);
    return items.size();
}

void StyleController::UpdateStrokeStyleActions()
{
    for (QAction* action : m_window->m_strokeStyleActions) {
        if (!action) {
            continue;
        }
        const Qt::PenStyle style = static_cast<Qt::PenStyle>(action->data().toInt());
        action->setChecked(style == m_window->m_strokeStyle);
        action->setIcon(ItemStyleVisuals::BorderStyleIcon(style, 16));
        action->setToolTip(ItemStyleVisuals::BorderStyleName(style));
    }
    if (m_window->m_strokeStyleButton) {
        m_window->m_strokeStyleButton->setIcon(
            ItemStyleVisuals::BorderStyleIcon(m_window->m_strokeStyle, MainWindow::TOOL_BAR_ICON_SIZE));
        m_window->m_strokeStyleButton->setToolTip(MainWindow::tr("Border style: %1 (click to choose)")
                                                      .arg(ItemStyleVisuals::BorderStyleName(m_window->m_strokeStyle)));
    }
    if (m_window->m_strokeStyleMenu) {
        m_window->m_strokeStyleMenu->menuAction()->setIcon(
            ItemStyleVisuals::BorderStyleIcon(m_window->m_strokeStyle, 16));
    }
}

int StyleController::ApplyStrokeStyleToSelection(const QColor& color, qreal width, bool changeColor, bool changeWidth,
                                                 const QString& text)
{
    if (!m_window->m_scene || !m_window->m_undo) {
        return 0;
    }

    QList<CanvasItem*> items;
    const QList<CanvasItem*> selected = m_window->m_scene->SelectedCanvasItems();
    for (CanvasItem* item : selected) {
        const ItemPropertySet properties = item->Properties();
        if (properties.Contains(ItemProperty::STROKE_COLOR) && properties.Contains(ItemProperty::STROKE_WIDTH)) {
            items.append(item);
        }
    }
    if (items.isEmpty()) {
        return 0;
    }

    QList<QMap<PropertyKey, QVariant>> before;
    QList<QMap<PropertyKey, QVariant>> after;
    before.reserve(items.size());
    after.reserve(items.size());
    for (CanvasItem* item : items) {
        QMap<PropertyKey, QVariant> beforeValues;
        beforeValues.insert(ItemProperty::STROKE_COLOR, item->PropertyValue(ItemProperty::STROKE_COLOR));
        beforeValues.insert(ItemProperty::STROKE_WIDTH, item->PropertyValue(ItemProperty::STROKE_WIDTH));
        QMap<PropertyKey, QVariant> afterValues = beforeValues;
        if (changeColor) {
            afterValues.insert(ItemProperty::STROKE_COLOR, color);
        }
        if (changeWidth) {
            afterValues.insert(ItemProperty::STROKE_WIDTH, width);
        }
        before.append(beforeValues);
        after.append(afterValues);
    }

    if (before == after) {
        m_window->statusBar()->showMessage(MainWindow::tr("The border style did not change"), 3000);
        return items.size();
    }

    for (int index = 0; index < items.size(); ++index) {
        for (auto it = after.at(index).constBegin(); it != after.at(index).constEnd(); ++it) {
            items.at(index)->SetPropertyValueByKey(it.key(), it.value());
        }
    }
    m_window->m_undo->push(new ItemPropertyCommand(m_window->m_scene, items, before, after, text));
    m_window->statusBar()->showMessage(MainWindow::tr("Applied border style to %n Item(s)", nullptr, items.size()),
                                       4000);
    return items.size();
}

void StyleController::ApplyFillToSelection()
{
    if (!m_window->m_scene || !m_window->m_undo) {
        return;
    }

    QList<ShapeAbstractItem*> targets;
    int outlineOnlyCount = 0;
    const QList<CanvasItem*> selected = m_window->m_scene->SelectedCanvasItems();
    for (CanvasItem* item : selected) {
        auto* shape = dynamic_cast<ShapeAbstractItem*>(item);
        if (!shape) {
            continue;
        }
        // 填充图形与多边形都可带填充；线、花括号等纯描线图元不参与。
        if (ShapeTypeIsFilled(shape->shapeType()) || PolygonAccess::Is(shape)) { // 多边形走访问者判别（D16）
            targets.append(shape);
        } else {
            ++outlineOnlyCount;
        }
    }

    if (targets.isEmpty()) {
        if (outlineOnlyCount > 0) {
            m_window->statusBar()->showMessage(
                MainWindow::tr("The selected shape is stroke-only (line, brace, etc.); fill is not supported"), 4000);
        } else {
            m_window->statusBar()->showMessage(
                MainWindow::tr("Fill set to \"%1\"; it will be used for newly inserted shapes")
                    .arg(FillStyles::Name(m_window->m_fillStyle)),
                4000);
        }
        return;
    }

    QList<CanvasItem*> items;
    QList<QMap<PropertyKey, QVariant>> before;
    QList<QMap<PropertyKey, QVariant>> after;
    before.reserve(targets.size());
    after.reserve(targets.size());
    for (ShapeAbstractItem* shape : targets) {
        QMap<PropertyKey, QVariant> beforeValues;
        beforeValues.insert(ItemProperty::FILL_COLOR, shape->PropertyValue(ItemProperty::FILL_COLOR));
        beforeValues.insert(ItemProperty::FILL_STYLE, shape->PropertyValue(ItemProperty::FILL_STYLE));
        QColor fill = m_window->m_fillColor.isValid() ? m_window->m_fillColor : shape->FillColor();
        if (fill.alpha() == 0) {
            fill = shape->StrokeColor();
        }
        QMap<PropertyKey, QVariant> afterValues = beforeValues;
        afterValues.insert(ItemProperty::FILL_COLOR, fill);
        afterValues.insert(ItemProperty::FILL_STYLE, static_cast<int>(m_window->m_fillStyle));
        items.append(shape);
        before.append(beforeValues);
        after.append(afterValues);
    }

    if (before == after) {
        m_window->statusBar()->showMessage(MainWindow::tr("The fill style did not change"), 3000);
        return;
    }

    for (int index = 0; index < items.size(); ++index) {
        for (auto it = after.at(index).constBegin(); it != after.at(index).constEnd(); ++it) {
            items.at(index)->SetPropertyValueByKey(it.key(), it.value());
        }
    }
    m_window->m_undo->push(
        new ItemPropertyCommand(m_window->m_scene, items, before, after, MainWindow::tr("Change fill style")));
    m_window->statusBar()->showMessage(MainWindow::tr("Applied \"%2\" to %1 shape(s)")
                                           .arg(targets.size())
                                           .arg(FillStyles::Name(m_window->m_fillStyle)),
                                       4000);
}

void StyleController::UpdateFillActions()
{
    if (!m_window->m_fillButton) {
        return;
    }

    const QList<FillStyle> fillStyles = FillStyleMeta::Styles();
    for (int i = 0; i < fillStyles.size() && i < m_window->m_fillStyleActions.size(); ++i) {
        m_window->m_fillStyleActions.at(i)->setChecked(m_window->m_fillStyle == fillStyles.at(i));
    }

    const QColor swatch = m_window->m_fillColor.isValid() ? m_window->m_fillColor : m_window->m_brushColor;
    m_window->m_fillButton->setIcon(
        ItemStyleVisuals::FillStyleIcon(m_window->m_fillStyle, swatch, MainWindow::TOOL_BAR_ICON_SIZE));
    for (int i = 0; i < fillStyles.size() && i < m_window->m_fillStyleActions.size(); ++i) {
        m_window->m_fillStyleActions.at(i)->setIcon(ItemStyleVisuals::FillStyleIcon(fillStyles.at(i), swatch, 16));
    }
    const QString colorText = m_window->m_fillColor.isValid() ? m_window->m_fillColor.name(QColor::HexArgb).toUpper()
                                                              : MainWindow::tr("Follow brush color");
    m_window->m_fillButton->setToolTip(
        MainWindow::tr("Fill: %1 · %2 (click to choose)").arg(FillStyles::Name(m_window->m_fillStyle), colorText));
    if (m_window->m_fillColorAct) {
        m_window->m_fillColorAct->setIcon(FillColorIcon(swatch, 16));
    }
    if (m_window->m_fillColorButton) {
        // 工具栏按钮的色板图标单独按工具栏尺寸绘制，菜单项保持 16px。
        m_window->m_fillColorButton->setIcon(FillColorIcon(swatch, MainWindow::TOOL_BAR_ICON_SIZE));
        m_window->m_fillColorButton->setToolTip(MainWindow::tr("Fill color: %1 (click to choose)").arg(colorText));
    }
}

void StyleController::SetBrushColor(const QColor& color, bool remember)
{
    if (!color.isValid()) {
        return;
    }

    m_window->m_brushColor = color;
    m_window->m_view->SetBrushColor(color);
    if (m_window->m_strokeColorAct) {
        m_window->m_strokeColorAct->setIcon(StrokeColorIcon(color, 16));
    }
    if (m_window->m_strokeColorButton) {
        m_window->m_strokeColorButton->setIcon(StrokeColorIcon(color, MainWindow::TOOL_BAR_ICON_SIZE));
        m_window->m_strokeColorButton->setToolTip(
            MainWindow::tr("Border color: %1 (click to choose)").arg(color.name(QColor::HexArgb).toUpper()));
    }
    if (m_window->m_fillButton) {
        UpdateFillActions();
    }
    if (remember) {
        m_window->m_settings.brushColor = color;
        m_window->m_settings.Save();
    }
}

void StyleController::SetBrushWidthUi(int width)
{
    width = qBound(1, width, 64);
    m_window->m_brushWidth = width;
    if (m_window->m_view) {
        m_window->m_view->SetBrushWidth(width);
    }
    for (QSpinBox* spin : {m_window->m_widthSpin}) {
        if (spin) {
            const QSignalBlocker blocker(spin);
            spin->setValue(width);
        }
    }
}

void StyleController::SetStrokeSmoothingUi(qreal tolerance)
{
    tolerance = qBound<qreal>(0.0, tolerance, 20.0);
    m_window->m_settings.strokeSmoothing = tolerance;
    if (m_window->m_view) {
        m_window->m_view->SetStrokeSmoothing(tolerance);
    }
    if (m_window->m_smoothingSpin) {
        const QSignalBlocker blocker(m_window->m_smoothingSpin);
        m_window->m_smoothingSpin->setValue(tolerance);
    }
}

void StyleController::ChooseBrushColor()
{
    const QColor color = QColorDialog::getColor(m_window->m_brushColor, m_window, MainWindow::tr("Choose brush color"),
                                                QColorDialog::ShowAlphaChannel);
    if (color.isValid()) {
        SetBrushColor(color, true);
    }
}
