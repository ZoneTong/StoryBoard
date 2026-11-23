#include "items/properties/ItemPropertyCatalog.h"

#include <QCoreApplication>

const QList<ItemPropertyDescriptor>& ItemPropertyCatalog::All()
{
    // 顺序与 ItemProperty 枚举一致：编辑面板与属性窗都按此顺序建行。
    static const QList<ItemPropertyDescriptor> DESCRIPTORS = {
        {ItemProperty::TEXT_CONTENT, "textContent", "Text content", ItemPropertyGroup::TEXT, ItemPropertyKind::TEXT,
         0.0, 0.0, 1.0, 0, "", false},
        {ItemProperty::TEXT_COLOR, "textColor", "Text color", ItemPropertyGroup::TEXT, ItemPropertyKind::COLOR, 0.0,
         0.0, 1.0, 0, "", false},
        {ItemProperty::TEXT_FONT, "textFont", "Font", ItemPropertyGroup::TEXT, ItemPropertyKind::ENUM, 0.0, 0.0, 1.0, 0,
         "", false},
        {ItemProperty::FILL_STYLE, "fillStyle", "Fill style", ItemPropertyGroup::FILL, ItemPropertyKind::ENUM, 0.0, 0.0,
         1.0, 0, "", false},
        {ItemProperty::FILL_COLOR, "fillColor", "Fill color", ItemPropertyGroup::FILL, ItemPropertyKind::COLOR, 0.0,
         0.0, 1.0, 0, "", false},
        {ItemProperty::FILL_IMAGE, "fillImage", "Fill image", ItemPropertyGroup::FILL, ItemPropertyKind::TEXT, 0.0, 0.0,
         1.0, 0, "", false},
        {ItemProperty::FILL_IMAGE_MODE, "fillImageMode", "Fill image mode", ItemPropertyGroup::FILL,
         ItemPropertyKind::ENUM, 0.0, 2.0, 1.0, 0, "", false},
        {ItemProperty::STROKE_STYLE, "strokeStyle", "Border style", ItemPropertyGroup::STROKE, ItemPropertyKind::ENUM,
         0.0, 0.0, 1.0, 0, "", false},
        {ItemProperty::STROKE_COLOR, "strokeColor", "Border color", ItemPropertyGroup::STROKE, ItemPropertyKind::COLOR,
         0.0, 0.0, 1.0, 0, "", false},
        {ItemProperty::STROKE_WIDTH, "strokeWidth", "Border width", ItemPropertyGroup::STROKE, ItemPropertyKind::REAL,
         1.0, 64.0, 1.0, 1, " px", false},
        {ItemProperty::OPACITY, "opacity", "Opacity", ItemPropertyGroup::VALUE, ItemPropertyKind::INTEGER, 0.0, 100.0,
         1.0, 0, " %", false},
        {ItemProperty::CORNER_RADIUS, "cornerRadius", "Corner radius", ItemPropertyGroup::VALUE,
         ItemPropertyKind::INTEGER, 0.0, 500000.0, 1.0, 0, " px", false},
        {ItemProperty::START_ANGLE, "startAngle", "Start angle", ItemPropertyGroup::VALUE, ItemPropertyKind::INTEGER,
         -360.0, 360.0, 5.0, 0, "°", false},
        {ItemProperty::SWEEP_ANGLE, "sweepAngle", "Sweep angle", ItemPropertyGroup::VALUE, ItemPropertyKind::INTEGER,
         -360.0, 360.0, 5.0, 0, "°", false},
        {ItemProperty::MAJOR_AXIS, "majorAxis", "Semi-major axis", ItemPropertyGroup::GEOMETRY, ItemPropertyKind::REAL,
         0.5, 500000.0, 1.0, 1, " px", true},
        {ItemProperty::MINOR_AXIS, "minorAxis", "Semi-minor axis", ItemPropertyGroup::GEOMETRY, ItemPropertyKind::REAL,
         0.5, 500000.0, 1.0, 1, " px", true},
        {ItemProperty::SINE_CYCLES, "sineCycles", "Sine cycles", ItemPropertyGroup::VALUE, ItemPropertyKind::INTEGER,
         1.0, 12.0, 1.0, 0, "", false},
        {ItemProperty::SINE_AMPLITUDE, "sineAmplitude", "Sine amplitude", ItemPropertyGroup::VALUE,
         ItemPropertyKind::INTEGER, 1.0, 200.0, 1.0, 0, " px", false},
        {ItemProperty::BOTTOM_ANGLE1, "bottomAngle1", "Interior angle A", ItemPropertyGroup::VALUE,
         ItemPropertyKind::INTEGER, 5.0, 175.0, 1.0, 0, "°", false},
        {ItemProperty::BOTTOM_ANGLE2, "bottomAngle2", "Interior angle B", ItemPropertyGroup::VALUE,
         ItemPropertyKind::INTEGER, 5.0, 170.0, 1.0, 0, "°", false},
        {ItemProperty::PARALLELOGRAM_ANGLE, "parallelogramAngle", "Interior angle A", ItemPropertyGroup::VALUE,
         ItemPropertyKind::INTEGER, 5.0, 175.0, 1.0, 0, "°", false},
        {ItemProperty::ARROW_START, "arrowStart", "Arrow heads", ItemPropertyGroup::VALUE, ItemPropertyKind::ENUM, 0.0,
         0.0, 1.0, 0, "", false},
        {ItemProperty::ARROW_END, "arrowEnd", "Arrow heads", ItemPropertyGroup::VALUE, ItemPropertyKind::ENUM, 0.0, 0.0,
         1.0, 0, "", false},
        {ItemProperty::NAME, "name", "Item name", ItemPropertyGroup::IDENTITY, ItemPropertyKind::TEXT, 0.0, 0.0, 1.0, 0,
         "", false},
        {ItemProperty::LAYER, "layer", "Layer", ItemPropertyGroup::IDENTITY, ItemPropertyKind::INTEGER, 0.0, 999.0, 1.0,
         0, "", false},
        {ItemProperty::HEIGHT, "height", "Height", ItemPropertyGroup::IDENTITY, ItemPropertyKind::INTEGER, 0.0,
         999999.0, 1.0, 0, "", false},
        {ItemProperty::POSITION, "position", "Position", ItemPropertyGroup::GEOMETRY, ItemPropertyKind::REAL,
         -1000000.0, 1000000.0, 1.0, 1, "", false},
        {ItemProperty::SIZE, "size", "Item size", ItemPropertyGroup::GEOMETRY, ItemPropertyKind::REAL, 1.0, 1000000.0,
         1.0, 1, "", false},
        {ItemProperty::ROTATION, "rotation", "Rotation", ItemPropertyGroup::GEOMETRY, ItemPropertyKind::REAL, -360.0,
         360.0, 5.0, 1, "°", false},
        {ItemProperty::CENTER, "center", "Center point", ItemPropertyGroup::GEOMETRY, ItemPropertyKind::REAL,
         -1000000.0, 1000000.0, 1.0, 1, "", true},
    };
    return DESCRIPTORS;
}

const ItemPropertyDescriptor* ItemPropertyCatalog::Find(ItemProperty property)
{
    for (const ItemPropertyDescriptor& descriptor : All()) {
        if (descriptor.property == property) {
            return &descriptor;
        }
    }
    return nullptr;
}

QString ItemPropertyCatalog::Label(ItemProperty property)
{
    const ItemPropertyDescriptor* descriptor = Find(property);
    if (!descriptor) {
        return QString();
    }
    // 标签统一走属性窗上下文：手工维护的 storyboard_zh.ts 已按该上下文收录这些条目。
    return QCoreApplication::translate("ItemPropertiesDialog", descriptor->label);
}

QString ItemPropertyCatalog::Key(ItemProperty property)
{
    const ItemPropertyDescriptor* descriptor = Find(property);
    return descriptor ? QString::fromLatin1(descriptor->key) : QString();
}
