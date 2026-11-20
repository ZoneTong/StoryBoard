#include "PropertiesPanelValueCard.h"

#include "ArcItem.h"
#include "CanvasItem.h"
#include "IconFactory.h"
#include "LineItemAbstract.h"
#include "PropertiesPanel.h"
#include "ShapeItem.h"
#include "ShapeMeta.h"
#include "SineItem.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kSmallIconSize = 20;
constexpr int kButtonSize = 32;

QToolButton* makeRowIcon(IconId id, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("ItemRowIcon"));
    button->setIcon(makeIcon(id, QColor(139, 144, 156), kSmallIconSize));
    button->setIconSize(QSize(kSmallIconSize, kSmallIconSize));
    button->setFixedSize(kButtonSize, kButtonSize);
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

    auto* opacityRow = new QWidget(this);
    auto* opacityLayout = new QHBoxLayout(opacityRow);
    opacityLayout->setContentsMargins(0, 0, 0, 0);
    opacityLayout->setSpacing(4);

    QToolButton* opacityIcon = makeRowIcon(IconId::Opacity, opacityRow);
    opacityIcon->setToolTip(PropertiesPanel::tr("Opacity (100% = opaque)"));
    opacityLayout->addWidget(opacityIcon);

    m_opacitySpin = new QSpinBox(opacityRow);
    m_opacitySpin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_opacitySpin->setRange(0, 100);
    m_opacitySpin->setSuffix(QStringLiteral(" %"));
    m_opacitySpin->setToolTip(PropertiesPanel::tr("Opacity (100% = opaque)"));
    // 关闭键盘跟踪：输入过程不触发 valueChanged，回车或失焦才提交，
    // 选中变化触发的 refresh() 也就不会打断正在输入的值。
    m_opacitySpin->setKeyboardTracking(false);
    connect(m_opacitySpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_refreshing) {
            emit opacityChosen(value);
        }
    });
    opacityLayout->addWidget(m_opacitySpin, 1);
    outer->addWidget(opacityRow);

    m_cornerSection = new QWidget(this);
    auto* cornerLayout = new QHBoxLayout(m_cornerSection);
    cornerLayout->setContentsMargins(0, 0, 0, 0);
    cornerLayout->setSpacing(4);

    QToolButton* cornerIcon = makeRowIcon(IconId::Corner, m_cornerSection);
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

    // 需求 9b/9c：起始角在上、扫过角在下，与属性窗的相对次序一致。
    m_startAngleSection = new QWidget(this);
    auto* startLayout = new QHBoxLayout(m_startAngleSection);
    startLayout->setContentsMargins(0, 0, 0, 0);
    startLayout->setSpacing(4);
    QToolButton* startIcon = makeRowIcon(IconId::RotateCcw, m_startAngleSection);
    startIcon->setToolTip(PropertiesPanel::tr("Start angle"));
    startLayout->addWidget(startIcon);
    m_startAngleSpin = new QSpinBox(m_startAngleSection);
    m_startAngleSpin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_startAngleSpin->setRange(-360, 360);
    m_startAngleSpin->setSuffix(QStringLiteral("°"));
    m_startAngleSpin->setToolTip(PropertiesPanel::tr("Start angle"));
    m_startAngleSpin->setKeyboardTracking(false);
    connect(m_startAngleSpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_refreshing) {
            emit startAngleChosen(value);
        }
    });
    startLayout->addWidget(m_startAngleSpin, 1);
    outer->addWidget(m_startAngleSection);

    m_angleSection = new QWidget(this);
    auto* angleLayout = new QHBoxLayout(m_angleSection);
    angleLayout->setContentsMargins(0, 0, 0, 0);
    angleLayout->setSpacing(4);

    QToolButton* angleIcon = makeRowIcon(IconId::RotateCcw, m_angleSection);
    angleIcon->setToolTip(PropertiesPanel::tr("Sweep angle"));
    angleLayout->addWidget(angleIcon);

    m_angleSpin = new QSpinBox(m_angleSection);
    m_angleSpin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_angleSpin->setRange(1, 359);
    m_angleSpin->setSuffix(QStringLiteral("°"));
    m_angleSpin->setToolTip(PropertiesPanel::tr("Sweep angle"));
    m_angleSpin->setKeyboardTracking(false);
    connect(m_angleSpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_refreshing) {
            emit sweepAngleChosen(value);
        }
    });
    angleLayout->addWidget(m_angleSpin, 1);
    outer->addWidget(m_angleSection);

    // 正弦波形：周期数与振幅是几何真相，改参数即按新参数重采样（拉伸不改参数）。
    m_sineSection = new QWidget(this);
    auto* sineOuter = new QVBoxLayout(m_sineSection);
    sineOuter->setContentsMargins(0, 0, 0, 0);
    sineOuter->setSpacing(2);

    auto* cyclesRow = new QWidget(m_sineSection);
    auto* cyclesLayout = new QHBoxLayout(cyclesRow);
    cyclesLayout->setContentsMargins(0, 0, 0, 0);
    cyclesLayout->setSpacing(4);
    QToolButton* cyclesIcon = makeRowIcon(IconId::RotateCcw, cyclesRow);
    cyclesIcon->setToolTip(PropertiesPanel::tr("Sine cycles"));
    cyclesLayout->addWidget(cyclesIcon);
    m_sineCyclesSpin = new QSpinBox(cyclesRow);
    m_sineCyclesSpin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_sineCyclesSpin->setRange(1, 12);
    m_sineCyclesSpin->setToolTip(PropertiesPanel::tr("Sine cycles"));
    m_sineCyclesSpin->setKeyboardTracking(false);
    connect(m_sineCyclesSpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_refreshing) {
            emit sineCyclesChosen(value);
        }
    });
    cyclesLayout->addWidget(m_sineCyclesSpin, 1);
    sineOuter->addWidget(cyclesRow);

    auto* amplitudeRow = new QWidget(m_sineSection);
    auto* amplitudeLayout = new QHBoxLayout(amplitudeRow);
    amplitudeLayout->setContentsMargins(0, 0, 0, 0);
    amplitudeLayout->setSpacing(4);
    QToolButton* amplitudeIcon = makeRowIcon(IconId::RotateCcw, amplitudeRow);
    amplitudeIcon->setToolTip(PropertiesPanel::tr("Sine amplitude"));
    amplitudeLayout->addWidget(amplitudeIcon);
    m_sineAmplitudeSpin = new QSpinBox(amplitudeRow);
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
    amplitudeLayout->addWidget(m_sineAmplitudeSpin, 1);
    sineOuter->addWidget(amplitudeRow);
    outer->addWidget(m_sineSection);

    // 需求 1：三角形以角度为真相——底边长度、角 A、角 B 三行
    // 直接决定当前几何，高度由底边与两个底角派生（拖高等价于等比改底边）。
    m_triangleSection = new QWidget(this);
    auto* triangleOuter = new QVBoxLayout(m_triangleSection);
    triangleOuter->setContentsMargins(0, 0, 0, 0);
    triangleOuter->setSpacing(2);

    auto* baseRow = new QWidget(m_triangleSection);
    auto* baseLayout = new QHBoxLayout(baseRow);
    baseLayout->setContentsMargins(0, 0, 0, 0);
    baseLayout->setSpacing(4);
    QToolButton* baseIcon = makeRowIcon(IconId::TriangleBase, baseRow);
    baseIcon->setToolTip(PropertiesPanel::tr("Triangle base length"));
    baseLayout->addWidget(baseIcon);
    m_triangleBaseSpin = new QSpinBox(baseRow);
    m_triangleBaseSpin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_triangleBaseSpin->setRange(8, 2000);
    m_triangleBaseSpin->setSuffix(QStringLiteral(" px"));
    m_triangleBaseSpin->setToolTip(PropertiesPanel::tr("Triangle base length"));
    m_triangleBaseSpin->setKeyboardTracking(false);
    connect(m_triangleBaseSpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_refreshing) {
            emit triangleBaseChosen(value);
        }
    });
    baseLayout->addWidget(m_triangleBaseSpin, 1);
    triangleOuter->addWidget(baseRow);

    auto* angle1Row = new QWidget(m_triangleSection);
    auto* angle1Layout = new QHBoxLayout(angle1Row);
    angle1Layout->setContentsMargins(0, 0, 0, 0);
    angle1Layout->setSpacing(4);
    QToolButton* angle1Icon = makeRowIcon(IconId::AngleRight, angle1Row);
    angle1Icon->setToolTip(PropertiesPanel::tr("Triangle base angle A"));
    angle1Layout->addWidget(angle1Icon);
    m_triangleAngle1Spin = new QSpinBox(angle1Row);
    m_triangleAngle1Spin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_triangleAngle1Spin->setRange(1, 174);
    m_triangleAngle1Spin->setSuffix(QStringLiteral("°"));
    m_triangleAngle1Spin->setToolTip(PropertiesPanel::tr("Triangle base angle A"));
    m_triangleAngle1Spin->setKeyboardTracking(false);
    connect(m_triangleAngle1Spin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_refreshing) {
            emit triangleAngle1Chosen(value);
        }
    });
    angle1Layout->addWidget(m_triangleAngle1Spin, 1);
    triangleOuter->addWidget(angle1Row);

    auto* angle2Row = new QWidget(m_triangleSection);
    auto* angle2Layout = new QHBoxLayout(angle2Row);
    angle2Layout->setContentsMargins(0, 0, 0, 0);
    angle2Layout->setSpacing(4);
    QToolButton* angle2Icon = makeRowIcon(IconId::AngleLeft, angle2Row);
    angle2Icon->setToolTip(PropertiesPanel::tr("Triangle base angle B"));
    angle2Layout->addWidget(angle2Icon);
    m_triangleAngle2Spin = new QSpinBox(angle2Row);
    m_triangleAngle2Spin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_triangleAngle2Spin->setRange(1, 174);
    m_triangleAngle2Spin->setSuffix(QStringLiteral("°"));
    m_triangleAngle2Spin->setToolTip(PropertiesPanel::tr("Triangle base angle B"));
    m_triangleAngle2Spin->setKeyboardTracking(false);
    connect(m_triangleAngle2Spin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_refreshing) {
            emit triangleAngle2Chosen(value);
        }
    });
    angle2Layout->addWidget(m_triangleAngle2Spin, 1);
    triangleOuter->addWidget(angle2Row);

    m_triangleSection->setVisible(false);
    outer->addWidget(m_triangleSection);

    // 需求 6：直线两端箭头样式，两行下拉（起始 / 末端）。
    m_arrowSection = new QWidget(this);
    auto* arrowOuter = new QVBoxLayout(m_arrowSection);
    arrowOuter->setContentsMargins(0, 0, 0, 0);
    arrowOuter->setSpacing(2);

    auto makeArrowRow = [this, arrowOuter](IconId iconId, const QString& tooltip, QComboBox** target,
                                           void (PropertiesPanelValueCard::*signal)(int)) {
        auto* row = new QWidget(m_arrowSection);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);
        QToolButton* icon = makeRowIcon(iconId, row);
        icon->setToolTip(tooltip);
        rowLayout->addWidget(icon);
        QComboBox* combo = new QComboBox(row);
        combo->setObjectName(QStringLiteral("ItemArrowCombo"));
        combo->setToolTip(tooltip);
        for (ArrowStyle style : allArrowStyles()) {
            combo->addItem(arrowStyleName(style), static_cast<int>(style));
        }
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, combo, signal](int) {
            if (!m_refreshing) {
                (this->*signal)(combo->currentData().toInt());
            }
        });
        rowLayout->addWidget(combo, 1);
        arrowOuter->addWidget(row);
        *target = combo;
    };
    makeArrowRow(IconId::ArrowStart, PropertiesPanel::tr("Start arrow"), &m_startArrowCombo,
                 static_cast<void (PropertiesPanelValueCard::*)(int)>(&PropertiesPanelValueCard::arrowStartChosen));
    makeArrowRow(IconId::ArrowEnd, PropertiesPanel::tr("End arrow"), &m_endArrowCombo,
                 static_cast<void (PropertiesPanelValueCard::*)(int)>(&PropertiesPanelValueCard::arrowEndChosen));
    m_arrowSection->setVisible(false);
    outer->addWidget(m_arrowSection);
}

