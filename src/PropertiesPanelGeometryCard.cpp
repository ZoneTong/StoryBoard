#include "PropertiesPanelGeometryCard.h"

#include "CanvasItem.h"
#include "PropertiesPanel.h"
#include "PropertyRowWidgets.h"
#include "ShapeItem.h"
#include "ShapeMeta.h"

#include <QDoubleSpinBox>
#include <QSizeF>
#include <QVBoxLayout>

PropertiesPanelGeometryCard::PropertiesPanelGeometryCard(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_majorAxisRow = new NumberAttributeRow(IconId::SHAPES, true, 1, 0.5, 500000.0, QStringLiteral(" px"),
                                            QStringLiteral("ItemMajorAxisSpin"), this);
    m_majorAxisRow->Spin()->setToolTip(PropertiesPanel::tr("Semi-major axis"));
    m_majorAxisRow->setVisible(false);
    layout->addWidget(m_majorAxisRow);

    m_minorAxisRow = new NumberAttributeRow(IconId::SHAPES, true, 1, 0.5, 500000.0, QStringLiteral(" px"),
                                            QStringLiteral("ItemMinorAxisSpin"), this);
    m_minorAxisRow->Spin()->setToolTip(PropertiesPanel::tr("Semi-minor axis"));
    m_minorAxisRow->setVisible(false);
    layout->addWidget(m_minorAxisRow);

    // 行序与属性窗几何区一致：长短半轴 → 旋转 → 位置 → 尺寸。
    m_rotationRow = new NumberAttributeRow(IconId::ROTATE_CCW, true, 1, -360.0, 360.0, QStringLiteral("°"),
                                           QStringLiteral("ItemRotationSpin"), this);
    m_rotationRow->SetSingleStep(5.0);
    m_rotationRow->Spin()->setToolTip(PropertiesPanel::tr("Rotation"));
    layout->addWidget(m_rotationRow);

    m_positionRow = new PairAttributeRow(IconId::FIT, true, QStringLiteral("ItemPositionXSpin"),
                                         QStringLiteral("ItemPositionYSpin"), 1, -1000000.0, 1000000.0, QString(),
                                         QStringLiteral(","), this);
    m_positionRow->FirstSpin()->setToolTip(PropertiesPanel::tr("Position"));
    m_positionRow->SecondSpin()->setToolTip(PropertiesPanel::tr("Position"));
    layout->addWidget(m_positionRow);

    m_sizeRow = new PairAttributeRow(IconId::SHAPES, true, QStringLiteral("ItemSizeWidthSpin"),
                                     QStringLiteral("ItemSizeHeightSpin"), 1, 1.0, 1000000.0, QString(),
                                     QStringLiteral("×"), this);
    m_sizeRow->FirstSpin()->setToolTip(PropertiesPanel::tr("Item size"));
    m_sizeRow->SecondSpin()->setToolTip(PropertiesPanel::tr("Item size"));
    layout->addWidget(m_sizeRow);

    m_parallelogramRow = new NumberAttributeRow(IconId::ANGLE_RIGHT, true, 0, 15.0, 165.0, QStringLiteral("°"),
                                                QStringLiteral("ItemParallelogramAngleSpin"), this);
    m_parallelogramRow->Spin()->setToolTip(PropertiesPanel::tr("Parallelogram bottom-left angle"));
    m_parallelogramRow->setVisible(false);
    layout->addWidget(m_parallelogramRow);

    connect(m_positionRow, &PairAttributeRow::valuesChosen, this, [this](qreal x, qreal y) {
        if (!m_refreshing) {
            emit geometryChosen(QPointF(x, y), QSizeF(m_sizeRow->First(), m_sizeRow->Second()));
        }
    });
    connect(m_sizeRow, &PairAttributeRow::valuesChosen, this, [this](qreal width, qreal height) {
        if (!m_refreshing) {
            emit geometryChosen(QPointF(m_positionRow->First(), m_positionRow->Second()), QSizeF(width, height));
        }
    });
    connect(m_rotationRow, &NumberAttributeRow::valueChosen, this, [this](qreal angle) {
        if (!m_refreshing) {
            emit rotationChosen(angle);
        }
    });
    connect(m_majorAxisRow, &NumberAttributeRow::valueChosen, this, [this](qreal) { ApplyAxes(true); });
    connect(m_minorAxisRow, &NumberAttributeRow::valueChosen, this, [this](qreal) { ApplyAxes(false); });
    connect(m_parallelogramRow, &NumberAttributeRow::valueChosen, this, [this](qreal angle) {
        if (!m_refreshing) {
            emit parallelogramAngleChosen(angle);
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

    const QSizeF size = item->ContentSize();
    m_majorAxisRow->Refresh(qMax(size.width(), size.height()) / 2.0);
    m_minorAxisRow->Refresh(qMin(size.width(), size.height()) / 2.0);

    qreal angle = 60.0;
    if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
        if (shape->Properties().Contains(ItemProperty::PARALLELOGRAM_ANGLE)) {
            angle = shape->ParallelogramAngle();
        }
    }
    m_parallelogramRow->Refresh(angle);
    m_refreshing = false;
}

void PropertiesPanelGeometryCard::ApplyAxes(bool majorChanged)
{
    if (m_refreshing || !m_item) {
        return;
    }

    m_refreshing = true;
    qreal major = m_majorAxisRow->Value();
    qreal minor = m_minorAxisRow->Value();
    if (major < minor) {
        if (majorChanged) {
            minor = major;
            m_minorAxisRow->Refresh(minor);
        } else {
            major = minor;
            m_majorAxisRow->Refresh(major);
        }
    }

    const qreal oldWidth = m_sizeRow->First();
    const qreal oldHeight = m_sizeRow->Second();
    const bool widthIsMajor = oldWidth >= oldHeight;
    const qreal newWidth = widthIsMajor ? major * 2.0 : minor * 2.0;
    const qreal newHeight = widthIsMajor ? minor * 2.0 : major * 2.0;
    const QPointF oldPosition(m_positionRow->First(), m_positionRow->Second());
    const QPointF center(oldPosition.x() + oldWidth / 2.0, oldPosition.y() + oldHeight / 2.0);
    const QPointF newPosition(center.x() - newWidth / 2.0, center.y() - newHeight / 2.0);

    m_positionRow->Refresh(newPosition.x(), newPosition.y());
    m_sizeRow->Refresh(newWidth, newHeight);
    m_refreshing = false;
    emit geometryChosen(newPosition, QSizeF(newWidth, newHeight));
}

void PropertiesPanelGeometryCard::SetAxisVisible(bool visible)
{
    m_majorAxisRow->setVisible(visible);
    m_minorAxisRow->setVisible(visible);
}

void PropertiesPanelGeometryCard::SetParallelogramVisible(bool visible)
{
    m_parallelogramRow->setVisible(visible);
}
