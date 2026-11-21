#include "ItemPropertiesDialogStyleBlock.h"

#include "AttributeRowWidgets.h"
#include "ItemPropertiesDialog.h"
#include "ItemStyleVisuals.h"
#include "LineItemAbstract.h"
#include "PolygonItem.h"
#include "ShapeItem.h"
#include "ShapeMeta.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QToolButton>

ItemPropertiesDialogStyleBlock::ItemPropertiesDialogStyleBlock(CanvasItem* item, QWidget* parent) : QWidget(parent)
{
    if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
        // 多边形派生自 ShapeItem 但没有形状类型：填充与描线照旧，圆角 / 角度等行不给。
        const bool filled = shape->ShapeLayerCount() > 0 || ShapeTypeIsFilled(shape->shapeType());
        m_hasFill = filled;

        // 需求 5/9a：直线、弧线这类只描线的图形根本没有填充属性，
        // 不建行也不留“Not applicable”占位。
        if (filled) {
            BuildFillRows(item);
        }
        BuildStrokeRows(item);

        m_opacitySpin = new QSpinBox(this);
        m_opacitySpin->setObjectName(QStringLiteral("ItemPropertiesOpacityValue"));
        m_opacitySpin->setRange(0, 100);
        m_opacitySpin->setSuffix(QStringLiteral(" %"));
        m_opacitySpin->setValue(qRound(shape->ItemOpacity() * 100.0));
        m_initialOpacity = shape->ItemOpacity();
        m_initialOpacityPercent = m_opacitySpin->value();
        m_opacitySpin->setMinimumWidth(88);
        const ShapeType type = shape->shapeType();
        // 多边形没有形状类型：圆角 / 角度 / 箭头这些按类型出现的行一律不给。
        const bool plainShape = shape->ShapeLayerCount() == 0;
        if (plainShape && type == ShapeType::SECTOR) {
            m_initialStartAngle = shape->StartAngle();
            m_initialSweepAngle = shape->SweepAngle();
            m_startAngleSpin = new QSpinBox(this);
            m_startAngleSpin->setObjectName(QStringLiteral("ItemPropertiesStartAngleValue"));
            m_startAngleSpin->setRange(-360, 360);
            m_startAngleSpin->setSingleStep(5);
            m_startAngleSpin->setSuffix(QStringLiteral("°"));
            m_startAngleSpin->setValue(qRound(m_initialStartAngle));
            m_startAngleSpin->setMinimumWidth(88);

            m_sweepAngleSpin = new QSpinBox(this);
            m_sweepAngleSpin->setObjectName(QStringLiteral("ItemPropertiesSweepAngleValue"));
            m_sweepAngleSpin->setRange(-360, 360);
            m_sweepAngleSpin->setSingleStep(5);
            m_sweepAngleSpin->setSuffix(QStringLiteral("°"));
            m_sweepAngleSpin->setValue(qRound(m_initialSweepAngle));
            m_sweepAngleSpin->setMinimumWidth(88);
        }

        if (plainShape && !shape->IsCustomShape() && type == ShapeType::RECTANGLE) {
            m_contentSize = shape->ContentSize();
            const qreal radius = shape->CornerRadius();
            const qreal minSide = qMin(m_contentSize.width(), m_contentSize.height());
            m_defaultCornerRatio = minSide > 0.0 ? radius / minSide : 0.0;
            m_initialStoredCornerRadius = shape->CornerRadius();
            m_initialCornerRadius = qRound(radius);

            m_cornerRadiusSpin = new QSpinBox(this);
            m_cornerRadiusSpin->setObjectName(QStringLiteral("ItemPropertiesCornerRadiusValue"));
            m_cornerRadiusSpin->setRange(0, qMax(0, qRound(qMin(m_contentSize.width(), m_contentSize.height()) / 2.0)));
            m_cornerRadiusSpin->setSuffix(QStringLiteral(" px"));
            m_cornerRadiusSpin->setToolTip(ItemPropertiesDialog::tr("Corner radius"));
            m_cornerRadiusSpin->setMinimumWidth(88);
            m_cornerRadiusSpin->setValue(m_initialCornerRadius);
            connect(m_cornerRadiusSpin, &QSpinBox::valueChanged, this,
                    [this](int value) { m_cornerEdited = value != m_initialCornerRadius; });
        }

        // 需求 1：三角形两个底角在属性窗里也可编辑；底边宽度随图元外框调整，
        // 不再提供独立的底边长度字段。
        if (plainShape && !shape->IsCustomShape() && type == ShapeType::TRIANGLE) {
            m_initialBottomAngle1 = shape->BottomAngle1();
            m_initialBottomAngle2 = shape->BottomAngle2();

            m_bottomAngle1Spin = new QSpinBox(this);
            m_bottomAngle1Spin->setObjectName(QStringLiteral("ItemPropertiesBottomAngle1Value"));
            m_bottomAngle1Spin->setRange(5, 170);
            m_bottomAngle1Spin->setSingleStep(1);
            m_bottomAngle1Spin->setSuffix(QStringLiteral("°"));
            m_bottomAngle1Spin->setToolTip(ItemPropertiesDialog::tr("Triangle base angle A"));
            m_bottomAngle1Spin->setMinimumWidth(88);
            m_bottomAngle1Spin->setValue(qRound(m_initialBottomAngle1));

            m_bottomAngle2Spin = new QSpinBox(this);
            m_bottomAngle2Spin->setObjectName(QStringLiteral("ItemPropertiesBottomAngle2Value"));
            m_bottomAngle2Spin->setRange(5, 170);
            m_bottomAngle2Spin->setSingleStep(1);
            m_bottomAngle2Spin->setSuffix(QStringLiteral("°"));
            m_bottomAngle2Spin->setToolTip(ItemPropertiesDialog::tr("Triangle base angle B"));
            m_bottomAngle2Spin->setMinimumWidth(88);
            m_bottomAngle2Spin->setValue(qRound(m_initialBottomAngle2));
        }

        // 需求 9：平行四边形的左下内角可编辑；角度只影响上边相对底边的偏移。
        if (plainShape && !shape->IsCustomShape() && type == ShapeType::PARALLELOGRAM) {
            m_initialParallelogramAngle = shape->ParallelogramAngle();

            m_parallelogramAngleSpin = new QSpinBox(this);
            m_parallelogramAngleSpin->setObjectName(QStringLiteral("ItemPropertiesParallelogramAngleValue"));
            m_parallelogramAngleSpin->setRange(5, 175);
            m_parallelogramAngleSpin->setSingleStep(1);
            m_parallelogramAngleSpin->setSuffix(QStringLiteral("°"));
            m_parallelogramAngleSpin->setToolTip(ItemPropertiesDialog::tr("Parallelogram bottom-left angle"));
            m_parallelogramAngleSpin->setMinimumWidth(88);
            m_parallelogramAngleSpin->setValue(qRound(m_initialParallelogramAngle));
        }
    } else if (auto* stroke = dynamic_cast<LineItemAbstract*>(item)) {
        BuildStrokeRows(item);

        // 不透明度统一紧跟描线宽度：曲线族与图形走同一行序，确认时并入同一条撤销记录。
        m_opacitySpin = new QSpinBox(this);
        m_opacitySpin->setObjectName(QStringLiteral("ItemPropertiesOpacityValue"));
        m_opacitySpin->setRange(0, 100);
        m_opacitySpin->setSuffix(QStringLiteral(" %"));
        m_opacitySpin->setValue(qRound(stroke->ItemOpacity() * 100.0));
        m_initialOpacity = stroke->ItemOpacity();
        m_initialOpacityPercent = m_opacitySpin->value();
        m_opacitySpin->setMinimumWidth(88);

        // 需求 27：曲线与直线一样，属性窗提供两端箭头样式；箭头对全部曲线子型开放（含花括号）。
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
        m_startArrowCombo->setCurrentIndex(
            qMax(0, m_startArrowCombo->findData(static_cast<int>(stroke->StartArrow()))));
        m_endArrowCombo->setCurrentIndex(qMax(0, m_endArrowCombo->findData(static_cast<int>(stroke->EndArrow()))));
        arrowLayout->addWidget(m_startArrowCombo, 1);
        arrowLayout->addWidget(m_endArrowCombo, 1);
    }
}

