#include "ui/panels/itemdialog/ItemPropertiesDialogStyleBlock.h"

#include "items/geometry/ShapeMeta.h"
#include "items/lines/LineAbstractItem.h"
#include "items/polygon/PolygonItem.h"
#include "items/properties/ItemStyleVisuals.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "ui/panels/itemdialog/ItemPropertiesDialog.h"
#include "ui/panels/properties/PropertyRowRegistry.h"
#include "ui/widgets/AttributeRowWidgets.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QToolButton>

ItemPropertiesDialogStyleBlock::ItemPropertiesDialogStyleBlock(QWidget* parent) : QWidget(parent)
{
    BuildFillRows();
    BuildStrokeRows();
    BuildTypeRows();
}

void ItemPropertiesDialogStyleBlock::BuildFillRows()
{
    m_fillStyleRow = new FillStyleRow(this);
    // 属性窗行首不放图标：行标题已说明属性名，控件区更紧凑。
    SetRowIconVisible(m_fillStyleRow, false);
    // 行控件构造时即绑定私有属性副本；SetItem 只负责回填与显隐。
    m_fillStyleRow->SetTargets({&m_fillAttributes});
    connect(m_fillStyleRow, &FillStyleRow::fillStyleChosen, this, [this](FillStyle style) {
        m_fillAttributes.SetFillStyle(style);
        m_fillStyleRow->Refresh();
    });

    m_fillColorRow = new FillColorRow(this);
    SetRowIconVisible(m_fillColorRow, false);
    m_fillColorRow->SetTargets({&m_fillAttributes});
    connect(m_fillColorRow, &FillColorRow::fillColorChosen, this, [this](const QColor& color) {
        m_fillAttributes.SetFill(color, m_fillAttributes.fillStyle());
        m_fillColorRow->Refresh();
    });

    // 需求 15：独立「填充图片」属性，不进 FillStyle；图片透明处露出填充色 / 图案。
    m_fillImageRow = new FillImageRow(this);
    SetRowIconVisible(m_fillImageRow, false);
    m_fillImageRow->SetTargets({&m_fillAttributes});
    connect(m_fillImageRow, &FillImageRow::chooseFillImageClicked, this, [this]() {
        const QString path =
            QFileDialog::getOpenFileName(this, ItemPropertiesDialog::tr("Choose fill image"), QString(),
                                         ItemPropertiesDialog::tr("Images (*.png *.jpg *.jpeg *.bmp *.webp *.gif)"));
        if (path.isEmpty()) {
            return;
        }
        QPixmap image(path);
        if (image.isNull()) {
            return;
        }
        m_fillAttributes.SetFillImage(image, m_fillAttributes.FillImageMode());
        m_fillImageRow->Refresh();
    });
    connect(m_fillImageRow, &FillImageRow::fillImageModeChosen, this, [this](int mode) {
        if (!m_fillAttributes.HasFillImage()) {
            return;
        }
        m_fillAttributes.SetFillImage(m_fillAttributes.FillImage(), static_cast<ImageFill::Mode>(mode));
        m_fillImageRow->Refresh();
    });
    connect(m_fillImageRow, &FillImageRow::clearFillImageClicked, this, [this]() {
        m_fillAttributes.ClearFillImage();
        m_fillImageRow->Refresh();
    });
}

void ItemPropertiesDialogStyleBlock::BuildStrokeRows()
{
    m_borderStyleRow = new StrokeStyleRow(this);
    SetRowIconVisible(m_borderStyleRow, false);
    m_borderStyleRow->SetTargets({&m_strokeAttributes});
    connect(m_borderStyleRow, &StrokeStyleRow::borderStyleChosen, this, [this](Qt::PenStyle style) {
        m_strokeAttributes.SetStrokeStyle(style);
        m_borderStyleRow->Refresh();
    });

    m_strokeColorRow = new StrokeColorRow(this);
    SetRowIconVisible(m_strokeColorRow, false);
    m_strokeColorRow->SetTargets({&m_strokeAttributes});
    connect(m_strokeColorRow, &StrokeColorRow::strokeColorChosen, this, [this](const QColor& color) {
        m_strokeAttributes.SetStrokeColor(color);
        m_strokeColorRow->Refresh();
    });

    m_borderWidthRow = new StrokeWidthRow(this);
    SetRowIconVisible(m_borderWidthRow, false);
    m_borderWidthRow->SetTargets({&m_strokeAttributes});
    connect(m_borderWidthRow, &StrokeWidthRow::borderWidthChosen, this, [this](qreal width) {
        m_strokeAttributes.SetStrokeWidth(width);
        m_borderWidthRow->Refresh();
    });

    m_opacitySpin = new QSpinBox(this);
    m_opacitySpin->setObjectName(QStringLiteral("ItemPropertiesOpacityValue"));
    m_opacitySpin->setRange(0, 100);
    m_opacitySpin->setSuffix(QStringLiteral(" %"));
    m_opacitySpin->setMinimumWidth(88);
}

