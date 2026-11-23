#include "ui/panels/properties/PropertiesPanelFillBorderCard.h"

#include "ui/widgets/AttributeRowWidgets.h"

PropertiesPanelFillBorderCard::PropertiesPanelFillBorderCard(QWidget* parent) : QWidget(parent)
{
    m_fillStyleRow = new FillStyleRow(this);
    m_borderStyleRow = new StrokeStyleRow(this);
    m_borderWidthRow = new StrokeWidthRow(this);

    connect(m_fillStyleRow, &FillStyleRow::fillStyleChosen, this, &PropertiesPanelFillBorderCard::fillStyleChosen);
    connect(m_borderStyleRow, &StrokeStyleRow::borderStyleChosen, this,
            &PropertiesPanelFillBorderCard::borderStyleChosen);
    connect(m_borderWidthRow, &StrokeWidthRow::borderWidthChosen, this,
            &PropertiesPanelFillBorderCard::borderWidthChosen);
}

void PropertiesPanelFillBorderCard::Refresh(const QList<FillAttributes*>& fills,
                                            const QList<StrokeAttributes*>& strokes)
{
    m_fillStyleRow->SetTargets(fills);
    m_borderStyleRow->SetTargets(strokes);
    m_borderWidthRow->SetTargets(strokes);
}

QWidget* PropertiesPanelFillBorderCard::FillStyleWidget() const
{
    return m_fillStyleRow;
}

QWidget* PropertiesPanelFillBorderCard::BorderStyleWidget() const
{
    return m_borderStyleRow;
}

QWidget* PropertiesPanelFillBorderCard::BorderWidthRow() const
{
    return m_borderWidthRow;
}