void ItemPropertiesDialogStyleBlock::BuildFillRows(CanvasItem* item)
{
    auto* shape = dynamic_cast<ShapeItem*>(item);
    if (!shape) {
        return;
    }
    // 属性窗用私有副本承载填充属性：行控件直接读它，取消时图元不受影响。
    m_fillAttributes.SetFill(shape->FillColor(), shape->fillStyle());
    m_fillAttributes.SetFillImage(shape->FillImage(), shape->FillImageMode());

    m_fillStyleRow = new FillStyleRow(this);
    m_fillStyleRow->SetTargets({&m_fillAttributes});
    connect(m_fillStyleRow, &FillStyleRow::fillStyleChosen, this, [this](FillStyle style) {
        m_fillAttributes.SetFillStyle(style);
        m_fillStyleRow->Refresh();
    });

    m_fillColorRow = new FillColorRow(this);
    m_fillColorRow->SetTargets({&m_fillAttributes});
    connect(m_fillColorRow, &FillColorRow::fillColorChosen, this, [this](const QColor& color) {
        m_fillAttributes.SetFill(color, m_fillAttributes.fillStyle());
        m_fillColorRow->Refresh();
    });

    // 需求 15：独立「填充图片」属性，不进 FillStyle；图片透明处露出填充色 / 图案。
    m_fillImageRow = new FillImageRow(this);
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

void ItemPropertiesDialogStyleBlock::BuildStrokeRows(CanvasItem* item)
{
    if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
        m_strokeAttributes.SetStroke(shape->StrokeColor(), shape->StrokeWidth());
        m_strokeAttributes.SetStrokeStyle(shape->StrokeStyle());
    } else if (auto* stroke = dynamic_cast<LineItemAbstract*>(item)) {
        m_strokeAttributes.SetStroke(stroke->StrokeColor(), stroke->StrokeWidth());
        m_strokeAttributes.SetStrokeStyle(stroke->strokePattern());
    } else {
        return;
    }
    m_initialStrokeWidth = m_strokeAttributes.StrokeWidth();

    m_borderStyleRow = new StrokeStyleRow(this);
    m_borderStyleRow->SetTargets({&m_strokeAttributes});
    connect(m_borderStyleRow, &StrokeStyleRow::borderStyleChosen, this, [this](Qt::PenStyle style) {
        m_strokeAttributes.SetStrokeStyle(style);
        m_borderStyleRow->Refresh();
    });

    m_strokeColorRow = new StrokeColorRow(this);
    m_strokeColorRow->SetTargets({&m_strokeAttributes});
    connect(m_strokeColorRow, &StrokeColorRow::strokeColorChosen, this, [this](const QColor& color) {
        m_strokeAttributes.SetStrokeColor(color);
        m_strokeColorRow->Refresh();
    });

    m_borderWidthRow = new StrokeWidthRow(this);
    m_borderWidthRow->SetTargets({&m_strokeAttributes});
    connect(m_borderWidthRow, &StrokeWidthRow::borderWidthChosen, this, [this](qreal width) {
        m_strokeAttributes.SetStrokeWidth(width);
        m_borderWidthRow->Refresh();
    });

    // 弧线 / 正弦曲线：描线类图元的形状参数行，与编辑面板同一套取值范围。
    if (auto* stroke = dynamic_cast<LineItemAbstract*>(item)) {
        if (stroke->SupportsArcAngles()) {
            const LineItemAbstract::ParamState param = stroke->paramState();
            m_initialArcStartAngle = param.first;
            m_initialArcSweepAngle = param.second;
            m_arcStartAngleSpin = new QSpinBox(this);
            m_arcStartAngleSpin->setObjectName(QStringLiteral("ItemPropertiesArcStartAngleValue"));
            m_arcStartAngleSpin->setRange(-360, 360);
            m_arcStartAngleSpin->setSingleStep(5);
            m_arcStartAngleSpin->setSuffix(QStringLiteral("°"));
            m_arcStartAngleSpin->setValue(qRound(m_initialArcStartAngle));
            m_arcSweepAngleSpin = new QSpinBox(this);
            m_arcSweepAngleSpin->setObjectName(QStringLiteral("ItemPropertiesArcSweepAngleValue"));
            m_arcSweepAngleSpin->setRange(-360, 360);
            m_arcSweepAngleSpin->setSingleStep(5);
            m_arcSweepAngleSpin->setSuffix(QStringLiteral("°"));
            m_arcSweepAngleSpin->setValue(qRound(m_initialArcSweepAngle));
        } else if (stroke->SupportsSineWave()) {
            const LineItemAbstract::ParamState param = stroke->paramState();
            m_initialSineCycles = qBound(1, qRound(param.first), 12);
            m_initialSineAmplitude = qBound(1, qRound(param.second), 200);
            m_sineCyclesSpin = new QSpinBox(this);
            m_sineCyclesSpin->setObjectName(QStringLiteral("ItemPropertiesSineCyclesValue"));
            m_sineCyclesSpin->setRange(1, 12);
            m_sineCyclesSpin->setValue(m_initialSineCycles);
            m_sineAmplitudeSpin = new QSpinBox(this);
            m_sineAmplitudeSpin->setObjectName(QStringLiteral("ItemPropertiesSineAmplitudeValue"));
            m_sineAmplitudeSpin->setRange(1, 200);
            m_sineAmplitudeSpin->setSuffix(QStringLiteral(" px"));
            m_sineAmplitudeSpin->setValue(m_initialSineAmplitude);
        }
    }
}

