#pragma once

#include "ShapeItem.h"

#include <QList>
#include <QWidget>

class QSpinBox;
class QToolButton;
class LineItemAbstract;

// 属性编辑面板的填充样式、边框样式与边框宽度三行：样式按钮按当前图元勾选，
// 宽度数值框跟随选中状态启用并回显；改动交回面板走撤销命令。
// 三行由面板按“填充样式、填充颜色、边框样式、边框颜色、边框宽度”的顺序插入布局。
class PropertiesPanelFillBorderCard : public QWidget {
    Q_OBJECT

  public:
    explicit PropertiesPanelFillBorderCard(QWidget* parent = nullptr);

    void Refresh(const QList<ShapeItem*>& shapes, const QList<LineItemAbstract*>& strokes,
                 const QList<ShapeItem*>& composites = QList<ShapeItem*>());
    QWidget* FillStyleRow() const { return m_fillStyleRow; }
    QWidget* BorderStyleRow() const { return m_borderStyleRow; }
    QWidget* BorderWidthRow() const { return m_borderWidthRow; }

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
    QSpinBox* m_borderWidthSpin = nullptr;
};
