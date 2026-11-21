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


void PropertiesPanel::ChangeSelectedShapes(const QString& text, const std::function<void(ShapeItem*)>& mutate)
{
    const QList<ShapeItem*> shapes = SelectedShapes();
    if (shapes.isEmpty() || !m_scene || !m_scene->UndoStack()) {
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
        for (ItemProperty property : shape->Properties().Values()) {
            beforeValues.insert(property, shape->PropertyValue(property));
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
        for (ItemProperty property : shape->Properties().Values()) {
            afterValues.insert(property, shape->PropertyValue(property));
        }
        after.append(afterValues);
    }

    m_scene->UndoStack()->push(new ItemPropertyCommand(m_scene, items, before, after, text));
    Refresh();
}

void PropertiesPanel::ApplyPropertyValues(const QMap<ItemProperty, QVariant>& values, const QString& text,
                                          const QList<CanvasItem*>& targets)
{
    if (!m_scene || !m_scene->UndoStack()) {
        return;
    }
    const QList<CanvasItem*> items = targets.isEmpty() ? SelectedItems() : targets;

    QList<CanvasItem*> applyItems;
    QList<QMap<ItemProperty, QVariant>> before;
    QList<QMap<ItemProperty, QVariant>> after;
    m_refreshing = true;
    for (CanvasItem* item : items) {
        const ItemPropertySet properties = item->Properties();
        QMap<ItemProperty, QVariant> beforeValues;
        QMap<ItemProperty, QVariant> afterValues;
        for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
            if (!properties.Contains(it.key())) {
                continue;
            }
            beforeValues.insert(it.key(), item->PropertyValue(it.key()));
            afterValues.insert(it.key(), it.value());
        }
        if (afterValues.isEmpty()) {
            continue;
        }
        // 先写值，命令随后 push 时的 redo 幂等执行；撤销 / 重做都按快照回放。
        for (auto it = afterValues.constBegin(); it != afterValues.constEnd(); ++it) {
            item->SetPropertyValue(it.key(), it.value());
        }
        applyItems.append(item);
        before.append(beforeValues);
        after.append(afterValues);
    }
    m_refreshing = false;

    if (applyItems.isEmpty()) {
        return;
    }
    m_scene->UndoStack()->push(new ItemPropertyCommand(m_scene, applyItems, before, after, text));
    Refresh();
}

void PropertiesPanel::ApplyStrokeColor(const QColor& color)
{
    const QList<CanvasItem*> items = SelectedStrokeTargets();
    if (!color.isValid() || items.isEmpty()) {
        return;
    }
    emit strokeColorPicked(color);
    ApplyPropertyValues({{ItemProperty::STROKE_COLOR, color}}, tr("Change border color"), items);
}

void PropertiesPanel::ApplyFillColor(const QColor& color)
{
    const QList<CanvasItem*> items = SelectedItems();
    if (!color.isValid() || items.isEmpty() || !m_scene || !m_scene->UndoStack()) {
        return;
    }

    QList<CanvasItem*> targets;
    QList<QMap<ItemProperty, QVariant>> before;
    QList<QMap<ItemProperty, QVariant>> after;
    m_refreshing = true;
    for (CanvasItem* item : items) {
        const ItemPropertySet properties = item->Properties();
        if (!properties.Contains(ItemProperty::FILL_COLOR)) {
            continue;
        }
        QMap<ItemProperty, QVariant> beforeValues;
        beforeValues.insert(ItemProperty::FILL_COLOR, item->PropertyValue(ItemProperty::FILL_COLOR));
        if (properties.Contains(ItemProperty::FILL_STYLE)) {
            beforeValues.insert(ItemProperty::FILL_STYLE, item->PropertyValue(ItemProperty::FILL_STYLE));
        }
        QMap<ItemProperty, QVariant> afterValues = beforeValues;
        afterValues.insert(ItemProperty::FILL_COLOR, color);
        if (properties.Contains(ItemProperty::FILL_STYLE) &&
            item->PropertyValue(ItemProperty::FILL_STYLE).toInt() == static_cast<int>(FillStyle::NONE)) {
            afterValues.insert(ItemProperty::FILL_STYLE, static_cast<int>(FillStyle::SOLID));
        }
        targets.append(item);
        before.append(beforeValues);
        after.append(afterValues);
    }
    // 先写值再入栈：命令的 redo 会被 push 重复执行一次，幂等即可，这里保持与旧实现一致。
    for (int index = 0; index < targets.size(); ++index) {
        for (auto it = after.at(index).constBegin(); it != after.at(index).constEnd(); ++it) {
            targets.at(index)->SetPropertyValue(it.key(), it.value());
        }
    }
    m_refreshing = false;
    if (targets.isEmpty()) {
        return;
    }
    m_scene->UndoStack()->push(new ItemPropertyCommand(m_scene, targets, before, after, tr("Change shape fill color")));
    Refresh();
}

