#include "ui/panels/properties/PropertiesPanelGeometryCard.h"

#include "items/CanvasItem.h"
#include "ui/panels/properties/PropertiesPanel.h"
#include "ui/widgets/rows/ValueRows.h"

#include <QDoubleSpinBox>
#include <QSizeF>
#include <QVBoxLayout>

PropertiesPanelGeometryCard::PropertiesPanelGeometryCard(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    // 行序：旋转 → 位置 → 尺寸 → 中心；半轴 / 起止角 / 内角等成对属性行已在数值卡片里。
    m_rotationRow = new NumberValueRow(IconId::ROTATE_OBJECT, true, ItemProperty::ROTATION,
                                       QStringLiteral("ItemRotationSpin"), this);
    m_rotationRow->Spin()->setToolTip(PropertiesPanel::tr("Rotation"));
    m_rotationRow->SetIconToolTip(PropertiesPanel::tr("Rotation"));
    layout->addWidget(m_rotationRow);

    m_positionRow =
        new PairValueRow(IconId::POSITION, true, QStringLiteral("ItemPositionXSpin"),
                         QStringLiteral("ItemPositionYSpin"), ItemProperty::POSITION, QStringLiteral(","), this);
    m_positionRow->FirstSpin()->setToolTip(PropertiesPanel::tr("Position"));
    m_positionRow->SecondSpin()->setToolTip(PropertiesPanel::tr("Position"));
    m_positionRow->SetIconToolTip(PropertiesPanel::tr("Position"));
    layout->addWidget(m_positionRow);

    m_sizeRow = new PairValueRow(IconId::SIZE, true, QStringLiteral("ItemSizeWidthSpin"),
                                 QStringLiteral("ItemSizeHeightSpin"), ItemProperty::SIZE, QStringLiteral("×"), this);
    m_sizeRow->FirstSpin()->setToolTip(PropertiesPanel::tr("Item size"));
    m_sizeRow->SecondSpin()->setToolTip(PropertiesPanel::tr("Item size"));
    m_sizeRow->SetIconToolTip(PropertiesPanel::tr("Item size"));
    layout->addWidget(m_sizeRow);

    // §3.4 中心点：派生属性，行放在几何区最后；多选时绝对对齐到同一中心。
    m_centerRow = new PairValueRow(IconId::CENTER, true, QStringLiteral("ItemCenterXSpin"),
                                   QStringLiteral("ItemCenterYSpin"), ItemProperty::CENTER, QStringLiteral(","), this);
    m_centerRow->FirstSpin()->setToolTip(PropertiesPanel::tr("Center point"));
    m_centerRow->SecondSpin()->setToolTip(PropertiesPanel::tr("Center point"));
    m_centerRow->SetIconToolTip(PropertiesPanel::tr("Center point"));
    m_centerRow->setVisible(false);
    layout->addWidget(m_centerRow);

    connect(m_positionRow, &PairValueRow::valuesChosen, this, [this](qreal x, qreal y) {
        if (!m_refreshing) {
            emit geometryChosen(QPointF(x, y), QSizeF(m_sizeRow->First(), m_sizeRow->Second()));
        }
    });
    connect(m_sizeRow, &PairValueRow::valuesChosen, this, [this](qreal width, qreal height) {
        if (!m_refreshing) {
            emit geometryChosen(QPointF(m_positionRow->First(), m_positionRow->Second()), QSizeF(width, height));
        }
    });
    connect(m_rotationRow, &NumberValueRow::valueChosen, this, [this](qreal angle) {
        if (!m_refreshing) {
            emit rotationChosen(angle);
        }
    });
    connect(m_centerRow, &PairValueRow::valuesChosen, this, [this](qreal x, qreal y) {
        if (!m_refreshing) {
            emit centerChosen(QPointF(x, y));
        }
    });
}

void PropertiesPanelGeometryCard::Refresh(CanvasItem* item)
{
    m_item = item;
    setEnabled(item != nullptr);
    if (!item) {
        return;
    }

    m_refreshing = true;
    m_positionRow->Refresh(item->pos().x(), item->pos().y());
    m_sizeRow->Refresh(item->ContentSize().width(), item->ContentSize().height());
    m_rotationRow->Refresh(item->RotationAngle());

    // 中心点是派生属性：按图元几何回显，写入侧只平移位置；提示文案与属性窗一致。
    const QPointF center = item->PropertyValue(ItemProperty::CENTER).toPointF();
    m_centerRow->Refresh(center.x(), center.y());
    const QString centerTip = PropertiesPanel::tr("Center point");
    m_centerRow->FirstSpin()->setToolTip(centerTip);
    m_centerRow->SecondSpin()->setToolTip(centerTip);
    m_centerRow->SetIconToolTip(centerTip);
    m_refreshing = false;
}

void PropertiesPanelGeometryCard::SetCenterVisible(bool visible)
{
    m_centerRow->setVisible(visible);
}
