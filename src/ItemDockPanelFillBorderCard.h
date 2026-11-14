#pragma once

#include "ShapeItem.h"

#include <QList>
#include <QWidget>

class QDoubleSpinBox;
class QToolButton;

// 编辑图元卡的填充样式、边框样式与边框宽度三行：样式按钮按当前图元勾选，
// 宽度数值框跟随选中状态启用并回显；改动交回面板走撤销命令。
// 三行由面板按“填充样式、填充颜色、边框样式、边框颜色、边框宽度”的顺序插入布局。
class ItemDockPanelFillBorderCard : public QWidget {
    Q_OBJECT

  public:
    explicit ItemDockPanelFillBorderCard(QWidget* parent = nullptr);

    void refresh(const QList<ShapeItem*>& shapes);
    QWidget* fillStyleRow() const { return m_fillStyleRow; }
    QWidget* borderStyleRow() const { return m_borderStyleRow; }
    QWidget* borderWidthRow() const { return m_borderWidthRow; }

  signals:
    void fillStyleChosen(FillStyle style);
    void borderStyleChosen(Qt::PenStyle style);
    void borderWidthChosen(qreal width);

  private:
    QWidget* m_fillStyleRow = nullptr;
    QWidget* m_borderStyleRow = nullptr;
    QWidget* m_borderWidthRow = nullptr;
    bool m_refreshing = false;
    QList<QToolButton*> m_fillStyleButtons;
    QList<QToolButton*> m_borderStyleButtons;
    QDoubleSpinBox* m_borderWidthSpin = nullptr;
};
