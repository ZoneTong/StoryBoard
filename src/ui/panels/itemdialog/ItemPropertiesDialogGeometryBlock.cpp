#include "ui/panels/itemdialog/ItemPropertiesDialogGeometryBlock.h"

#include "items/lines/ArcItem.h"
#include "items/lines/LineAbstractItem.h"
#include "items/properties/ItemPropertyCatalog.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "ui/panels/itemdialog/ItemPropertiesDialog.h"
#include "ui/panels/properties/PropertyRowRegistry.h"
#include "ui/panels/properties/PropertyRowSpec.h"
#include "ui/widgets/rows/ValueRows.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>

ItemPropertiesDialogGeometryBlock::ItemPropertiesDialogGeometryBlock(QWidget* parent) : QWidget(parent)
{
    m_positionRow = new PairValueRow(IconId::POSITION, false, QStringLiteral("ItemPropertiesPositionXValue"),
                                     QStringLiteral("ItemPropertiesPositionYValue"), ItemProperty::POSITION,
                                     QStringLiteral(","), this);
    m_positionXSpin = m_positionRow->FirstSpin();
    m_positionYSpin = m_positionRow->SecondSpin();
    m_positionXSpin->setMinimumWidth(88);
    m_positionYSpin->setMinimumWidth(88);

    m_sizeRow = new PairValueRow(IconId::SHAPES, false, QStringLiteral("ItemPropertiesWidthValue"),
                                 QStringLiteral("ItemPropertiesHeightSizeValue"), ItemProperty::SIZE,
                                 QStringLiteral("×"), this);
    m_widthSpin = m_sizeRow->FirstSpin();
    m_heightSizeSpin = m_sizeRow->SecondSpin();
    m_widthSpin->setMinimumWidth(88);
    m_heightSizeSpin->setMinimumWidth(88);

    m_rotationRow = new NumberValueRow(IconId::ROTATE_CCW, false, ItemProperty::ROTATION,
                                       QStringLiteral("ItemPropertiesRotationValue"), this);
    m_rotationSpin = m_rotationRow->Spin();
    m_rotationSpin->setMinimumWidth(88);

    // 曲线族里的弧线：椭圆框与笔迹包围盒不同，半轴按真实椭圆外接框读写；
    // 椭圆 / 扇形的内容框即外接框，半轴与内容宽高双向同步。
    m_majorAxisRow = new NumberValueRow(IconId::SHAPES, false, ItemProperty::MAJOR_AXIS,
                                        QStringLiteral("ItemPropertiesMajorAxisValue"), this);
    m_majorAxisSpin = m_majorAxisRow->Spin();
    m_majorAxisSpin->setMinimumWidth(88);

    m_minorAxisRow = new NumberValueRow(IconId::SHAPES, false, ItemProperty::MINOR_AXIS,
                                        QStringLiteral("ItemPropertiesMinorAxisValue"), this);
    m_minorAxisSpin = m_minorAxisRow->Spin();
    m_minorAxisSpin->setMinimumWidth(88);

    // §3.4 中心点：派生属性，行放在椭圆相关行之后，固定为属性窗最后一行。
    m_centerRow =
        new PairValueRow(IconId::FIT, false, QStringLiteral("ItemPropertiesCenterXValue"),
                         QStringLiteral("ItemPropertiesCenterYValue"), ItemProperty::CENTER, QStringLiteral(","), this);
    m_centerXSpin = m_centerRow->FirstSpin();
    m_centerYSpin = m_centerRow->SecondSpin();
    m_centerXSpin->setMinimumWidth(88);
    m_centerYSpin->setMinimumWidth(88);

    // 连接一次、运行时按 m_hasAxes / m_arcAxes 分派：同一实例换图元无需重连。
    connect(m_widthSpin, &QDoubleSpinBox::valueChanged, this, [this]() { SyncAxesFromSize(); });
    connect(m_heightSizeSpin, &QDoubleSpinBox::valueChanged, this, [this]() { SyncAxesFromSize(); });
    connect(m_majorAxisSpin, &QDoubleSpinBox::valueChanged, this,
            [this]() { m_arcAxes ? ApplyArcAxes(true) : ApplyAxesToSize(true); });
    connect(m_minorAxisSpin, &QDoubleSpinBox::valueChanged, this,
            [this]() { m_arcAxes ? ApplyArcAxes(false) : ApplyAxesToSize(false); });
}

