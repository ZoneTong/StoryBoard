#include "items/properties/ItemPropertyCatalog.h"

#include "items/CanvasItem.h"
#include "items/SectorBase.h"
#include "items/geometry/EllipseAxis.h"
#include "items/lines/ArcItem.h"
#include "items/lines/LineAbstractItem.h"
#include "items/lines/SineItem.h"
#include "items/properties/FillAttributes.h"
#include "items/properties/StrokeAttributes.h"
#include "items/shapes/ParallelogramAbstractItem.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "items/text/TextItem.h"

#include <QCoreApplication>
#include <utility>

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
                                   -360.0, 360.0, 1.0, 0, QStringLiteral("°")},
        new ItemPropertyDescriptor{ItemProperty::SWEEP_ANGLE, QStringLiteral("sweepAngle"),
                                   QStringLiteral("Sweep angle"), ItemPropertyGroup::VALUE, ItemPropertyKind::INTEGER,
                                   -360.0, 360.0, 1.0, 0, QStringLiteral("°")},
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
                                   ItemPropertyGroup::GEOMETRY, ItemPropertyKind::REAL, -360.0, 360.0, 1.0, 1,
                                   QStringLiteral("°")},
        new ItemPropertyDescriptor{ItemProperty::CENTER, QStringLiteral("center"), QStringLiteral("Center point"),
                                   ItemPropertyGroup::GEOMETRY, ItemPropertyKind::REAL, -1000000.0, 1000000.0, 1.0, 1,
                                   QString(), true},
    };
    // 公共身份 / 几何属性先走描述符读写回调，属性键通道不再进入 CanvasItem 的 switch。
    static const bool CALLBACKS_ATTACHED = []() {
        for (ItemPropertyDescriptor* descriptor : DESCRIPTORS) {
            if (descriptor->key == ItemProperty::NAME) {
                descriptor->reader = [](const CanvasItem* item) { return item->Name(); };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) { item->SetName(value.toString()); };
            } else if (descriptor->key == ItemProperty::LAYER) {
                descriptor->reader = [](const CanvasItem* item) { return item->LayerId(); };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) { item->SetLayerId(value.toInt()); };
            } else if (descriptor->key == ItemProperty::HEIGHT) {
                descriptor->reader = [](const CanvasItem* item) { return item->ZOrder(); };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) { item->SetZOrder(value.toInt()); };
            } else if (descriptor->key == ItemProperty::POSITION) {
                descriptor->reader = [](const CanvasItem* item) { return item->pos(); };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) { item->setPos(value.toPointF()); };
            } else if (descriptor->key == ItemProperty::SIZE) {
                descriptor->reader = [](const CanvasItem* item) { return item->ContentSize(); };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    item->SetContentSize(value.toSizeF());
                };
            } else if (descriptor->key == ItemProperty::ROTATION) {
                descriptor->reader = [](const CanvasItem* item) { return item->RotationAngle(); };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    item->SetRotationAngle(value.toDouble());
                };
            } else if (descriptor->key == ItemProperty::CENTER) {
                descriptor->reader = [](const CanvasItem* item) { return item->CenterScenePoint(); };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    item->setPos(item->pos() + value.toPointF() - item->CenterScenePoint());
                };
            } else if (descriptor->key == ItemProperty::OPACITY) {
                descriptor->reader = [](const CanvasItem* item) { return item->ItemOpacity(); };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    item->SetItemOpacity(value.toDouble());
                };
            } else if (descriptor->key == ItemProperty::TEXT_CONTENT) {
                descriptor->reader = [](const CanvasItem* item) -> QVariant {
                    const auto* text = dynamic_cast<const TextItem*>(item);
                    return text ? text->TextContent() : QVariant();
                };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    if (auto* text = dynamic_cast<TextItem*>(item)) {
                        text->SetTextContent(value.toString());
                    }
                };
            } else if (descriptor->key == ItemProperty::TEXT_FONT) {
                descriptor->reader = [](const CanvasItem* item) -> QVariant {
                    const auto* text = dynamic_cast<const TextItem*>(item);
                    return text ? text->TextFont() : QVariant();
                };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    if (auto* text = dynamic_cast<TextItem*>(item)) {
                        text->SetTextFont(value.value<QFont>());
                    }
                };
            } else if (descriptor->key == ItemProperty::TEXT_COLOR) {
                descriptor->reader = [](const CanvasItem* item) -> QVariant {
                    const auto* text = dynamic_cast<const TextItem*>(item);
                    return text ? text->TextColor() : QVariant();
                };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    if (auto* text = dynamic_cast<TextItem*>(item)) {
                        text->SetTextColor(value.value<QColor>());
                    }
                };
            } else if (descriptor->key == ItemProperty::FILL_STYLE) {
                descriptor->reader = [](const CanvasItem* item) -> QVariant {
                    const auto* fill = dynamic_cast<const FillAttributes*>(item);
                    return fill ? static_cast<int>(fill->fillStyle()) : QVariant();
                };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    if (auto* fill = dynamic_cast<FillAttributes*>(item)) {
                        fill->SetFillStyle(static_cast<FillStyle>(value.toInt()));
                    }
                };
            } else if (descriptor->key == ItemProperty::FILL_COLOR) {
                descriptor->reader = [](const CanvasItem* item) -> QVariant {
                    const auto* fill = dynamic_cast<const FillAttributes*>(item);
                    return fill ? fill->FillColor() : QVariant();
                };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    if (auto* fill = dynamic_cast<FillAttributes*>(item)) {
                        fill->SetFill(value.value<QColor>(), fill->fillStyle());
                    }
                };
            } else if (descriptor->key == ItemProperty::FILL_IMAGE) {
                descriptor->reader = [](const CanvasItem* item) -> QVariant {
                    const auto* fill = dynamic_cast<const FillAttributes*>(item);
                    return fill ? QVariant::fromValue(fill->FillImage()) : QVariant();
                };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    if (auto* fill = dynamic_cast<FillAttributes*>(item)) {
                        const QPixmap image = value.value<QPixmap>();
                        if (image.isNull()) {
                            fill->ClearFillImage();
                        } else {
                            fill->SetFillImage(image, fill->FillImageMode());
                        }
                    }
                };
            } else if (descriptor->key == ItemProperty::FILL_IMAGE_MODE) {
                descriptor->reader = [](const CanvasItem* item) -> QVariant {
                    const auto* fill = dynamic_cast<const FillAttributes*>(item);
                    return fill ? static_cast<int>(fill->FillImageMode()) : QVariant();
                };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    if (auto* fill = dynamic_cast<FillAttributes*>(item)) {
                        fill->SetFillImage(fill->FillImage(), static_cast<ImageFill::Mode>(value.toInt()));
                    }
                };
            } else if (descriptor->key == ItemProperty::STROKE_STYLE) {
                descriptor->reader = [](const CanvasItem* item) -> QVariant {
                    const auto* stroke = dynamic_cast<const StrokeAttributes*>(item);
                    return stroke ? static_cast<int>(stroke->StrokeStyle()) : QVariant();
                };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    if (auto* stroke = dynamic_cast<StrokeAttributes*>(item)) {
                        stroke->SetStrokeStyle(static_cast<Qt::PenStyle>(value.toInt()));
                    }
                };
            } else if (descriptor->key == ItemProperty::STROKE_COLOR) {
                descriptor->reader = [](const CanvasItem* item) -> QVariant {
                    const auto* stroke = dynamic_cast<const StrokeAttributes*>(item);
                    return stroke ? stroke->StrokeColor() : QVariant();
                };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    if (auto* stroke = dynamic_cast<StrokeAttributes*>(item)) {
                        stroke->SetStrokeColor(value.value<QColor>());
                    }
                };
            } else if (descriptor->key == ItemProperty::STROKE_WIDTH) {
                descriptor->reader = [](const CanvasItem* item) -> QVariant {
                    const auto* stroke = dynamic_cast<const StrokeAttributes*>(item);
                    return stroke ? stroke->StrokeWidth() : QVariant();
                };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    if (auto* stroke = dynamic_cast<StrokeAttributes*>(item)) {
                        stroke->SetStrokeWidth(value.toDouble());
                    }
                };
            } else if (descriptor->key == ItemProperty::MAJOR_AXIS) {
                descriptor->reader = [](const CanvasItem* item) -> QVariant {
                    if (const auto* arc = dynamic_cast<const ArcItem*>(item)) {
                        return EllipseAxis::Major(arc->EllipseSize());
                    }
                    const auto* shape = dynamic_cast<const ShapeAbstractItem*>(item);
                    return shape ? EllipseAxis::Major(shape->ContentSize()) : QVariant();
                };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    if (auto* arc = dynamic_cast<ArcItem*>(item)) {
                        arc->SetArcAxes(value.toDouble(), EllipseAxis::Minor(arc->EllipseSize()));
                    } else if (auto* shape = dynamic_cast<ShapeAbstractItem*>(item)) {
                        const EllipseAxis::ResizeResult resize =
                            EllipseAxis::Apply(true, value.toDouble(), shape->pos(), shape->ContentSize());
                        shape->setPos(resize.pos);
                        shape->SetContentSize(resize.size);
                    }
                };
            } else if (descriptor->key == ItemProperty::MINOR_AXIS) {
                descriptor->reader = [](const CanvasItem* item) -> QVariant {
                    if (const auto* arc = dynamic_cast<const ArcItem*>(item)) {
                        return EllipseAxis::Minor(arc->EllipseSize());
                    }
                    const auto* shape = dynamic_cast<const ShapeAbstractItem*>(item);
                    return shape ? EllipseAxis::Minor(shape->ContentSize()) : QVariant();
                };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    if (auto* arc = dynamic_cast<ArcItem*>(item)) {
                        arc->SetArcAxes(EllipseAxis::Major(arc->EllipseSize()), value.toDouble());
                    } else if (auto* shape = dynamic_cast<ShapeAbstractItem*>(item)) {
                        const EllipseAxis::ResizeResult resize =
                            EllipseAxis::Apply(false, value.toDouble(), shape->pos(), shape->ContentSize());
                        shape->setPos(resize.pos);
                        shape->SetContentSize(resize.size);
                    }
                };
            } else if (descriptor->key == ItemProperty::CORNER_RADIUS) {
                descriptor->reader = [](const CanvasItem* item) -> QVariant {
                    const auto* shape = dynamic_cast<const ShapeAbstractItem*>(item);
                    return shape ? shape->CornerRadius() : QVariant();
                };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    if (auto* shape = dynamic_cast<ShapeAbstractItem*>(item)) {
                        shape->SetCornerRadius(value.toDouble());
                    }
                };
            } else if (descriptor->key == ItemProperty::START_ANGLE) {
                descriptor->reader = [](const CanvasItem* item) -> QVariant {
                    const auto* arc = dynamic_cast<const SectorBase*>(item);
                    return arc ? arc->StartAngle() : QVariant();
                };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    if (auto* arc = dynamic_cast<SectorBase*>(item)) {
                        arc->SetStartAngle(value.toDouble());
                    }
                };
            } else if (descriptor->key == ItemProperty::SWEEP_ANGLE) {
                descriptor->reader = [](const CanvasItem* item) -> QVariant {
                    const auto* arc = dynamic_cast<const SectorBase*>(item);
                    return arc ? arc->SweepAngle() : QVariant();
                };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    if (auto* arc = dynamic_cast<SectorBase*>(item)) {
                        arc->SetSweepAngle(value.toDouble());
                    }
                };
            } else if (descriptor->key == ItemProperty::SINE_CYCLES) {
                descriptor->reader = [](const CanvasItem* item) -> QVariant {
                    const auto* sine = dynamic_cast<const SineItem*>(item);
                    return sine ? sine->params().cycles : QVariant();
                };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    if (auto* sine = dynamic_cast<SineItem*>(item)) {
                        SineItem::Params params = sine->params();
                        params.cycles = value.toInt();
                        sine->SetParams(params);
                    }
                };
            } else if (descriptor->key == ItemProperty::SINE_AMPLITUDE) {
                descriptor->reader = [](const CanvasItem* item) -> QVariant {
                    const auto* sine = dynamic_cast<const SineItem*>(item);
                    return sine ? sine->params().amplitude : QVariant();
                };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    if (auto* sine = dynamic_cast<SineItem*>(item)) {
                        SineItem::Params params = sine->params();
                        params.amplitude = value.toDouble();
                        sine->SetParams(params);
                    }
                };
            } else if (descriptor->key == ItemProperty::BOTTOM_ANGLE1) {
                descriptor->reader = [](const CanvasItem* item) -> QVariant {
                    const auto* shape = dynamic_cast<const ShapeAbstractItem*>(item);
                    return shape ? shape->BottomAngle1() : QVariant();
                };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    if (auto* shape = dynamic_cast<ShapeAbstractItem*>(item)) {
                        shape->SetTriangleAngles(value.toDouble(), shape->BottomAngle2());
                    }
                };
            } else if (descriptor->key == ItemProperty::BOTTOM_ANGLE2) {
                descriptor->reader = [](const CanvasItem* item) -> QVariant {
                    const auto* shape = dynamic_cast<const ShapeAbstractItem*>(item);
                    return shape ? shape->BottomAngle2() : QVariant();
                };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    if (auto* shape = dynamic_cast<ShapeAbstractItem*>(item)) {
                        shape->SetTriangleAngles(shape->BottomAngle1(), value.toDouble());
                    }
                };
            } else if (descriptor->key == ItemProperty::PARALLELOGRAM_ANGLE) {
                descriptor->reader = [](const CanvasItem* item) -> QVariant {
                    const auto* shape = dynamic_cast<const ParallelogramAbstractItem*>(item);
                    return shape ? shape->ParallelogramAngle() : QVariant();
                };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    if (auto* shape = dynamic_cast<ParallelogramAbstractItem*>(item)) {
                        shape->SetParallelogramAngle(value.toDouble());
                    }
                };
            } else if (descriptor->key == ItemProperty::ARROW_START) {
                descriptor->reader = [](const CanvasItem* item) -> QVariant {
                    const auto* line = dynamic_cast<const LineAbstractItem*>(item);
                    return line ? static_cast<int>(line->StartArrow()) : QVariant();
                };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    if (auto* line = dynamic_cast<LineAbstractItem*>(item)) {
                        line->SetArrowStyles(static_cast<ArrowStyle>(value.toInt()), line->EndArrow());
                    }
                };
            } else if (descriptor->key == ItemProperty::ARROW_END) {
                descriptor->reader = [](const CanvasItem* item) -> QVariant {
                    const auto* line = dynamic_cast<const LineAbstractItem*>(item);
                    return line ? static_cast<int>(line->EndArrow()) : QVariant();
                };
                descriptor->writer = [](CanvasItem* item, const QVariant& value) {
                    if (auto* line = dynamic_cast<LineAbstractItem*>(item)) {
                        line->SetArrowStyles(line->StartArrow(), static_cast<ArrowStyle>(value.toInt()));
                    }
                };
            }
        }
        return true;
    }();
    Q_UNUSED(CALLBACKS_ATTACHED)
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

const ItemPropertyDescriptor*
ItemPropertyCatalog::RegisterDynamic(PropertyKey property, const QString& label, ItemPropertyGroup group,
                                     ItemPropertyKind kind, double minimum, double maximum, double step, int decimals,
                                     const QString& suffix, std::function<QVariant(const CanvasItem*)> reader,
                                     std::function<void(CanvasItem*, const QVariant&)> writer)
{
    if (!property.IsValid() || property.IsCommon()) {
        return nullptr;
    }
    if (const ItemPropertyDescriptor* existing = Find(property)) {
        // 已注册的描述保留首次回调；后续调用只补空字段，不覆盖已有读写。
        auto* mutableDescriptor = const_cast<ItemPropertyDescriptor*>(existing);
        if (mutableDescriptor->reader == nullptr && reader) {
            mutableDescriptor->reader = std::move(reader);
        }
        if (mutableDescriptor->writer == nullptr && writer) {
            mutableDescriptor->writer = std::move(writer);
        }
        return existing;
    }
    auto* descriptor = new ItemPropertyDescriptor{
        property, property.StorageKey(), label, group, kind, minimum, maximum, step, decimals, suffix, false, reader,
        writer};
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
