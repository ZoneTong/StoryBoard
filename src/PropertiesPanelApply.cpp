// 属性面板的全部应用方法（颜色、文字、线型与几何参数）。
#include "ArcItem.h"
#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "CurveParamCommand.h"
#include "ImagePickerDialog.h"
#include "LineItemAbstract.h"
#include "PolygonItem.h"
#include "PropertiesPanel.h"
#include "PropertiesPanelArrangeCard.h"
#include "PropertiesPanelColorsCard.h"
#include "PropertiesPanelContentCard.h"
#include "PropertiesPanelFillBorderCard.h"
#include "PropertiesPanelSelectionCard.h"
#include "PropertiesPanelValueCard.h"
#include "ShapeItem.h"
#include "ShapeMeta.h"
#include "SineItem.h"
#include "TextItem.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QScrollArea>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>


void PropertiesPanel::changeSelectedShapes(const QString& text, const std::function<void(ShapeItem*)>& mutate)
{
    const QList<ShapeItem*> shapes = selectedShapes();
    if (shapes.isEmpty() || !m_scene || !m_scene->undoStack()) {
        return;
    }

    // 按各图形上报的属性全集做快照：角度 / 底边等编辑会连带派生尺寸，
    // 全量快照保证撤销 / 重做与原整份状态命令等价。
    QList<CanvasItem*> items;
    QList<QMap<ItemProperty, QVariant>> before;
    QList<QMap<ItemProperty, QVariant>> after;
    before.reserve(shapes.size());
    after.reserve(shapes.size());
    for (ShapeItem* shape : shapes) {
        QMap<ItemProperty, QVariant> beforeValues;
        for (ItemProperty property : shape->properties().values()) {
            beforeValues.insert(property, shape->propertyValue(property));
        }
        items.append(shape);
        before.append(beforeValues);
    }
    m_refreshing = true;
    for (ShapeItem* shape : shapes) {
        mutate(shape);
    }
    m_refreshing = false;
    for (ShapeItem* shape : shapes) {
        QMap<ItemProperty, QVariant> afterValues;
        for (ItemProperty property : shape->properties().values()) {
            afterValues.insert(property, shape->propertyValue(property));
        }
        after.append(afterValues);
    }

    m_scene->undoStack()->push(new ItemPropertyCommand(m_scene, items, before, after, text));
    refresh();
}

void PropertiesPanel::applyPropertyValues(const QMap<ItemProperty, QVariant>& values, const QString& text,
                                          const QList<CanvasItem*>& targets)
{
    if (!m_scene || !m_scene->undoStack()) {
        return;
    }
    const QList<CanvasItem*> items = targets.isEmpty() ? selectedItems() : targets;

    QList<CanvasItem*> applyItems;
    QList<QMap<ItemProperty, QVariant>> before;
    QList<QMap<ItemProperty, QVariant>> after;
    m_refreshing = true;
    for (CanvasItem* item : items) {
        const ItemPropertySet properties = item->properties();
        QMap<ItemProperty, QVariant> beforeValues;
        QMap<ItemProperty, QVariant> afterValues;
        for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
            if (!properties.contains(it.key())) {
                continue;
            }
            beforeValues.insert(it.key(), item->propertyValue(it.key()));
            afterValues.insert(it.key(), it.value());
        }
        if (afterValues.isEmpty()) {
            continue;
        }
        // 先写值，命令随后 push 时的 redo 幂等执行；撤销 / 重做都按快照回放。
        for (auto it = afterValues.constBegin(); it != afterValues.constEnd(); ++it) {
            item->setPropertyValue(it.key(), it.value());
        }
        applyItems.append(item);
        before.append(beforeValues);
        after.append(afterValues);
    }
    m_refreshing = false;

    if (applyItems.isEmpty()) {
        return;
    }
    m_scene->undoStack()->push(new ItemPropertyCommand(m_scene, applyItems, before, after, text));
    refresh();
}

void PropertiesPanel::applyStrokeColor(const QColor& color)
{
    const QList<CanvasItem*> items = selectedStrokeTargets();
    if (!color.isValid() || items.isEmpty()) {
        return;
    }
    emit strokeColorPicked(color);
    applyPropertyValues({{ItemProperty::STROKE_COLOR, color}}, tr("Change border color"), items);
}

