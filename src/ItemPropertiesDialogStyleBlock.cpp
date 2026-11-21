#include "ItemPropertiesDialogStyleBlock.h"

#include "ItemPropertiesDialog.h"
#include "ItemPropertiesDialogFields.h"
#include "ItemStyleVisuals.h"
#include "LineItemAbstract.h"
#include "PolygonItem.h"
#include "ShapeItem.h"
#include "ShapeMeta.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>


ItemPropertiesDialogStyleBlock::ItemPropertiesDialogStyleBlock(CanvasItem* item, QWidget* parent) : QWidget(parent)
{
    auto buildFillImageRow = [this](const QPixmap& initial, ImageFill::Mode mode) {
        m_fillImage = initial;
        m_fillImageRow = new QWidget(this);
        auto* rowLayout = new QHBoxLayout(m_fillImageRow);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);

        m_fillImageButton = new QPushButton(m_fillImageRow);
        m_fillImageButton->setObjectName(QStringLiteral("ItemPropertiesFillImageValue"));
        m_fillImageButton->setText(initial.isNull() ? ItemPropertiesDialog::tr("Choose image…")
                                                    : ItemPropertiesDialog::tr("Change image…"));

        m_fillImageModeCombo = new QComboBox(m_fillImageRow);
        m_fillImageModeCombo->setObjectName(QStringLiteral("ItemPropertiesFillImageModeValue"));
        m_fillImageModeCombo->addItem(ItemPropertiesDialog::tr("Stretch"), static_cast<int>(ImageFill::Mode::STRETCH));
        m_fillImageModeCombo->addItem(ItemPropertiesDialog::tr("Fit"), static_cast<int>(ImageFill::Mode::FIT));
        m_fillImageModeCombo->addItem(ItemPropertiesDialog::tr("Tile"), static_cast<int>(ImageFill::Mode::TILE));
        m_fillImageModeCombo->setCurrentIndex(qMax(0, m_fillImageModeCombo->findData(static_cast<int>(mode))));

        m_fillImageClearButton = new QPushButton(ItemPropertiesDialog::tr("Clear"), m_fillImageRow);
        m_fillImageClearButton->setObjectName(QStringLiteral("ItemPropertiesFillImageClear"));

        rowLayout->addWidget(m_fillImageButton, 1);
        rowLayout->addWidget(m_fillImageModeCombo);
        rowLayout->addWidget(m_fillImageClearButton);

        connect(m_fillImageButton, &QPushButton::clicked, this, [this]() {
            const QString path = QFileDialog::getOpenFileName(
                this, ItemPropertiesDialog::tr("Choose fill image"), QString(),
                ItemPropertiesDialog::tr("Images (*.png *.jpg *.jpeg *.bmp *.webp *.gif)"));
            if (path.isEmpty()) {
                return;
            }
            QPixmap image(path);
            if (image.isNull()) {
                return;
            }
            m_fillImage = image;
            m_fillImageButton->setText(ItemPropertiesDialog::tr("Change image…"));
        });
        connect(m_fillImageClearButton, &QPushButton::clicked, this, [this]() {
            m_fillImage = QPixmap();
            m_fillImageButton->setText(ItemPropertiesDialog::tr("Choose image…"));
        });
    };

    if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
        // 多边形派生自 ShapeItem 但没有形状类型：填充与描线照旧，圆角 / 角度等行不给。
        const bool filled = shape->ShapeLayerCount() > 0 || ShapeTypeIsFilled(shape->shapeType());
        m_hasFill = filled;

        // 需求 5/9a：直线、弧线这类只描线的图形根本没有填充属性，
        // 不建行也不留“Not applicable”占位。
        if (filled) {
            m_fillStyleCombo = new QComboBox(this);
            m_fillStyleCombo->setObjectName(QStringLiteral("ItemPropertiesFillStyleValue"));
            m_fillStyleCombo->setMinimumWidth(180);
            for (FillStyle style : AllFillStyles()) {
                m_fillStyleCombo->addItem(ItemStyleVisuals::FillStyleIcon(style, 16), FillStyleName(style),
                                          static_cast<int>(style));
            }
            const int index = m_fillStyleCombo->findData(static_cast<int>(shape->fillStyle()));
            if (index >= 0) {
                m_fillStyleCombo->setCurrentIndex(index);
            }

            m_fillColorButton = ItemPropertiesFields::MakeColorField(this, shape->FillColor(),
                                                                     QStringLiteral("ItemPropertiesFillColorValue"),
                                                                     ItemPropertiesFields::ColorIconRole::FILL);
            // 需求 15：独立「填充图片」属性，不进 FillStyle；图片透明处露出填充色 / 图案。
            buildFillImageRow(shape->FillImage(), shape->FillImageMode());
        }

        m_strokeColorButton = ItemPropertiesFields::MakeColorField(this, shape->StrokeColor(),
                                                                   QStringLiteral("ItemPropertiesStrokeColorValue"),
                                                                   ItemPropertiesFields::ColorIconRole::BORDER);

        m_borderStyleCombo = new QComboBox(this);
        m_borderStyleCombo->setObjectName(QStringLiteral("ItemPropertiesBorderStyleValue"));
        m_borderStyleCombo->setMinimumWidth(180);
        for (Qt::PenStyle penStyle : ItemStyleVisuals::BorderStyles()) {
            m_borderStyleCombo->addItem(ItemStyleVisuals::BorderStyleIcon(penStyle, 16),
                                        ItemStyleVisuals::BorderStyleName(penStyle), static_cast<int>(penStyle));
        }
        const int borderStyleIndex = m_borderStyleCombo->findData(static_cast<int>(shape->StrokeStyle()));
        if (borderStyleIndex >= 0) {
            m_borderStyleCombo->setCurrentIndex(borderStyleIndex);
        }

        m_strokeWidthSpin = new QSpinBox(this);
        m_strokeWidthSpin->setObjectName(QStringLiteral("ItemPropertiesStrokeWidthValue"));
        m_strokeWidthSpin->setRange(1, 64);
        m_strokeWidthSpin->setSingleStep(1);
        m_strokeWidthSpin->setSuffix(QStringLiteral(" px"));
        m_strokeWidthSpin->setValue(qRound(shape->StrokeWidth()));
        m_initialStrokeWidth = shape->StrokeWidth();
        m_strokeWidthSpin->setMinimumWidth(88);

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
            m_bottomAngle1Spin->setRange(1, 174);
            m_bottomAngle1Spin->setSingleStep(1);
            m_bottomAngle1Spin->setSuffix(QStringLiteral("°"));
            m_bottomAngle1Spin->setToolTip(ItemPropertiesDialog::tr("Triangle base angle A"));
            m_bottomAngle1Spin->setMinimumWidth(88);
            m_bottomAngle1Spin->setValue(qRound(m_initialBottomAngle1));

            m_bottomAngle2Spin = new QSpinBox(this);
            m_bottomAngle2Spin->setObjectName(QStringLiteral("ItemPropertiesBottomAngle2Value"));
            m_bottomAngle2Spin->setRange(1, 174);
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
            m_parallelogramAngleSpin->setRange(15, 165);
            m_parallelogramAngleSpin->setSingleStep(1);
            m_parallelogramAngleSpin->setSuffix(QStringLiteral("°"));
            m_parallelogramAngleSpin->setToolTip(ItemPropertiesDialog::tr("Parallelogram bottom-left angle"));
            m_parallelogramAngleSpin->setMinimumWidth(88);
            m_parallelogramAngleSpin->setValue(qRound(m_initialParallelogramAngle));
        }

    } else if (auto* composite = dynamic_cast<ShapeItem*>(item); composite && composite->ShapeLayerCount() > 0) {
        m_hasFill = true;

        m_fillStyleCombo = new QComboBox(this);
        m_fillStyleCombo->setObjectName(QStringLiteral("ItemPropertiesFillStyleValue"));
        m_fillStyleCombo->setMinimumWidth(180);
        for (FillStyle style : AllFillStyles()) {
            m_fillStyleCombo->addItem(ItemStyleVisuals::FillStyleIcon(style, 16), FillStyleName(style),
                                      static_cast<int>(style));
        }
        const int fillIndex = m_fillStyleCombo->findData(static_cast<int>(composite->fillStyle()));
        if (fillIndex >= 0) {
            m_fillStyleCombo->setCurrentIndex(fillIndex);
        }

        // 需求 15：多边形同样支持图片填充，裁剪到 FilledArea 层并集（孔洞留空）。
        buildFillImageRow(composite->FillImage(), composite->FillImageMode());

        m_fillColorButton = ItemPropertiesFields::MakeColorField(this, composite->FillColor(),
                                                                 QStringLiteral("ItemPropertiesFillColorValue"),
                                                                 ItemPropertiesFields::ColorIconRole::FILL);
        m_strokeColorButton = ItemPropertiesFields::MakeColorField(this, composite->StrokeColor(),
                                                                   QStringLiteral("ItemPropertiesStrokeColorValue"),
                                                                   ItemPropertiesFields::ColorIconRole::BORDER);

        m_borderStyleCombo = new QComboBox(this);
        m_borderStyleCombo->setObjectName(QStringLiteral("ItemPropertiesBorderStyleValue"));
        m_borderStyleCombo->setMinimumWidth(180);
        for (Qt::PenStyle penStyle : ItemStyleVisuals::BorderStyles()) {
            m_borderStyleCombo->addItem(ItemStyleVisuals::BorderStyleIcon(penStyle, 16),
                                        ItemStyleVisuals::BorderStyleName(penStyle), static_cast<int>(penStyle));
        }
        const int compositeBorderIndex = m_borderStyleCombo->findData(static_cast<int>(composite->StrokeStyle()));
        if (compositeBorderIndex >= 0) {
            m_borderStyleCombo->setCurrentIndex(compositeBorderIndex);
        }

        m_strokeWidthSpin = new QSpinBox(this);
        m_strokeWidthSpin->setObjectName(QStringLiteral("ItemPropertiesStrokeWidthValue"));
        m_strokeWidthSpin->setRange(1, 64);
        m_strokeWidthSpin->setSingleStep(1);
        m_strokeWidthSpin->setSuffix(QStringLiteral(" px"));
        m_strokeWidthSpin->setValue(qRound(qMax<qreal>(1.0, composite->StrokeWidth())));
        m_initialStrokeWidth = composite->StrokeWidth();
        m_strokeWidthSpin->setMinimumWidth(88);
    } else if (auto* stroke = dynamic_cast<LineItemAbstract*>(item)) {
        m_strokeColorButton = ItemPropertiesFields::MakeColorField(this, stroke->StrokeColor(),
                                                                   QStringLiteral("ItemPropertiesStrokeColorValue"),
                                                                   ItemPropertiesFields::ColorIconRole::BORDER);

        m_borderStyleCombo = new QComboBox(this);
        m_borderStyleCombo->setObjectName(QStringLiteral("ItemPropertiesBorderStyleValue"));
        m_borderStyleCombo->setMinimumWidth(180);
        for (Qt::PenStyle penStyle : ItemStyleVisuals::BorderStyles()) {
            m_borderStyleCombo->addItem(ItemStyleVisuals::BorderStyleIcon(penStyle, 16),
                                        ItemStyleVisuals::BorderStyleName(penStyle), static_cast<int>(penStyle));
        }
        const int borderStyleIndex = m_borderStyleCombo->findData(static_cast<int>(stroke->strokePattern()));
        if (borderStyleIndex >= 0) {
            m_borderStyleCombo->setCurrentIndex(borderStyleIndex);
        }

        m_strokeWidthSpin = new QSpinBox(this);
        m_strokeWidthSpin->setObjectName(QStringLiteral("ItemPropertiesStrokeWidthValue"));
        m_strokeWidthSpin->setRange(1, 64);
        m_strokeWidthSpin->setSingleStep(1);
        m_strokeWidthSpin->setSuffix(QStringLiteral(" px"));
        m_strokeWidthSpin->setValue(qRound(stroke->StrokeWidth()));
        m_initialStrokeWidth = stroke->StrokeWidth();
        m_strokeWidthSpin->setMinimumWidth(88);

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

void ItemPropertiesDialogStyleBlock::AddRows(QFormLayout* form)
{
    if (m_fillStyleCombo) {
        form->addRow(ItemPropertiesDialog::tr("Fill style"), m_fillStyleCombo);
    }
    if (m_fillColorButton) {
        form->addRow(ItemPropertiesDialog::tr("Fill color"), m_fillColorButton);
    }
    if (m_fillImageRow) {
        form->addRow(ItemPropertiesDialog::tr("Fill image"), m_fillImageRow);
    }
    // 需求 1 行序：描线样式排在描线颜色之前（与菜单、工具栏、编辑卡统一）。
    if (m_borderStyleCombo) {
        form->addRow(ItemPropertiesDialog::tr("Border style"), m_borderStyleCombo);
    }
    if (m_strokeColorButton) {
        form->addRow(ItemPropertiesDialog::tr("Border color"), m_strokeColorButton);
    }
    if (m_strokeWidthSpin) {
        form->addRow(ItemPropertiesDialog::tr("Border width"), m_strokeWidthSpin);
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
}

ImageFill::Mode ItemPropertiesDialogStyleBlock::FillImageMode() const
{
    if (!m_fillImageModeCombo) {
        return ImageFill::Mode::STRETCH;
    }
    return static_cast<ImageFill::Mode>(m_fillImageModeCombo->currentData().toInt());
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