void ItemPropertiesDialogStyleBlock::BuildTypeRows()
{
    m_cornerRadiusSpin = new QSpinBox(this);
    m_cornerRadiusSpin->setObjectName(QStringLiteral("ItemPropertiesCornerRadiusValue"));
    m_cornerRadiusSpin->setRange(0, 500000);
    m_cornerRadiusSpin->setSuffix(QStringLiteral(" px"));
    m_cornerRadiusSpin->setToolTip(ItemPropertiesDialog::tr("Corner radius"));
    m_cornerRadiusSpin->setMinimumWidth(88);
    connect(m_cornerRadiusSpin, &QSpinBox::valueChanged, this,
            [this](int value) { m_cornerEdited = value != m_initialCornerRadius; });

    // 内角 A / B 两行由三角形与平行四边形共用（§4.2）：同一控件按目标回填与提交。
    m_interiorAngleASpin = new QSpinBox(this);
    m_interiorAngleASpin->setObjectName(QStringLiteral("ItemPropertiesInteriorAngleAValue"));
    m_interiorAngleASpin->setRange(5, 175);
    m_interiorAngleASpin->setSingleStep(1);
    m_interiorAngleASpin->setSuffix(QStringLiteral("°"));
    m_interiorAngleASpin->setToolTip(ItemPropertiesDialog::tr("Interior angle A"));
    m_interiorAngleASpin->setMinimumWidth(88);

    m_interiorAngleBSpin = new QSpinBox(this);
    m_interiorAngleBSpin->setObjectName(QStringLiteral("ItemPropertiesInteriorAngleBValue"));
    m_interiorAngleBSpin->setRange(5, 170);
    m_interiorAngleBSpin->setSingleStep(1);
    m_interiorAngleBSpin->setSuffix(QStringLiteral("°"));
    m_interiorAngleBSpin->setToolTip(ItemPropertiesDialog::tr("Interior angle B"));
    m_interiorAngleBSpin->setMinimumWidth(88);

    m_arrowRow = new QWidget(this);
    auto* arrowLayout = new QHBoxLayout(m_arrowRow);
    arrowLayout->setContentsMargins(0, 0, 0, 0);
    arrowLayout->setSpacing(6);
    m_startArrowCombo = new QComboBox(m_arrowRow);
    m_startArrowCombo->setObjectName(QStringLiteral("ItemPropertiesStartArrowValue"));
    m_startArrowCombo->setMinimumWidth(120);
    m_startArrowCombo->setToolTip(ItemPropertiesDialog::tr("Start arrow"));
    m_endArrowCombo = new QComboBox(m_arrowRow);
    m_endArrowCombo->setObjectName(QStringLiteral("ItemPropertiesEndArrowValue"));
    m_endArrowCombo->setMinimumWidth(120);
    m_endArrowCombo->setToolTip(ItemPropertiesDialog::tr("End arrow"));
    for (QComboBox* combo : {m_startArrowCombo, m_endArrowCombo}) {
        for (ArrowStyle arrowStyle : AllArrowStyles()) {
            combo->addItem(ArrowStyleName(arrowStyle), static_cast<int>(arrowStyle));
        }
    }
    arrowLayout->addWidget(m_startArrowCombo, 1);
    arrowLayout->addWidget(m_endArrowCombo, 1);

    // 起始角 / 扫过角：扇形与弧线共用同一对控件，值统一按属性读写。
    m_startAngleSpin = new QSpinBox(this);
    m_startAngleSpin->setObjectName(QStringLiteral("ItemPropertiesStartAngleValue"));
    m_startAngleSpin->setRange(-360, 360);
    m_startAngleSpin->setSingleStep(5);
    m_startAngleSpin->setSuffix(QStringLiteral("°"));
    m_startAngleSpin->setToolTip(ItemPropertiesDialog::tr("Start angle"));
    m_startAngleSpin->setMinimumWidth(88);

    m_sweepAngleSpin = new QSpinBox(this);
    m_sweepAngleSpin->setObjectName(QStringLiteral("ItemPropertiesSweepAngleValue"));
    m_sweepAngleSpin->setRange(-360, 360);
    m_sweepAngleSpin->setSingleStep(5);
    m_sweepAngleSpin->setSuffix(QStringLiteral("°"));
    m_sweepAngleSpin->setToolTip(ItemPropertiesDialog::tr("Sweep angle"));
    m_sweepAngleSpin->setMinimumWidth(88);

    m_sineCyclesSpin = new QSpinBox(this);
    m_sineCyclesSpin->setObjectName(QStringLiteral("ItemPropertiesSineCyclesValue"));
    m_sineCyclesSpin->setRange(1, 12);
    m_sineCyclesSpin->setMinimumWidth(88);

    m_sineAmplitudeSpin = new QSpinBox(this);
    m_sineAmplitudeSpin->setObjectName(QStringLiteral("ItemPropertiesSineAmplitudeValue"));
    m_sineAmplitudeSpin->setRange(1, 200);
    m_sineAmplitudeSpin->setSuffix(QStringLiteral(" px"));
    m_sineAmplitudeSpin->setMinimumWidth(88);
}

