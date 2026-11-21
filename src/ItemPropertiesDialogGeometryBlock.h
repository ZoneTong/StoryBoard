#pragma once

#include "CanvasItem.h"

#include <QPointer>
#include <QSizeF>
#include <QWidget>

class QDoubleSpinBox;
class QFormLayout;

// 属性窗的几何区块：位置、内容尺寸与旋转角度三组数值框；椭圆、扇形与弧线
// 额外提供长半轴、短半轴，两者与内容宽高双向同步，并保持图元中心不动。
// 文字正文或图片替换后由外壳把自然尺寸写回尺寸框，确定时随其它属性一并提交撤销记录。
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
    QDoubleSpinBox* RotationSpin() const { return m_rotationSpin; }

    void SetNaturalSize(const QSizeF& size);

  private:
    void SyncAxesFromSize();
    void ApplyAxesToSize(bool majorChanged);

    QDoubleSpinBox* m_positionXSpin = nullptr;
    QDoubleSpinBox* m_positionYSpin = nullptr;
    QDoubleSpinBox* m_widthSpin = nullptr;
    QDoubleSpinBox* m_heightSizeSpin = nullptr;
    QDoubleSpinBox* m_majorAxisSpin = nullptr;
    QDoubleSpinBox* m_minorAxisSpin = nullptr;
    QDoubleSpinBox* m_rotationSpin = nullptr;
    bool m_syncingAxes = false;
};
