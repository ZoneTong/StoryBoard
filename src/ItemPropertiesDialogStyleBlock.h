#pragma once

#include "CanvasItem.h"

#include <QPointer>
#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QToolButton;

// 属性窗的填充 / 描边区块：图形图元提供填充样式、填充颜色、描边颜色与宽度，
// 笔画图元只提供描边颜色与宽度；控件初始值取自图元，快照值供确定时提交撤销记录。
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
    QDoubleSpinBox* strokeWidthSpin() const { return m_strokeWidthSpin; }

  private:
    bool m_hasFill = false;
    qreal m_initialStrokeWidth = 4.0;
    QComboBox* m_fillStyleCombo = nullptr;
    QToolButton* m_fillColorButton = nullptr;
    QToolButton* m_strokeColorButton = nullptr;
    QDoubleSpinBox* m_strokeWidthSpin = nullptr;
};
