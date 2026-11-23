#include "ui/panels/itemdialog/ItemPropertiesDialogStyleBlock.h"

#include "items/geometry/ShapeMeta.h"
#include "items/lines/LineAbstractItem.h"
#include "items/polygon/PolygonItem.h"
#include "items/properties/ItemPropertyCatalog.h"
#include "items/properties/ItemStyleVisuals.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "ui/panels/itemdialog/ItemPropertiesDialog.h"
#include "ui/panels/properties/PropertyRowRegistry.h"
#include "ui/panels/properties/PropertyRowSpec.h"
#include "ui/widgets/AttributeRowWidgets.h"
#include "ui/widgets/PropertyRowWidgets.h"

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
    ApplyItemPropertyRange(m_opacitySpin, ItemProperty::OPACITY);
    m_opacitySpin->setMinimumWidth(88);
}

void ItemPropertiesDialogStyleBlock::BuildTypeRows()
{
    m_cornerRadiusSpin = new QSpinBox(this);
    m_cornerRadiusSpin->setObjectName(QStringLiteral("ItemPropertiesCornerRadiusValue"));
    ApplyItemPropertyRange(m_cornerRadiusSpin, ItemProperty::CORNER_RADIUS);
    m_cornerRadiusSpin->setToolTip(ItemPropertyCatalog::Label(ItemProperty::CORNER_RADIUS));
    m_cornerRadiusSpin->setMinimumWidth(88);
    connect(m_cornerRadiusSpin, &QSpinBox::valueChanged, this,
            [this](int value) { m_cornerEdited = value != m_initialCornerRadius; });

    // 内角 A / B 两行由三角形与平行四边形共用（§4.2）：同一控件按目标回填与提交。
    m_interiorAngleASpin = new QSpinBox(this);
    m_interiorAngleASpin->setObjectName(QStringLiteral("ItemPropertiesInteriorAngleAValue"));
    ApplyItemPropertyRange(m_interiorAngleASpin, ItemProperty::BOTTOM_ANGLE1);
    m_interiorAngleASpin->setToolTip(ItemPropertyCatalog::Label(ItemProperty::BOTTOM_ANGLE1));
    m_interiorAngleASpin->setMinimumWidth(88);

    m_interiorAngleBSpin = new QSpinBox(this);
    m_interiorAngleBSpin->setObjectName(QStringLiteral("ItemPropertiesInteriorAngleBValue"));
    ApplyItemPropertyRange(m_interiorAngleBSpin, ItemProperty::BOTTOM_ANGLE2);
    m_interiorAngleBSpin->setToolTip(ItemPropertyCatalog::Label(ItemProperty::BOTTOM_ANGLE2));
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
            combo->addItem(ArrowStyles::Name(arrowStyle), static_cast<int>(arrowStyle));
        }
    }
    arrowLayout->addWidget(m_startArrowCombo, 1);
    arrowLayout->addWidget(m_endArrowCombo, 1);

    // 起始角 / 扫过角：扇形与弧线共用同一对控件，值统一按属性读写。
    m_startAngleSpin = new QSpinBox(this);
    m_startAngleSpin->setObjectName(QStringLiteral("ItemPropertiesStartAngleValue"));
    ApplyItemPropertyRange(m_startAngleSpin, ItemProperty::START_ANGLE);
    m_startAngleSpin->setToolTip(ItemPropertyCatalog::Label(ItemProperty::START_ANGLE));
    m_startAngleSpin->setMinimumWidth(88);

    m_sweepAngleSpin = new QSpinBox(this);
    m_sweepAngleSpin->setObjectName(QStringLiteral("ItemPropertiesSweepAngleValue"));
    ApplyItemPropertyRange(m_sweepAngleSpin, ItemProperty::SWEEP_ANGLE);
    m_sweepAngleSpin->setToolTip(ItemPropertyCatalog::Label(ItemProperty::SWEEP_ANGLE));
    m_sweepAngleSpin->setMinimumWidth(88);

    m_sineCyclesSpin = new QSpinBox(this);
    m_sineCyclesSpin->setObjectName(QStringLiteral("ItemPropertiesSineCyclesValue"));
    ApplyItemPropertyRange(m_sineCyclesSpin, ItemProperty::SINE_CYCLES);
    m_sineCyclesSpin->setMinimumWidth(88);

    m_sineAmplitudeSpin = new QSpinBox(this);
    m_sineAmplitudeSpin->setObjectName(QStringLiteral("ItemPropertiesSineAmplitudeValue"));
    ApplyItemPropertyRange(m_sineAmplitudeSpin, ItemProperty::SINE_AMPLITUDE);
    m_sineAmplitudeSpin->setMinimumWidth(88);
}

