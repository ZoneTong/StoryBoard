#include "ItemPropertiesDialogStyleBlock.h"

#include "ItemPropertiesDialog.h"
#include "ItemPropertiesDialogFields.h"
#include "ItemStyleVisuals.h"
#include "ShapeItem.h"
#include "StrokeItem.h"

#include <QComboBox>
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
        m_fillStyleCombo->setMinimumWidth(180);
        if (filled) {
            for (FillStyle style : allFillStyles()) {
                m_fillStyleCombo->addItem(ItemStyleVisuals::fillStyleIcon(style, 16), fillStyleName(style),
                                          static_cast<int>(style));
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
                                                                 QStringLiteral("ItemPropertiesFillColorValue"),
                                                                 ItemPropertiesFields::ColorIconRole::Fill);
        m_fillColorButton->setEnabled(filled);

        m_strokeColorButton = ItemPropertiesFields::makeColorField(this, shape->strokeColor(),
                                                                   QStringLiteral("ItemPropertiesStrokeColorValue"),
                                                                   ItemPropertiesFields::ColorIconRole::Border);

        m_borderStyleCombo = new QComboBox(this);
        m_borderStyleCombo->setObjectName(QStringLiteral("ItemPropertiesBorderStyleValue"));
        m_borderStyleCombo->setMinimumWidth(180);
        for (Qt::PenStyle penStyle : ItemStyleVisuals::borderStyles()) {
            m_borderStyleCombo->addItem(ItemStyleVisuals::borderStyleIcon(penStyle, 16),
                                        ItemStyleVisuals::borderStyleName(penStyle), static_cast<int>(penStyle));
        }
        const int borderStyleIndex = m_borderStyleCombo->findData(static_cast<int>(shape->strokeStyle()));
        if (borderStyleIndex >= 0) {
            m_borderStyleCombo->setCurrentIndex(borderStyleIndex);
        }

        m_strokeWidthSpin = new QSpinBox(this);
        m_strokeWidthSpin->setObjectName(QStringLiteral("ItemPropertiesStrokeWidthValue"));
        m_strokeWidthSpin->setRange(1, 64);
        m_strokeWidthSpin->setSingleStep(1);
        m_strokeWidthSpin->setSuffix(QStringLiteral(" px"));
        m_strokeWidthSpin->setValue(qRound(shape->strokeWidth()));
        m_initialStrokeWidth = shape->strokeWidth();
        m_strokeWidthSpin->setMinimumWidth(88);

        m_opacitySpin = new QSpinBox(this);
        m_opacitySpin->setObjectName(QStringLiteral("ItemPropertiesOpacityValue"));
        m_opacitySpin->setRange(0, 100);
        m_opacitySpin->setSuffix(QStringLiteral(" %"));
        m_opacitySpin->setValue(qRound(shape->opacity() * 100.0));
        m_initialOpacity = shape->opacity();
        m_initialOpacityPercent = m_opacitySpin->value();
        m_opacitySpin->setMinimumWidth(88);

        const ShapeType type = shape->shapeType();
        if (type == ShapeType::Sector || type == ShapeType::Arc) {
            m_initialStartAngle = shape->startAngle();
            m_initialSweepAngle = shape->sweepAngle();
            m_initialStoredShapeRadius = shape->shapeRadius();
            const int maximumRadius = qMax(1, qRound(shape->contentSize().width() / 2.0));
            m_initialShapeRadius =
                qBound(1,
                       qRound(m_initialStoredShapeRadius >= 0.0 ? m_initialStoredShapeRadius
                                                                : shape->contentSize().width() / 2.0),
                       maximumRadius);

            m_startAngleSpin = new QSpinBox(this);
            m_startAngleSpin->setObjectName(QStringLiteral("ItemPropertiesStartAngleValue"));
            m_startAngleSpin->setRange(-360, 360);
            m_startAngleSpin->setSingleStep(5);
            m_startAngleSpin->setSuffix(QStringLiteral("°"));
            m_startAngleSpin->setValue(qRound(m_initialStartAngle));
            m_startAngleSpin->setMinimumWidth(88);

            m_sweepAngleSpin = new QSpinBox(this);
            m_sweepAngleSpin->setObjectName(QStringLiteral("ItemPropertiesSweepAngleValue"));
            m_sweepAngleSpin->setRange(-360, 360);
            m_sweepAngleSpin->setSingleStep(5);
            m_sweepAngleSpin->setSuffix(QStringLiteral("°"));
            m_sweepAngleSpin->setValue(qRound(m_initialSweepAngle));
            m_sweepAngleSpin->setMinimumWidth(88);

            m_shapeRadiusSpin = new QSpinBox(this);
            m_shapeRadiusSpin->setObjectName(QStringLiteral("ItemPropertiesShapeRadiusValue"));
            m_shapeRadiusSpin->setRange(1, maximumRadius);
            m_shapeRadiusSpin->setSuffix(QStringLiteral(" px"));
            m_shapeRadiusSpin->setValue(m_initialShapeRadius);
            m_shapeRadiusSpin->setMinimumWidth(88);
            connect(m_shapeRadiusSpin, &QSpinBox::valueChanged, this,
                    [this](int value) { m_shapeRadiusEdited = value != m_initialShapeRadius; });
        }

        if (!shape->isCustomShape() && (type == ShapeType::Rectangle || type == ShapeType::RoundedRectangle)) {
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
                                                                   QStringLiteral("ItemPropertiesStrokeColorValue"),
                                                                   ItemPropertiesFields::ColorIconRole::Border);

        m_strokeWidthSpin = new QSpinBox(this);
        m_strokeWidthSpin->setObjectName(QStringLiteral("ItemPropertiesStrokeWidthValue"));
        m_strokeWidthSpin->setRange(1, 64);
        m_strokeWidthSpin->setSingleStep(1);
        m_strokeWidthSpin->setSuffix(QStringLiteral(" px"));
        m_strokeWidthSpin->setValue(qRound(stroke->strokeWidth()));
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
        form->addRow(ItemPropertiesDialog::tr("Border color"), m_strokeColorButton);
    }
    if (m_borderStyleCombo) {
        form->addRow(ItemPropertiesDialog::tr("Border style"), m_borderStyleCombo);
    }
    if (m_strokeWidthSpin) {
        form->addRow(ItemPropertiesDialog::tr("Border width"), m_strokeWidthSpin);
    }
    if (m_opacitySpin) {
        form->addRow(ItemPropertiesDialog::tr("Opacity"), m_opacitySpin);
    }
    if (m_cornerRadiusSpin) {
        form->addRow(ItemPropertiesDialog::tr("Corner radius"), m_cornerRadiusSpin);
    }
    if (m_startAngleSpin) {
        form->addRow(ItemPropertiesDialog::tr("Start angle"), m_startAngleSpin);
    }
    if (m_sweepAngleSpin) {
        form->addRow(ItemPropertiesDialog::tr("Sweep angle"), m_sweepAngleSpin);
    }
    if (m_shapeRadiusSpin) {
        form->addRow(ItemPropertiesDialog::tr("Shape radius"), m_shapeRadiusSpin);
    }
}