void PropertiesPanel::applyFillColor(const QColor& color)
{
    const QList<CanvasItem*> items = selectedItems();
    if (!color.isValid() || items.isEmpty() || !m_scene || !m_scene->undoStack()) {
        return;
    }

    QList<CanvasItem*> targets;
    QList<QMap<ItemProperty, QVariant>> before;
    QList<QMap<ItemProperty, QVariant>> after;
    m_refreshing = true;
    for (CanvasItem* item : items) {
        const ItemPropertySet properties = item->properties();
        if (!properties.contains(ItemProperty::FILL_COLOR)) {
            continue;
        }
        QMap<ItemProperty, QVariant> beforeValues;
        beforeValues.insert(ItemProperty::FILL_COLOR, item->propertyValue(ItemProperty::FILL_COLOR));
        if (properties.contains(ItemProperty::FILL_STYLE)) {
            beforeValues.insert(ItemProperty::FILL_STYLE, item->propertyValue(ItemProperty::FILL_STYLE));
        }
        QMap<ItemProperty, QVariant> afterValues = beforeValues;
        afterValues.insert(ItemProperty::FILL_COLOR, color);
        if (properties.contains(ItemProperty::FILL_STYLE) &&
            item->propertyValue(ItemProperty::FILL_STYLE).toInt() == static_cast<int>(FillStyle::None)) {
            afterValues.insert(ItemProperty::FILL_STYLE, static_cast<int>(FillStyle::Solid));
        }
        targets.append(item);
        before.append(beforeValues);
        after.append(afterValues);
    }
    // 先写值再入栈：命令的 redo 会被 push 重复执行一次，幂等即可，这里保持与旧实现一致。
    for (int index = 0; index < targets.size(); ++index) {
        for (auto it = after.at(index).constBegin(); it != after.at(index).constEnd(); ++it) {
            targets.at(index)->setPropertyValue(it.key(), it.value());
        }
    }
    m_refreshing = false;
    if (targets.isEmpty()) {
        return;
    }
    m_scene->undoStack()->push(new ItemPropertyCommand(m_scene, targets, before, after, tr("Change shape fill color")));
    refresh();
}

void PropertiesPanel::applyFillStyle(FillStyle style)
{
    if (selectedItems().isEmpty() || !m_scene || !m_scene->undoStack()) {
        return;
    }
    applyPropertyValues({{ItemProperty::FILL_STYLE, static_cast<int>(style)}}, tr("Change shape fill style"));
}

void PropertiesPanel::applyBorderStyle(Qt::PenStyle style)
{
    const QList<CanvasItem*> items = selectedStrokeTargets();
    if (items.isEmpty()) {
        return;
    }
    applyPropertyValues({{ItemProperty::STROKE_STYLE, static_cast<int>(style)}}, tr("Change shape border style"),
                        items);
}

void PropertiesPanel::applyBorderWidth(qreal width)
{
    const QList<CanvasItem*> items = selectedStrokeTargets();
    if (items.isEmpty()) {
        return;
    }
    emit borderWidthPicked(width);
    applyPropertyValues({{ItemProperty::STROKE_WIDTH, width}}, tr("Change border width"), items);
}

void PropertiesPanel::applyOpacity(int percent)
{
    if (selectedItems().isEmpty()) {
        return;
    }
    applyPropertyValues({{ItemProperty::OPACITY, percent / 100.0}}, tr("Change opacity"));
}

void PropertiesPanel::changeSelectedTexts(const QString& text, const std::function<void(TextItemState&)>& mutate)
{
    const QList<CanvasItem*> texts = selectedTexts();
    if (texts.isEmpty() || !m_scene || !m_scene->undoStack()) {
        return;
    }

    QList<TextItemState> beforeStates;
    QList<TextItemState> afterStates;
    bool changed = false;
    for (CanvasItem* item : texts) {
        const TextItemState before = item->textState();
        TextItemState after = before;
        mutate(after);
        beforeStates.append(before);
        afterStates.append(after);
        changed = changed || after != before;
    }
    if (!changed) {
        return;
    }

    QUndoStack* undo = m_scene->undoStack();
    undo->beginMacro(text);
    for (int i = 0; i < texts.size(); ++i) {
        undo->push(new TextItemCommand(m_scene, texts.at(i), beforeStates.at(i), afterStates.at(i), text));
    }
    undo->endMacro();
    refresh();
}

