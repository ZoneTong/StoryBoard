#include "ui/panels/properties/PropertiesPanelValueCard.h"

#include "items/CanvasItem.h"
#include "items/geometry/ShapeMeta.h"
#include "items/lines/ArcItem.h"
#include "items/lines/LineAbstractItem.h"
#include "items/lines/SineItem.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "ui/icons/IconFactory.h"
#include "ui/panels/properties/PropertiesPanel.h"
#include "ui/widgets/RowMetrics.h"
#include "ui/widgets/rows/ValueRows.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace {


QToolButton* MakeRowIcon(IconId id, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("ItemRowIcon"));
    button->setIcon(IconFactory::Make(id, QColor(139, 144, 156), RowMetrics::GlyphSize()));
    button->setIconSize(QSize(RowMetrics::GlyphSize(), RowMetrics::GlyphSize()));
    button->setFixedSize(RowMetrics::IconButtonSize());
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

} // namespace

PropertiesPanelValueCard::PropertiesPanelValueCard(QWidget* parent) : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(2);

    // 需求 6：直线两端箭头样式合成一行（起点箭头, 终点箭头两个下拉）。
    // 本卡片紧跟描线宽度行排入面板，所以箭头行放在卡片最前。
    // 行首只放一枚图标（起点箭头）；两个下拉各自就是含义，不再在前面重复放图标。
    m_arrowSection = new QWidget(this);
    m_arrowSection->setObjectName(QStringLiteral("ItemArrowRow"));
    RowMetrics::ApplyRow(m_arrowSection);
    m_arrowSection->setToolTip(PropertiesPanel::tr("Arrow heads"));
    auto* arrowLayout = new QHBoxLayout(m_arrowSection);
    arrowLayout->setContentsMargins(0, 0, 0, 0);
    arrowLayout->setSpacing(4);
    QToolButton* arrowIcon = MakeRowIcon(IconId::ARROW_START, m_arrowSection);
    arrowIcon->setToolTip(PropertiesPanel::tr("Arrow heads"));
    arrowLayout->addWidget(arrowIcon);

    auto makeArrowCombo = [this, arrowLayout](const QString& tooltip, QComboBox** target,
                                              void (PropertiesPanelValueCard::*signal)(int)) {
        QComboBox* combo = new QComboBox(m_arrowSection);
        combo->setObjectName(QStringLiteral("ItemArrowCombo"));
        combo->setToolTip(tooltip);
        for (ArrowStyle style : AllArrowStyles()) {
            combo->addItem(ArrowStyles::Name(style), static_cast<int>(style));
        }
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, combo, signal](int) {
            if (!m_refreshing) {
                (this->*signal)(combo->currentData().toInt());
            }
        });
        arrowLayout->addWidget(combo, 1);
        *target = combo;
    };
    makeArrowCombo(PropertiesPanel::tr("Start arrow"), &m_startArrowCombo,
                   static_cast<void (PropertiesPanelValueCard::*)(int)>(&PropertiesPanelValueCard::arrowStartChosen));
    makeArrowCombo(PropertiesPanel::tr("End arrow"), &m_endArrowCombo,
                   static_cast<void (PropertiesPanelValueCard::*)(int)>(&PropertiesPanelValueCard::arrowEndChosen));
    m_arrowSection->setVisible(false);
    outer->addWidget(m_arrowSection);

    auto* opacityRow = new QWidget(this);
    RowMetrics::ApplyRow(opacityRow);
    auto* opacityLayout = new QHBoxLayout(opacityRow);
    opacityLayout->setContentsMargins(0, 0, 0, 0);
    opacityLayout->setSpacing(4);

    QToolButton* opacityIcon = MakeRowIcon(IconId::OPACITY, opacityRow);
    opacityIcon->setToolTip(PropertiesPanel::tr("Opacity (100% = opaque)"));
    opacityLayout->addWidget(opacityIcon);

    m_opacitySpin = new QSpinBox(opacityRow);
    m_opacitySpin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_opacitySpin->setRange(0, 100);
    m_opacitySpin->setSuffix(QStringLiteral(" %"));
    m_opacitySpin->setToolTip(PropertiesPanel::tr("Opacity (100% = opaque)"));
    // 关闭键盘跟踪：输入过程不触发 valueChanged，回车或失焦才提交，
    // 选中变化触发的 Refresh() 也就不会打断正在输入的值。
    m_opacitySpin->setKeyboardTracking(false);
    connect(m_opacitySpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_refreshing) {
            emit opacityChosen(value);
        }
    });
    opacityLayout->addWidget(m_opacitySpin, 1);
    outer->addWidget(opacityRow);

    // 半轴合成一行（长半轴, 短半轴）：只对椭圆 / 扇形 / 弧线开放。
    m_axisRow = new PairValueRow(IconId::AXIS_MAJOR, true, QStringLiteral("ItemMajorAxisSpin"),
                                 QStringLiteral("ItemMinorAxisSpin"), ItemProperty::MAJOR_AXIS, this);
    m_axisRow->FirstSpin()->setToolTip(PropertiesPanel::tr("Semi-major axis"));
    m_axisRow->SecondSpin()->setToolTip(PropertiesPanel::tr("Semi-minor axis"));
    m_axisRow->SetIconToolTip(PropertiesPanel::tr("Semi-axes"));
    connect(m_axisRow, &PairValueRow::valuesChosen, this, [this](qreal major, qreal minor) {
        if (!m_refreshing) {
            ApplyAxes(major, minor);
        }
    });
    m_axisRow->setVisible(false);
    outer->addWidget(m_axisRow);

    // 起止角合成一行（起始角, 扫过角），行图标是半圆量角器：显示口径回到扫过角，
    // 与数据层的「起始角 + 扫过角」一致，写入时不需要再换算终止角。
    m_arcSection = new QWidget(this);
    RowMetrics::ApplyRow(m_arcSection);
    m_arcSection->setToolTip(PropertiesPanel::tr("Sector start / sweep angles"));
    auto* arcLayout = new QHBoxLayout(m_arcSection);
    arcLayout->setContentsMargins(0, 0, 0, 0);
    arcLayout->setSpacing(4);
    QToolButton* arcIcon = MakeRowIcon(IconId::SECTOR_ANGLE, m_arcSection);
    arcIcon->setToolTip(PropertiesPanel::tr("Sector start / sweep angles"));
    arcLayout->addWidget(arcIcon);

    m_arcStartSpin = new QSpinBox(m_arcSection);
    m_arcStartSpin->setObjectName(QStringLiteral("ItemArcStartSpin"));
    m_arcStartSpin->setRange(-360, 360);
    m_arcStartSpin->setSuffix(QStringLiteral("°"));
    m_arcStartSpin->setToolTip(PropertiesPanel::tr("Start angle"));
    m_arcStartSpin->setKeyboardTracking(false);
    arcLayout->addWidget(m_arcStartSpin, 1);

    m_arcSweepSpin = new QSpinBox(m_arcSection);
    m_arcSweepSpin->setObjectName(QStringLiteral("ItemArcSweepSpin"));
    m_arcSweepSpin->setRange(-360, 360);
    m_arcSweepSpin->setSuffix(QStringLiteral("°"));
    m_arcSweepSpin->setToolTip(PropertiesPanel::tr("Sweep angle"));
    m_arcSweepSpin->setKeyboardTracking(false);
    arcLayout->addWidget(m_arcSweepSpin, 1);

    // 任一格改动都把当前两格整体交给面板，写入侧按属性表落到图形 / 线条。
    auto emitArcAngles = [this]() {
        if (!m_refreshing) {
            emit arcAnglesChosen(m_arcStartSpin->value(), m_arcSweepSpin->value());
        }
    };
    connect(m_arcStartSpin, &QSpinBox::valueChanged, this, emitArcAngles);
    connect(m_arcSweepSpin, &QSpinBox::valueChanged, this, emitArcAngles);
    m_arcSection->setVisible(false);
    outer->addWidget(m_arcSection);

    // 内角合成一行（内角 A, 内角 B）：三角形是两个底角，平行四边形 B 为 A 的补角。
    m_interiorAngleRow = new PairValueRow(IconId::ANGLE_RIGHT, true, QStringLiteral("ItemInteriorAngleSpin"),
                                          QStringLiteral("ItemInteriorAngleBSpin"), ItemProperty::BOTTOM_ANGLE1, this);
    m_interiorAngleRow->FirstSpin()->setToolTip(PropertiesPanel::tr("Interior angle A"));
    m_interiorAngleRow->SecondSpin()->setToolTip(PropertiesPanel::tr("Interior angle B"));
    m_interiorAngleRow->SecondSpin()->setRange(5.0, 175.0);
    m_interiorAngleRow->SetIconToolTip(PropertiesPanel::tr("Interior angle"));
    connect(m_interiorAngleRow, &PairValueRow::valuesChosen, this, [this](qreal angleA, qreal angleB) {
        if (m_refreshing) {
            return;
        }
        // 只可能有一个框被改：靠刷新基准判断改的是 A 还是 B。
        const bool bChanged = !qFuzzyCompare(angleB + 2.0, m_interiorAngleB + 2.0);
        const qreal resolvedA = (m_interiorSupplement && bChanged) ? 180.0 - angleB : angleA;
        emit interiorAngleChosen(resolvedA);
        if (!m_interiorSupplement) {
            emit interiorAngleBChosen(angleB);
        }
    });
    m_interiorAngleRow->setVisible(false);
    outer->addWidget(m_interiorAngleRow);

    // 圆角行排在四组形状属性之后：半轴 → 起止角 → 内角 → 圆角。
    m_cornerSection = new QWidget(this);
    auto* cornerLayout = new QHBoxLayout(m_cornerSection);
    cornerLayout->setContentsMargins(0, 0, 0, 0);
    cornerLayout->setSpacing(4);

    QToolButton* cornerIcon = MakeRowIcon(IconId::CORNER, m_cornerSection);
    cornerIcon->setToolTip(PropertiesPanel::tr("Corner radius"));
    cornerLayout->addWidget(cornerIcon);

    m_cornerSpin = new QSpinBox(m_cornerSection);
    m_cornerSpin->setObjectName(QStringLiteral("ItemValueCornerSpin"));
    m_cornerSpin->setRange(0, 400);
    m_cornerSpin->setSuffix(QStringLiteral(" px"));
    m_cornerSpin->setToolTip(PropertiesPanel::tr("Corner radius"));
    m_cornerSpin->setKeyboardTracking(false);
    connect(m_cornerSpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_refreshing) {
            emit cornerRadiusChosen(value);
        }
    });
    cornerLayout->addWidget(m_cornerSpin, 1);
    outer->addWidget(m_cornerSection);

    // 正弦波形：周期数与振幅是几何真相，改参数即按新参数重采样（拉伸不改参数）。
    // 两行合成一行：周期数（1–12）, 振幅（相对中心线的像素高度，1–200）。
    m_sineSection = new QWidget(this);
    RowMetrics::ApplyRow(m_sineSection);
    m_sineSection->setToolTip(PropertiesPanel::tr("Sine wave"));
    auto* sineLayout = new QHBoxLayout(m_sineSection);
    sineLayout->setContentsMargins(0, 0, 0, 0);
    sineLayout->setSpacing(4);
    QToolButton* sineIcon = MakeRowIcon(IconId::SINE_CYCLES, m_sineSection);
    sineIcon->setToolTip(PropertiesPanel::tr("Sine wave"));
    sineLayout->addWidget(sineIcon);

    m_sineCyclesSpin = new QSpinBox(m_sineSection);
    m_sineCyclesSpin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_sineCyclesSpin->setRange(1, 12);
    m_sineCyclesSpin->setToolTip(PropertiesPanel::tr("Sine cycles"));
    m_sineCyclesSpin->setKeyboardTracking(false);
    connect(m_sineCyclesSpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_refreshing) {
            emit sineCyclesChosen(value);
        }
    });
    sineLayout->addWidget(m_sineCyclesSpin, 1);

    m_sineAmplitudeSpin = new QSpinBox(m_sineSection);
    m_sineAmplitudeSpin->setObjectName(QStringLiteral("ItemValueSpin"));
    // 振幅是相对中心线的像素高度：1–200，与波高（2 倍振幅）一一对应。
    m_sineAmplitudeSpin->setRange(1, 200);
    m_sineAmplitudeSpin->setSuffix(QStringLiteral(" px"));
    m_sineAmplitudeSpin->setToolTip(PropertiesPanel::tr("Sine amplitude"));
    m_sineAmplitudeSpin->setKeyboardTracking(false);
    connect(m_sineAmplitudeSpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_refreshing) {
            emit sineAmplitudeChosen(value);
        }
    });
    sineLayout->addWidget(m_sineAmplitudeSpin, 1);
    outer->addWidget(m_sineSection);
}

