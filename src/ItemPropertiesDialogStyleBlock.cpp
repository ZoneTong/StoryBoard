#include "ItemPropertiesDialogStyleBlock.h"

#include "AttributeRowWidgets.h"
#include "ItemPropertiesDialog.h"
#include "ItemStyleVisuals.h"
#include "LineAbstractItem.h"
#include "PolygonItem.h"
#include "ShapeAbstractItem.h"
#include "ShapeMeta.h"

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

    m_bottomAngle1Spin = new QSpinBox(this);
    m_bottomAngle1Spin->setObjectName(QStringLiteral("ItemPropertiesBottomAngle1Value"));
    m_bottomAngle1Spin->setRange(5, 170);
    m_bottomAngle1Spin->setSingleStep(1);
    m_bottomAngle1Spin->setSuffix(QStringLiteral("°"));
    m_bottomAngle1Spin->setToolTip(ItemPropertiesDialog::tr("Triangle base angle A"));
    m_bottomAngle1Spin->setMinimumWidth(88);

    m_bottomAngle2Spin = new QSpinBox(this);
    m_bottomAngle2Spin->setObjectName(QStringLiteral("ItemPropertiesBottomAngle2Value"));
    m_bottomAngle2Spin->setRange(5, 170);
    m_bottomAngle2Spin->setSingleStep(1);
    m_bottomAngle2Spin->setSuffix(QStringLiteral("°"));
    m_bottomAngle2Spin->setToolTip(ItemPropertiesDialog::tr("Triangle base angle B"));
    m_bottomAngle2Spin->setMinimumWidth(88);

    m_parallelogramAngleSpin = new QSpinBox(this);
    m_parallelogramAngleSpin->setObjectName(QStringLiteral("ItemPropertiesParallelogramAngleValue"));
    m_parallelogramAngleSpin->setRange(5, 175);
    m_parallelogramAngleSpin->setSingleStep(1);
    m_parallelogramAngleSpin->setSuffix(QStringLiteral("°"));
    m_parallelogramAngleSpin->setToolTip(ItemPropertiesDialog::tr("Parallelogram bottom-left angle"));
    m_parallelogramAngleSpin->setMinimumWidth(88);

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

    m_startAngleSpin = new QSpinBox(this);
    m_startAngleSpin->setObjectName(QStringLiteral("ItemPropertiesStartAngleValue"));
    m_startAngleSpin->setRange(-360, 360);
    m_startAngleSpin->setSingleStep(5);
    m_startAngleSpin->setSuffix(QStringLiteral("°"));
    m_startAngleSpin->setMinimumWidth(88);

    m_sweepAngleSpin = new QSpinBox(this);
    m_sweepAngleSpin->setObjectName(QStringLiteral("ItemPropertiesSweepAngleValue"));
    m_sweepAngleSpin->setRange(-360, 360);
    m_sweepAngleSpin->setSingleStep(5);
    m_sweepAngleSpin->setSuffix(QStringLiteral("°"));
    m_sweepAngleSpin->setMinimumWidth(88);

    m_arcStartAngleSpin = new QSpinBox(this);
    m_arcStartAngleSpin->setObjectName(QStringLiteral("ItemPropertiesArcStartAngleValue"));
    m_arcStartAngleSpin->setRange(-360, 360);
    m_arcStartAngleSpin->setSingleStep(5);
    m_arcStartAngleSpin->setSuffix(QStringLiteral("°"));
    m_arcStartAngleSpin->setMinimumWidth(88);

    m_arcSweepAngleSpin = new QSpinBox(this);
    m_arcSweepAngleSpin->setObjectName(QStringLiteral("ItemPropertiesArcSweepAngleValue"));
    m_arcSweepAngleSpin->setRange(-360, 360);
    m_arcSweepAngleSpin->setSingleStep(5);
    m_arcSweepAngleSpin->setSuffix(QStringLiteral("°"));
    m_arcSweepAngleSpin->setMinimumWidth(88);

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