void ItemPropertiesDialogGeometryBlock::AddRows(QFormLayout* form, PropertyRowRegistry* rows)
{
    m_form = form;
    // 行序定稿（§4.1 第 23~28 行）：长半轴 → 短半轴 → 旋转 → 位置 → 图元尺寸 → 中心点。
    m_centerLabel = new QLabel(ItemPropertyCatalog::Label(ItemProperty::CENTER), this);
    form->addRow(ItemPropertyCatalog::Label(ItemProperty::MAJOR_AXIS), m_majorAxisRow);
    form->addRow(ItemPropertyCatalog::Label(ItemProperty::MINOR_AXIS), m_minorAxisRow);
    form->addRow(ItemPropertyCatalog::Label(ItemProperty::ROTATION), m_rotationRow);
    form->addRow(ItemPropertyCatalog::Label(ItemProperty::POSITION), m_positionRow);
    form->addRow(ItemPropertyCatalog::Label(ItemProperty::SIZE), m_sizeRow);
    form->addRow(m_centerLabel, m_centerRow);

    if (rows) {
        auto registerRow = [rows](ItemProperty property, QWidget* field) {
            if (const PropertyRowSpec* spec = PropertyRowSpecs::For(property)) {
                rows->Register(*spec, field);
            }
        };
        registerRow(ItemProperty::MAJOR_AXIS, m_majorAxisRow);
        registerRow(ItemProperty::MINOR_AXIS, m_minorAxisRow);
        registerRow(ItemProperty::ROTATION, m_rotationRow);
        registerRow(ItemProperty::POSITION, m_positionRow);
        registerRow(ItemProperty::SIZE, m_sizeRow);
        registerRow(ItemProperty::CENTER, m_centerRow);
    }
}

void ItemPropertiesDialogGeometryBlock::SetItem(CanvasItem* item)
{
    m_hasAxes = false;
    m_arcAxes = false;
    m_axisItem = item;
    m_initialCenter = QPointF();
    if (item) {
        m_positionRow->Refresh(item->pos().x(), item->pos().y());
        m_sizeRow->Refresh(item->ContentSize().width(), item->ContentSize().height());
        m_rotationRow->Refresh(item->RotationAngle());
        m_hasAxes = item->Properties().Contains(ItemProperty::MAJOR_AXIS);
        m_arcAxes = m_hasAxes && dynamic_cast<LineAbstractItem*>(item) != nullptr;
        if (m_hasAxes) {
            SyncAxesFromSize();
            if (m_arcAxes) {
                m_initialArcMajor = m_majorAxisSpin->value();
                m_initialArcMinor = m_minorAxisSpin->value();
            }
        }

        // 中心点按图元几何回显：椭圆族显示“椭圆中心点”，其余显示“中心点”。
        m_initialCenter = item->PropertyValue(ItemProperty::CENTER).toPointF();
        m_centerRow->Refresh(m_initialCenter.x(), m_initialCenter.y());
        if (m_centerLabel) {
            // 行标题统一为“中心点”，椭圆族的差异只放在悬浮提示里。
            m_centerLabel->setText(ItemPropertiesDialog::tr("Center point"));
            m_centerLabel->setToolTip(
                ItemPropertiesDialog::tr(IsEllipseFamily(item) ? "Ellipse center" : "Center point"));
            m_centerRow->SetIconToolTip(m_centerLabel->toolTip());
        }
    }
}

