#include "ui/panels/properties/PropertiesPanelGeometryCard.h"

#include "items/CanvasItem.h"
#include "items/geometry/ShapeMeta.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "ui/panels/properties/PropertiesPanel.h"
#include "ui/widgets/PropertyRowWidgets.h"

#include <QDoubleSpinBox>
#include <QSizeF>
#include <QVBoxLayout>

PropertiesPanelGeometryCard::PropertiesPanelGeometryCard(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_majorAxisRow = new NumberAttributeRow(IconId::AXIS_MAJOR, true, 1, 0.5, 500000.0, QStringLiteral(" px"),
                                            QStringLiteral("ItemMajorAxisSpin"), this);
    m_majorAxisRow->Spin()->setToolTip(PropertiesPanel::tr("Semi-major axis"));
    m_majorAxisRow->SetIconToolTip(PropertiesPanel::tr("Semi-major axis"));
    m_majorAxisRow->setVisible(false);
    layout->addWidget(m_majorAxisRow);

    m_minorAxisRow = new NumberAttributeRow(IconId::AXIS_MINOR, true, 1, 0.5, 500000.0, QStringLiteral(" px"),
                                            QStringLiteral("ItemMinorAxisSpin"), this);
    m_minorAxisRow->Spin()->setToolTip(PropertiesPanel::tr("Semi-minor axis"));
    m_minorAxisRow->SetIconToolTip(PropertiesPanel::tr("Semi-minor axis"));
    m_minorAxisRow->setVisible(false);
    layout->addWidget(m_minorAxisRow);

    // 行序与属性窗几何区一致：长短半轴 → 旋转 → 位置 → 尺寸。
    m_rotationRow = new NumberAttributeRow(IconId::ROTATE_CCW, true, 1, -360.0, 360.0, QStringLiteral("°"),
                                           QStringLiteral("ItemRotationSpin"), this);
    m_rotationRow->SetSingleStep(5.0);
    m_rotationRow->Spin()->setToolTip(PropertiesPanel::tr("Rotation"));
    m_rotationRow->SetIconToolTip(PropertiesPanel::tr("Rotation"));
    layout->addWidget(m_rotationRow);

    m_positionRow = new PairAttributeRow(IconId::FIT, true, QStringLiteral("ItemPositionXSpin"),
                                         QStringLiteral("ItemPositionYSpin"), 1, -1000000.0, 1000000.0, QString(),
                                         QStringLiteral(","), this);
    m_positionRow->FirstSpin()->setToolTip(PropertiesPanel::tr("Position"));
    m_positionRow->SecondSpin()->setToolTip(PropertiesPanel::tr("Position"));
    m_positionRow->SetIconToolTip(PropertiesPanel::tr("Position"));
    layout->addWidget(m_positionRow);

    m_sizeRow = new PairAttributeRow(IconId::SHAPES, true, QStringLiteral("ItemSizeWidthSpin"),
                                     QStringLiteral("ItemSizeHeightSpin"), 1, 1.0, 1000000.0, QString(),
                                     QStringLiteral("×"), this);
    m_sizeRow->FirstSpin()->setToolTip(PropertiesPanel::tr("Item size"));
    m_sizeRow->SecondSpin()->setToolTip(PropertiesPanel::tr("Item size"));
    m_sizeRow->SetIconToolTip(PropertiesPanel::tr("Item size"));
    layout->addWidget(m_sizeRow);

    m_interiorAngleRow = new NumberAttributeRow(IconId::ANGLE_RIGHT, true, 0, 5.0, 175.0, QStringLiteral("°"),
                                                QStringLiteral("ItemInteriorAngleSpin"), this);
    m_interiorAngleRow->Spin()->setToolTip(PropertiesPanel::tr("Interior angle A"));
    m_interiorAngleRow->SetIconToolTip(PropertiesPanel::tr("Interior angle A"));
    m_interiorAngleRow->setVisible(false);
    layout->addWidget(m_interiorAngleRow);

    // §3.4 中心点：派生属性，行放在几何区最后；多选时绝对对齐到同一中心。
    m_centerRow =
        new PairAttributeRow(IconId::FIT, true, QStringLiteral("ItemCenterXSpin"), QStringLiteral("ItemCenterYSpin"), 1,
                             -1000000.0, 1000000.0, QString(), QStringLiteral(","), this);
    m_centerRow->FirstSpin()->setToolTip(PropertiesPanel::tr("Center point"));
    m_centerRow->SecondSpin()->setToolTip(PropertiesPanel::tr("Center point"));
    m_centerRow->SetIconToolTip(PropertiesPanel::tr("Center point"));
    m_centerRow->setVisible(false);
    layout->addWidget(m_centerRow);

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
    connect(m_interiorAngleRow, &NumberAttributeRow::valueChosen, this, [this](qreal angle) {
        if (!m_refreshing) {
            emit interiorAngleChosen(angle);
        }
    });
    connect(m_centerRow, &PairAttributeRow::valuesChosen, this, [this](qreal x, qreal y) {
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

    // 长短半轴按属性回显：椭圆 / 扇形由内容尺寸派生，弧线按真实椭圆外接框。
    m_majorAxisRow->Refresh(item->PropertyValue(ItemProperty::MAJOR_AXIS).toDouble());
    m_minorAxisRow->Refresh(item->PropertyValue(ItemProperty::MINOR_AXIS).toDouble());

    qreal angle = 60.0;
    if (auto* shape = dynamic_cast<ShapeAbstractItem*>(item)) {
        if (shape->Properties().Contains(ItemProperty::PARALLELOGRAM_ANGLE)) {
            // 内角 A：普通平行四边形为字段值，菱形为派生量，统一走属性读取。
            angle = shape->PropertyValue(ItemProperty::PARALLELOGRAM_ANGLE).toDouble();
        }
    }
    m_interiorAngleRow->Refresh(angle);

    // 中心点是派生属性：按图元几何回显，写入侧只平移位置。
    const QPointF center = item->PropertyValue(ItemProperty::CENTER).toPointF();
    m_centerRow->Refresh(center.x(), center.y());
    m_refreshing = false;
}

void PropertiesPanelGeometryCard::ApplyAxes(bool majorChanged)
{
    if (m_refreshing || !m_item) {
        return;
    }

    // 长半轴不得小于短半轴：改哪根就把另一根钳到同值并回显。
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
    m_refreshing = false;
    // 椭圆 / 扇形与弧线共用一条属性写入路径：MAJOR_AXIS / MINOR_AXIS
    // 由各图元自行换算（尺寸派生或按真实椭圆框重采样）。
    emit axesChosen(major, minor);
}

void PropertiesPanelGeometryCard::SetAxisVisible(bool visible)
{
    m_majorAxisRow->setVisible(visible);
    m_minorAxisRow->setVisible(visible);
}

void PropertiesPanelGeometryCard::SetInteriorAngleVisible(bool visible)
{
    m_interiorAngleRow->setVisible(visible);
}

void PropertiesPanelGeometryCard::SetCenterVisible(bool visible)
{
    m_centerRow->setVisible(visible);
}
