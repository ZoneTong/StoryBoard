// 属性面板的全部应用方法（颜色、文字、线型与几何参数）。
#include "ArcItem.h"
#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "CurveParamCommand.h"
#include "FillAttributes.h"
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
#include "StrokeAttributes.h"
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

void PropertiesPanel::ApplyFillMutation(const QString& text, const std::function<void(FillAttributes*)>& mutate)
{
    if (!m_scene || !m_scene->UndoStack()) {
        return;
    }
    QList<CanvasItem*> items;
    QList<FillAttributes*> fills;
    for (CanvasItem* item : SelectedItems()) {
        if (auto* fill = dynamic_cast<FillAttributes*>(item)) {
            items.append(item);
            fills.append(fill);
        }
    }
    if (fills.isEmpty()) {
        return;
    }

    // 填充属性的撤销快照仍然走属性表，回放时 SetPropertyValue 会分派回这些基类虚函数。
    QList<QMap<ItemProperty, QVariant>> before;
    QList<QMap<ItemProperty, QVariant>> after;
    m_refreshing = true;
    for (CanvasItem* item : items) {
        const ItemPropertySet properties = item->Properties();
        QMap<ItemProperty, QVariant> values;
        for (ItemProperty property : {ItemProperty::FILL_STYLE, ItemProperty::FILL_COLOR, ItemProperty::FILL_IMAGE,
                                      ItemProperty::FILL_IMAGE_MODE}) {
            if (properties.Contains(property)) {
                values.insert(property, item->PropertyValue(property));
            }
        }
        before.append(values);
    }
    for (FillAttributes* fill : fills) {
        mutate(fill);
    }
    for (CanvasItem* item : items) {
        const ItemPropertySet properties = item->Properties();
        QMap<ItemProperty, QVariant> values;
        for (ItemProperty property : {ItemProperty::FILL_STYLE, ItemProperty::FILL_COLOR, ItemProperty::FILL_IMAGE,
                                      ItemProperty::FILL_IMAGE_MODE}) {
            if (properties.Contains(property)) {
                values.insert(property, item->PropertyValue(property));
            }
        }
        after.append(values);
    }
    m_refreshing = false;

    if (before == after) {
        return;
    }
    m_scene->UndoStack()->push(new ItemPropertyCommand(m_scene, items, before, after, text));
    Refresh();
}

void PropertiesPanel::ApplyStrokeMutation(const QString& text, const std::function<void(StrokeAttributes*)>& mutate)
{
    if (!m_scene || !m_scene->UndoStack()) {
        return;
    }
    QList<CanvasItem*> items;
    QList<StrokeAttributes*> strokes;
    for (CanvasItem* item : SelectedItems()) {
        if (auto* stroke = dynamic_cast<StrokeAttributes*>(item)) {
            items.append(item);
            strokes.append(stroke);
        }
    }
    if (strokes.isEmpty()) {
        return;
    }

    QList<QMap<ItemProperty, QVariant>> before;
    QList<QMap<ItemProperty, QVariant>> after;
    m_refreshing = true;
    for (CanvasItem* item : items) {
        const ItemPropertySet properties = item->Properties();
        QMap<ItemProperty, QVariant> values;
        for (ItemProperty property :
             {ItemProperty::STROKE_STYLE, ItemProperty::STROKE_COLOR, ItemProperty::STROKE_WIDTH}) {
            if (properties.Contains(property)) {
                values.insert(property, item->PropertyValue(property));
            }
        }
        before.append(values);
    }
    for (StrokeAttributes* stroke : strokes) {
        mutate(stroke);
    }
    for (CanvasItem* item : items) {
        const ItemPropertySet properties = item->Properties();
        QMap<ItemProperty, QVariant> values;
        for (ItemProperty property :
             {ItemProperty::STROKE_STYLE, ItemProperty::STROKE_COLOR, ItemProperty::STROKE_WIDTH}) {
            if (properties.Contains(property)) {
                values.insert(property, item->PropertyValue(property));
            }
        }
        after.append(values);
    }
    m_refreshing = false;

    if (before == after) {
        return;
    }
    m_scene->UndoStack()->push(new ItemPropertyCommand(m_scene, items, before, after, text));
    Refresh();
}

void PropertiesPanel::ApplyStrokeColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    emit strokeColorPicked(color);
    ApplyStrokeMutation(tr("Change border color"),
                        [color](StrokeAttributes* stroke) { stroke->SetStrokeColor(color); });
}

