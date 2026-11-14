#include "ItemPropertiesDialogStyleBlock.h"

#include "ItemPropertiesDialog.h"
#include "ItemPropertiesDialogFields.h"
#include "ShapeItem.h"
#include "StrokeItem.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QToolButton>

ItemPropertiesDialogStyleBlock::ItemPropertiesDialogStyleBlock(CanvasItem* item, QWidget* parent) : QWidget(parent)
{
    if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
        const bool filled = shapeTypeIsFilled(shape->shapeType());
        m_hasFill = filled;

        m_fillStyleCombo = new QComboBox(this);
        m_fillStyleCombo->setObjectName(QStringLiteral("ItemPropertiesFillStyleValue"));
        m_fillStyleCombo->setMinimumWidth(150);
        if (filled) {
            for (FillStyle style : allFillStyles()) {
                m_fillStyleCombo->addItem(fillStyleName(style), static_cast<int>(style));
            }
            const int index = m_fillStyleCombo->findData(static_cast<int>(shape->fillStyle()));
            if (index >= 0) {
                m_fillStyleCombo->setCurrentIndex(index);
            }
        } else {
            m_fillStyleCombo->addItem(ItemPropertiesDialog::tr("Not applicable"), -1);
            m_fillStyleCombo->setEnabled(false);
        }

        m_fillColorButton = ItemPropertiesFields::makeColorField(this, shape->fillColor(),
                                                                 QStringLiteral("ItemPropertiesFillColorValue"));
        m_fillColorButton->setEnabled(filled);

        m_strokeColorButton = ItemPropertiesFields::makeColorField(this, shape->strokeColor(),
                                                                   QStringLiteral("ItemPropertiesStrokeColorValue"));

        m_strokeWidthSpin = new QDoubleSpinBox(this);
        m_strokeWidthSpin->setObjectName(QStringLiteral("ItemPropertiesStrokeWidthValue"));
        m_strokeWidthSpin->setDecimals(1);
        m_strokeWidthSpin->setRange(0.5, 64.0);
        m_strokeWidthSpin->setSingleStep(0.5);
        m_strokeWidthSpin->setValue(shape->strokeWidth());
        m_initialStrokeWidth = shape->strokeWidth();
        m_strokeWidthSpin->setMinimumWidth(88);
    } else if (auto* stroke = dynamic_cast<StrokeItem*>(item)) {
        m_strokeColorButton = ItemPropertiesFields::makeColorField(this, stroke->strokeColor(),
                                                                   QStringLiteral("ItemPropertiesStrokeColorValue"));

        m_strokeWidthSpin = new QDoubleSpinBox(this);
        m_strokeWidthSpin->setObjectName(QStringLiteral("ItemPropertiesStrokeWidthValue"));
        m_strokeWidthSpin->setDecimals(1);
        m_strokeWidthSpin->setRange(0.5, 64.0);
        m_strokeWidthSpin->setSingleStep(0.5);
        m_strokeWidthSpin->setValue(stroke->strokeWidth());
        m_initialStrokeWidth = stroke->strokeWidth();
        m_strokeWidthSpin->setMinimumWidth(88);
    }
}

void ItemPropertiesDialogStyleBlock::addRows(QFormLayout* form)
{
    if (m_fillStyleCombo) {
        form->addRow(ItemPropertiesDialog::tr("Fill style"), m_fillStyleCombo);
    }
    if (m_fillColorButton) {
        form->addRow(ItemPropertiesDialog::tr("Fill color"), m_fillColorButton);
    }
    if (m_strokeColorButton) {
        form->addRow(ItemPropertiesDialog::tr("Stroke color"), m_strokeColorButton);
    }
    if (m_strokeWidthSpin) {
        form->addRow(ItemPropertiesDialog::tr("Stroke width"), m_strokeWidthSpin);
    }
}