void ItemPropertiesDialogStyleBlock::AddRows(QFormLayout* form, PropertyRowRegistry* rows)
{
    m_form = form;
    // 行序固定（§4.1 第 8~22 行）：填充 → 描线 → 不透明度 → 圆角 →
    // 内角 A/B → 箭头 → 起始 / 扫过角 → 正弦周期 / 振幅。
    form->addRow(ItemPropertiesDialog::tr("Fill style"), m_fillStyleRow);
    form->addRow(ItemPropertiesDialog::tr("Fill color"), m_fillColorRow);
    form->addRow(ItemPropertiesDialog::tr("Fill image"), m_fillImageRow);
    form->addRow(ItemPropertiesDialog::tr("Border style"), m_borderStyleRow);
    form->addRow(ItemPropertiesDialog::tr("Border color"), m_strokeColorRow);
    form->addRow(ItemPropertiesDialog::tr("Border width"), m_borderWidthRow);
    m_opacitySpin->setToolTip(ItemPropertiesDialog::tr("Opacity (100% = opaque)"));
    form->addRow(ItemPropertiesDialog::tr("Opacity"), m_opacitySpin);
    form->addRow(ItemPropertiesDialog::tr("Corner radius"), m_cornerRadiusSpin);
    form->addRow(ItemPropertiesDialog::tr("Interior angle A"), m_interiorAngleASpin);
    form->addRow(ItemPropertiesDialog::tr("Interior angle B"), m_interiorAngleBSpin);
    form->addRow(ItemPropertiesDialog::tr("Arrow heads"), m_arrowRow);
    form->addRow(ItemPropertiesDialog::tr("Start angle"), m_startAngleSpin);
    form->addRow(ItemPropertiesDialog::tr("Sweep angle"), m_sweepAngleSpin);
    form->addRow(ItemPropertiesDialog::tr("Sine cycles"), m_sineCyclesSpin);
    form->addRow(ItemPropertiesDialog::tr("Sine amplitude"), m_sineAmplitudeSpin);

    if (rows) {
        rows->Register(ItemProperty::FILL_STYLE, m_fillStyleRow);
        rows->Register(ItemProperty::FILL_COLOR, m_fillColorRow);
        rows->Register(ItemProperty::FILL_IMAGE, m_fillImageRow);
        rows->Register(ItemProperty::STROKE_STYLE, m_borderStyleRow);
        rows->Register(ItemProperty::STROKE_COLOR, m_strokeColorRow);
        rows->Register(ItemProperty::STROKE_WIDTH, m_borderWidthRow);
        rows->Register(ItemProperty::OPACITY, m_opacitySpin);
        rows->Register(ItemProperty::CORNER_RADIUS, m_cornerRadiusSpin);
        // 内角 A 接受三角形底角 A 与平行四边形内角，两者互斥。
        rows->Register(ItemPropertySet{ItemProperty::BOTTOM_ANGLE1, ItemProperty::PARALLELOGRAM_ANGLE},
                       m_interiorAngleASpin);
        rows->Register(ItemProperty::BOTTOM_ANGLE2, m_interiorAngleBSpin);
        rows->Register(ItemProperty::ARROW_START, m_arrowRow);
        rows->Register(ItemProperty::START_ANGLE, m_startAngleSpin);
        rows->Register(ItemProperty::SWEEP_ANGLE, m_sweepAngleSpin);
        rows->Register(ItemProperty::SINE_CYCLES, m_sineCyclesSpin);
        rows->Register(ItemProperty::SINE_AMPLITUDE, m_sineAmplitudeSpin);
    }
}

