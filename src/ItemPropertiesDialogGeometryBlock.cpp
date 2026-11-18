#include "ItemPropertiesDialogGeometryBlock.h"

#include "ItemPropertiesDialog.h"
#include "ItemPropertiesDialogFields.h"
#include "ShapeItem.h"

#include <QDoubleSpinBox>
#include <QFormLayout>

ItemPropertiesDialogGeometryBlock::ItemPropertiesDialogGeometryBlock(CanvasItem* item, QWidget* parent)
    : QWidget(parent)
{
    if (!item) {
        return;
    }

    m_positionXSpin =
        ItemPropertiesFields::makeSpin(this, QStringLiteral("ItemPropertiesPositionXValue"), -1000000.0, 1000000.0);
    m_positionYSpin =
        ItemPropertiesFields::makeSpin(this, QStringLiteral("ItemPropertiesPositionYValue"), -1000000.0, 1000000.0);
    m_positionXSpin->setValue(item->pos().x());
    m_positionYSpin->setValue(item->pos().y());

    m_widthSpin = ItemPropertiesFields::makeSpin(this, QStringLiteral("ItemPropertiesWidthValue"), 1.0, 1000000.0);
    m_heightSizeSpin =
        ItemPropertiesFields::makeSpin(this, QStringLiteral("ItemPropertiesHeightSizeValue"), 1.0, 1000000.0);
    m_widthSpin->setValue(item->contentSize().width());
    m_heightSizeSpin->setValue(item->contentSize().height());

    m_rotationSpin = ItemPropertiesFields::makeSpin(this, QStringLiteral("ItemPropertiesRotationValue"), -360.0, 360.0);
    m_rotationSpin->setSingleStep(5.0);
    m_rotationSpin->setSuffix(QStringLiteral("°"));
    m_rotationSpin->setValue(item->rotationAngle());

    auto* shape = dynamic_cast<ShapeItem*>(item);
    if (shape && !shape->isCustomShape() &&
        (shape->shapeType() == ShapeType::Ellipse || shape->shapeType() == ShapeType::Sector ||
         shape->shapeType() == ShapeType::Arc)) {
        m_majorAxisSpin =
            ItemPropertiesFields::makeSpin(this, QStringLiteral("ItemPropertiesMajorAxisValue"), 0.5, 500000.0);
        m_minorAxisSpin =
            ItemPropertiesFields::makeSpin(this, QStringLiteral("ItemPropertiesMinorAxisValue"), 0.5, 500000.0);
        m_majorAxisSpin->setSuffix(QStringLiteral(" px"));
        m_minorAxisSpin->setSuffix(QStringLiteral(" px"));
        syncAxesFromSize();
        connect(m_widthSpin, &QDoubleSpinBox::valueChanged, this, [this]() { syncAxesFromSize(); });
        connect(m_heightSizeSpin, &QDoubleSpinBox::valueChanged, this, [this]() { syncAxesFromSize(); });
        connect(m_majorAxisSpin, &QDoubleSpinBox::valueChanged, this, [this]() { applyAxesToSize(true); });
        connect(m_minorAxisSpin, &QDoubleSpinBox::valueChanged, this, [this]() { applyAxesToSize(false); });
    }
}

void ItemPropertiesDialogGeometryBlock::addRows(QFormLayout* form)
{
    // 需求 8 行序定稿：长半轴 → 短半轴 → 旋转 → 位置 → 图元尺寸。
    // 位置与尺寸固定在最后两行，旋转保持在位置之前。
    if (m_majorAxisSpin && m_minorAxisSpin) {
        form->addRow(ItemPropertiesDialog::tr("Semi-major axis"), m_majorAxisSpin);
        form->addRow(ItemPropertiesDialog::tr("Semi-minor axis"), m_minorAxisSpin);
    }
    if (m_rotationSpin) {
        form->addRow(ItemPropertiesDialog::tr("Rotation"), m_rotationSpin);
    }
    if (m_positionXSpin && m_positionYSpin) {
        form->addRow(ItemPropertiesDialog::tr("Position"),
                     ItemPropertiesFields::makePairField(this, QStringLiteral("ItemPropertiesPositionValue"),
                                                         m_positionXSpin, m_positionYSpin, QStringLiteral(",")));
    }
    if (m_widthSpin && m_heightSizeSpin) {
        form->addRow(ItemPropertiesDialog::tr("Item size"),
                     ItemPropertiesFields::makePairField(this, QStringLiteral("ItemPropertiesSizeValue"), m_widthSpin,
                                                         m_heightSizeSpin, QStringLiteral("×")));
    }
}

void ItemPropertiesDialogGeometryBlock::setNaturalSize(const QSizeF& size)
{
    if (m_widthSpin) {
        m_widthSpin->setValue(size.width());
    }
    if (m_heightSizeSpin) {
        m_heightSizeSpin->setValue(size.height());
    }
}

void ItemPropertiesDialogGeometryBlock::syncAxesFromSize()
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

void ItemPropertiesDialogGeometryBlock::applyAxesToSize(bool majorChanged)
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
