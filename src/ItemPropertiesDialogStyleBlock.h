#pragma once

#include "CanvasItem.h"
#include "ImageFill.h"

#include <QPixmap>
#include <QPointer>
#include <QSizeF>
#include <QWidget>

class QComboBox;
class QFormLayout;
class QPushButton;
class QSpinBox;
class QToolButton;

// 属性窗的填充 / 边框区块：图形图元提供填充样式、填充颜色、边框颜色、边框样式、
// 边框宽度、透明度；矩形 / 圆角矩形额外提供圆角半径，扇形 / 弧线额外提供
// 起始角、扫过角与半径；笔画图元只提供边框颜色与宽度。控件初始值取自图元，
// 快照值供确定时提交撤销记录。
class ItemPropertiesDialogStyleBlock : public QWidget {
    Q_OBJECT

  public:
    ItemPropertiesDialogStyleBlock(CanvasItem* item, QWidget* parent = nullptr);

    void AddRows(QFormLayout* form);

    bool HasFill() const { return m_hasFill; }
    qreal InitialStrokeWidth() const { return m_initialStrokeWidth; }
    QComboBox* FillStyleCombo() const { return m_fillStyleCombo; }
    QToolButton* FillColorButton() const { return m_fillColorButton; }
    QToolButton* StrokeColorButton() const { return m_strokeColorButton; }
    QComboBox* BorderStyleCombo() const { return m_borderStyleCombo; }
    QSpinBox* StrokeWidthSpin() const { return m_strokeWidthSpin; }
    QSpinBox* OpacitySpin() const { return m_opacitySpin; }
    // 需求 15：填充图片与摆放模式（仅可填充图元有这一行）。
    bool HasFillImageRow() const { return m_fillImageButton != nullptr; }
    bool FillImagePresent() const { return !m_fillImage.isNull(); }
    QPixmap FillImage() const { return m_fillImage; }
    ImageFill::Mode FillImageMode() const;
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

    // 三角形：底边长度与两个底角；底边即内容矩形宽度，角度优先、高度派生。
    QSpinBox* TriangleBaseSpin() const { return m_triangleBaseSpin; }
    int InitialTriangleBase() const { return m_initialTriangleBase; }
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
    bool m_hasFill = false;
    qreal m_initialStrokeWidth = 4.0;
    QComboBox* m_fillStyleCombo = nullptr;
    QToolButton* m_fillColorButton = nullptr;
    QToolButton* m_strokeColorButton = nullptr;
    QComboBox* m_borderStyleCombo = nullptr;
    QSpinBox* m_strokeWidthSpin = nullptr;
    QSpinBox* m_opacitySpin = nullptr;
    QPushButton* m_fillImageButton = nullptr;
    QComboBox* m_fillImageModeCombo = nullptr;
    QPushButton* m_fillImageClearButton = nullptr;
    QPixmap m_fillImage;
    QWidget* m_fillImageRow = nullptr;
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
    QSpinBox* m_triangleBaseSpin = nullptr;
    int m_initialTriangleBase = 64;

    QComboBox* m_startArrowCombo = nullptr;
    QComboBox* m_endArrowCombo = nullptr;
    QWidget* m_arrowRow = nullptr;
};