void PropertiesPanel::ApplyFillStyle(FillStyle style)
{
    if (SelectedItems().isEmpty() || !m_scene || !m_scene->UndoStack()) {
        return;
    }
    ApplyPropertyValues({{ItemProperty::FILL_STYLE, static_cast<int>(style)}}, tr("Change shape fill style"));
}

void PropertiesPanel::ApplyBorderStyle(Qt::PenStyle style)
{
    const QList<CanvasItem*> items = SelectedStrokeTargets();
    if (items.isEmpty()) {
        return;
    }
    ApplyPropertyValues({{ItemProperty::STROKE_STYLE, static_cast<int>(style)}}, tr("Change shape border style"),
                        items);
}

void PropertiesPanel::ApplyBorderWidth(qreal width)
{
    const QList<CanvasItem*> items = SelectedStrokeTargets();
    if (items.isEmpty()) {
        return;
    }
    emit borderWidthPicked(width);
    ApplyPropertyValues({{ItemProperty::STROKE_WIDTH, width}}, tr("Change border width"), items);
}

void PropertiesPanel::ApplyOpacity(int percent)
{
    if (SelectedItems().isEmpty()) {
        return;
    }
    ApplyPropertyValues({{ItemProperty::OPACITY, percent / 100.0}}, tr("Change opacity"));
}

void PropertiesPanel::ChangeSelectedTexts(const QString& text, const std::function<void(TextItemState&)>& mutate)
{
    const QList<CanvasItem*> texts = SelectedTexts();
    if (texts.isEmpty() || !m_scene || !m_scene->UndoStack()) {
        return;
    }

    QList<TextItemState> beforeStates;
    QList<TextItemState> afterStates;
    bool changed = false;
    for (CanvasItem* item : texts) {
        const TextItemState before = item->TextState();
        TextItemState after = before;
        mutate(after);
        beforeStates.append(before);
        afterStates.append(after);
        changed = changed || after != before;
    }
    if (!changed) {
        return;
    }

    QUndoStack* undo = m_scene->UndoStack();
    undo->beginMacro(text);
    for (int i = 0; i < texts.size(); ++i) {
        undo->push(new TextItemCommand(m_scene, texts.at(i), beforeStates.at(i), afterStates.at(i), text));
    }
    undo->endMacro();
    Refresh();
}

void PropertiesPanel::ApplyTextContent(const QString& text)
{
    ChangeSelectedTexts(tr("Change text content"), [&text](TextItemState& state) {
        if (state.text == text) {
            return;
        }
        state.text = text;
        state.baseSize = TextItem::Measure(state.text, state.font);
        state.size = state.baseSize;
    });
}

void PropertiesPanel::ApplyTextFont(const QFont& font)
{
    ChangeSelectedTexts(tr("Change text font"), [&font](TextItemState& state) {
        if (state.font == font) {
            return;
        }
        state.font = font;
        state.baseSize = TextItem::Measure(state.text, state.font);
        state.size = state.baseSize;
    });
}

void PropertiesPanel::ApplyTextColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    ChangeSelectedTexts(tr("Change text color"), [&color](TextItemState& state) { state.color = color; });
}

