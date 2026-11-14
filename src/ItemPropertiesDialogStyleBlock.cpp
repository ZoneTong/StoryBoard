#include "ItemPropertiesDialogStyleBlock.h"

#include "ItemPropertiesDialog.h"
#include "ItemPropertiesDialogFields.h"
#include "ShapeItem.h"
#include "StrokeItem.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QToolButton>

namespace {
qreal defaultCornerRadius(ShapeType type, const QSizeF& size)
{
    if (type == ShapeType::RoundedRectangle) {
        return qMin(size.width(), size.height()) * 0.16;
    }
    return 0.0;
}
} // namespace

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

        const ShapeType type = shape->shapeType();
        if (type == ShapeType::Rectangle || type == ShapeType::RoundedRectangle) {
            m_defaultCornerRatio = type == ShapeType::RoundedRectangle ? 0.16 : 0.0;
            m_contentSize = shape->contentSize();
            m_initialStoredCornerRadius = shape->cornerRadius();
            const qreal radius = m_initialStoredCornerRadius >= 0.0 ? m_initialStoredCornerRadius
                                                                    : defaultCornerRadius(type, m_contentSize);
            m_initialCornerRadius = qRound(radius);

            m_cornerRadiusSpin = new QSpinBox(this);
            m_cornerRadiusSpin->setObjectName(QStringLiteral("ItemPropertiesCornerRadiusValue"));
            m_cornerRadiusSpin->setRange(0, qMax(0, qRound(qMin(m_contentSize.width(), m_contentSize.height()) / 2.0)));
            m_cornerRadiusSpin->setSuffix(QStringLiteral(" px"));
            m_cornerRadiusSpin->setToolTip(ItemPropertiesDialog::tr("Corner radius"));
            m_cornerRadiusSpin->setMinimumWidth(88);
            m_cornerRadiusSpin->setValue(m_initialCornerRadius);
            connect(m_cornerRadiusSpin, &QSpinBox::valueChanged, this,
                    [this](int value) { m_cornerEdited = value != m_initialCornerRadius; });
        }
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
    if (m_cornerRadiusSpin) {
        form->addRow(ItemPropertiesDialog::tr("Corner radius"), m_cornerRadiusSpin);
    }
}

void ItemPropertiesDialogStyleBlock::setContentSize(const QSizeF& size)
{
    if (!m_cornerRadiusSpin) {
        return;
    }
    m_contentSize = size;
    const int maximum = qMax(0, qRound(qMin(size.width(), size.height()) / 2.0));
    const bool blocked = m_cornerRadiusSpin->blockSignals(true);
    m_cornerRadiusSpin->setMaximum(maximum);
    if (!m_cornerEdited && m_initialStoredCornerRadius < 0.0) {
        m_initialCornerRadius = qRound(qMin(size.width(), size.height()) * m_defaultCornerRatio);
        m_cornerRadiusSpin->setValue(m_initialCornerRadius);
    }
    m_cornerRadiusSpin->blockSignals(blocked);
}
