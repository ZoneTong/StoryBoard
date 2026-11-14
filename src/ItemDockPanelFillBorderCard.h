#pragma once

#include "ShapeItem.h"

#include <QList>
#include <QWidget>

class QDoubleSpinBox;
class QToolButton;

// 编辑图元卡的填充样式、边框样式与边框宽度三行：样式按钮按当前图元勾选，
// 宽度数值框跟随选中状态启用并回显；改动交回面板走撤销命令。
class ItemDockPanelFillBorderCard : public QWidget {
    Q_OBJECT

  public:
    explicit ItemDockPanelFillBorderCard(QWidget* parent = nullptr);

    void refresh(const QList<ShapeItem*>& shapes);

  signals:
    void fillStyleChosen(FillStyle style);
    void borderStyleChosen(Qt::PenStyle style);
    void borderWidthChosen(qreal width);

  private:
    bool m_refreshing = false;
    QList<QToolButton*> m_fillStyleButtons;
    QList<QToolButton*> m_borderStyleButtons;
    QDoubleSpinBox* m_borderWidthSpin = nullptr;
};
