#include "ItemPropertiesDialogGeometryBlock.h"

#include "ItemPropertiesDialog.h"
#include "PropertyRowWidgets.h"
#include "ShapeItem.h"

#include <QDoubleSpinBox>
#include <QFormLayout>

ItemPropertiesDialogGeometryBlock::ItemPropertiesDialogGeometryBlock(CanvasItem* item, QWidget* parent)
    : QWidget(parent)
{
    if (!item) {
        return;
    }

    m_positionRow = new PairAttributeRow(IconId::FIT, false, QStringLiteral("ItemPropertiesPositionXValue"),
                                         QStringLiteral("ItemPropertiesPositionYValue"), 1, -1000000.0, 1000000.0,
                                         QString(), QStringLiteral(","), this);
    m_positionXSpin = m_positionRow->FirstSpin();
    m_positionYSpin = m_positionRow->SecondSpin();
    m_positionXSpin->setMinimumWidth(88);
    m_positionYSpin->setMinimumWidth(88);
    m_positionRow->Refresh(item->pos().x(), item->pos().y());

    m_sizeRow = new PairAttributeRow(IconId::SHAPES, false, QStringLiteral("ItemPropertiesWidthValue"),
                                     QStringLiteral("ItemPropertiesHeightSizeValue"), 1, 1.0, 1000000.0, QString(),
                                     QStringLiteral("×"), this);
    m_widthSpin = m_sizeRow->FirstSpin();
    m_heightSizeSpin = m_sizeRow->SecondSpin();
    m_widthSpin->setMinimumWidth(88);
    m_heightSizeSpin->setMinimumWidth(88);
    m_sizeRow->Refresh(item->ContentSize().width(), item->ContentSize().height());

    m_rotationRow = new NumberAttributeRow(IconId::ROTATE_CCW, false, 1, -360.0, 360.0, QStringLiteral("°"),
                                           QStringLiteral("ItemPropertiesRotationValue"), this);
    m_rotationRow->SetSingleStep(5.0);
    m_rotationSpin = m_rotationRow->Spin();
    m_rotationSpin->setMinimumWidth(88);
    m_rotationRow->Refresh(item->RotationAngle());

    auto* shape = dynamic_cast<ShapeItem*>(item);
    if (shape && !shape->IsCustomShape() &&
        (shape->shapeType() == ShapeType::ELLIPSE || shape->shapeType() == ShapeType::SECTOR)) {
        m_majorAxisRow = new NumberAttributeRow(IconId::SHAPES, false, 1, 0.5, 500000.0, QStringLiteral(" px"),
                                                QStringLiteral("ItemPropertiesMajorAxisValue"), this);
        m_majorAxisSpin = m_majorAxisRow->Spin();
        m_majorAxisSpin->setMinimumWidth(88);

        m_minorAxisRow = new NumberAttributeRow(IconId::SHAPES, false, 1, 0.5, 500000.0, QStringLiteral(" px"),
                                                QStringLiteral("ItemPropertiesMinorAxisValue"), this);
        m_minorAxisSpin = m_minorAxisRow->Spin();
        m_minorAxisSpin->setMinimumWidth(88);

        SyncAxesFromSize();
        connect(m_widthSpin, &QDoubleSpinBox::valueChanged, this, [this]() { SyncAxesFromSize(); });
        connect(m_heightSizeSpin, &QDoubleSpinBox::valueChanged, this, [this]() { SyncAxesFromSize(); });
        connect(m_majorAxisSpin, &QDoubleSpinBox::valueChanged, this, [this]() { ApplyAxesToSize(true); });
        connect(m_minorAxisSpin, &QDoubleSpinBox::valueChanged, this, [this]() { ApplyAxesToSize(false); });
    }
}

void ItemPropertiesDialogGeometryBlock::AddRows(QFormLayout* form)
{
    // 需求 8 行序定稿：长半轴 → 短半轴 → 旋转 → 位置 → 图元尺寸。
    // 位置与尺寸固定在最后两行，旋转保持在位置之前。
    if (m_majorAxisRow && m_minorAxisRow) {
        form->addRow(ItemPropertiesDialog::tr("Semi-major axis"), m_majorAxisRow);
        form->addRow(ItemPropertiesDialog::tr("Semi-minor axis"), m_minorAxisRow);
    }
    if (m_rotationRow) {
        form->addRow(ItemPropertiesDialog::tr("Rotation"), m_rotationRow);
    }
    if (m_positionRow) {
        form->addRow(ItemPropertiesDialog::tr("Position"), m_positionRow);
    }
    if (m_sizeRow) {
        form->addRow(ItemPropertiesDialog::tr("Item size"), m_sizeRow);
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
    if (m_syncingAxes || !m_widthSpin || !m_heightSizeSpin || !m_majorAxisSpin || !m_minorAxisSpin) {
        return;
    }
    const qreal width = m_widthSpin->value();
    const qreal height = m_heightSizeSpin->value();
    m_syncingAxes = true;
    m_majorAxisSpin->setValue(qMax(width, height) / 2.0);
    m_minorAxisSpin->setValue(qMin(width, height) / 2.0);
    m_syncingAxes = false;
}

void ItemPropertiesDialogGeometryBlock::ApplyAxesToSize(bool majorChanged)
{
    if (m_syncingAxes || !m_widthSpin || !m_heightSizeSpin || !m_majorAxisSpin || !m_minorAxisSpin ||
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
