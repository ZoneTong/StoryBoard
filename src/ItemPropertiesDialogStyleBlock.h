#pragma once

#include "CanvasItem.h"

#include <QPointer>
#include <QSizeF>
#include <QWidget>

class QComboBox;
class QFormLayout;
class QSpinBox;
class QToolButton;

// 属性窗的填充 / 边框区块：图形图元提供填充样式、填充颜色、边框颜色、边框样式、
// 边框宽度、透明度；矩形 / 圆角矩形额外提供圆角半径，扇形 / 弧线额外提供
// 起始角、扫过角与半径；笔画图元只提供边框颜色与宽度。控件初始值取自图元，
// 快照值供确定时提交撤销记录。
class ItemPropertiesDialogStyleBlock : public QWidget {
    Q_OBJECT

  public:
    ItemPropertiesDialogStyleBlock(CanvasItem* item, QWidget* parent = nullptr);

    void addRows(QFormLayout* form);

    bool hasFill() const { return m_hasFill; }
    qreal initialStrokeWidth() const { return m_initialStrokeWidth; }
    QComboBox* fillStyleCombo() const { return m_fillStyleCombo; }
    QToolButton* fillColorButton() const { return m_fillColorButton; }
    QToolButton* strokeColorButton() const { return m_strokeColorButton; }
    QComboBox* borderStyleCombo() const { return m_borderStyleCombo; }
    QSpinBox* strokeWidthSpin() const { return m_strokeWidthSpin; }
    QSpinBox* opacitySpin() const { return m_opacitySpin; }
    qreal initialOpacity() const { return m_initialOpacity; }
    int initialOpacityPercent() const { return m_initialOpacityPercent; }

    QSpinBox* startAngleSpin() const { return m_startAngleSpin; }
    QSpinBox* sweepAngleSpin() const { return m_sweepAngleSpin; }
    QSpinBox* shapeRadiusSpin() const { return m_shapeRadiusSpin; }
    qreal initialStartAngle() const { return m_initialStartAngle; }
    qreal initialSweepAngle() const { return m_initialSweepAngle; }
    int initialShapeRadius() const { return m_initialShapeRadius; }
    qreal initialStoredShapeRadius() const { return m_initialStoredShapeRadius; }

    bool hasCornerRadius() const { return m_cornerRadiusSpin != nullptr; }
    QSpinBox* cornerRadiusSpin() const { return m_cornerRadiusSpin; }
    int initialCornerRadius() const { return m_initialCornerRadius; }
    qreal initialStoredCornerRadius() const { return m_initialStoredCornerRadius; }

    // 三角形：底边由几何区的宽度表示，这里只放两个底角；角度优先，高度随后派生。
    QSpinBox* bottomAngle1Spin() const { return m_bottomAngle1Spin; }
    QSpinBox* bottomAngle2Spin() const { return m_bottomAngle2Spin; }
    qreal initialBottomAngle1() const { return m_initialBottomAngle1; }
    qreal initialBottomAngle2() const { return m_initialBottomAngle2; }

    // 尺寸框变化时同步圆角上限；用户尚未改过圆角时，圆角矩形的默认值
    // 按新短边重新计算，保证对话框显示值与最终轮廓一致。
    void setContentSize(const QSizeF& size);

  private:
    bool m_hasFill = false;
    qreal m_initialStrokeWidth = 4.0;
    QComboBox* m_fillStyleCombo = nullptr;
    QToolButton* m_fillColorButton = nullptr;
    QToolButton* m_strokeColorButton = nullptr;
    QComboBox* m_borderStyleCombo = nullptr;
    QSpinBox* m_strokeWidthSpin = nullptr;
    QSpinBox* m_opacitySpin = nullptr;
    qreal m_initialOpacity = 1.0;
    int m_initialOpacityPercent = 100;

    QSpinBox* m_startAngleSpin = nullptr;
    QSpinBox* m_sweepAngleSpin = nullptr;
    QSpinBox* m_shapeRadiusSpin = nullptr;
    qreal m_initialStartAngle = 0.0;
    qreal m_initialSweepAngle = 0.0;
    int m_initialShapeRadius = 0;
    qreal m_initialStoredShapeRadius = -1.0;
    bool m_shapeRadiusEdited = false;

    qreal m_defaultCornerRatio = 0.0;
    QSizeF m_contentSize;
    QSpinBox* m_cornerRadiusSpin = nullptr;
    int m_initialCornerRadius = 0;
    qreal m_initialStoredCornerRadius = -1.0;
    bool m_cornerEdited = false;

    QSpinBox* m_bottomAngle1Spin = nullptr;
    QSpinBox* m_bottomAngle2Spin = nullptr;
    qreal m_initialBottomAngle1 = 60.0;
    qreal m_initialBottomAngle2 = 60.0;
};