void ItemPropertiesDialogStyleBlock::AddRows(QFormLayout* form)
{
    if (m_fillStyleRow) {
        form->addRow(ItemPropertiesDialog::tr("Fill style"), m_fillStyleRow);
    }
    if (m_fillColorRow) {
        form->addRow(ItemPropertiesDialog::tr("Fill color"), m_fillColorRow);
    }
    if (m_fillImageRow) {
        form->addRow(ItemPropertiesDialog::tr("Fill image"), m_fillImageRow);
    }
    // 需求 1 行序：描线样式排在描线颜色之前（与菜单、工具栏、编辑面板统一）。
    if (m_borderStyleRow) {
        form->addRow(ItemPropertiesDialog::tr("Border style"), m_borderStyleRow);
    }
    if (m_strokeColorRow) {
        form->addRow(ItemPropertiesDialog::tr("Border color"), m_strokeColorRow);
    }
    if (m_borderWidthRow) {
        form->addRow(ItemPropertiesDialog::tr("Border width"), m_borderWidthRow);
    }
    if (m_opacitySpin) {
        // 需求 5：该选项实际是“不透明度”（100% = 完全不透明），名称与提示一并纠正。
        m_opacitySpin->setToolTip(ItemPropertiesDialog::tr("Opacity (100% = opaque)"));
        form->addRow(ItemPropertiesDialog::tr("Opacity"), m_opacitySpin);
    }
    if (m_cornerRadiusSpin) {
        form->addRow(ItemPropertiesDialog::tr("Corner radius"), m_cornerRadiusSpin);
    }
    if (m_bottomAngle1Spin) {
        form->addRow(ItemPropertiesDialog::tr("Triangle base angle A"), m_bottomAngle1Spin);
    }
    if (m_bottomAngle2Spin) {
        form->addRow(ItemPropertiesDialog::tr("Triangle base angle B"), m_bottomAngle2Spin);
    }
    if (m_parallelogramAngleSpin) {
        form->addRow(ItemPropertiesDialog::tr("Parallelogram bottom-left angle"), m_parallelogramAngleSpin);
    }
    if (m_arrowRow) {
        form->addRow(ItemPropertiesDialog::tr("Arrow heads"), m_arrowRow);
    }
    if (m_startAngleSpin) {
        form->addRow(ItemPropertiesDialog::tr("Start angle"), m_startAngleSpin);
    }
    if (m_sweepAngleSpin) {
        form->addRow(ItemPropertiesDialog::tr("Sweep angle"), m_sweepAngleSpin);
    }
    // 弧线起始角 / 扫过角与正弦周期 / 振幅：参数化描线的形状行。
    if (m_arcStartAngleSpin) {
        form->addRow(ItemPropertiesDialog::tr("Start angle"), m_arcStartAngleSpin);
    }
    if (m_arcSweepAngleSpin) {
        form->addRow(ItemPropertiesDialog::tr("Sweep angle"), m_arcSweepAngleSpin);
    }
    if (m_sineCyclesSpin) {
        form->addRow(ItemPropertiesDialog::tr("Sine cycles"), m_sineCyclesSpin);
    }
    if (m_sineAmplitudeSpin) {
        form->addRow(ItemPropertiesDialog::tr("Sine amplitude"), m_sineAmplitudeSpin);
    }
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
