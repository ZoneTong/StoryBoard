#include "ItemDockPanelValueCard.h"

#include "CanvasItem.h"
#include "CurveItem.h"
#include "IconFactory.h"
#include "ItemDockPanel.h"
#include "ShapeItem.h"
#include "ShapeMeta.h"

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

ItemDockPanelValueCard::ItemDockPanelValueCard(QWidget* parent) : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(2);

    auto* opacityRow = new QWidget(this);
    auto* opacityLayout = new QHBoxLayout(opacityRow);
    opacityLayout->setContentsMargins(0, 0, 0, 0);
    opacityLayout->setSpacing(4);

    QToolButton* opacityIcon = makeRowIcon(IconId::Opacity, opacityRow);
    opacityIcon->setToolTip(ItemDockPanel::tr("Opacity (100% = opaque)"));
    opacityLayout->addWidget(opacityIcon);

    m_opacitySpin = new QSpinBox(opacityRow);
    m_opacitySpin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_opacitySpin->setRange(0, 100);
    m_opacitySpin->setSuffix(QStringLiteral(" %"));
    m_opacitySpin->setToolTip(ItemDockPanel::tr("Opacity (100% = opaque)"));
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
    cornerIcon->setToolTip(ItemDockPanel::tr("Corner radius"));
    cornerLayout->addWidget(cornerIcon);

    m_cornerSpin = new QSpinBox(m_cornerSection);
    m_cornerSpin->setObjectName(QStringLiteral("ItemValueCornerSpin"));
    m_cornerSpin->setRange(0, 400);
    m_cornerSpin->setSuffix(QStringLiteral(" px"));
    m_cornerSpin->setToolTip(ItemDockPanel::tr("Corner radius"));
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
    startIcon->setToolTip(ItemDockPanel::tr("Start angle"));
    startLayout->addWidget(startIcon);
    m_startAngleSpin = new QSpinBox(m_startAngleSection);
    m_startAngleSpin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_startAngleSpin->setRange(-360, 360);
    m_startAngleSpin->setSuffix(QStringLiteral("°"));
    m_startAngleSpin->setToolTip(ItemDockPanel::tr("Start angle"));
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
    angleIcon->setToolTip(ItemDockPanel::tr("Sweep angle"));
    angleLayout->addWidget(angleIcon);

    m_angleSpin = new QSpinBox(m_angleSection);
    m_angleSpin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_angleSpin->setRange(1, 359);
    m_angleSpin->setSuffix(QStringLiteral("°"));
    m_angleSpin->setToolTip(ItemDockPanel::tr("Sweep angle"));
    m_angleSpin->setKeyboardTracking(false);
    connect(m_angleSpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_refreshing) {
            emit sweepAngleChosen(value);
        }
    });
    angleLayout->addWidget(m_angleSpin, 1);
    outer->addWidget(m_angleSection);

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
    baseIcon->setToolTip(ItemDockPanel::tr("Triangle base length"));
    baseLayout->addWidget(baseIcon);
    m_triangleBaseSpin = new QSpinBox(baseRow);
    m_triangleBaseSpin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_triangleBaseSpin->setRange(8, 2000);
    m_triangleBaseSpin->setSuffix(QStringLiteral(" px"));
    m_triangleBaseSpin->setToolTip(ItemDockPanel::tr("Triangle base length"));
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
    angle1Icon->setToolTip(ItemDockPanel::tr("Triangle base angle A"));
    angle1Layout->addWidget(angle1Icon);
    m_triangleAngle1Spin = new QSpinBox(angle1Row);
    m_triangleAngle1Spin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_triangleAngle1Spin->setRange(1, 174);
    m_triangleAngle1Spin->setSuffix(QStringLiteral("°"));
    m_triangleAngle1Spin->setToolTip(ItemDockPanel::tr("Triangle base angle A"));
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
    angle2Icon->setToolTip(ItemDockPanel::tr("Triangle base angle B"));
    angle2Layout->addWidget(angle2Icon);
    m_triangleAngle2Spin = new QSpinBox(angle2Row);
    m_triangleAngle2Spin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_triangleAngle2Spin->setRange(1, 174);
    m_triangleAngle2Spin->setSuffix(QStringLiteral("°"));
    m_triangleAngle2Spin->setToolTip(ItemDockPanel::tr("Triangle base angle B"));
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
                                           void (ItemDockPanelValueCard::*signal)(int)) {
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
    makeArrowRow(IconId::ArrowStart, ItemDockPanel::tr("Start arrow"), &m_startArrowCombo,
                 static_cast<void (ItemDockPanelValueCard::*)(int)>(&ItemDockPanelValueCard::arrowStartChosen));
    makeArrowRow(IconId::ArrowEnd, ItemDockPanel::tr("End arrow"), &m_endArrowCombo,
                 static_cast<void (ItemDockPanelValueCard::*)(int)>(&ItemDockPanelValueCard::arrowEndChosen));
    m_arrowSection->setVisible(false);
    outer->addWidget(m_arrowSection);
}

void ItemDockPanelValueCard::refresh(const QList<ShapeItem*>& shapes, const QList<CurveItem*>& strokes,
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
    if (m_startAngleSpin) {
        m_startAngleSpin->setEnabled(enabled);
        m_startAngleSpin->setValue(shapes.isEmpty() ? 30 : qRound(shapes.first()->startAngle()));
    }
    if (m_angleSpin) {
        m_angleSpin->setEnabled(enabled);
        m_angleSpin->setValue(shapes.isEmpty() ? 120 : qRound(shapes.first()->sweepAngle()));
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
        ShapeItem* line = nullptr;
        for (ShapeItem* shape : shapes) {
            if (shape && ShapeMeta::isLineLike(shape->shapeType()) && !shape->isCustomShape()) {
                line = shape;
                break;
            }
        }
        // 需求 9b：直线与曲线共用这两行；花括号是精确采样曲线，不显示箭头。
        CurveItem* curve = nullptr;
        for (CurveItem* stroke : strokes) {
            if (stroke && stroke->source() != CurveSource::Brace) {
                curve = stroke;
                break;
            }
        }
        const bool lineOn = line != nullptr;
        const bool curveOn = curve != nullptr;
        m_startArrowCombo->setEnabled(lineOn || curveOn);
        m_endArrowCombo->setEnabled(lineOn || curveOn);
        const ArrowStyle startStyle = lineOn ? line->startArrow() : (curveOn ? curve->startArrow() : ArrowStyle::None);
        const ArrowStyle endStyle = lineOn ? line->endArrow() : (curveOn ? curve->endArrow() : ArrowStyle::None);
        m_startArrowCombo->setCurrentIndex(qMax(0, m_startArrowCombo->findData(static_cast<int>(startStyle))));
        m_endArrowCombo->setCurrentIndex(qMax(0, m_endArrowCombo->findData(static_cast<int>(endStyle))));
    }

    m_refreshing = false;
}

void ItemDockPanelValueCard::setArrowVisible(bool visible)
{
    if (m_arrowSection) {
        m_arrowSection->setVisible(visible);
    }
}

void ItemDockPanelValueCard::setTriangleVisible(bool visible)
{
    if (m_triangleSection) {
        m_triangleSection->setVisible(visible);
    }
}

void ItemDockPanelValueCard::setCornerVisible(bool visible)
{
    if (m_cornerSection) {
        m_cornerSection->setVisible(visible);
    }
}

void ItemDockPanelValueCard::setArcRadiusVisible(bool visible)
{
    if (m_startAngleSection) {
        m_startAngleSection->setVisible(visible);
    }
    if (m_angleSection) {
        m_angleSection->setVisible(visible);
    }
}