void PropertiesPanel::ApplyFillColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    ApplyFillMutation(tr("Change shape fill color"), [color](FillAttributes* fill) {
        // 原逻辑：无填充时改颜色顺带切回纯色，否则只换颜色不动样式。
        const FillStyle style = fill->fillStyle() == FillStyle::NONE ? FillStyle::SOLID : fill->fillStyle();
        fill->SetFill(color, style);
    });
}

void PropertiesPanel::ApplyFillStyle(FillStyle style)
{
    ApplyFillMutation(tr("Change shape fill style"), [style](FillAttributes* fill) { fill->SetFillStyle(style); });
}

void PropertiesPanel::ApplyBorderStyle(Qt::PenStyle style)
{
    ApplyStrokeMutation(tr("Change shape border style"),
                        [style](StrokeAttributes* stroke) { stroke->SetStrokeStyle(style); });
}

void PropertiesPanel::ApplyBorderWidth(qreal width)
{
    emit borderWidthPicked(width);
    ApplyStrokeMutation(tr("Change border width"),
                        [width](StrokeAttributes* stroke) { stroke->SetStrokeWidth(width); });
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
    bool hasTarget = false;
    for (CanvasItem* item : SelectedItems()) {
        auto* fill = dynamic_cast<FillAttributes*>(item);
        auto* shape = dynamic_cast<ShapeItem*>(item);
        if (fill && shape && !shape->IsCustomShape() && ShapeMeta::IsFilled(shape->shapeType())) {
            hasTarget = true;
            break;
        }
    }
    if (!hasTarget || !m_scene || !m_scene->UndoStack()) {
        return;
    }
    const QPixmap pixmap = ImagePickerDialog::Pick(this, CollectCanvasImages(m_scene), tr("Fill image"));
    if (pixmap.isNull()) {
        return;
    }
    ApplyFillMutation(tr("Replace fill image"), [pixmap](FillAttributes* fill) {
        if (auto* shape = dynamic_cast<ShapeItem*>(fill); shape && shape->IsCustomShape()) {
            return;
        }
        // 替换图片同时重置为拉伸模式，与原整份状态命令语义一致。
        fill->SetFillImage(pixmap, ImageFill::Mode::STRETCH);
    });
}

void PropertiesPanel::ClearFillImage()
{
    ApplyFillMutation(tr("Clear fill image"), [](FillAttributes* fill) { fill->ClearFillImage(); });
}

