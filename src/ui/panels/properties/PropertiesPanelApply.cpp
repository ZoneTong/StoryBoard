// 属性面板的全部应用方法（颜色、文字、线型与几何参数）。
#include "items/CanvasItem.h"
#include "items/geometry/ShapeMeta.h"
#include "items/lines/ArcItem.h"
#include "items/lines/LineAbstractItem.h"
#include "items/lines/SineItem.h"
#include "items/polygon/PolygonItem.h"
#include "items/properties/FillAttributes.h"
#include "items/properties/StrokeAttributes.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "items/text/TextItem.h"
#include "scene/CanvasScene.h"
#include "scene/Commands.h"
#include "ui/dialogs/ImagePickerDialog.h"
#include "ui/panels/properties/PropertiesPanel.h"
#include "ui/panels/properties/PropertiesPanelArrangeCard.h"
#include "ui/panels/properties/PropertiesPanelColorsCard.h"
#include "ui/panels/properties/PropertiesPanelContentCard.h"
#include "ui/panels/properties/PropertiesPanelFillBorderCard.h"
#include "ui/panels/properties/PropertiesPanelSelectionCard.h"
#include "ui/panels/properties/PropertiesPanelValueCard.h"
#include "ui/panels/properties/PropertyCommitService.h"

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


void PropertiesPanel::ChangeSelectedShapes(const QString& text, const std::function<void(ShapeAbstractItem*)>& mutate)
{
    const QList<ShapeAbstractItem*> shapes = SelectedShapes();
    if (shapes.isEmpty() || !m_scene || !m_scene->UndoStack()) {
        return;
    }

    // 按各图形上报的属性全集做快照：角度 / 底边等编辑会连带派生尺寸，
    // 全量快照保证撤销 / 重做与原整份状态命令等价。
    QList<CanvasItem*> items;
    QList<QMap<PropertyKey, QVariant>> before;
    QList<QMap<PropertyKey, QVariant>> after;
    before.reserve(shapes.size());
    after.reserve(shapes.size());
    for (ShapeAbstractItem* shape : shapes) {
        QMap<PropertyKey, QVariant> beforeValues;
        for (PropertyKey property : shape->Properties().Keys()) {
            beforeValues.insert(property, shape->PropertyValueByKey(property));
        }
        items.append(shape);
        before.append(beforeValues);
    }
    m_refreshing = true;
    for (ShapeAbstractItem* shape : shapes) {
        mutate(shape);
    }
    m_refreshing = false;
    for (ShapeAbstractItem* shape : shapes) {
        QMap<PropertyKey, QVariant> afterValues;
        for (PropertyKey property : shape->Properties().Keys()) {
            afterValues.insert(property, shape->PropertyValueByKey(property));
        }
        after.append(afterValues);
    }

    PropertyCommitService::Push(m_scene, items, before, after, text);
    Refresh();
}