void PropertiesPanel::applyTextContent(const QString& text)
{
    changeSelectedTexts(tr("Change text content"), [&text](TextItemState& state) {
        if (state.text == text) {
            return;
        }
        state.text = text;
        state.baseSize = TextItem::measure(state.text, state.font);
        state.size = state.baseSize;
    });
}

void PropertiesPanel::applyTextFont(const QFont& font)
{
    changeSelectedTexts(tr("Change text font"), [&font](TextItemState& state) {
        if (state.font == font) {
            return;
        }
        state.font = font;
        state.baseSize = TextItem::measure(state.text, state.font);
        state.size = state.baseSize;
    });
}

void PropertiesPanel::applyTextColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    changeSelectedTexts(tr("Change text color"), [&color](TextItemState& state) { state.color = color; });
}

void PropertiesPanel::changeSelectedArcs(const QString& text, const std::function<void(LineItemAbstract*)>& mutate)
{
    QList<LineItemAbstract*> arcs;
    for (LineItemAbstract* stroke : selectedStrokes()) {
        if (stroke->supportsArcAngles()) {
            arcs.append(stroke);
        }
    }
    if (arcs.isEmpty() || !m_scene || !m_scene->undoStack()) {
        return;
    }

    QList<CurveParamState> paramsBefore;
    QList<LineItemAbstract::GeometryState> geometryBefore;
    QList<CurveParamState> paramsAfter;
    QList<LineItemAbstract::GeometryState> geometryAfter;
    QList<LineItemAbstract*> targets;
    for (LineItemAbstract* arc : arcs) {
        targets.append(arc);
        paramsBefore.append(curveParamState(arc));
        geometryBefore.append(arc->geometryState());
    }
    m_refreshing = true;
    for (LineItemAbstract* arc : arcs) {
        mutate(arc);
        paramsAfter.append(curveParamState(arc));
        geometryAfter.append(arc->geometryState());
    }
    m_refreshing = false;

    m_scene->undoStack()->push(
        new CurveParamCommand(m_scene, targets, paramsBefore, geometryBefore, paramsAfter, geometryAfter, text));
    refresh();
}

void PropertiesPanel::changeSelectedSines(const QString& text, const std::function<void(LineItemAbstract*)>& mutate)
{
    QList<LineItemAbstract*> sines;
    for (LineItemAbstract* stroke : selectedStrokes()) {
        if (stroke->supportsSineWave()) {
            sines.append(stroke);
        }
    }
    if (sines.isEmpty() || !m_scene || !m_scene->undoStack()) {
        return;
    }

    QList<CurveParamState> paramsBefore;
    QList<LineItemAbstract::GeometryState> geometryBefore;
    QList<CurveParamState> paramsAfter;
    QList<LineItemAbstract::GeometryState> geometryAfter;
    QList<LineItemAbstract*> targets;
    for (LineItemAbstract* sine : sines) {
        targets.append(sine);
        paramsBefore.append(curveParamState(sine));
        geometryBefore.append(sine->geometryState());
    }
    m_refreshing = true;
    for (LineItemAbstract* sine : sines) {
        mutate(sine);
        paramsAfter.append(curveParamState(sine));
        geometryAfter.append(sine->geometryState());
    }
    m_refreshing = false;

    m_scene->undoStack()->push(
        new CurveParamCommand(m_scene, targets, paramsBefore, geometryBefore, paramsAfter, geometryAfter, text));
    refresh();
}