void PropertiesPanel::ApplyFillImageMode(int mode)
{
    // 与属性窗同一套摆放方式；无图时组合框已禁用，这里再兜一次底。
    ApplyFillMutation(tr("Change fill image mode"), [mode](FillAttributes* fill) {
        if (!fill->HasFillImage()) {
            return;
        }
        fill->SetFillImage(fill->FillImage(), static_cast<ImageFill::Mode>(mode));
    });
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

void PropertiesPanel::ApplyItemName(const QString& name)
{
    const QList<CanvasItem*> items = SelectedItems();
    if (items.size() != 1 || !m_scene) {
        return;
    }
    CanvasItem* item = items.first();
    const QString typed = name.trimmed();
    if (!typed.isEmpty() && !m_scene->IsItemNameAvailable(typed, item)) {
        emit statusMessage(tr("Another item already uses this name."));
        Refresh();
        return;
    }
    // 清空名称回到自动命名；未自定义时保持原显示名，不因打开面板就写成一个自定义名。
    QString value = typed;
    if (typed.isEmpty()) {
        value = m_scene->UniqueItemName(item->TypeDisplayName(), item);
    } else if (!item->HasCustomName() && typed == item->DisplayName()) {
        value.clear();
    }
    ApplyPropertyValues({{ItemProperty::NAME, value}}, tr("Rename item"), {item});
}

void PropertiesPanel::ApplyItemLayer(int layerId)
{
    const QList<CanvasItem*> items = SelectedItems();
    if (items.size() != 1 || !m_scene) {
        return;
    }
    ApplyItemStack(layerId, items.first()->ZOrder(), tr("Move item to another layer"));
}

void PropertiesPanel::ApplyItemHeight(int height)
{
    const QList<CanvasItem*> items = SelectedItems();
    if (items.size() != 1 || !m_scene) {
        return;
    }
    ApplyItemStack(items.first()->LayerId(), height, tr("Change item height"));
}

void PropertiesPanel::ApplyItemStack(int layerId, int height, const QString& text)
{
    const QList<CanvasItem*> items = SelectedItems();
    if (items.size() != 1 || !m_scene || !m_scene->UndoStack()) {
        return;
    }
    CanvasItem* item = items.first();
    if (item->LayerId() == layerId && item->ZOrder() == height) {
        return;
    }

    // 与属性窗一致：目标位置已有图元时，两条记录互换，不留同层同高度。
    CanvasItem* colliding = nullptr;
    for (CanvasItem* peer : m_scene->LayerItems(layerId)) {
        if (peer != item && peer->ZOrder() == height) {
            colliding = peer;
            break;
        }
    }

    ItemStackList before{{item, item->LayerId(), item->ZOrder()}};
    ItemStackList after{{item, layerId, height}};
    if (colliding) {
        before.append({colliding, colliding->LayerId(), colliding->ZOrder()});
        after.append({colliding, item->LayerId(), item->ZOrder()});
    }
    m_scene->UndoStack()->push(new ItemStackCommand(m_scene, before, after, text));
    Refresh();
}

void PropertiesPanel::ApplyItemAxes(qreal majorAxis, qreal minorAxis)
{
    // 长短半轴与位置尺寸共用属性表路径：只有上报了 MAJOR_AXIS 的图元
    // （椭圆 / 扇形 / 弧线）会真正写入，其余选中项自动跳过。
    ApplyPropertyValues({{ItemProperty::MAJOR_AXIS, majorAxis}, {ItemProperty::MINOR_AXIS, minorAxis}},
                        tr("Edit semi-axes"));
}

void PropertiesPanel::ApplyItemGeometry(const QPointF& position, const QSizeF& size)
{
    const QList<CanvasItem*> items = SelectedItems();
    if (items.isEmpty() || !m_scene || !m_scene->UndoStack()) {
        return;
    }

    // 单选按属性窗语义写绝对位置与尺寸；多选时面板回显首个图元的值，
    // 这里把面板变化折算成增量套到每个图元，保持它们之间的相对位置和大小。
    if (items.size() == 1) {
        ApplyPropertyValues({{ItemProperty::POSITION, position}, {ItemProperty::SIZE, size}},
                            tr("Change item geometry"));
        return;
    }

    CanvasItem* anchor = items.first();
    const QPointF positionDelta = position - anchor->pos();
    const QSizeF sizeDelta = size - anchor->ContentSize();
    if (qFuzzyIsNull(positionDelta.x()) && qFuzzyIsNull(positionDelta.y()) && qFuzzyIsNull(sizeDelta.width()) &&
        qFuzzyIsNull(sizeDelta.height())) {
        return;
    }

    const QSizeF minimumSize = CanvasItem::MinimumSize();
    QList<CanvasItem*> applyItems;
    QList<QMap<ItemProperty, QVariant>> before;
    QList<QMap<ItemProperty, QVariant>> after;
    m_refreshing = true;
    for (CanvasItem* item : items) {
        const ItemPropertySet properties = item->Properties();
        QMap<ItemProperty, QVariant> beforeValues;
        QMap<ItemProperty, QVariant> afterValues;

        if (properties.Contains(ItemProperty::POSITION)) {
            const QPointF currentPosition = item->pos();
            const QPointF targetPosition = currentPosition + positionDelta;
            beforeValues.insert(ItemProperty::POSITION, currentPosition);
            item->SetPropertyValue(ItemProperty::POSITION, targetPosition);
            afterValues.insert(ItemProperty::POSITION, item->PropertyValue(ItemProperty::POSITION));
        }
        if (properties.Contains(ItemProperty::SIZE)) {
            const QSizeF currentSize = item->ContentSize();
            const QSizeF targetSize(qMax(minimumSize.width(), currentSize.width() + sizeDelta.width()),
                                    qMax(minimumSize.height(), currentSize.height() + sizeDelta.height()));
            beforeValues.insert(ItemProperty::SIZE, currentSize);
            item->SetPropertyValue(ItemProperty::SIZE, targetSize);
            afterValues.insert(ItemProperty::SIZE, item->PropertyValue(ItemProperty::SIZE));
        }

        if (afterValues.isEmpty()) {
            continue;
        }
        applyItems.append(item);
        before.append(beforeValues);
        after.append(afterValues);
    }
    m_refreshing = false;

    if (applyItems.isEmpty()) {
        return;
    }
    m_scene->UndoStack()->push(new ItemPropertyCommand(m_scene, applyItems, before, after, tr("Change item geometry")));
    Refresh();
}

void PropertiesPanel::ApplyItemRotation(qreal angle)
{
    if (SelectedItems().isEmpty()) {
        return;
    }
    ApplyPropertyValues({{ItemProperty::ROTATION, angle}}, tr("Change rotation"));
}

void PropertiesPanel::ApplyParallelogramAngle(qreal angle)
{
    if (SelectedItems().isEmpty()) {
        return;
    }
    ApplyPropertyValues({{ItemProperty::PARALLELOGRAM_ANGLE, angle}}, tr("Change parallelogram angle"));
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