void ItemPropertiesDialogStyleBlock::SetItem(CanvasItem* item)
{
    m_hasFill = false;
    m_initialStrokeWidth = 4.0;
    m_cornerEdited = false;
    m_interiorAngleTarget = InteriorAngleTarget::NONE;
    m_initialInteriorAngleA = 60.0;
    m_initialInteriorAngleB = 60.0;
    m_initialStartAngle = 0.0;
    m_initialSweepAngle = 0.0;

    if (auto* shape = dynamic_cast<ShapeAbstractItem*>(item)) {
        // 多边形派生自 ShapeAbstractItem 但没有形状类型：填充与描线照旧，圆角 / 角度等行按属性表。
        const ItemPropertySet properties = shape->Properties();
        m_hasFill = properties.Contains(ItemProperty::FILL_STYLE);
        if (m_hasFill) {
            m_fillAttributes.SetFill(shape->FillColor(), shape->fillStyle());
            m_fillAttributes.SetFillImage(shape->FillImage(), shape->FillImageMode());
        }
        m_strokeAttributes.SetStroke(shape->StrokeColor(), shape->StrokeWidth());
        m_strokeAttributes.SetStrokeStyle(shape->StrokeStyle());
        m_initialStrokeWidth = m_strokeAttributes.StrokeWidth();

        // 圆角行由属性表决定是否出现；这里只负责回填上限与默认比例。
        m_contentSize = shape->ContentSize();
        const qreal radius = shape->CornerRadius();
        const qreal minSide = qMin(m_contentSize.width(), m_contentSize.height());
        m_defaultCornerRatio = minSide > 0.0 ? radius / minSide : 0.0;
        m_initialStoredCornerRadius = radius;
        m_initialCornerRadius = qRound(radius);
        m_cornerRadiusSpin->setMaximum(qMax(0, qRound(minSide / 2.0)));
        m_cornerRadiusSpin->setValue(m_initialCornerRadius);

        // 内角 A 按属性表挑提交目标：三角形底角 A 为真值，平行四边形 / 菱形同用一行。
        if (properties.Contains(ItemProperty::BOTTOM_ANGLE1)) {
            m_interiorAngleTarget = InteriorAngleTarget::TRIANGLE_A;
            m_initialInteriorAngleA = shape->BottomAngle1();
            m_initialInteriorAngleB = shape->BottomAngle2();
        } else if (properties.Contains(ItemProperty::PARALLELOGRAM_ANGLE)) {
            m_interiorAngleTarget = InteriorAngleTarget::PARALLELOGRAM;
            m_initialInteriorAngleA = shape->PropertyValue(ItemProperty::PARALLELOGRAM_ANGLE).toDouble();
            m_initialInteriorAngleB = 60.0;
        }
        m_interiorAngleASpin->setValue(qRound(m_initialInteriorAngleA));
        m_interiorAngleBSpin->setValue(qRound(m_initialInteriorAngleB));

        // 弧角是扇形 / 弧线的属性：有该属性才回填，其余按 0 占位（行本身由属性表隐藏）。
        const QVariant startAngleValue = shape->PropertyValue(ItemProperty::START_ANGLE);
        const QVariant sweepAngleValue = shape->PropertyValue(ItemProperty::SWEEP_ANGLE);
        m_initialStartAngle = startAngleValue.isValid() ? startAngleValue.toDouble() : 0.0;
        m_initialSweepAngle = sweepAngleValue.isValid() ? sweepAngleValue.toDouble() : 0.0;
        m_startAngleSpin->setValue(qRound(m_initialStartAngle));
        m_sweepAngleSpin->setValue(qRound(m_initialSweepAngle));
    } else if (auto* stroke = dynamic_cast<LineAbstractItem*>(item)) {
        m_strokeAttributes.SetStroke(stroke->StrokeColor(), stroke->StrokeWidth());
        m_strokeAttributes.SetStrokeStyle(stroke->strokePattern());
        m_initialStrokeWidth = m_strokeAttributes.StrokeWidth();
        m_startArrowCombo->setCurrentIndex(
            qMax(0, m_startArrowCombo->findData(static_cast<int>(stroke->StartArrow()))));
        m_endArrowCombo->setCurrentIndex(qMax(0, m_endArrowCombo->findData(static_cast<int>(stroke->EndArrow()))));

        // 弧线角度 / 正弦周期振幅不再是基类能力位，按属性表回答“这一行该不该开”。
        m_initialStartAngle = stroke->PropertyValue(ItemProperty::START_ANGLE).toDouble();
        m_initialSweepAngle = stroke->PropertyValue(ItemProperty::SWEEP_ANGLE).toDouble();
        m_startAngleSpin->setValue(qRound(m_initialStartAngle));
        m_sweepAngleSpin->setValue(qRound(m_initialSweepAngle));
        m_initialSineCycles = qBound(1, qRound(stroke->PropertyValue(ItemProperty::SINE_CYCLES).toDouble()), 12);
        m_initialSineAmplitude = qBound(1, qRound(stroke->PropertyValue(ItemProperty::SINE_AMPLITUDE).toDouble()), 200);
        m_sineCyclesSpin->setValue(m_initialSineCycles);
        m_sineAmplitudeSpin->setValue(m_initialSineAmplitude);
    }

    if (item) {
        // 不透明度是所有图元共有的属性：样式区这一行对全部类型可见。
        m_initialOpacity = item->ItemOpacity();
        m_initialOpacityPercent = qRound(m_initialOpacity * 100.0);
        m_opacitySpin->setValue(m_initialOpacityPercent);
    }

    // 属性副本已按图元回填，刷新各行控件的选中态与数值。
    m_fillStyleRow->Refresh();
    m_fillColorRow->Refresh();
    m_fillImageRow->Refresh();
    m_borderStyleRow->Refresh();
    m_strokeColorRow->Refresh();
    m_borderWidthRow->Refresh();
}