void PropertiesPanel::chooseFillImage()
{
    // 需求 13：选中多个可填充图元时一次替换；图片从资源面板里选（也可以现场从本地加一张）。
    QList<ShapeItem*> shapes;
    for (CanvasItem* item : selectedItems()) {
        if (auto* shape = dynamic_cast<ShapeItem*>(item);
            shape && !shape->isCustomShape() && ShapeMeta::isFilled(shape->shapeType())) {
            shapes.append(shape);
        }
    }
    if (shapes.isEmpty()) {
        return;
    }
    const QPixmap pixmap = ImagePickerDialog::pick(this, collectCanvasImages(m_scene), tr("Fill image"));
    if (pixmap.isNull() || !m_scene || !m_scene->undoStack()) {
        return;
    }
    QList<CanvasItem*> items;
    QList<QMap<ItemProperty, QVariant>> before;
    QList<QMap<ItemProperty, QVariant>> after;
    for (ShapeItem* shape : shapes) {
        QMap<ItemProperty, QVariant> beforeValues;
        beforeValues.insert(ItemProperty::FILL_IMAGE, shape->propertyValue(ItemProperty::FILL_IMAGE));
        beforeValues.insert(ItemProperty::FILL_IMAGE_MODE, static_cast<int>(shape->fillImageMode()));
        QMap<ItemProperty, QVariant> afterValues = beforeValues;
        afterValues.insert(ItemProperty::FILL_IMAGE, QVariant::fromValue(pixmap));
        // 替换图片同时重置为拉伸模式，与原整份状态命令语义一致。
        afterValues.insert(ItemProperty::FILL_IMAGE_MODE, static_cast<int>(ImageFill::Mode::Stretch));
        items.append(shape);
        before.append(beforeValues);
        after.append(afterValues);
    }
    for (int index = 0; index < items.size(); ++index) {
        for (auto it = after.at(index).constBegin(); it != after.at(index).constEnd(); ++it) {
            items.at(index)->setPropertyValue(it.key(), it.value());
        }
    }
    m_scene->undoStack()->push(new ItemPropertyCommand(m_scene, items, before, after, tr("Replace fill image")));
    refresh();
}

void PropertiesPanel::clearFillImage()
{
    if (selectedItems().isEmpty()) {
        return;
    }
    applyPropertyValues({{ItemProperty::FILL_IMAGE, QVariant()}}, tr("Clear fill image"));
}

void PropertiesPanel::applyCornerRadius(int radius)
{
    changeSelectedShapes(tr("Change rectangle corner radius"),
                         [radius](ShapeItem* shape) { shape->setCornerRadius(radius); });
}

void PropertiesPanel::applyStartAngle(int angle)
{
    changeSelectedShapes(tr("Change start angle"),
                         [angle](ShapeItem* shape) { shape->setArcAngles(angle, shape->sweepAngle()); });
    changeSelectedArcs(tr("Change start angle"), [angle](LineItemAbstract* arc) {
        LineItemAbstract::ParamState params = arc->paramState();
        params.first = angle;
        arc->applyParamState(params);
    });
}

void PropertiesPanel::applySweepAngle(int angle)
{
    changeSelectedShapes(tr("Change sector angle"),
                         [angle](ShapeItem* shape) { shape->setArcAngles(shape->startAngle(), angle); });
    changeSelectedArcs(tr("Change arc sweep angle"), [angle](LineItemAbstract* arc) {
        LineItemAbstract::ParamState params = arc->paramState();
        params.second = angle;
        arc->applyParamState(params);
    });
}

void PropertiesPanel::applySineCycles(int cycles)
{
    const int bounded = qBound(1, cycles, 12);
    changeSelectedSines(tr("Change sine cycles"), [bounded](LineItemAbstract* sine) {
        LineItemAbstract::ParamState params = sine->paramState();
        params.first = bounded;
        sine->applyParamState(params);
    });
}

void PropertiesPanel::applySineAmplitude(int pixels)
{
    const qreal amplitude = qBound<qreal>(1.0, pixels, 200.0);
    changeSelectedSines(tr("Change sine amplitude"), [amplitude](LineItemAbstract* sine) {
        LineItemAbstract::ParamState params = sine->paramState();
        params.second = amplitude;
        sine->applyParamState(params);
    });
}

void PropertiesPanel::applyTriangleBase(int base)
{
    // 底边即 A 边；改底边后高度由两个底角派生（contentResized 内完成）。
    changeSelectedShapes(tr("Change triangle base"), [base](ShapeItem* shape) {
        if (!shape || shape->shapeType() != ShapeType::Triangle || shape->isCustomShape()) {
            return;
        }
        const QSizeF current = shape->contentSize();
        shape->setContentSize(QSizeF(qMax<qreal>(8.0, base), current.height()));
    });
}

