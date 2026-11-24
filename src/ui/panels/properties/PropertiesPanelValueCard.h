#pragma once

#include "items/shapes/ShapeAbstractItem.h"

#include <QList>
#include <QWidget>

class CanvasItem;
class LineAbstractItem;
class PairValueRow;
class QComboBox;
class QSpinBox;

// 编辑面板的透明度、半轴、起止角、内角、圆角、正弦参数与箭头各行：成对属性
// 合成一行（逗号分隔两个输入框），数值框跟随选中状态启用并回显，改动交回
// 面板走撤销命令。
class PropertiesPanelValueCard : public QWidget {
    Q_OBJECT

  public:
    explicit PropertiesPanelValueCard(QWidget* parent = nullptr);

    // 需求 9b：曲线也要显示起点 / 终点箭头行，因此 refresh 同时接收选中的曲线。
    void Refresh(const QList<ShapeAbstractItem*>& shapes, const QList<LineAbstractItem*>& strokes,
                 const QList<CanvasItem*>& items);
    // 圆角行是否可见：只有矩形类图元才显示圆角编辑。
    void SetCornerVisible(bool visible);
    // 半轴行（长半轴, 短半轴）是否可见：椭圆 / 扇形 / 弧线。
    void SetAxisVisible(bool visible);
    // 起止角行（起始角, 终止角）是否可见：扇形与弧线族。
    void SetArcVisible(bool visible);
    // 内角行（内角 A, 内角 B）是否可见：三角形与平行四边形族。
    void SetInteriorAngleVisible(bool visible);
    // 正弦波形：周期数与振幅两行是否可见（只对正弦曲线开放）。
    void SetSineVisible(bool visible);
    // 直线（含直线箭头）的两端箭头行是否可见。
    void SetArrowVisible(bool visible);

  signals:
    void opacityChosen(int percent);
    void cornerRadiusChosen(int radius);
    // 半轴：长半轴不小于短半轴，越界时行内先钳再发信号。
    void axesChosen(qreal majorAxis, qreal minorAxis);
    // 起止角：起始角, 扫过角；写入侧两格一起落到 START_ANGLE / SWEEP_ANGLE。
    void arcAnglesChosen(qreal startAngle, qreal sweepAngle);
    // 内角：三角形两个底角都上报；平行四边形把内角 B 当 A 的补角，只上报换算后的 A。
    void interiorAngleChosen(qreal angleA);
    void interiorAngleBChosen(qreal angleB);
    // 正弦波形：周期数（1–12）、振幅（相对中心线的像素高度，1–200）。
    void sineCyclesChosen(int cycles);
    void sineAmplitudeChosen(int percent);
    // 直线两端箭头样式（ArrowStyle 的整数值）。
    void arrowStartChosen(int style);
    void arrowEndChosen(int style);

  private:
    // 半轴一行里改任一根：另一根跟着钳到同值并回显，再交给面板写撤销命令。
    void ApplyAxes(qreal majorAxis, qreal minorAxis);

    bool m_refreshing = false;
    QSpinBox* m_opacitySpin = nullptr;
    QSpinBox* m_cornerSpin = nullptr;
    QWidget* m_cornerSection = nullptr;
    PairValueRow* m_axisRow = nullptr;
    QWidget* m_arcSection = nullptr;
    QSpinBox* m_arcStartSpin = nullptr;
    QSpinBox* m_arcSweepSpin = nullptr;
    PairValueRow* m_interiorAngleRow = nullptr;
    // 内角回显基准：判断用户改的是哪一个框，以及内角 B 是否为补角。
    qreal m_interiorAngleA = 60.0;
    qreal m_interiorAngleB = 60.0;
    bool m_interiorSupplement = false;
    QWidget* m_sineSection = nullptr;
    QSpinBox* m_sineCyclesSpin = nullptr;
    QSpinBox* m_sineAmplitudeSpin = nullptr;
    QWidget* m_arrowSection = nullptr;
    QComboBox* m_startArrowCombo = nullptr;
    QComboBox* m_endArrowCombo = nullptr;
};
