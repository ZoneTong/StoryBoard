#include "ItemPropertiesDialogGeometryBlock.h"

#include "ItemPropertiesDialog.h"
#include "LineItemAbstract.h"
#include "PropertyRowWidgets.h"
#include "ShapeItem.h"

#include <QDoubleSpinBox>
#include <QFormLayout>

ItemPropertiesDialogGeometryBlock::ItemPropertiesDialogGeometryBlock(QWidget* parent) : QWidget(parent)
{
    m_positionRow = new PairAttributeRow(IconId::FIT, false, QStringLiteral("ItemPropertiesPositionXValue"),
                                         QStringLiteral("ItemPropertiesPositionYValue"), 1, -1000000.0, 1000000.0,
                                         QString(), QStringLiteral(","), this);
    m_positionXSpin = m_positionRow->FirstSpin();
    m_positionYSpin = m_positionRow->SecondSpin();
    m_positionXSpin->setMinimumWidth(88);
    m_positionYSpin->setMinimumWidth(88);

    m_sizeRow = new PairAttributeRow(IconId::SHAPES, false, QStringLiteral("ItemPropertiesWidthValue"),
                                     QStringLiteral("ItemPropertiesHeightSizeValue"), 1, 1.0, 1000000.0, QString(),
                                     QStringLiteral("×"), this);
    m_widthSpin = m_sizeRow->FirstSpin();
    m_heightSizeSpin = m_sizeRow->SecondSpin();
    m_widthSpin->setMinimumWidth(88);
    m_heightSizeSpin->setMinimumWidth(88);

    m_rotationRow = new NumberAttributeRow(IconId::ROTATE_CCW, false, 1, -360.0, 360.0, QStringLiteral("°"),
                                           QStringLiteral("ItemPropertiesRotationValue"), this);
    m_rotationRow->SetSingleStep(5.0);
    m_rotationSpin = m_rotationRow->Spin();
    m_rotationSpin->setMinimumWidth(88);

    // 曲线族里的弧线：椭圆框与笔迹包围盒不同，半轴按真实椭圆外接框读写；
    // 椭圆 / 扇形的内容框即外接框，半轴与内容宽高双向同步。
    m_majorAxisRow = new NumberAttributeRow(IconId::SHAPES, false, 1, 0.5, 500000.0, QStringLiteral(" px"),
                                            QStringLiteral("ItemPropertiesMajorAxisValue"), this);
    m_majorAxisSpin = m_majorAxisRow->Spin();
    m_majorAxisSpin->setMinimumWidth(88);

    m_minorAxisRow = new NumberAttributeRow(IconId::SHAPES, false, 1, 0.5, 500000.0, QStringLiteral(" px"),
                                            QStringLiteral("ItemPropertiesMinorAxisValue"), this);
    m_minorAxisSpin = m_minorAxisRow->Spin();
    m_minorAxisSpin->setMinimumWidth(88);

    // 连接一次、运行时按 m_hasAxes / m_arcAxes 分派：同一实例换图元无需重连。
    connect(m_widthSpin, &QDoubleSpinBox::valueChanged, this, [this]() { SyncAxesFromSize(); });
    connect(m_heightSizeSpin, &QDoubleSpinBox::valueChanged, this, [this]() { SyncAxesFromSize(); });
    connect(m_majorAxisSpin, &QDoubleSpinBox::valueChanged, this,
            [this]() { m_arcAxes ? ApplyArcAxes(true) : ApplyAxesToSize(true); });
    connect(m_minorAxisSpin, &QDoubleSpinBox::valueChanged, this,
            [this]() { m_arcAxes ? ApplyArcAxes(false) : ApplyAxesToSize(false); });
}

void ItemPropertiesDialogGeometryBlock::AddRows(QFormLayout* form)
{
    m_form = form;
    // 需求 8 行序定稿：长半轴 → 短半轴 → 旋转 → 位置 → 图元尺寸。
    // 位置与尺寸固定在最后两行，旋转保持在位置之前。
    form->addRow(ItemPropertiesDialog::tr("Semi-major axis"), m_majorAxisRow);
    form->addRow(ItemPropertiesDialog::tr("Semi-minor axis"), m_minorAxisRow);
    form->addRow(ItemPropertiesDialog::tr("Rotation"), m_rotationRow);
    form->addRow(ItemPropertiesDialog::tr("Position"), m_positionRow);
    form->addRow(ItemPropertiesDialog::tr("Item size"), m_sizeRow);
}

void ItemPropertiesDialogGeometryBlock::SetItem(CanvasItem* item)
{
    m_hasAxes = false;
    m_arcAxes = false;
    m_axisItem = item;
    if (item) {
        m_positionRow->Refresh(item->pos().x(), item->pos().y());
        m_sizeRow->Refresh(item->ContentSize().width(), item->ContentSize().height());
        m_rotationRow->Refresh(item->RotationAngle());
        m_hasAxes = item->Properties().Contains(ItemProperty::MAJOR_AXIS);
        m_arcAxes = m_hasAxes && dynamic_cast<LineItemAbstract*>(item) != nullptr;
        if (m_hasAxes) {
            SyncAxesFromSize();
            if (m_arcAxes) {
                m_initialArcMajor = m_majorAxisSpin->value();
                m_initialArcMinor = m_minorAxisSpin->value();
            }
        }
    }
    if (m_form) {
        const bool showAxes = item != nullptr && m_hasAxes;
        m_form->setRowVisible(m_majorAxisRow, showAxes);
        m_form->setRowVisible(m_minorAxisRow, showAxes);
        m_form->setRowVisible(m_rotationRow, item != nullptr);
        m_form->setRowVisible(m_positionRow, item != nullptr);
        m_form->setRowVisible(m_sizeRow, item != nullptr);
    }
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
