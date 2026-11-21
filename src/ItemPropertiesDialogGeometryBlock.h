#pragma once

#include "CanvasItem.h"

#include <QPointer>
#include <QSizeF>
#include <QWidget>

class QDoubleSpinBox;
class QFormLayout;
class NumberAttributeRow;
class PairAttributeRow;

// 属性窗的几何区块：位置、内容尺寸与旋转角度三组数值框；椭圆、扇形与弧线
// 额外提供长半轴、短半轴。椭圆 / 扇形与内容宽高双向同步；弧线按真实椭圆
// 外接框读写，提交时并入属性窗的同一份快照撤销记录。
class ItemPropertiesDialogGeometryBlock : public QWidget {
    Q_OBJECT

  public:
    ItemPropertiesDialogGeometryBlock(CanvasItem* item, QWidget* parent = nullptr);

    void AddRows(QFormLayout* form);

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

    void SetNaturalSize(const QSizeF& size);

  private:
    void SyncAxesFromSize();
    void ApplyArcAxes(bool majorChanged);
    void ApplyAxesToSize(bool majorChanged);

    PairAttributeRow* m_positionRow = nullptr;
    PairAttributeRow* m_sizeRow = nullptr;
    NumberAttributeRow* m_rotationRow = nullptr;
    NumberAttributeRow* m_majorAxisRow = nullptr;
    NumberAttributeRow* m_minorAxisRow = nullptr;
    QDoubleSpinBox* m_positionXSpin = nullptr;
    QDoubleSpinBox* m_positionYSpin = nullptr;
    QDoubleSpinBox* m_widthSpin = nullptr;
    QDoubleSpinBox* m_heightSizeSpin = nullptr;
    QDoubleSpinBox* m_majorAxisSpin = nullptr;
    QDoubleSpinBox* m_minorAxisSpin = nullptr;
    QDoubleSpinBox* m_rotationSpin = nullptr;
    // 半轴行归属的图元：弧线按属性回显真实椭圆半轴。
    CanvasItem* m_axisItem = nullptr;
    bool m_arcAxes = false;
    qreal m_initialArcMajor = 0.0;
    qreal m_initialArcMinor = 0.0;
    bool m_syncingAxes = false;
};
