#pragma once

#include "CanvasItem.h"

#include <QPointer>
#include <QSizeF>
#include <QWidget>

class QDoubleSpinBox;
class QFormLayout;

// 属性窗的几何区块：位置、内容尺寸与旋转角度三组数值框；文字正文或图片
// 替换后由外壳把自然尺寸写回尺寸框，确定时随其它属性一并提交撤销记录。
class ItemPropertiesDialogGeometryBlock : public QWidget {
    Q_OBJECT

  public:
    ItemPropertiesDialogGeometryBlock(CanvasItem* item, QWidget* parent = nullptr);

    void addRows(QFormLayout* form);

    QDoubleSpinBox* positionXSpin() const { return m_positionXSpin; }
    QDoubleSpinBox* positionYSpin() const { return m_positionYSpin; }
    QDoubleSpinBox* widthSpin() const { return m_widthSpin; }
    QDoubleSpinBox* heightSizeSpin() const { return m_heightSizeSpin; }
    QDoubleSpinBox* rotationSpin() const { return m_rotationSpin; }

    void setNaturalSize(const QSizeF& size);

  private:
    QDoubleSpinBox* m_positionXSpin = nullptr;
    QDoubleSpinBox* m_positionYSpin = nullptr;
    QDoubleSpinBox* m_widthSpin = nullptr;
    QDoubleSpinBox* m_heightSizeSpin = nullptr;
    QDoubleSpinBox* m_rotationSpin = nullptr;
};