void PropertiesPanelValueCard::Refresh(const QList<ShapeAbstractItem*>& shapes, const QList<LineAbstractItem*>& strokes,
                                       const QList<CanvasItem*>& items)
{
    m_refreshing = true;

    const bool enabled = !shapes.isEmpty();
    if (m_opacitySpin) {
        m_opacitySpin->setEnabled(!items.isEmpty());
        m_opacitySpin->setValue(items.isEmpty() ? 100 : qRound(items.first()->ItemOpacity() * 100.0));
    }
    if (m_cornerSpin) {
        m_cornerSpin->setEnabled(enabled);
        // 需求 4：圆角上限统一为短边一半，不再写死 400。
        const QSizeF size = shapes.isEmpty() ? QSizeF() : shapes.first()->ContentSize();
        const int limit = (size.width() > 0.0 && size.height() > 0.0)
                              ? qMax(1, qRound(qMin(size.width(), size.height()) / 2.0))
                              : 400;
        m_cornerSpin->setMaximum(limit);
        const qreal radius = shapes.isEmpty() ? 0.0 : qMax<qreal>(0.0, shapes.first()->CornerRadius());
        m_cornerSpin->setValue(qBound(0, qRound(radius), limit));
    }
    // 弧线角度：扇形等形状与曲线族里的弧线共用这两行，优先回显当前选中的弧线曲线。
    // “谁能提供这两行”由属性表回答，不再查基类能力位。
    LineAbstractItem* arcCurve = nullptr;
    ShapeAbstractItem* arcShape = nullptr;
    LineAbstractItem* sineCurve = nullptr;
    for (LineAbstractItem* stroke : strokes) {
        const ItemPropertySet properties = stroke->Properties();
        const bool hasArcAngles =
            properties.Contains(ItemProperty::START_ANGLE) && properties.Contains(ItemProperty::SWEEP_ANGLE);
        if (hasArcAngles && !arcCurve) {
            arcCurve = stroke;
        } else if (!hasArcAngles && properties.Contains(ItemProperty::SINE_CYCLES) &&
                   properties.Contains(ItemProperty::SINE_AMPLITUDE) && !sineCurve) {
            sineCurve = stroke;
        }
    }
    for (ShapeAbstractItem* shape : shapes) {
        if (shape && shape->shapeType() == ShapeType::SECTOR) {
            arcShape = shape;
            break;
        }
    }
    if (m_arcStartSpin && m_arcSweepSpin) {
        const bool on = arcCurve != nullptr || arcShape != nullptr;
        m_arcStartSpin->setEnabled(on);
        m_arcSweepSpin->setEnabled(on);
        const CanvasItem* arc =
            arcCurve ? static_cast<const CanvasItem*>(arcCurve) : static_cast<const CanvasItem*>(arcShape);
        const qreal start = arc ? arc->PropertyValue(ItemProperty::START_ANGLE).toDouble() : 30.0;
        const qreal sweep = arc ? arc->PropertyValue(ItemProperty::SWEEP_ANGLE).toDouble() : 120.0;
        m_arcStartSpin->setValue(qBound(-360, qRound(start), 360));
        m_arcSweepSpin->setValue(qBound(-360, qRound(sweep), 360));
    }
    if (m_axisRow) {
        // 半轴与起止角同源：弧线优先，其次扇形等形状；能不能编辑由属性表决定。
        const CanvasItem* axisSource = arcCurve ? static_cast<const CanvasItem*>(arcCurve)
                                                : (arcShape ? static_cast<const CanvasItem*>(arcShape)
                                                            : (shapes.isEmpty() ? nullptr : shapes.first()));
        const bool on = axisSource && axisSource->Properties().Contains(ItemProperty::MAJOR_AXIS);
        m_axisRow->setEnabled(on);
        const qreal major = on ? axisSource->PropertyValue(ItemProperty::MAJOR_AXIS).toDouble() : 100.0;
        const qreal minor = on ? axisSource->PropertyValue(ItemProperty::MINOR_AXIS).toDouble() : 60.0;
        m_axisRow->Refresh(major, minor);
    }
    if (m_interiorAngleRow) {
        // 内角 A 由属性表回显（三角形底角与平行四边形内角同名）；平行四边形第二格是补角。
        m_interiorAngleA = 60.0;
        m_interiorAngleB = 60.0;
        m_interiorSupplement = false;
        if (!shapes.isEmpty() && shapes.first()) {
            ShapeAbstractItem* shape = shapes.first();
            const ItemPropertySet properties = shape->Properties();
            if (properties.Contains(ItemProperty::PARALLELOGRAM_ANGLE)) {
                m_interiorAngleA = shape->PropertyValue(ItemProperty::PARALLELOGRAM_ANGLE).toDouble();
                m_interiorAngleB = 180.0 - m_interiorAngleA;
                m_interiorSupplement = true;
            } else if (properties.Contains(ItemProperty::BOTTOM_ANGLE1)) {
                m_interiorAngleA = shape->PropertyValue(ItemProperty::BOTTOM_ANGLE1).toDouble();
                m_interiorAngleB = shape->PropertyValue(ItemProperty::BOTTOM_ANGLE2).toDouble();
            }
        }
        m_interiorAngleRow->Refresh(qRound(m_interiorAngleA), qRound(m_interiorAngleB));
    }
    if (m_sineCyclesSpin && m_sineAmplitudeSpin) {
        const bool on = sineCurve != nullptr;
        m_sineCyclesSpin->setEnabled(on);
        m_sineAmplitudeSpin->setEnabled(on);
        const int cycles = on ? sineCurve->PropertyValue(ItemProperty::SINE_CYCLES).toInt() : 1;
        const qreal amplitude = on ? sineCurve->PropertyValue(ItemProperty::SINE_AMPLITUDE).toDouble() : 20.0;
        m_sineCyclesSpin->setValue(qBound(1, cycles, 12));
        m_sineAmplitudeSpin->setValue(qBound(1, qRound(amplitude), 200));
    }
    if (m_startArrowCombo && m_endArrowCombo) {
        // 图形族不再有箭头，只有线条族（直线 / 弧线 / 正弦 / 手绘 / 花括号）显示这两行。
        LineAbstractItem* line = strokes.isEmpty() ? nullptr : strokes.first();
        const bool on = line && line->ArrowsEnabled();
        m_startArrowCombo->setEnabled(on);
        m_endArrowCombo->setEnabled(on);
        const ArrowStyle startStyle = on ? line->StartArrow() : ArrowStyle::NONE;
        const ArrowStyle endStyle = on ? line->EndArrow() : ArrowStyle::NONE;
        m_startArrowCombo->setCurrentIndex(qMax(0, m_startArrowCombo->findData(static_cast<int>(startStyle))));
        m_endArrowCombo->setCurrentIndex(qMax(0, m_endArrowCombo->findData(static_cast<int>(endStyle))));
    }
    m_refreshing = false;
}

