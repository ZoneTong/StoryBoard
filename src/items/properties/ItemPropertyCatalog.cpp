#include "items/properties/ItemPropertyCatalog.h"

#include <QCoreApplication>

namespace {
QList<ItemPropertyDescriptor*>& DynamicDescriptors()
{
    static QList<ItemPropertyDescriptor*> descriptors;
    return descriptors;
}
} // namespace

const QList<ItemPropertyDescriptor*>& ItemPropertyCatalog::All()
{
    // 顺序与 ItemProperty 枚举一致：编辑面板与属性窗都按此顺序建行。
    static const QList<ItemPropertyDescriptor*> DESCRIPTORS = {
        new ItemPropertyDescriptor{ItemProperty::TEXT_CONTENT, QStringLiteral("textContent"),
                                   QStringLiteral("Text content"), ItemPropertyGroup::TEXT, ItemPropertyKind::TEXT},
        new ItemPropertyDescriptor{ItemProperty::TEXT_COLOR, QStringLiteral("textColor"), QStringLiteral("Text color"),
                                   ItemPropertyGroup::TEXT, ItemPropertyKind::COLOR},
        new ItemPropertyDescriptor{ItemProperty::TEXT_FONT, QStringLiteral("textFont"), QStringLiteral("Font"),
                                   ItemPropertyGroup::TEXT, ItemPropertyKind::ENUM},
        new ItemPropertyDescriptor{ItemProperty::FILL_STYLE, QStringLiteral("fillStyle"), QStringLiteral("Fill style"),
                                   ItemPropertyGroup::FILL, ItemPropertyKind::ENUM},
        new ItemPropertyDescriptor{ItemProperty::FILL_COLOR, QStringLiteral("fillColor"), QStringLiteral("Fill color"),
                                   ItemPropertyGroup::FILL, ItemPropertyKind::COLOR},
        new ItemPropertyDescriptor{ItemProperty::FILL_IMAGE, QStringLiteral("fillImage"), QStringLiteral("Fill image"),
                                   ItemPropertyGroup::FILL, ItemPropertyKind::TEXT},
        new ItemPropertyDescriptor{ItemProperty::FILL_IMAGE_MODE, QStringLiteral("fillImageMode"),
                                   QStringLiteral("Fill image mode"), ItemPropertyGroup::FILL, ItemPropertyKind::ENUM,
                                   0.0, 2.0},
        new ItemPropertyDescriptor{ItemProperty::STROKE_STYLE, QStringLiteral("strokeStyle"),
                                   QStringLiteral("Border style"), ItemPropertyGroup::STROKE, ItemPropertyKind::ENUM},
        new ItemPropertyDescriptor{ItemProperty::STROKE_COLOR, QStringLiteral("strokeColor"),
                                   QStringLiteral("Border color"), ItemPropertyGroup::STROKE, ItemPropertyKind::COLOR},
        new ItemPropertyDescriptor{ItemProperty::STROKE_WIDTH, QStringLiteral("strokeWidth"),
                                   QStringLiteral("Border width"), ItemPropertyGroup::STROKE, ItemPropertyKind::REAL,
                                   1.0, 64.0, 1.0, 1, QStringLiteral(" px")},
        new ItemPropertyDescriptor{ItemProperty::OPACITY, QStringLiteral("opacity"), QStringLiteral("Opacity"),
                                   ItemPropertyGroup::VALUE, ItemPropertyKind::INTEGER, 0.0, 100.0, 1.0, 0,
                                   QStringLiteral(" %")},
        new ItemPropertyDescriptor{ItemProperty::CORNER_RADIUS, QStringLiteral("cornerRadius"),
                                   QStringLiteral("Corner radius"), ItemPropertyGroup::VALUE, ItemPropertyKind::INTEGER,
                                   0.0, 500000.0, 1.0, 0, QStringLiteral(" px")},
        new ItemPropertyDescriptor{ItemProperty::START_ANGLE, QStringLiteral("startAngle"),
                                   QStringLiteral("Start angle"), ItemPropertyGroup::VALUE, ItemPropertyKind::INTEGER,
                                   -360.0, 360.0, 5.0, 0, QStringLiteral("°")},
        new ItemPropertyDescriptor{ItemProperty::SWEEP_ANGLE, QStringLiteral("sweepAngle"),
                                   QStringLiteral("Sweep angle"), ItemPropertyGroup::VALUE, ItemPropertyKind::INTEGER,
                                   -360.0, 360.0, 5.0, 0, QStringLiteral("°")},
        new ItemPropertyDescriptor{ItemProperty::MAJOR_AXIS, QStringLiteral("majorAxis"),
                                   QStringLiteral("Semi-major axis"), ItemPropertyGroup::GEOMETRY,
                                   ItemPropertyKind::REAL, 0.5, 500000.0, 1.0, 1, QStringLiteral(" px"), true},
        new ItemPropertyDescriptor{ItemProperty::MINOR_AXIS, QStringLiteral("minorAxis"),
                                   QStringLiteral("Semi-minor axis"), ItemPropertyGroup::GEOMETRY,
                                   ItemPropertyKind::REAL, 0.5, 500000.0, 1.0, 1, QStringLiteral(" px"), true},
        new ItemPropertyDescriptor{ItemProperty::SINE_CYCLES, QStringLiteral("sineCycles"),
                                   QStringLiteral("Sine cycles"), ItemPropertyGroup::VALUE, ItemPropertyKind::INTEGER,
                                   1.0, 12.0},
        new ItemPropertyDescriptor{ItemProperty::SINE_AMPLITUDE, QStringLiteral("sineAmplitude"),
                                   QStringLiteral("Sine amplitude"), ItemPropertyGroup::VALUE,
                                   ItemPropertyKind::INTEGER, 1.0, 200.0, 1.0, 0, QStringLiteral(" px")},
        new ItemPropertyDescriptor{ItemProperty::BOTTOM_ANGLE1, QStringLiteral("bottomAngle1"),
                                   QStringLiteral("Interior angle A"), ItemPropertyGroup::VALUE,
                                   ItemPropertyKind::INTEGER, 5.0, 175.0, 1.0, 0, QStringLiteral("°")},
        new ItemPropertyDescriptor{ItemProperty::BOTTOM_ANGLE2, QStringLiteral("bottomAngle2"),
                                   QStringLiteral("Interior angle B"), ItemPropertyGroup::VALUE,
                                   ItemPropertyKind::INTEGER, 5.0, 170.0, 1.0, 0, QStringLiteral("°")},
        new ItemPropertyDescriptor{ItemProperty::PARALLELOGRAM_ANGLE, QStringLiteral("parallelogramAngle"),
                                   QStringLiteral("Interior angle A"), ItemPropertyGroup::VALUE,
                                   ItemPropertyKind::INTEGER, 5.0, 175.0, 1.0, 0, QStringLiteral("°")},
        new ItemPropertyDescriptor{ItemProperty::ARROW_START, QStringLiteral("arrowStart"),
                                   QStringLiteral("Arrow heads"), ItemPropertyGroup::VALUE, ItemPropertyKind::ENUM},
        new ItemPropertyDescriptor{ItemProperty::ARROW_END, QStringLiteral("arrowEnd"), QStringLiteral("Arrow heads"),
                                   ItemPropertyGroup::VALUE, ItemPropertyKind::ENUM},
        new ItemPropertyDescriptor{ItemProperty::NAME, QStringLiteral("name"), QStringLiteral("Item name"),
                                   ItemPropertyGroup::IDENTITY, ItemPropertyKind::TEXT},
        new ItemPropertyDescriptor{ItemProperty::LAYER, QStringLiteral("layer"), QStringLiteral("Layer"),
                                   ItemPropertyGroup::IDENTITY, ItemPropertyKind::INTEGER, 0.0, 999.0},
        new ItemPropertyDescriptor{ItemProperty::HEIGHT, QStringLiteral("height"), QStringLiteral("Height"),
                                   ItemPropertyGroup::IDENTITY, ItemPropertyKind::INTEGER, 0.0, 999999.0},
        new ItemPropertyDescriptor{ItemProperty::POSITION, QStringLiteral("position"), QStringLiteral("Position"),
                                   ItemPropertyGroup::GEOMETRY, ItemPropertyKind::REAL, -1000000.0, 1000000.0, 1.0, 1},
        new ItemPropertyDescriptor{ItemProperty::SIZE, QStringLiteral("size"), QStringLiteral("Item size"),
                                   ItemPropertyGroup::GEOMETRY, ItemPropertyKind::REAL, 1.0, 1000000.0, 1.0, 1},
        new ItemPropertyDescriptor{ItemProperty::ROTATION, QStringLiteral("rotation"), QStringLiteral("Rotation"),
                                   ItemPropertyGroup::GEOMETRY, ItemPropertyKind::REAL, -360.0, 360.0, 5.0, 1,
                                   QStringLiteral("°")},
        new ItemPropertyDescriptor{ItemProperty::CENTER, QStringLiteral("center"), QStringLiteral("Center point"),
                                   ItemPropertyGroup::GEOMETRY, ItemPropertyKind::REAL, -1000000.0, 1000000.0, 1.0, 1,
                                   QString(), true},
    };
    return DESCRIPTORS;
}