void ItemPropertiesDialogStyleBlock::AddRows(QFormLayout* form)
{
    m_form = form;
    // 行序固定（所有图元共用）：填充样式 → 填充颜色 → 填充图片 → 描线样式 →
    // 描线颜色 → 描线宽度 → 不透明度 → 圆角 → 三角形底角 A/B → 平行四边形内角 →
    // 箭头 → 扇形起始 / 扫过角 → 弧线起始 / 扫过角 → 正弦周期 / 振幅。
    form->addRow(ItemPropertiesDialog::tr("Fill style"), m_fillStyleRow);
    form->addRow(ItemPropertiesDialog::tr("Fill color"), m_fillColorRow);
    form->addRow(ItemPropertiesDialog::tr("Fill image"), m_fillImageRow);
    form->addRow(ItemPropertiesDialog::tr("Border style"), m_borderStyleRow);
    form->addRow(ItemPropertiesDialog::tr("Border color"), m_strokeColorRow);
    form->addRow(ItemPropertiesDialog::tr("Border width"), m_borderWidthRow);
    m_opacitySpin->setToolTip(ItemPropertiesDialog::tr("Opacity (100% = opaque)"));
    form->addRow(ItemPropertiesDialog::tr("Opacity"), m_opacitySpin);
    form->addRow(ItemPropertiesDialog::tr("Corner radius"), m_cornerRadiusSpin);
    form->addRow(ItemPropertiesDialog::tr("Triangle base angle A"), m_bottomAngle1Spin);
    form->addRow(ItemPropertiesDialog::tr("Triangle base angle B"), m_bottomAngle2Spin);
    form->addRow(ItemPropertiesDialog::tr("Parallelogram bottom-left angle"), m_parallelogramAngleSpin);
    form->addRow(ItemPropertiesDialog::tr("Arrow heads"), m_arrowRow);
    form->addRow(ItemPropertiesDialog::tr("Start angle"), m_startAngleSpin);
    form->addRow(ItemPropertiesDialog::tr("Sweep angle"), m_sweepAngleSpin);
    form->addRow(ItemPropertiesDialog::tr("Start angle"), m_arcStartAngleSpin);
    form->addRow(ItemPropertiesDialog::tr("Sweep angle"), m_arcSweepAngleSpin);
    form->addRow(ItemPropertiesDialog::tr("Sine cycles"), m_sineCyclesSpin);
    form->addRow(ItemPropertiesDialog::tr("Sine amplitude"), m_sineAmplitudeSpin);
}

