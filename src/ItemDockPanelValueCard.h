#pragma once

#include "ShapeItem.h"

#include <QList>
#include <QWidget>

class QSpinBox;

// 编辑图元卡的透明度、圆角半径、扇形 / 弧线扫过角与半径各行：圆角行只在矩形类
// 图元上出现，角度与半径行只在扇形 / 弧线上出现；数值框跟随选中状态启用并回显，
// 改动交回面板走撤销命令。
class ItemDockPanelValueCard : public QWidget {
    Q_OBJECT

  public:
    explicit ItemDockPanelValueCard(QWidget* parent = nullptr);

    void refresh(const QList<ShapeItem*>& shapes);
    // 圆角行是否可见：只有矩形类图元才显示圆角编辑。
    void setCornerVisible(bool visible);
    // 扫过角与半径两行是否可见：只有扇形 / 弧线才显示角度与半径编辑。
    void setArcRadiusVisible(bool visible);

  signals:
    void opacityChosen(int percent);
    void cornerRadiusChosen(int radius);
    void sweepAngleChosen(int angle);
    void shapeRadiusChosen(int radius);

  private:
    bool m_refreshing = false;
    QSpinBox* m_opacitySpin = nullptr;
    QSpinBox* m_cornerSpin = nullptr;
    QWidget* m_cornerSection = nullptr;
    QSpinBox* m_angleSpin = nullptr;
    QSpinBox* m_radiusSpin = nullptr;
    QWidget* m_angleSection = nullptr;
    QWidget* m_radiusSection = nullptr;
};