void PropertiesPanelValueCard::SetCornerVisible(bool visible)
{
    if (m_cornerSection) {
        m_cornerSection->setVisible(visible);
    }
}

void PropertiesPanelValueCard::SetSineVisible(bool visible)
{
    if (m_sineSection) {
        m_sineSection->setVisible(visible);
    }
}

void PropertiesPanelValueCard::SetAxisVisible(bool visible)
{
    if (m_axisRow) {
        m_axisRow->setVisible(visible);
    }
}

void PropertiesPanelValueCard::SetArcVisible(bool visible)
{
    if (m_arcSection) {
        m_arcSection->setVisible(visible);
    }
}

void PropertiesPanelValueCard::SetInteriorAngleVisible(bool visible)
{
    if (m_interiorAngleRow) {
        m_interiorAngleRow->setVisible(visible);
    }
}

void PropertiesPanelValueCard::SetArrowVisible(bool visible)
{
    if (m_arrowSection) {
        m_arrowSection->setVisible(visible);
    }
}

void PropertiesPanelValueCard::ApplyAxes(qreal majorAxis, qreal minorAxis)
{
    // 长半轴不得小于短半轴：改哪根就把另一根钳到同值并回显。
    m_refreshing = true;
    if (majorAxis < minorAxis) {
        minorAxis = majorAxis;
        m_axisRow->Refresh(majorAxis, minorAxis);
    }
    m_refreshing = false;
    emit axesChosen(majorAxis, minorAxis);
}