const ItemPropertyDescriptor* ItemPropertyCatalog::Find(ItemProperty property)
{
    return Find(PropertyKey(property));
}

const ItemPropertyDescriptor* ItemPropertyCatalog::Find(PropertyKey property)
{
    if (!property.IsValid()) {
        return nullptr;
    }
    if (property.IsCommon()) {
        for (const ItemPropertyDescriptor* descriptor : All()) {
            if (descriptor->key == property) {
                return descriptor;
            }
        }
        return nullptr;
    }
    for (const ItemPropertyDescriptor* descriptor : DynamicDescriptors()) {
        if (descriptor->key == property) {
            return descriptor;
        }
    }
    return nullptr;
}

const ItemPropertyDescriptor* ItemPropertyCatalog::RegisterDynamic(PropertyKey property, const QString& label,
                                                                   ItemPropertyGroup group, ItemPropertyKind kind,
                                                                   double minimum, double maximum, double step,
                                                                   int decimals, const QString& suffix)
{
    if (!property.IsValid() || property.IsCommon()) {
        return nullptr;
    }
    if (const ItemPropertyDescriptor* existing = Find(property)) {
        return existing;
    }
    auto* descriptor = new ItemPropertyDescriptor{
        property, property.StorageKey(), label, group, kind, minimum, maximum, step, decimals, suffix, false};
    DynamicDescriptors().append(descriptor);
    return descriptor;
}

QString ItemPropertyCatalog::Label(ItemProperty property)
{
    return Label(PropertyKey(property));
}

QString ItemPropertyCatalog::Label(PropertyKey property)
{
    const ItemPropertyDescriptor* descriptor = Find(property);
    if (!descriptor) {
        return QString();
    }
    // 标签统一走属性窗上下文：手工维护的 storyboard_zh.ts 已按该上下文收录这些条目。
    return QCoreApplication::translate("ItemPropertiesDialog", descriptor->label.toUtf8().constData());
}

QString ItemPropertyCatalog::Key(ItemProperty property)
{
    return Key(PropertyKey(property));
}

QString ItemPropertyCatalog::Key(PropertyKey property)
{
    const ItemPropertyDescriptor* descriptor = Find(property);
    return descriptor ? descriptor->storageKey : QString();
}
