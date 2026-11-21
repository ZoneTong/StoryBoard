#pragma once

#include "ShapeItem.h"

#include <QList>
#include <QWidget>

class CanvasItem;
class LineItemAbstract;
class QComboBox;
class QSpinBox;

// 编辑面板的透明度、圆角半径、扇形 / 弧线扫过角与半径各行：圆角行只在矩形类
// 图元上出现，角度与半径行只在扇形 / 弧线上出现；数值框跟随选中状态启用并回显，
// 改动交回面板走撤销命令。
class PropertiesPanelValueCard : public QWidget {
    Q_OBJECT

  public:
    explicit PropertiesPanelValueCard(QWidget* parent = nullptr);

    // 需求 9b：曲线也要显示起点 / 终点箭头行，因此 refresh 同时接收选中的曲线。
    void Refresh(const QList<ShapeItem*>& shapes, const QList<LineItemAbstract*>& strokes,
                 const QList<CanvasItem*>& items);
    // 圆角行是否可见：只有矩形类图元才显示圆角编辑。
    void SetCornerVisible(bool visible);
    // 扫过角与半径两行是否可见：只有扇形 / 弧线才显示角度与半径编辑。
    void SetArcRadiusVisible(bool visible);
    // 正弦波形：周期数与振幅两行是否可见（只对正弦曲线开放）。
    void SetSineVisible(bool visible);
    // 三角形的底边 / 角 A / 角 B 三行是否可见。
    void SetTriangleVisible(bool visible);
    // 直线（含直线箭头）的两端箭头行是否可见。
    void SetArrowVisible(bool visible);

  signals:
    void opacityChosen(int percent);
    void cornerRadiusChosen(int radius);
    void startAngleChosen(int angle);
    void sweepAngleChosen(int angle);
    // 正弦波形：周期数（1–12）、振幅（相对中心线的像素高度，1–200）。
    void sineCyclesChosen(int cycles);
    void sineAmplitudeChosen(int percent);
    // 三角形：底边长度（px）、底角 1 / 底角 2（度）。
    void triangleAngle1Chosen(int degrees);
    void triangleAngle2Chosen(int degrees);
    // 直线两端箭头样式（ArrowStyle 的整数值）。
    void arrowStartChosen(int style);
    void arrowEndChosen(int style);

  private:
    bool m_refreshing = false;
    QSpinBox* m_opacitySpin = nullptr;
    QSpinBox* m_cornerSpin = nullptr;
    QWidget* m_cornerSection = nullptr;
    QSpinBox* m_startAngleSpin = nullptr;
    QWidget* m_startAngleSection = nullptr;
    QSpinBox* m_angleSpin = nullptr;
    QWidget* m_angleSection = nullptr;
    QWidget* m_sineSection = nullptr;
    QSpinBox* m_sineCyclesSpin = nullptr;
    QSpinBox* m_sineAmplitudeSpin = nullptr;
    QWidget* m_triangleSection = nullptr;
    QSpinBox* m_triangleAngle1Spin = nullptr;
    QSpinBox* m_triangleAngle2Spin = nullptr;
    QWidget* m_arrowSection = nullptr;
    QComboBox* m_startArrowCombo = nullptr;
    QComboBox* m_endArrowCombo = nullptr;
};