void PropertiesPanel::ChangeSelectedArcs(const QString& text, const std::function<void(LineItemAbstract*)>& mutate)
{
    QList<LineItemAbstract*> arcs;
    for (LineItemAbstract* stroke : SelectedStrokes()) {
        if (stroke->SupportsArcAngles()) {
            arcs.append(stroke);
        }
    }
    if (arcs.isEmpty() || !m_scene || !m_scene->UndoStack()) {
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

    m_scene->UndoStack()->push(
        new CurveParamCommand(m_scene, targets, paramsBefore, geometryBefore, paramsAfter, geometryAfter, text));
    Refresh();
}

void PropertiesPanel::ChangeSelectedSines(const QString& text, const std::function<void(LineItemAbstract*)>& mutate)
{
    QList<LineItemAbstract*> sines;
    for (LineItemAbstract* stroke : SelectedStrokes()) {
        if (stroke->SupportsSineWave()) {
            sines.append(stroke);
        }
    }
    if (sines.isEmpty() || !m_scene || !m_scene->UndoStack()) {
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

    m_scene->UndoStack()->push(
        new CurveParamCommand(m_scene, targets, paramsBefore, geometryBefore, paramsAfter, geometryAfter, text));
    Refresh();
}

void PropertiesPanel::ChooseFillImage()
{
    // 需求 13：选中多个可填充图元时一次替换；图片从资源面板里选（也可以现场从本地加一张）。
    QList<ShapeItem*> shapes;
    for (CanvasItem* item : SelectedItems()) {
        if (auto* shape = dynamic_cast<ShapeItem*>(item);
            shape && !shape->IsCustomShape() && ShapeMeta::IsFilled(shape->shapeType())) {
            shapes.append(shape);
        }
    }
    if (shapes.isEmpty()) {
        return;
    }
    const QPixmap pixmap = ImagePickerDialog::Pick(this, CollectCanvasImages(m_scene), tr("Fill image"));
    if (pixmap.isNull() || !m_scene || !m_scene->UndoStack()) {
        return;
    }
    QList<CanvasItem*> items;
    QList<QMap<ItemProperty, QVariant>> before;
    QList<QMap<ItemProperty, QVariant>> after;
    for (ShapeItem* shape : shapes) {
        QMap<ItemProperty, QVariant> beforeValues;
        beforeValues.insert(ItemProperty::FILL_IMAGE, shape->PropertyValue(ItemProperty::FILL_IMAGE));
        beforeValues.insert(ItemProperty::FILL_IMAGE_MODE, static_cast<int>(shape->FillImageMode()));
        QMap<ItemProperty, QVariant> afterValues = beforeValues;
        afterValues.insert(ItemProperty::FILL_IMAGE, QVariant::fromValue(pixmap));
        // 替换图片同时重置为拉伸模式，与原整份状态命令语义一致。
        afterValues.insert(ItemProperty::FILL_IMAGE_MODE, static_cast<int>(ImageFill::Mode::STRETCH));
        items.append(shape);
        before.append(beforeValues);
        after.append(afterValues);
    }
    for (int index = 0; index < items.size(); ++index) {
        for (auto it = after.at(index).constBegin(); it != after.at(index).constEnd(); ++it) {
            items.at(index)->SetPropertyValue(it.key(), it.value());
        }
    }
    m_scene->UndoStack()->push(new ItemPropertyCommand(m_scene, items, before, after, tr("Replace fill image")));
    Refresh();
}

void PropertiesPanel::ClearFillImage()
{
    if (SelectedItems().isEmpty()) {
        return;
    }
    ApplyPropertyValues({{ItemProperty::FILL_IMAGE, QVariant()}}, tr("Clear fill image"));
}

void PropertiesPanel::ApplyCornerRadius(int radius)
{
    ChangeSelectedShapes(tr("Change rectangle corner radius"),
                         [radius](ShapeItem* shape) { shape->SetCornerRadius(radius); });
}

void PropertiesPanel::ApplyStartAngle(int angle)
{
    ChangeSelectedShapes(tr("Change start angle"),
                         [angle](ShapeItem* shape) { shape->SetArcAngles(angle, shape->SweepAngle()); });
    ChangeSelectedArcs(tr("Change start angle"), [angle](LineItemAbstract* arc) {
        LineItemAbstract::ParamState params = arc->paramState();
        params.first = angle;
        arc->ApplyParamState(params);
    });
}

void PropertiesPanel::ApplySweepAngle(int angle)
{
    ChangeSelectedShapes(tr("Change sector angle"),
                         [angle](ShapeItem* shape) { shape->SetArcAngles(shape->StartAngle(), angle); });
    ChangeSelectedArcs(tr("Change arc sweep angle"), [angle](LineItemAbstract* arc) {
        LineItemAbstract::ParamState params = arc->paramState();
        params.second = angle;
        arc->ApplyParamState(params);
    });
}

void PropertiesPanel::ApplySineCycles(int cycles)
{
    const int bounded = qBound(1, cycles, 12);
    ChangeSelectedSines(tr("Change sine cycles"), [bounded](LineItemAbstract* sine) {
        LineItemAbstract::ParamState params = sine->paramState();
        params.first = bounded;
        sine->ApplyParamState(params);
    });
}

void PropertiesPanel::ApplySineAmplitude(int pixels)
{
    const qreal amplitude = qBound<qreal>(1.0, pixels, 200.0);
    ChangeSelectedSines(tr("Change sine amplitude"), [amplitude](LineItemAbstract* sine) {
        LineItemAbstract::ParamState params = sine->paramState();
        params.second = amplitude;
        sine->ApplyParamState(params);
    });
}

void PropertiesPanel::ApplyTriangleBase(int base)
{
    // 底边即 A 边；改底边后高度由两个底角派生（contentResized 内完成）。
    ChangeSelectedShapes(tr("Change triangle base"), [base](ShapeItem* shape) {
        if (!shape || shape->shapeType() != ShapeType::TRIANGLE || shape->IsCustomShape()) {
            return;
        }
        const QSizeF current = shape->ContentSize();
        shape->SetContentSize(QSizeF(qMax<qreal>(8.0, base), current.height()));
    });
}

void PropertiesPanel::ApplyTriangleAngle1(int degrees)
{
    ChangeSelectedShapes(tr("Change triangle base angle"), [degrees](ShapeItem* shape) {
        if (!shape || shape->shapeType() != ShapeType::TRIANGLE || shape->IsCustomShape()) {
            return;
        }
        shape->SetTriangleAngles(degrees, shape->BottomAngle2());
    });
}

void PropertiesPanel::ApplyTriangleAngle2(int degrees)
{
    ChangeSelectedShapes(tr("Change triangle base angle"), [degrees](ShapeItem* shape) {
        if (!shape || shape->shapeType() != ShapeType::TRIANGLE || shape->IsCustomShape()) {
            return;
        }
        shape->SetTriangleAngles(shape->BottomAngle1(), degrees);
    });
}

void PropertiesPanel::ApplyArrowStart(int style)
{
    // 需求 9b/9d：直线与曲线共用一条箭头撤销记录，只改箭头样式。
    const QList<LineItemAbstract*> curves = SelectedStrokes();
    QList<CanvasItem*> targets;
    QList<QMap<ItemProperty, QVariant>> before;
    QList<QMap<ItemProperty, QVariant>> after;
    for (LineItemAbstract* curve : curves) {
        if (!curve || !curve->Properties().Contains(ItemProperty::ARROW_START)) {
            continue;
        }
        QMap<ItemProperty, QVariant> beforeValues;
        beforeValues.insert(ItemProperty::ARROW_START, curve->PropertyValue(ItemProperty::ARROW_START));
        beforeValues.insert(ItemProperty::ARROW_END, curve->PropertyValue(ItemProperty::ARROW_END));
        QMap<ItemProperty, QVariant> afterValues = beforeValues;
        afterValues.insert(ItemProperty::ARROW_START, style);
        targets.append(curve);
        before.append(beforeValues);
        after.append(afterValues);
    }
    if (targets.isEmpty() || !m_scene || !m_scene->UndoStack()) {
        return;
    }
    for (CanvasItem* target : targets) {
        target->SetPropertyValue(ItemProperty::ARROW_START, style);
    }
    m_scene->UndoStack()->push(new ItemPropertyCommand(m_scene, targets, before, after, tr("Change line start arrow")));
    Refresh();
}

void PropertiesPanel::ApplyArrowEnd(int style)
{
    const QList<LineItemAbstract*> curves = SelectedStrokes();
    QList<CanvasItem*> targets;
    QList<QMap<ItemProperty, QVariant>> before;
    QList<QMap<ItemProperty, QVariant>> after;
    for (LineItemAbstract* curve : curves) {
        if (!curve || !curve->Properties().Contains(ItemProperty::ARROW_END)) {
            continue;
        }
        QMap<ItemProperty, QVariant> beforeValues;
        beforeValues.insert(ItemProperty::ARROW_START, curve->PropertyValue(ItemProperty::ARROW_START));
        beforeValues.insert(ItemProperty::ARROW_END, curve->PropertyValue(ItemProperty::ARROW_END));
        QMap<ItemProperty, QVariant> afterValues = beforeValues;
        afterValues.insert(ItemProperty::ARROW_END, style);
        targets.append(curve);
        before.append(beforeValues);
        after.append(afterValues);
    }
    if (targets.isEmpty() || !m_scene || !m_scene->UndoStack()) {
        return;
    }
    for (CanvasItem* target : targets) {
        target->SetPropertyValue(ItemProperty::ARROW_END, style);
    }
    m_scene->UndoStack()->push(new ItemPropertyCommand(m_scene, targets, before, after, tr("Change line end arrow")));
    Refresh();
}

void PropertiesPanel::AdjustLayer(int delta)
{
    if (m_scene) {
        const int count = m_scene->MoveSelectedItemsToAdjacentLayer(delta);
        statusMessage(count > 0 ? tr("Moved %1 Item(s) to another layer.").arg(count)
                                : tr("No layer to move to in that direction."));
        Refresh();
    }
}

void PropertiesPanel::AdjustHeight(int delta)
{
    if (m_scene) {
        const int count = m_scene->AdjustSelectedHeights(delta);
        statusMessage(count > 0 ? tr("Adjusted the height of %1 Item(s).").arg(count)
                                : tr("The height did not change."));
        Refresh();
    }
}
