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

void PropertiesPanel::ApplyCornerRadius(int radius)
{
    ChangeSelectedShapes(tr("Change rectangle corner radius"),
                         [radius](ShapeAbstractItem* shape) { shape->SetCornerRadius(radius); });
}

void PropertiesPanel::ApplyArcAngles(qreal startAngle, qreal sweepAngle)
{
    // 显示与落盘同为「起始角 + 扫过角」，这里只做范围收敛；扫过角为 0 时按 1° 兜底保形。
    const qreal start = qBound<qreal>(-360.0, startAngle, 360.0);
    qreal sweep = qBound<qreal>(-360.0, sweepAngle, 360.0);
    if (qAbs(sweep) < 1.0) {
        sweep = sweepAngle < 0.0 ? -1.0 : 1.0;
    }
    const QMap<PropertyKey, QVariant> values{{ItemProperty::START_ANGLE, start}, {ItemProperty::SWEEP_ANGLE, sweep}};
    ChangeSelectedShapes(tr("Change arc angles"), [start, sweep](ShapeAbstractItem* shape) {
        shape->SetPropertyValue(ItemProperty::START_ANGLE, start);
        shape->SetPropertyValue(ItemProperty::SWEEP_ANGLE, sweep);
    });
    const QList<LineAbstractItem*> strokes = SelectedStrokes();
    QList<CanvasItem*> targets;
    targets.reserve(strokes.size());
    for (LineAbstractItem* stroke : strokes) {
        targets.append(stroke);
    }
    ApplyPropertyValues(values, tr("Change arc angles"), targets);
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

void PropertiesPanel::ApplyInteriorAngleB(qreal angle)
{
    if (SelectedItems().isEmpty()) {
        return;
    }
    // 内角 B 只有三角形上报；字段名与内角 A 同族，写入仍走属性表。
    ApplyPropertyValues({{ItemProperty::BOTTOM_ANGLE2, angle}}, tr("Change interior angle B"));
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

void PropertiesPanel::ApplyItemAxes(qreal majorAxis, qreal minorAxis)
{
    // 长短半轴与位置尺寸共用属性表路径：只有上报了 MAJOR_AXIS 的图元
    // （椭圆 / 扇形 / 弧线）会真正写入，其余选中项自动跳过。
    ApplyPropertyValues({{ItemProperty::MAJOR_AXIS, majorAxis}, {ItemProperty::MINOR_AXIS, minorAxis}},
                        tr("Edit semi-axes"));
}

void PropertiesPanel::ApplyItemPlacement(const QPointF& position, const QSizeF& size)
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
    // 内角 A 对选中集合里所有上报该属性的图元生效：平行四边形走 PARALLELOGRAM_ANGLE，
    // 三角形走共用的 BOTTOM_ANGLE1；两个键同时给出，属性过滤只落到各自支持的那一个。
    ApplyPropertyValues({{ItemProperty::PARALLELOGRAM_ANGLE, angle}, {ItemProperty::BOTTOM_ANGLE1, angle}},
                        tr("Change interior angle"));
}

void PropertiesPanel::ApplyItemCenter(const QPointF& center)
{
    if (SelectedItems().isEmpty()) {
        return;
    }
    // 中心点写回只平移位置：多选时把所有成员绝对对齐到同一中心，一条撤销记录。
    ApplyPropertyValues({{ItemProperty::CENTER, center}}, tr("Align item centers"));
}