void ItemPropertiesDialogStyleBlock::SetItem(CanvasItem* item)
{
    m_hasFill = false;
    m_initialStrokeWidth = 4.0;
    m_cornerEdited = false;
    bool showFill = false;
    bool showStroke = false;
    bool showOpacity = false;
    bool showCorner = false;
    bool showBottomAngles = false;
    bool showParallelogram = false;
    bool showArrows = false;
    bool showSectorAngles = false;
    bool showArcAngles = false;
    bool showSine = false;

    if (auto* shape = dynamic_cast<ShapeAbstractItem*>(item)) {
        // 多边形派生自 ShapeAbstractItem 但没有形状类型：填充与描线照旧，圆角 / 角度等行不给。
        const bool filled = shape->ShapeLayerCount() > 0 || ShapeTypeIsFilled(shape->shapeType());
        const bool plainShape = shape->ShapeLayerCount() == 0;
        showStroke = true;
        showOpacity = true;
        m_hasFill = filled;
        showFill = filled;
        // 属性窗用私有副本承载填充属性：行控件直接读它，取消时图元不受影响。
        if (filled) {
            m_fillAttributes.SetFill(shape->FillColor(), shape->fillStyle());
            m_fillAttributes.SetFillImage(shape->FillImage(), shape->FillImageMode());
        }
        m_strokeAttributes.SetStroke(shape->StrokeColor(), shape->StrokeWidth());
        m_strokeAttributes.SetStrokeStyle(shape->StrokeStyle());
        m_initialStrokeWidth = m_strokeAttributes.StrokeWidth();

        const ShapeType type = shape->shapeType();
        if (plainShape && !shape->IsCustomShape() && type == ShapeType::RECTANGLE) {
            showCorner = true;
            m_contentSize = shape->ContentSize();
            const qreal radius = shape->CornerRadius();
            const qreal minSide = qMin(m_contentSize.width(), m_contentSize.height());
            m_defaultCornerRatio = minSide > 0.0 ? radius / minSide : 0.0;
            m_initialStoredCornerRadius = radius;
            m_initialCornerRadius = qRound(radius);
            m_cornerRadiusSpin->setMaximum(qMax(0, qRound(minSide / 2.0)));
        } else {
            m_defaultCornerRatio = 0.0;
            m_initialStoredCornerRadius = shape->CornerRadius();
            m_initialCornerRadius = qRound(shape->CornerRadius());
            m_cornerRadiusSpin->setMaximum(qMax(0, qRound(shape->ContentSize().width() / 2.0)));
        }
        m_cornerRadiusSpin->setValue(m_initialCornerRadius);

        // 以下各参数行无论是否适用都按图元当前值回填：隐藏行未被触碰时，
        // 提交路径按“未改动即保留原值”原样跳过。
        m_initialBottomAngle1 = shape->BottomAngle1();
        m_initialBottomAngle2 = shape->BottomAngle2();
        showBottomAngles = plainShape && !shape->IsCustomShape() && type == ShapeType::TRIANGLE;
        m_bottomAngle1Spin->setValue(qRound(m_initialBottomAngle1));
        m_bottomAngle2Spin->setValue(qRound(m_initialBottomAngle2));

        m_initialParallelogramAngle = shape->ParallelogramAngle();
        showParallelogram = plainShape && !shape->IsCustomShape() && type == ShapeType::PARALLELOGRAM;
        m_parallelogramAngleSpin->setValue(qRound(m_initialParallelogramAngle));

        m_initialStartAngle = shape->StartAngle();
        m_initialSweepAngle = shape->SweepAngle();
        showSectorAngles = plainShape && type == ShapeType::SECTOR;
        m_startAngleSpin->setValue(qRound(m_initialStartAngle));
        m_sweepAngleSpin->setValue(qRound(m_initialSweepAngle));
    } else if (auto* stroke = dynamic_cast<LineAbstractItem*>(item)) {
        showStroke = true;
        showOpacity = true;
        showArrows = true;
        m_strokeAttributes.SetStroke(stroke->StrokeColor(), stroke->StrokeWidth());
        m_strokeAttributes.SetStrokeStyle(stroke->strokePattern());
        m_initialStrokeWidth = m_strokeAttributes.StrokeWidth();
        m_startArrowCombo->setCurrentIndex(
            qMax(0, m_startArrowCombo->findData(static_cast<int>(stroke->StartArrow()))));
        m_endArrowCombo->setCurrentIndex(qMax(0, m_endArrowCombo->findData(static_cast<int>(stroke->EndArrow()))));

        const LineAbstractItem::ParamState param = stroke->paramState();
        m_initialArcStartAngle = param.first;
        m_initialArcSweepAngle = param.second;
        m_arcStartAngleSpin->setValue(qRound(param.first));
        m_arcSweepAngleSpin->setValue(qRound(param.second));
        m_initialSineCycles = qBound(1, qRound(param.first), 12);
        m_initialSineAmplitude = qBound(1, qRound(param.second), 200);
        m_sineCyclesSpin->setValue(m_initialSineCycles);
        m_sineAmplitudeSpin->setValue(m_initialSineAmplitude);
        showArcAngles = stroke->SupportsArcAngles();
        showSine = !showArcAngles && stroke->SupportsSineWave();
    }

    if (item) {
        // 不透明度是所有图元共有的属性：样式区这一行对全部类型可见。
        m_initialOpacity = item->ItemOpacity();
        m_initialOpacityPercent = qRound(m_initialOpacity * 100.0);
        m_opacitySpin->setValue(m_initialOpacityPercent);
    }

    if (m_form) {
        m_form->setRowVisible(m_fillStyleRow, showFill);
        m_form->setRowVisible(m_fillColorRow, showFill);
        m_form->setRowVisible(m_fillImageRow, showFill);
        m_form->setRowVisible(m_borderStyleRow, showStroke);
        m_form->setRowVisible(m_strokeColorRow, showStroke);
        m_form->setRowVisible(m_borderWidthRow, showStroke);
        m_form->setRowVisible(m_opacitySpin, showOpacity);
        m_form->setRowVisible(m_cornerRadiusSpin, showCorner);
        m_form->setRowVisible(m_bottomAngle1Spin, showBottomAngles);
        m_form->setRowVisible(m_bottomAngle2Spin, showBottomAngles);
        m_form->setRowVisible(m_parallelogramAngleSpin, showParallelogram);
        m_form->setRowVisible(m_arrowRow, showArrows);
        m_form->setRowVisible(m_startAngleSpin, showSectorAngles);
        m_form->setRowVisible(m_sweepAngleSpin, showSectorAngles);
        m_form->setRowVisible(m_arcStartAngleSpin, showArcAngles);
        m_form->setRowVisible(m_arcSweepAngleSpin, showArcAngles);
        m_form->setRowVisible(m_sineCyclesSpin, showSine);
        m_form->setRowVisible(m_sineAmplitudeSpin, showSine);
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
                         static_cast<QWidget*>(m_startAngleSpin), static_cast<QWidget*>(m_sweepAngleSpin),
                         static_cast<QWidget*>(m_arcStartAngleSpin), static_cast<QWidget*>(m_arcSweepAngleSpin),
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
