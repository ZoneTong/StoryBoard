#pragma once

#include "items/CanvasItem.h"

#include <QPointF>
#include <QPointer>
#include <QSizeF>
#include <QWidget>

class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class NumberValueRow;
class PairValueRow;
class PropertyRowRegistry;

// 属性窗的几何区块：长短半轴 → 旋转 → 位置 → 图元尺寸 → 中心点（§4.1 第 23~28 行）。
// 椭圆 / 扇形与内容宽高双向同步；弧线按真实椭圆外接框读写；中心点是派生属性，
// 回显取 PropertyValue(CENTER)，提交时折算到位置。
// 行显隐统一交给 PropertyRowRegistry。
class ItemPropertiesDialogGeometryBlock : public QWidget {
    Q_OBJECT

  public:
    explicit ItemPropertiesDialogGeometryBlock(QWidget* parent = nullptr);

    void AddRows(QFormLayout* form, PropertyRowRegistry* rows);

    void SetItem(CanvasItem* item);

    QDoubleSpinBox* PositionXSpin() const { return m_positionXSpin; }
    QDoubleSpinBox* PositionYSpin() const { return m_positionYSpin; }
    QDoubleSpinBox* WidthSpin() const { return m_widthSpin; }
    QDoubleSpinBox* HeightSizeSpin() const { return m_heightSizeSpin; }
    QDoubleSpinBox* MajorAxisSpin() const { return m_majorAxisSpin; }
    QDoubleSpinBox* MinorAxisSpin() const { return m_minorAxisSpin; }
    // 弧线半轴的初始值：提交时判断“未改动即不写入快照”。
    qreal InitialArcMajor() const { return m_initialArcMajor; }
    qreal InitialArcMinor() const { return m_initialArcMinor; }
    QDoubleSpinBox* RotationSpin() const { return m_rotationSpin; }
    QDoubleSpinBox* CenterXSpin() const { return m_centerXSpin; }
    QDoubleSpinBox* CenterYSpin() const { return m_centerYSpin; }
    QPointF InitialCenter() const { return m_initialCenter; }

    void SetNaturalSize(const QSizeF& size);

  private:
    void SyncAxesFromSize();
    void ApplyArcAxes(bool majorChanged);
    void ApplyAxesToSize(bool majorChanged);

    QFormLayout* m_form = nullptr;
    PairValueRow* m_positionRow = nullptr;
    PairValueRow* m_sizeRow = nullptr;
    NumberValueRow* m_rotationRow = nullptr;
    NumberValueRow* m_majorAxisRow = nullptr;
    NumberValueRow* m_minorAxisRow = nullptr;
    PairValueRow* m_centerRow = nullptr;
    QLabel* m_centerLabel = nullptr;
    QDoubleSpinBox* m_positionXSpin = nullptr;
    QDoubleSpinBox* m_positionYSpin = nullptr;
    QDoubleSpinBox* m_widthSpin = nullptr;
    QDoubleSpinBox* m_heightSizeSpin = nullptr;
    QDoubleSpinBox* m_majorAxisSpin = nullptr;
    QDoubleSpinBox* m_minorAxisSpin = nullptr;
    QDoubleSpinBox* m_rotationSpin = nullptr;
    QDoubleSpinBox* m_centerXSpin = nullptr;
    QDoubleSpinBox* m_centerYSpin = nullptr;
    QPointF m_initialCenter;
    // 半轴行归属的图元：弧线按属性回显真实椭圆半轴。
    QPointer<CanvasItem> m_axisItem;
    bool m_hasAxes = false;
    bool m_arcAxes = false;
    qreal m_initialArcMajor = 0.0;
    qreal m_initialArcMinor = 0.0;
    bool m_syncingAxes = false;
};