void PropertiesPanel::ApplyPropertyValues(const QMap<PropertyKey, QVariant>& values, const QString& text,
                                          const QList<CanvasItem*>& targets)
{
    if (!m_scene || !m_scene->UndoStack()) {
        return;
    }
    const QList<CanvasItem*> items = targets.isEmpty() ? SelectedItems() : targets;

    QList<CanvasItem*> applyItems;
    QList<QMap<PropertyKey, QVariant>> before;
    QList<QMap<PropertyKey, QVariant>> after;
    m_refreshing = true;
    for (CanvasItem* item : items) {
        const ItemPropertySet properties = item->Properties();
        QMap<PropertyKey, QVariant> beforeValues;
        QMap<PropertyKey, QVariant> afterValues;
        for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
            if (!properties.Contains(it.key())) {
                continue;
            }
            beforeValues.insert(it.key(), item->PropertyValueByKey(it.key()));
            afterValues.insert(it.key(), it.value());
        }
        if (afterValues.isEmpty()) {
            continue;
        }
        const QPointF posBefore = item->pos();
        const QSizeF sizeBefore = item->ContentSize();
        // 先写值，命令随后 push 时的 redo 幂等执行；撤销 / 重做都按快照回放。
        for (auto it = afterValues.constBegin(); it != afterValues.constEnd(); ++it) {
            item->SetPropertyValueByKey(it.key(), it.value());
        }
        // 参数 / 描线宽度这类写入会重建几何并挪动位置或内容框：把真实变化一并入快照，
        // 否则撤销只回滚属性、几何留在改动后的位置。位置与尺寸排在属性之后回放。
        const QPointF posAfter = item->pos();
        const QSizeF sizeAfter = item->ContentSize();
        if (posAfter != posBefore) {
            beforeValues.insert(ItemProperty::POSITION, posBefore);
            afterValues.insert(ItemProperty::POSITION, posAfter);
        }
        if (sizeAfter != sizeBefore) {
            beforeValues.insert(ItemProperty::SIZE, sizeBefore);
            afterValues.insert(ItemProperty::SIZE, sizeAfter);
        }
        applyItems.append(item);
        before.append(beforeValues);
        after.append(afterValues);
    }
    m_refreshing = false;

    if (applyItems.isEmpty()) {
        return;
    }
    PropertyCommitService::Push(m_scene, applyItems, before, after, text);
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
    QList<QMap<PropertyKey, QVariant>> before;
    QList<QMap<PropertyKey, QVariant>> after;
    m_refreshing = true;
    for (CanvasItem* item : items) {
        const ItemPropertySet properties = item->Properties();
        QMap<PropertyKey, QVariant> values;
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
        QMap<PropertyKey, QVariant> values;
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
    PropertyCommitService::Push(m_scene, items, before, after, text);
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

    QList<QMap<PropertyKey, QVariant>> before;
    QList<QMap<PropertyKey, QVariant>> after;
    m_refreshing = true;
    for (CanvasItem* item : items) {
        const ItemPropertySet properties = item->Properties();
        QMap<PropertyKey, QVariant> values;
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
        QMap<PropertyKey, QVariant> values;
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
    PropertyCommitService::Push(m_scene, items, before, after, text);
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
    const QList<TextItem*> texts = SelectedTexts();
    if (texts.isEmpty() || !m_scene || !m_scene->UndoStack()) {
        return;
    }

    QList<TextItemState> beforeStates;
    QList<TextItemState> afterStates;
    bool changed = false;
    for (TextItem* item : texts) {
        const TextItemState before = item->State();
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


void PropertiesPanel::ChooseFillImage()
{
    // 需求 13：选中多个可填充图元时一次替换；图片从资源面板里选（也可以现场从本地加一张）。
    bool hasTarget = false;
    for (CanvasItem* item : SelectedItems()) {
        auto* fill = dynamic_cast<FillAttributes*>(item);
        auto* shape = dynamic_cast<ShapeAbstractItem*>(item);
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
        if (auto* shape = dynamic_cast<ShapeAbstractItem*>(fill); shape && shape->IsCustomShape()) {
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
                         [radius](ShapeAbstractItem* shape) { shape->SetCornerRadius(radius); });
}

void PropertiesPanel::ApplyStartAngle(int angle)
{
    ChangeSelectedShapes(tr("Change start angle"), [angle](ShapeAbstractItem* shape) {
        shape->SetPropertyValue(ItemProperty::START_ANGLE, angle);
    });
    const QList<LineAbstractItem*> strokes = SelectedStrokes();
    QList<CanvasItem*> targets;
    targets.reserve(strokes.size());
    for (LineAbstractItem* stroke : strokes) {
        targets.append(stroke);
    }
    ApplyPropertyValues(QMap<PropertyKey, QVariant>{{ItemProperty::START_ANGLE, angle}}, tr("Change start angle"),
                        targets);
}

void PropertiesPanel::ApplySweepAngle(int angle)
{
    ChangeSelectedShapes(tr("Change sector angle"), [angle](ShapeAbstractItem* shape) {
        shape->SetPropertyValue(ItemProperty::SWEEP_ANGLE, angle);
    });
    const QList<LineAbstractItem*> strokes = SelectedStrokes();
    QList<CanvasItem*> targets;
    targets.reserve(strokes.size());
    for (LineAbstractItem* stroke : strokes) {
        targets.append(stroke);
    }
    ApplyPropertyValues(QMap<PropertyKey, QVariant>{{ItemProperty::SWEEP_ANGLE, angle}}, tr("Change arc sweep angle"),
                        targets);
}

void PropertiesPanel::ApplySineCycles(int cycles)
{
    const int bounded = qBound(1, cycles, 12);
    const QList<LineAbstractItem*> strokes = SelectedStrokes();
    QList<CanvasItem*> targets;
    targets.reserve(strokes.size());
    for (LineAbstractItem* stroke : strokes) {
        targets.append(stroke);
    }
    ApplyPropertyValues(QMap<PropertyKey, QVariant>{{ItemProperty::SINE_CYCLES, bounded}}, tr("Change sine cycles"),
                        targets);
}

void PropertiesPanel::ApplySineAmplitude(int pixels)
{
    const qreal amplitude = qBound<qreal>(1.0, pixels, 200.0);
    const QList<LineAbstractItem*> strokes = SelectedStrokes();
    QList<CanvasItem*> targets;
    targets.reserve(strokes.size());
    for (LineAbstractItem* stroke : strokes) {
        targets.append(stroke);
    }
    ApplyPropertyValues(QMap<PropertyKey, QVariant>{{ItemProperty::SINE_AMPLITUDE, amplitude}},
                        tr("Change sine amplitude"), targets);
}

void PropertiesPanel::ApplyTriangleAngle1(int degrees)
{
    ChangeSelectedShapes(tr("Change triangle base angle"), [degrees](ShapeAbstractItem* shape) {
        if (!shape || shape->shapeType() != ShapeType::TRIANGLE || shape->IsCustomShape()) {
            return;
        }
        shape->SetTriangleAngles(degrees, shape->BottomAngle2());
    });
}

void PropertiesPanel::ApplyTriangleAngle2(int degrees)
{
    ChangeSelectedShapes(tr("Change triangle base angle"), [degrees](ShapeAbstractItem* shape) {
        if (!shape || shape->shapeType() != ShapeType::TRIANGLE || shape->IsCustomShape()) {
            return;
        }
        shape->SetTriangleAngles(shape->BottomAngle1(), degrees);
    });
}

void PropertiesPanel::ApplyArrowStart(int style)
{
    // 需求 9b/9d：直线与曲线共用一条箭头撤销记录，只改箭头样式。
    const QList<LineAbstractItem*> curves = SelectedStrokes();
    QList<CanvasItem*> targets;
    QList<QMap<PropertyKey, QVariant>> before;
    QList<QMap<PropertyKey, QVariant>> after;
    for (LineAbstractItem* curve : curves) {
        if (!curve || !curve->Properties().Contains(ItemProperty::ARROW_START)) {
            continue;
        }
        QMap<PropertyKey, QVariant> beforeValues;
        beforeValues.insert(ItemProperty::ARROW_START, curve->PropertyValue(ItemProperty::ARROW_START));
        beforeValues.insert(ItemProperty::ARROW_END, curve->PropertyValue(ItemProperty::ARROW_END));
        QMap<PropertyKey, QVariant> afterValues = beforeValues;
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
    PropertyCommitService::Push(m_scene, targets, before, after, tr("Change line start arrow"));
    Refresh();
}

void PropertiesPanel::ApplyArrowEnd(int style)
{
    const QList<LineAbstractItem*> curves = SelectedStrokes();
    QList<CanvasItem*> targets;
    QList<QMap<PropertyKey, QVariant>> before;
    QList<QMap<PropertyKey, QVariant>> after;
    for (LineAbstractItem* curve : curves) {
        if (!curve || !curve->Properties().Contains(ItemProperty::ARROW_END)) {
            continue;
        }
        QMap<PropertyKey, QVariant> beforeValues;
        beforeValues.insert(ItemProperty::ARROW_START, curve->PropertyValue(ItemProperty::ARROW_START));
        beforeValues.insert(ItemProperty::ARROW_END, curve->PropertyValue(ItemProperty::ARROW_END));
        QMap<PropertyKey, QVariant> afterValues = beforeValues;
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
    PropertyCommitService::Push(m_scene, targets, before, after, tr("Change line end arrow"));
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
    // D6：组内成员不许单独换层，只有整组一起换层才放行。
    if (!m_scene->CanMoveItemToLayer(item, layerId)) {
        m_scene->NotifyGroupLayerBlocked();
        Refresh();
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
    QList<QMap<PropertyKey, QVariant>> before;
    QList<QMap<PropertyKey, QVariant>> after;
    m_refreshing = true;
    for (CanvasItem* item : items) {
        const ItemPropertySet properties = item->Properties();
        QMap<PropertyKey, QVariant> beforeValues;
        QMap<PropertyKey, QVariant> afterValues;

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
    PropertyCommitService::Push(m_scene, applyItems, before, after, tr("Change item geometry"));
    Refresh();
}

void PropertiesPanel::ApplyItemRotation(qreal angle)
{
    if (SelectedItems().isEmpty()) {
        return;
    }
    ApplyPropertyValues({{ItemProperty::ROTATION, angle}}, tr("Change rotation"));
}

void PropertiesPanel::ApplyInteriorAngle(qreal angle)
{
    if (SelectedItems().isEmpty()) {
        return;
    }
    // 内角 A 对选中集合里所有上报该属性的图元生效（普通平行四边形为真值，菱形按宽反算高）。
    ApplyPropertyValues({{ItemProperty::PARALLELOGRAM_ANGLE, angle}}, tr("Change interior angle"));
}

void PropertiesPanel::ApplyItemCenter(const QPointF& center)
{
    if (SelectedItems().isEmpty()) {
        return;
    }
    // 中心点写回只平移位置：多选时把所有成员绝对对齐到同一中心，一条撤销记录。
    ApplyPropertyValues({{ItemProperty::CENTER, center}}, tr("Align item centers"));
}

void PropertiesPanel::AdjustLayer(int delta)
{
    if (m_scene) {
        const int count = m_scene->MoveSelectedItemsToAdjacentLayer(delta);
        statusMessage(count > 0 ? tr("Moved %n Item(s) to another layer.", nullptr, count)
                                : tr("No layer to move to in that direction."));
        Refresh();
    }
}

void PropertiesPanel::AdjustHeight(int delta)
{
    if (m_scene) {
        const int count = m_scene->AdjustSelectedHeights(delta);
        statusMessage(count > 0 ? tr("Adjusted the height of %n Item(s).", nullptr, count)
                                : tr("The height did not change."));
        Refresh();
    }
}
