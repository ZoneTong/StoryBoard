#pragma once

#include "CanvasItem.h"

#include <QPointer>
#include <QSizeF>
#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QSpinBox;
class QToolButton;

// 属性窗的填充 / 描边区块：图形图元提供填充样式、填充颜色、描边颜色与宽度，
// 矩形 / 圆角矩形额外提供圆角半径；笔画图元只提供描边颜色与宽度。
// 控件初始值取自图元，快照值供确定时提交撤销记录。
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

    bool hasCornerRadius() const { return m_cornerRadiusSpin != nullptr; }
    QSpinBox* cornerRadiusSpin() const { return m_cornerRadiusSpin; }
    int initialCornerRadius() const { return m_initialCornerRadius; }
    qreal initialStoredCornerRadius() const { return m_initialStoredCornerRadius; }

    // 尺寸框变化时同步圆角上限；用户尚未改过圆角时，圆角矩形的默认值
    // 按新短边重新计算，保证对话框显示值与最终轮廓一致。
    void setContentSize(const QSizeF& size);

  private:
    bool m_hasFill = false;
    qreal m_initialStrokeWidth = 4.0;
    QComboBox* m_fillStyleCombo = nullptr;
    QToolButton* m_fillColorButton = nullptr;
    QToolButton* m_strokeColorButton = nullptr;
    QDoubleSpinBox* m_strokeWidthSpin = nullptr;

    qreal m_defaultCornerRatio = 0.0;
    QSizeF m_contentSize;
    QSpinBox* m_cornerRadiusSpin = nullptr;
    int m_initialCornerRadius = 0;
    qreal m_initialStoredCornerRadius = -1.0;
    bool m_cornerEdited = false;
};