void ItemPropertiesDialogStyleBlock::AddRows(QFormLayout* form, PropertyRowRegistry* rows)
{
    m_form = form;
    // 行序固定（§4.1 第 8~22 行）：填充 → 描线 → 不透明度 → 圆角 →
    // 内角 A/B → 箭头 → 起始 / 扫过角 → 正弦周期 / 振幅。
    form->addRow(ItemPropertyCatalog::Label(ItemProperty::FILL_STYLE), m_fillStyleRow);
    form->addRow(ItemPropertyCatalog::Label(ItemProperty::FILL_COLOR), m_fillColorRow);
    form->addRow(ItemPropertyCatalog::Label(ItemProperty::FILL_IMAGE), m_fillImageRow);
    form->addRow(ItemPropertyCatalog::Label(ItemProperty::STROKE_STYLE), m_borderStyleRow);
    form->addRow(ItemPropertyCatalog::Label(ItemProperty::STROKE_COLOR), m_strokeColorRow);
    form->addRow(ItemPropertyCatalog::Label(ItemProperty::STROKE_WIDTH), m_borderWidthRow);
    m_opacitySpin->setToolTip(ItemPropertiesDialog::tr("Opacity (100% = opaque)"));
    form->addRow(ItemPropertyCatalog::Label(ItemProperty::OPACITY), m_opacitySpin);
    form->addRow(ItemPropertyCatalog::Label(ItemProperty::CORNER_RADIUS), m_cornerRadiusSpin);
    form->addRow(ItemPropertyCatalog::Label(ItemProperty::BOTTOM_ANGLE1), m_interiorAngleASpin);
    form->addRow(ItemPropertyCatalog::Label(ItemProperty::BOTTOM_ANGLE2), m_interiorAngleBSpin);
    form->addRow(ItemPropertyCatalog::Label(ItemProperty::ARROW_START), m_arrowRow);
    form->addRow(ItemPropertyCatalog::Label(ItemProperty::START_ANGLE), m_startAngleSpin);
    form->addRow(ItemPropertyCatalog::Label(ItemProperty::SWEEP_ANGLE), m_sweepAngleSpin);
    form->addRow(ItemPropertyCatalog::Label(ItemProperty::SINE_CYCLES), m_sineCyclesSpin);
    form->addRow(ItemPropertyCatalog::Label(ItemProperty::SINE_AMPLITUDE), m_sineAmplitudeSpin);

    if (rows) {
        auto registerRow = [rows](ItemProperty property, QWidget* field) {
            if (const PropertyRowSpec* spec = PropertyRowSpecs::For(property)) {
                rows->Register(*spec, field);
            }
        };
        registerRow(ItemProperty::FILL_STYLE, m_fillStyleRow);
        registerRow(ItemProperty::FILL_COLOR, m_fillColorRow);
        registerRow(ItemProperty::FILL_IMAGE, m_fillImageRow);
        registerRow(ItemProperty::STROKE_STYLE, m_borderStyleRow);
        registerRow(ItemProperty::STROKE_COLOR, m_strokeColorRow);
        registerRow(ItemProperty::STROKE_WIDTH, m_borderWidthRow);
        registerRow(ItemProperty::OPACITY, m_opacitySpin);
        registerRow(ItemProperty::CORNER_RADIUS, m_cornerRadiusSpin);
        // 内角 A 的行描述已同时接受三角形底角 A 与平行四边形内角，两者互斥。
        registerRow(ItemProperty::BOTTOM_ANGLE1, m_interiorAngleASpin);
        registerRow(ItemProperty::BOTTOM_ANGLE2, m_interiorAngleBSpin);
        registerRow(ItemProperty::ARROW_START, m_arrowRow);
        registerRow(ItemProperty::START_ANGLE, m_startAngleSpin);
        registerRow(ItemProperty::SWEEP_ANGLE, m_sweepAngleSpin);
        registerRow(ItemProperty::SINE_CYCLES, m_sineCyclesSpin);
        registerRow(ItemProperty::SINE_AMPLITUDE, m_sineAmplitudeSpin);
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
