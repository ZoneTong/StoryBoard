#pragma once

#include "CanvasItem.h"
#include "FillAttributes.h"
#include "ImageFill.h"
#include "StrokeAttributes.h"

#include <QPixmap>
#include <QPointer>
#include <QSizeF>
#include <QWidget>

class QComboBox;
class QFormLayout;
class QSpinBox;
class FillStyleRow;
class FillColorRow;
class FillImageRow;
class StrokeStyleRow;
class StrokeColorRow;
class StrokeWidthRow;

// 属性窗的填充 / 边框区块：填充与描线两组行直接复用编辑面板的 AttributeRowWidgets，
// 行控件绑定本块私有的 FillAttributes / StrokeAttributes 副本（属性窗取消时不影响图元），
// 确定时把两份状态交给对话框写入撤销记录。
class ItemPropertiesDialogStyleBlock : public QWidget {
    Q_OBJECT

  public:
    ItemPropertiesDialogStyleBlock(CanvasItem* item, QWidget* parent = nullptr);

    void AddRows(QFormLayout* form);

    bool HasFill() const { return m_hasFill; }
    qreal InitialStrokeWidth() const { return m_initialStrokeWidth; }
    FillAttributesState FillState() const { return m_fillAttributes.FillState(); }
    StrokeAttributesState StrokeState() const { return m_strokeAttributes.StrokeState(); }
    QSpinBox* OpacitySpin() const { return m_opacitySpin; }
    bool HasFillImageRow() const { return m_fillImageRow != nullptr; }
    QList<QWidget*> StyleRows() const;
    bool FillImagePresent() const { return m_fillAttributes.HasFillImage(); }
    QPixmap FillImage() const { return m_fillAttributes.FillImage(); }
    ImageFill::Mode FillImageMode() const { return m_fillAttributes.FillImageMode(); }
    qreal InitialOpacity() const { return m_initialOpacity; }
    int InitialOpacityPercent() const { return m_initialOpacityPercent; }

    QSpinBox* StartAngleSpin() const { return m_startAngleSpin; }
    QSpinBox* SweepAngleSpin() const { return m_sweepAngleSpin; }
    qreal InitialStartAngle() const { return m_initialStartAngle; }
    qreal InitialSweepAngle() const { return m_initialSweepAngle; }

    bool HasCornerRadius() const { return m_cornerRadiusSpin != nullptr; }
    QSpinBox* CornerRadiusSpin() const { return m_cornerRadiusSpin; }
    int InitialCornerRadius() const { return m_initialCornerRadius; }
    qreal InitialStoredCornerRadius() const { return m_initialStoredCornerRadius; }

    // 三角形：两个底角；底边宽度随图元外框调整，角度优先、高度派生。
    QSpinBox* BottomAngle1Spin() const { return m_bottomAngle1Spin; }
    QSpinBox* BottomAngle2Spin() const { return m_bottomAngle2Spin; }
    qreal InitialBottomAngle1() const { return m_initialBottomAngle1; }
    qreal InitialBottomAngle2() const { return m_initialBottomAngle2; }

    // 需求 9：平行四边形左下内角（度），未改动时保留原值。
    QSpinBox* ParallelogramAngleSpin() const { return m_parallelogramAngleSpin; }
    qreal InitialParallelogramAngle() const { return m_initialParallelogramAngle; }

    // 直线两端箭头样式下拉（一行两个）。
    QComboBox* StartArrowCombo() const { return m_startArrowCombo; }
    QComboBox* EndArrowCombo() const { return m_endArrowCombo; }

    // 尺寸框变化时同步圆角上限；用户尚未改过圆角时，圆角矩形的默认值
    // 按新短边重新计算，保证对话框显示值与最终轮廓一致。
    void SetContentSize(const QSizeF& size);

  private:
    void BuildFillRows(CanvasItem* item);
    void BuildStrokeRows(CanvasItem* item);

    bool m_hasFill = false;
    qreal m_initialStrokeWidth = 4.0;
    FillAttributes m_fillAttributes;
    StrokeAttributes m_strokeAttributes;
    FillStyleRow* m_fillStyleRow = nullptr;
    FillColorRow* m_fillColorRow = nullptr;
    FillImageRow* m_fillImageRow = nullptr;
    StrokeStyleRow* m_borderStyleRow = nullptr;
    StrokeColorRow* m_strokeColorRow = nullptr;
    StrokeWidthRow* m_borderWidthRow = nullptr;
    QSpinBox* m_opacitySpin = nullptr;
    qreal m_initialOpacity = 1.0;
    int m_initialOpacityPercent = 100;

    QSpinBox* m_startAngleSpin = nullptr;
    QSpinBox* m_sweepAngleSpin = nullptr;
    qreal m_initialStartAngle = 0.0;
    qreal m_initialSweepAngle = 0.0;

    qreal m_defaultCornerRatio = 0.0;
    QSizeF m_contentSize;
    QSpinBox* m_cornerRadiusSpin = nullptr;
    int m_initialCornerRadius = 0;
    qreal m_initialStoredCornerRadius = -1.0;
    bool m_cornerEdited = false;

    QSpinBox* m_bottomAngle1Spin = nullptr;
    QSpinBox* m_bottomAngle2Spin = nullptr;
    qreal m_initialBottomAngle1 = 60.0;
    qreal m_initialBottomAngle2 = 60.0;
    qreal m_initialParallelogramAngle = 60.0;
    QSpinBox* m_parallelogramAngleSpin = nullptr;

    QComboBox* m_startArrowCombo = nullptr;
    QComboBox* m_endArrowCombo = nullptr;
    QWidget* m_arrowRow = nullptr;
};