void PropertiesPanelValueCard::refresh(const QList<ShapeItem*>& shapes, const QList<LineItemAbstract*>& strokes,
                                       const QList<CanvasItem*>& items)
{
    m_refreshing = true;

    const bool enabled = !shapes.isEmpty();
    if (m_opacitySpin) {
        m_opacitySpin->setEnabled(!items.isEmpty());
        m_opacitySpin->setValue(items.isEmpty() ? 100 : qRound(items.first()->itemOpacity() * 100.0));
    }
    if (m_cornerSpin) {
        m_cornerSpin->setEnabled(enabled);
        // 需求 4：圆角上限统一为短边一半，不再写死 400。
        const QSizeF size = shapes.isEmpty() ? QSizeF() : shapes.first()->contentSize();
        const int limit = (size.width() > 0.0 && size.height() > 0.0)
                              ? qMax(1, qRound(qMin(size.width(), size.height()) / 2.0))
                              : 400;
        m_cornerSpin->setMaximum(limit);
        const qreal radius = shapes.isEmpty() ? 0.0 : qMax<qreal>(0.0, shapes.first()->cornerRadius());
        m_cornerSpin->setValue(qBound(0, qRound(radius), limit));
    }
    // 弧线角度：扇形等形状与曲线族里的弧线共用这两行，优先回显当前选中的弧线曲线。
    LineItemAbstract* arcCurve = nullptr;
    ShapeItem* arcShape = nullptr;
    LineItemAbstract* sineCurve = nullptr;
    for (LineItemAbstract* stroke : strokes) {
        if (stroke->supportsArcAngles() && !arcCurve) {
            arcCurve = stroke;
        } else if (stroke->supportsSineWave() && !sineCurve) {
            sineCurve = stroke;
        }
    }
    for (ShapeItem* shape : shapes) {
        if (shape && shape->shapeType() == ShapeType::Sector) {
            arcShape = shape;
            break;
        }
    }
    if (m_startAngleSpin) {
        const bool on = arcCurve || arcShape;
        m_startAngleSpin->setEnabled(on);
        const qreal start = arcCurve ? arcCurve->paramState().first : (arcShape ? arcShape->startAngle() : 30.0);
        m_startAngleSpin->setValue(qRound(start));
    }
    if (m_angleSpin) {
        const bool on = arcCurve || arcShape;
        m_angleSpin->setEnabled(on);
        const qreal sweep = arcCurve ? arcCurve->paramState().second : (arcShape ? arcShape->sweepAngle() : 120.0);
        m_angleSpin->setValue(qMax(1, qRound(sweep)));
    }
    if (m_sineCyclesSpin && m_sineAmplitudeSpin) {
        const bool on = sineCurve != nullptr;
        m_sineCyclesSpin->setEnabled(on);
        m_sineAmplitudeSpin->setEnabled(on);
        const LineItemAbstract::ParamState params = on ? sineCurve->paramState() : LineItemAbstract::ParamState{};
        m_sineCyclesSpin->setValue(qBound(1, qRound(params.first), 12));
        m_sineAmplitudeSpin->setValue(qBound(1, qRound(params.second), 200));
    }

    ShapeItem* triangle = nullptr;
    for (ShapeItem* shape : shapes) {
        if (shape && shape->shapeType() == ShapeType::Triangle && !shape->isCustomShape()) {
            triangle = shape;
            break;
        }
    }
    if (m_triangleBaseSpin && m_triangleAngle1Spin && m_triangleAngle2Spin) {
        const bool triangleOn = triangle != nullptr;
        m_triangleBaseSpin->setEnabled(triangleOn);
        m_triangleAngle1Spin->setEnabled(triangleOn);
        m_triangleAngle2Spin->setEnabled(triangleOn);
        // 底边即内容矩形宽度；角度直接回显当前值，未选中三角形时给默认 64 / 60° / 60°。
        m_triangleBaseSpin->setValue(triangleOn ? qRound(triangle->contentSize().width()) : 64);
        m_triangleAngle1Spin->setValue(triangleOn ? qRound(triangle->bottomAngle1()) : 60);
        m_triangleAngle2Spin->setValue(triangleOn ? qRound(triangle->bottomAngle2()) : 60);
    }

    if (m_startArrowCombo && m_endArrowCombo) {
        // 图形族不再有箭头，只有线条族（直线 / 弧线 / 正弦 / 手绘 / 花括号）显示这两行。
        LineItemAbstract* line = strokes.isEmpty() ? nullptr : strokes.first();
        const bool on = line && line->arrowsEnabled();
        m_startArrowCombo->setEnabled(on);
        m_endArrowCombo->setEnabled(on);
        const ArrowStyle startStyle = on ? line->startArrow() : ArrowStyle::None;
        const ArrowStyle endStyle = on ? line->endArrow() : ArrowStyle::None;
        m_startArrowCombo->setCurrentIndex(qMax(0, m_startArrowCombo->findData(static_cast<int>(startStyle))));
        m_endArrowCombo->setCurrentIndex(qMax(0, m_endArrowCombo->findData(static_cast<int>(endStyle))));
    }

    m_refreshing = false;
}

void PropertiesPanelValueCard::setArrowVisible(bool visible)
{
    if (m_arrowSection) {
        m_arrowSection->setVisible(visible);
    }
}

void PropertiesPanelValueCard::setTriangleVisible(bool visible)
{
    if (m_triangleSection) {
        m_triangleSection->setVisible(visible);
    }
}

void PropertiesPanelValueCard::setCornerVisible(bool visible)
{
    if (m_cornerSection) {
        m_cornerSection->setVisible(visible);
    }
}

void PropertiesPanelValueCard::setSineVisible(bool visible)
{
    if (m_sineSection) {
        m_sineSection->setVisible(visible);
    }
}

void PropertiesPanelValueCard::setArcRadiusVisible(bool visible)
{
    if (m_startAngleSection) {
        m_startAngleSection->setVisible(visible);
    }
    if (m_angleSection) {
        m_angleSection->setVisible(visible);
    }
}