bool ItemPropertiesDialogGeometryBlock::IsEllipseFamily(const CanvasItem* item)
{
    if (dynamic_cast<const ArcItem*>(item) != nullptr) {
        return true;
    }
    const auto* shape = dynamic_cast<const ShapeAbstractItem*>(item);
    if (!shape) {
        return false;
    }
    return shape->shapeType() == ShapeType::ELLIPSE || shape->shapeType() == ShapeType::SECTOR;
}


void ItemPropertiesDialogGeometryBlock::SetNaturalSize(const QSizeF& size)
{
    if (m_widthSpin) {
        m_widthSpin->setValue(size.width());
    }
    if (m_heightSizeSpin) {
        m_heightSizeSpin->setValue(size.height());
    }
}

void ItemPropertiesDialogGeometryBlock::SyncAxesFromSize()
{
    if (m_syncingAxes || !m_hasAxes || !m_axisItem || !m_widthSpin || !m_heightSizeSpin || !m_majorAxisSpin ||
        !m_minorAxisSpin) {
        return;
    }
    m_syncingAxes = true;
    if (m_arcAxes) {
        // 弧线：半轴按真实椭圆外接框回显（图元状态未变时与打开时一致）。
        m_majorAxisSpin->setValue(m_axisItem->PropertyValue(ItemProperty::MAJOR_AXIS).toDouble());
        m_minorAxisSpin->setValue(m_axisItem->PropertyValue(ItemProperty::MINOR_AXIS).toDouble());
    } else {
        const QSizeF size(m_widthSpin->value(), m_heightSizeSpin->value());
        m_majorAxisSpin->setValue(qMax(size.width(), size.height()) / 2.0);
        m_minorAxisSpin->setValue(qMin(size.width(), size.height()) / 2.0);
    }
    m_syncingAxes = false;
}

void ItemPropertiesDialogGeometryBlock::ApplyArcAxes(bool majorChanged)
{
    if (m_syncingAxes || !m_hasAxes || !m_axisItem || !m_majorAxisSpin || !m_minorAxisSpin) {
        return;
    }

    // 弧线半轴行内互约束：长半轴不得小于短半轴，改哪根就把另一根钳到同值。
    m_syncingAxes = true;
    qreal major = m_majorAxisSpin->value();
    qreal minor = m_minorAxisSpin->value();
    if (major < minor) {
        if (majorChanged) {
            minor = major;
            m_minorAxisSpin->setValue(minor);
        } else {
            major = minor;
            m_majorAxisSpin->setValue(major);
        }
    }
    m_syncingAxes = false;
}

void ItemPropertiesDialogGeometryBlock::ApplyAxesToSize(bool majorChanged)
{
    if (m_syncingAxes || !m_hasAxes || !m_widthSpin || !m_heightSizeSpin || !m_majorAxisSpin || !m_minorAxisSpin ||
        !m_positionXSpin || !m_positionYSpin) {
        return;
    }

    m_syncingAxes = true;
    qreal major = m_majorAxisSpin->value();
    qreal minor = m_minorAxisSpin->value();
    if (major < minor) {
        if (majorChanged) {
            minor = major;
            m_minorAxisSpin->setValue(minor);
        } else {
            major = minor;
            m_majorAxisSpin->setValue(major);
        }
    }

    const qreal oldWidth = m_widthSpin->value();
    const qreal oldHeight = m_heightSizeSpin->value();
    const bool widthIsMajor = oldWidth >= oldHeight;
    const qreal newWidth = widthIsMajor ? major * 2.0 : minor * 2.0;
    const qreal newHeight = widthIsMajor ? minor * 2.0 : major * 2.0;
    const QPointF center(m_positionXSpin->value() + oldWidth / 2.0, m_positionYSpin->value() + oldHeight / 2.0);

    m_widthSpin->setValue(newWidth);
    m_heightSizeSpin->setValue(newHeight);
    m_positionXSpin->setValue(center.x() - newWidth / 2.0);
    m_positionYSpin->setValue(center.y() - newHeight / 2.0);
    m_syncingAxes = false;
}
