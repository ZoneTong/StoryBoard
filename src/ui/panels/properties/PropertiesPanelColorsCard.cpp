#include "ui/panels/properties/PropertiesPanelColorsCard.h"

#include "items/properties/FillAttributes.h"
#include "items/properties/StrokeAttributes.h"
#include "ui/widgets/rows/FillStrokeRows.h"

PropertiesPanelColorsCard::PropertiesPanelColorsCard(QWidget* parent) : QWidget(parent)
{
    m_borderColorRow = new StrokeColorRow(this);
    m_fillColorRow = new FillColorRow(this);
    connect(m_borderColorRow, &StrokeColorRow::strokeColorChosen, this, &PropertiesPanelColorsCard::strokeColorChosen);
    connect(m_fillColorRow, &FillColorRow::fillColorChosen, this, &PropertiesPanelColorsCard::fillColorChosen);
}

void PropertiesPanelColorsCard::Refresh(const QList<FillAttributes*>& fills, const QList<StrokeAttributes*>& strokes)
{
    m_fillColorRow->SetTargets(fills);
    m_borderColorRow->SetTargets(strokes);
}

QWidget* PropertiesPanelColorsCard::FillColorWidget() const
{
    return m_fillColorRow;
}

QWidget* PropertiesPanelColorsCard::BorderColorRow() const
{
    return m_borderColorRow;
}