QList<QWidget*> ItemPropertiesDialogStyleBlock::StyleRows() const
{
    QList<QWidget*> rows;
    for (QWidget* row : {static_cast<QWidget*>(m_fillStyleRow), static_cast<QWidget*>(m_fillColorRow),
                         static_cast<QWidget*>(m_fillImageRow), static_cast<QWidget*>(m_borderStyleRow),
                         static_cast<QWidget*>(m_strokeColorRow), static_cast<QWidget*>(m_borderWidthRow),
                         static_cast<QWidget*>(m_opacitySpin), static_cast<QWidget*>(m_cornerRadiusSpin),
                         static_cast<QWidget*>(m_interiorAngleASpin), static_cast<QWidget*>(m_interiorAngleBSpin),
                         static_cast<QWidget*>(m_startAngleSpin), static_cast<QWidget*>(m_sweepAngleSpin),
                         static_cast<QWidget*>(m_sineCyclesSpin), static_cast<QWidget*>(m_sineAmplitudeSpin)}) {
        if (row) {
            rows.append(row);
        }
    }
    return rows;
}

void ItemPropertiesDialogStyleBlock::SetContentSize(const QSizeF& size)
{
    m_contentSize = size;
    if (m_cornerRadiusSpin) {
        const int maximum = qMax(0, qRound(qMin(size.width(), size.height()) / 2.0));
        const bool blocked = m_cornerRadiusSpin->blockSignals(true);
        m_cornerRadiusSpin->setMaximum(maximum);
        if (!m_cornerEdited && m_initialStoredCornerRadius < 0.0) {
            m_initialCornerRadius = qRound(qMin(size.width(), size.height()) * m_defaultCornerRatio);
            m_cornerRadiusSpin->setValue(m_initialCornerRadius);
        }
        m_cornerRadiusSpin->blockSignals(blocked);
    }
}
