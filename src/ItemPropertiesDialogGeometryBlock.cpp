#include "ItemPropertiesDialogGeometryBlock.h"

#include "ItemPropertiesDialog.h"
#include "ItemPropertiesDialogFields.h"

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
}

void ItemPropertiesDialogGeometryBlock::addRows(QFormLayout* form)
{
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
    if (m_rotationSpin) {
        form->addRow(ItemPropertiesDialog::tr("Rotation"), m_rotationSpin);
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