void PropertiesPanel::applyTriangleAngle1(int degrees)
{
    changeSelectedShapes(tr("Change triangle base angle"), [degrees](ShapeItem* shape) {
        if (!shape || shape->shapeType() != ShapeType::Triangle || shape->isCustomShape()) {
            return;
        }
        shape->setTriangleAngles(degrees, shape->bottomAngle2());
    });
}

void PropertiesPanel::applyTriangleAngle2(int degrees)
{
    changeSelectedShapes(tr("Change triangle base angle"), [degrees](ShapeItem* shape) {
        if (!shape || shape->shapeType() != ShapeType::Triangle || shape->isCustomShape()) {
            return;
        }
        shape->setTriangleAngles(shape->bottomAngle1(), degrees);
    });
}

void PropertiesPanel::applyArrowStart(int style)
{
    // 需求 9b/9d：直线与曲线共用一条箭头撤销记录，只改箭头样式。
    const QList<LineItemAbstract*> curves = selectedStrokes();
    QList<CanvasItem*> targets;
    QList<QMap<ItemProperty, QVariant>> before;
    QList<QMap<ItemProperty, QVariant>> after;
    for (LineItemAbstract* curve : curves) {
        if (!curve || !curve->properties().contains(ItemProperty::ARROW_START)) {
            continue;
        }
        QMap<ItemProperty, QVariant> beforeValues;
        beforeValues.insert(ItemProperty::ARROW_START, curve->propertyValue(ItemProperty::ARROW_START));
        beforeValues.insert(ItemProperty::ARROW_END, curve->propertyValue(ItemProperty::ARROW_END));
        QMap<ItemProperty, QVariant> afterValues = beforeValues;
        afterValues.insert(ItemProperty::ARROW_START, style);
        targets.append(curve);
        before.append(beforeValues);
        after.append(afterValues);
    }
    if (targets.isEmpty() || !m_scene || !m_scene->undoStack()) {
        return;
    }
    for (CanvasItem* target : targets) {
        target->setPropertyValue(ItemProperty::ARROW_START, style);
    }
    m_scene->undoStack()->push(new ItemPropertyCommand(m_scene, targets, before, after, tr("Change line start arrow")));
    refresh();
}

void PropertiesPanel::applyArrowEnd(int style)
{
    const QList<LineItemAbstract*> curves = selectedStrokes();
    QList<CanvasItem*> targets;
    QList<QMap<ItemProperty, QVariant>> before;
    QList<QMap<ItemProperty, QVariant>> after;
    for (LineItemAbstract* curve : curves) {
        if (!curve || !curve->properties().contains(ItemProperty::ARROW_END)) {
            continue;
        }
        QMap<ItemProperty, QVariant> beforeValues;
        beforeValues.insert(ItemProperty::ARROW_START, curve->propertyValue(ItemProperty::ARROW_START));
        beforeValues.insert(ItemProperty::ARROW_END, curve->propertyValue(ItemProperty::ARROW_END));
        QMap<ItemProperty, QVariant> afterValues = beforeValues;
        afterValues.insert(ItemProperty::ARROW_END, style);
        targets.append(curve);
        before.append(beforeValues);
        after.append(afterValues);
    }
    if (targets.isEmpty() || !m_scene || !m_scene->undoStack()) {
        return;
    }
    for (CanvasItem* target : targets) {
        target->setPropertyValue(ItemProperty::ARROW_END, style);
    }
    m_scene->undoStack()->push(new ItemPropertyCommand(m_scene, targets, before, after, tr("Change line end arrow")));
    refresh();
}

void PropertiesPanel::adjustLayer(int delta)
{
    if (m_scene) {
        const int count = m_scene->moveSelectedItemsToAdjacentLayer(delta);
        statusMessage(count > 0 ? tr("Moved %1 item(s) to another layer.").arg(count)
                                : tr("No layer to move to in that direction."));
        refresh();
    }
}

void PropertiesPanel::adjustHeight(int delta)
{
    if (m_scene) {
        const int count = m_scene->adjustSelectedHeights(delta);
        statusMessage(count > 0 ? tr("Adjusted the height of %1 item(s).").arg(count)
                                : tr("The height did not change."));
        refresh();
    }
}
