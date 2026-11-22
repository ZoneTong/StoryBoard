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
class PropertyRowRegistry;
class FillStyleRow;
class FillColorRow;
class FillImageRow;
class StrokeStyleRow;
class StrokeColorRow;
class StrokeWidthRow;

// 属性窗的填充 / 边框区块：填充与描线两组行直接复用编辑面板的 AttributeRowWidgets，
// 行控件绑定本块私有的 FillAttributes / StrokeAttributes 副本（属性窗取消时不影响图元），
// 确定时把两份状态交给对话框写入撤销记录。
// 行显隐统一交给 PropertyRowRegistry：按图元上报的属性集合决定，不看 shapeType。
class ItemPropertiesDialogStyleBlock : public QWidget {
    Q_OBJECT

  public:
    // 内角 A 的提交目标：同一行控件按图元属性回填 / 提交，避免换绑后被旧值脏写。
    enum class InteriorAngleTarget { NONE, TRIANGLE_A, PARALLELOGRAM };

    explicit ItemPropertiesDialogStyleBlock(QWidget* parent = nullptr);

    void AddRows(QFormLayout* form, PropertyRowRegistry* rows);

    void SetItem(CanvasItem* item);

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

    // 内角 A / B：三角形两个底角、平行四边形左下内角共用两行（§4.2）。
    QSpinBox* InteriorAngleASpin() const { return m_interiorAngleASpin; }
    QSpinBox* InteriorAngleBSpin() const { return m_interiorAngleBSpin; }
    InteriorAngleTarget Target() const { return m_interiorAngleTarget; }
    qreal InitialInteriorAngleA() const { return m_initialInteriorAngleA; }
    qreal InitialInteriorAngleB() const { return m_initialInteriorAngleB; }

    bool HasCornerRadius() const { return m_cornerRadiusSpin != nullptr; }
    QSpinBox* CornerRadiusSpin() const { return m_cornerRadiusSpin; }
    int InitialCornerRadius() const { return m_initialCornerRadius; }
    qreal InitialStoredCornerRadius() const { return m_initialStoredCornerRadius; }

    // 起始角 / 扫过角：扇形与弧线共用两行，按 ItemProperty::START_ANGLE / SWEEP_ANGLE 读写。
    QSpinBox* StartAngleSpin() const { return m_startAngleSpin; }
    QSpinBox* SweepAngleSpin() const { return m_sweepAngleSpin; }
    qreal InitialStartAngle() const { return m_initialStartAngle; }
    qreal InitialSweepAngle() const { return m_initialSweepAngle; }

    // 直线两端箭头样式下拉（一行两个）。
    QComboBox* StartArrowCombo() const { return m_startArrowCombo; }
    QComboBox* EndArrowCombo() const { return m_endArrowCombo; }

    // 正弦：周期数（1–12）与振幅（1–200 px），按 ItemProperty::SINE_CYCLES / SINE_AMPLITUDE 读写。
    QSpinBox* SineCyclesSpin() const { return m_sineCyclesSpin; }
    QSpinBox* SineAmplitudeSpin() const { return m_sineAmplitudeSpin; }
    int InitialSineCycles() const { return m_initialSineCycles; }
    int InitialSineAmplitude() const { return m_initialSineAmplitude; }

    // 尺寸框变化时同步圆角上限；用户尚未改过圆角时，圆角矩形的默认值
    // 按新短边重新计算，保证对话框显示值与最终轮廓一致。
    void SetContentSize(const QSizeF& size);

  private:
    void BuildFillRows();
    void BuildStrokeRows();
    void BuildTypeRows();

    QFormLayout* m_form = nullptr;
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

    qreal m_defaultCornerRatio = 0.0;
    QSizeF m_contentSize;
    QSpinBox* m_cornerRadiusSpin = nullptr;
    int m_initialCornerRadius = 0;
    qreal m_initialStoredCornerRadius = -1.0;
    bool m_cornerEdited = false;

    QSpinBox* m_interiorAngleASpin = nullptr;
    QSpinBox* m_interiorAngleBSpin = nullptr;
    InteriorAngleTarget m_interiorAngleTarget = InteriorAngleTarget::NONE;
    qreal m_initialInteriorAngleA = 60.0;
    qreal m_initialInteriorAngleB = 60.0;

    QComboBox* m_startArrowCombo = nullptr;
    QComboBox* m_endArrowCombo = nullptr;
    QWidget* m_arrowRow = nullptr;

    QSpinBox* m_startAngleSpin = nullptr;
    QSpinBox* m_sweepAngleSpin = nullptr;
    qreal m_initialStartAngle = 0.0;
    qreal m_initialSweepAngle = 0.0;

    QSpinBox* m_sineCyclesSpin = nullptr;
    QSpinBox* m_sineAmplitudeSpin = nullptr;
    int m_initialSineCycles = 3;
    int m_initialSineAmplitude = 40;
};
