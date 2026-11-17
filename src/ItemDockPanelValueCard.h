#pragma once

#include "ShapeItem.h"

#include <QList>
#include <QWidget>

class CanvasItem;
class QSpinBox;

// 编辑图元卡的透明度、圆角半径、扇形 / 弧线扫过角与半径各行：圆角行只在矩形类
// 图元上出现，角度与半径行只在扇形 / 弧线上出现；数值框跟随选中状态启用并回显，
// 改动交回面板走撤销命令。
class ItemDockPanelValueCard : public QWidget {
    Q_OBJECT

  public:
    explicit ItemDockPanelValueCard(QWidget* parent = nullptr);

    void refresh(const QList<ShapeItem*>& shapes, const QList<CanvasItem*>& items);
    // 圆角行是否可见：只有矩形类图元才显示圆角编辑。
    void setCornerVisible(bool visible);
    // 扫过角与半径两行是否可见：只有扇形 / 弧线才显示角度与半径编辑。
    void setArcRadiusVisible(bool visible);
    // 三角形的底边 / 角 A / 角 B 三行是否可见。
    void setTriangleVisible(bool visible);

  signals:
    void opacityChosen(int percent);
    void cornerRadiusChosen(int radius);
    void sweepAngleChosen(int angle);
    void shapeRadiusChosen(int radius);
    // 三角形：底边长度（px）、底角 1 / 底角 2（度）。
    void triangleBaseChosen(int base);
    void triangleAngle1Chosen(int degrees);
    void triangleAngle2Chosen(int degrees);

  private:
    bool m_refreshing = false;
    QSpinBox* m_opacitySpin = nullptr;
    QSpinBox* m_cornerSpin = nullptr;
    QWidget* m_cornerSection = nullptr;
    QSpinBox* m_angleSpin = nullptr;
    QSpinBox* m_radiusSpin = nullptr;
    QWidget* m_angleSection = nullptr;
    QWidget* m_radiusSection = nullptr;
    QWidget* m_triangleSection = nullptr;
    QSpinBox* m_triangleBaseSpin = nullptr;
    QSpinBox* m_triangleAngle1Spin = nullptr;
    QSpinBox* m_triangleAngle2Spin = nullptr;
};