void ItemPropertiesDialogStyleBlock::setContentSize(const QSizeF& size)
{
    m_contentSize = size;
    if (m_cornerRadiusSpin) {
        const int maximum = qMax(0, qRound(qMin(size.width(), size.height()) / 2.0));
        const bool blocked = m_cornerRadiusSpin->blockSignals(true);
        m_cornerRadiusSpin->setMaximum(maximum);
        if (!m_cornerEdited && m_initialStoredCornerRadius < 0.0) {
            m_initialCornerRadius = qRound(qMin(size.width(), size.height()) * m_defaultCornerRatio);
            m_cornerRadiusSpin->setValue(m_initialCornerRadius);
        }
        m_cornerRadiusSpin->blockSignals(blocked);
    }
    if (m_shapeRadiusSpin) {
        const int maximum = qMax(1, qRound(size.width() / 2.0));
        const bool blocked = m_shapeRadiusSpin->blockSignals(true);
        m_shapeRadiusSpin->setMaximum(maximum);
        if (!m_shapeRadiusEdited && m_initialStoredShapeRadius < 0.0) {
            m_initialShapeRadius = maximum;
            m_shapeRadiusSpin->setValue(m_initialShapeRadius);
        }
        m_shapeRadiusSpin->blockSignals(blocked);
    }
}
