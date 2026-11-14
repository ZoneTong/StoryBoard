#include "ItemDockPanel.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "CustomShapeTemplates.h"
#include "IconFactory.h"
#include "Settings.h"
#include "ShapeItem.h"

#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QScrollArea>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>

namespace {

constexpr int kIconSize = 16;
constexpr int kSmallIconSize = 14;

QToolButton* makeSwatch(const QColor& color, QWidget* parent, const QString& objectName)
{
    auto* button = new QToolButton(parent);
    button->setObjectName(objectName);
    button->setFixedSize(20, 20);
    button->setCheckable(true);
    button->setToolTip(color.name(QColor::HexRgb).toUpper());
    button->setStyleSheet(QStringLiteral("QToolButton { border: 1px solid #CBD5E1; border-radius: 3px; "
                                         "background: %1; } QToolButton:hover { border-color: #2563EB; } "
                                         "QToolButton:checked { border: 2px solid #1D4ED8; }")
                              .arg(color.name(QColor::HexArgb)));
    button->setProperty("swatchColor", color);
    return button;
}

QString colorTip(const QColor& color)
{
    return color.name(QColor::HexRgb).toUpper();
}

// 填充样式按钮的缩略图：None 画叉、Solid 纯色、纹理复用 ShapeItem 的纹理中心线。
QIcon makeFillStyleIcon(FillStyle style, int logicalSize)
{
    constexpr qreal scale = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * scale), static_cast<int>(logicalSize * scale));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(scale);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF rect(3.0, 3.0, logicalSize - 6.0, logicalSize - 6.0);
    painter.setPen(QPen(QColor(120, 128, 145), 1.2));
    if (style == FillStyle::None) {
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect);
        painter.drawLine(rect.topLeft(), rect.bottomRight());
        painter.drawLine(rect.topRight(), rect.bottomLeft());
        painter.end();
        return QIcon(pixmap);
    }
    if (style == FillStyle::Solid) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(58, 64, 80));
        painter.drawRect(rect);
        painter.end();
        return QIcon(pixmap);
    }
    painter.setBrush(QColor(248, 249, 252));
    painter.drawRect(rect);
    painter.setPen(QPen(QColor(58, 64, 80), 1.0, Qt::SolidLine, Qt::FlatCap));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(fillPatternCenterLines(style, rect));
    painter.end();
    return QIcon(pixmap);
}

// 边框样式按钮的缩略图：按 4 种 PenStyle 画一小段线。
QIcon makeBorderStyleIcon(Qt::PenStyle style, int logicalSize)
{
    constexpr qreal scale = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * scale), static_cast<int>(logicalSize * scale));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(scale);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(58, 64, 80), 1.8, style, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(QPointF(4.0, logicalSize / 2.0), QPointF(logicalSize - 4.0, logicalSize / 2.0));
    painter.end();
    return QIcon(pixmap);
}

// 边框样式按钮的提示文案。
QString borderStyleName(Qt::PenStyle style)
{
    switch (style) {
    case Qt::SolidLine:
        return ItemDockPanel::tr("Solid");
    case Qt::DashLine:
        return ItemDockPanel::tr("Dashed");
    case Qt::DotLine:
        return ItemDockPanel::tr("Dotted");
    case Qt::DashDotLine:
        return ItemDockPanel::tr("Dash-dot");
    default:
        return ItemDockPanel::tr("Solid");
    }
}

// 行首小图标按钮：紧凑布局里用图标代替文字小标题。
QToolButton* makeRowIcon(IconId id, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("ItemRowIcon"));
    button->setIcon(makeIcon(id, QColor(139, 144, 156), kSmallIconSize));
    button->setIconSize(QSize(kSmallIconSize, kSmallIconSize));
    button->setFixedSize(kSmallIconSize + 4, kSmallIconSize + 4);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

} // namespace

ItemDockPanel::ItemDockPanel(CanvasScene* scene, QWidget* parent) : QWidget(parent), m_scene(scene)
{
    setObjectName(QStringLiteral("ItemPanel"));

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(6, 4, 6, 4);
    outer->setSpacing(3);

    m_titleLabel = new QLabel(tr("Selected item"), this);
    m_titleLabel->setObjectName(QStringLiteral("ItemTitle"));
    m_metaLabel = new QLabel(this);
    m_metaLabel->setObjectName(QStringLiteral("ItemMeta"));
    m_metaLabel->setWordWrap(true);
    outer->addWidget(m_titleLabel);
    outer->addWidget(m_metaLabel);

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("ItemPanelScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("ItemPanelContent"));
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // 紧凑布局：不再做带文字的折叠分组标题，各行内容直接平铺，
    // 行首用小图标标注用途（描边 / 填充 / 边框 / 透明度 / 圆角 / 图层 / 顺序）。
    QWidget* shapesBody = new QWidget(content);
    buildShapesSection(shapesBody);
    layout->addWidget(shapesBody);

    QWidget* colorsBody = new QWidget(content);
    buildColorsSection(colorsBody);
    layout->addWidget(colorsBody);

    QWidget* fillBody = new QWidget(content);
    buildFillSection(fillBody);
    layout->addWidget(fillBody);

    QWidget* borderBody = new QWidget(content);
    buildBorderSection(borderBody);
    layout->addWidget(borderBody);

    QWidget* opacityBody = new QWidget(content);
    buildOpacitySection(opacityBody);
    layout->addWidget(opacityBody);

    QWidget* cornerBody = new QWidget(content);
    buildCornerSection(cornerBody);
    m_cornerSection = cornerBody;
    layout->addWidget(cornerBody);

    QWidget* angleBody = new QWidget(content);
    buildAngleSection(angleBody);
    m_angleSection = angleBody;
    layout->addWidget(angleBody);

    QWidget* radiusBody = new QWidget(content);
    buildRadiusSection(radiusBody);
    m_radiusSection = radiusBody;
    layout->addWidget(radiusBody);

    QWidget* arrangeBody = new QWidget(content);
    buildArrangeSection(arrangeBody);
    layout->addWidget(arrangeBody);
    layout->addStretch(1);

    scroll->setWidget(content);
    outer->addWidget(scroll, 1);

    if (m_scene) {
        connect(m_scene, &QGraphicsScene::selectionChanged, this, &ItemDockPanel::scheduleRefresh);
        connect(m_scene, &CanvasScene::contentChanged, this, &ItemDockPanel::scheduleRefresh);
        if (m_scene->undoStack()) {
            // 撤销 / 重做后卡片里的数值也要回到图元的真实状态。
            connect(m_scene->undoStack(), &QUndoStack::indexChanged, this, &ItemDockPanel::scheduleRefresh);
        }
    }

    refresh();
}

void ItemDockPanel::buildShapesSection(QWidget* body)
{
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(2);
    const QList<ShapeType> types = allShapeTypes();
    for (int index = 0; index < types.size(); ++index) {
        const ShapeType type = types.at(index);
        auto* button = new QToolButton(body);
        button->setObjectName(QStringLiteral("ItemShapeButton"));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setIcon(makeShapeIcon(type, QColor(58, 64, 80), 18));
        button->setIconSize(QSize(18, 18));
        button->setFixedSize(26, 26);
        button->setToolTip(tr("Common shapes") + QStringLiteral(" · ") + shapeTypeName(type));
        connect(button, &QToolButton::clicked, this, [this, type]() { insertOrChangeShape(type); });
        m_shapeButtons.append(button);
        grid->addWidget(button, index / 6, index % 6);
    }
    layout->addLayout(grid);

    m_customShapeButton = new QToolButton(body);
    m_customShapeButton->setObjectName(QStringLiteral("ItemCustomShapeButton"));
    m_customShapeButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_customShapeButton->setPopupMode(QToolButton::InstantPopup);
    m_customShapeButton->setArrowType(Qt::DownArrow);
    m_customShapeButton->setText(tr("Custom shapes"));
    m_customShapeButton->setIcon(makeCustomShapeIcon(QPainterPath(), QColor(58, 64, 80), kIconSize));
    m_customShapeButton->setIconSize(QSize(kIconSize, kIconSize));
    m_customShapeButton->setFixedSize(26, 26);
    m_customShapeButton->setToolTip(tr("Custom shapes"));
    m_customShapeMenu = new QMenu(m_customShapeButton);
    m_customShapeButton->setMenu(m_customShapeMenu);
    connect(m_customShapeMenu, &QMenu::aboutToShow, this, &ItemDockPanel::rebuildCustomShapeMenu);
    grid->addWidget(m_customShapeButton, types.size() / 6, types.size() % 6);
    rebuildCustomShapeMenu();
}

void ItemDockPanel::buildColorsSection(QWidget* body)
{
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    const QStringList presets = {QStringLiteral("#1E293B"), QStringLiteral("#EF4444"), QStringLiteral("#F97316"),
                                 QStringLiteral("#EAB308"), QStringLiteral("#22C55E"), QStringLiteral("#14B8A6"),
                                 QStringLiteral("#3B82F6"), QStringLiteral("#8B5CF6")};

    auto makeRow = [this, body, &presets](IconId rowIcon, bool stroke) {
        auto* row = new QWidget(body);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(2);

        QToolButton* icon = makeRowIcon(rowIcon, row);
        icon->setToolTip(stroke ? tr("Stroke") : tr("Fill"));
        rowLayout->addWidget(icon);

        QList<QToolButton*>* swatches = stroke ? &m_strokeSwatches : &m_fillSwatches;
        for (const QString& hex : presets) {
            const QColor color(hex);
            QToolButton* swatch = makeSwatch(color, row, QStringLiteral("ItemColorSwatch"));
            connect(swatch, &QToolButton::clicked, this, [this, color, stroke]() {
                if (stroke) {
                    applyStrokeColor(color);
                } else {
                    applyFillColor(color);
                }
            });
            rowLayout->addWidget(swatch);
            swatches->append(swatch);
        }

        auto* custom = new QToolButton(row);
        custom->setObjectName(QStringLiteral("ItemColorButton"));
        custom->setFixedSize(20, 20);
        custom->setToolTip(stroke ? tr("Stroke color") : tr("Fill color"));
        custom->setText(QStringLiteral("…"));
        connect(custom, &QToolButton::clicked, this, [this, stroke]() {
            const QList<class ShapeItem*> shapes = selectedShapes();
            const QColor initial = shapes.isEmpty()
                                       ? QColor(QStringLiteral("#1E293B"))
                                       : (stroke ? shapes.first()->strokeColor() : shapes.first()->fillColor());
            const QColor chosen = QColorDialog::getColor(initial, this, stroke ? tr("Stroke color") : tr("Fill color"),
                                                         QColorDialog::ShowAlphaChannel);
            if (!chosen.isValid()) {
                return;
            }
            if (stroke) {
                applyStrokeColor(chosen);
            } else {
                applyFillColor(chosen);
            }
        });
        rowLayout->addWidget(custom);

        if (stroke) {
            m_strokeColorButton = custom;
        } else {
            m_fillColorButton = custom;
        }
        rowLayout->addStretch(1);
        return row;
    };

    // 描边行用画笔图标，填充行用色板图标，代替原来的文字小标题。
    layout->addWidget(makeRow(IconId::Brush, true));
    layout->addWidget(makeRow(IconId::Palette, false));
}

void ItemDockPanel::buildFillSection(QWidget* body)
{
    auto* row = new QHBoxLayout(body);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(2);

    QToolButton* icon = makeRowIcon(IconId::Image, body);
    icon->setToolTip(tr("Fill style"));
    row->addWidget(icon);

    const QList<FillStyle> styles = allFillStyles();
    for (const FillStyle style : styles) {
        auto* button = new QToolButton(body);
        button->setObjectName(QStringLiteral("ItemFillStyleButton"));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setIcon(makeFillStyleIcon(style, kIconSize));
        button->setIconSize(QSize(kIconSize, kIconSize));
        button->setFixedSize(24, 24);
        button->setToolTip(fillStyleName(style));
        button->setProperty("fillStyle", static_cast<int>(style));
        connect(button, &QToolButton::clicked, this, [this, style]() { applyFillStyle(style); });
        m_fillStyleButtons.append(button);
        row->addWidget(button);
    }
    row->addStretch(1);
}

void ItemDockPanel::buildBorderSection(QWidget* body)
{
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    auto* styleRow = new QWidget(body);
    auto* styleLayout = new QHBoxLayout(styleRow);
    styleLayout->setContentsMargins(0, 0, 0, 0);
    styleLayout->setSpacing(2);

    QToolButton* icon = makeRowIcon(IconId::Edit, styleRow);
    icon->setToolTip(tr("Border style"));
    styleLayout->addWidget(icon);

    const QList<Qt::PenStyle> styles = {Qt::SolidLine, Qt::DashLine, Qt::DotLine, Qt::DashDotLine};
    for (int index = 0; index < styles.size(); ++index) {
        auto* button = new QToolButton(styleRow);
        button->setObjectName(QStringLiteral("ItemBorderStyleButton"));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setIcon(makeBorderStyleIcon(styles.at(index), kIconSize));
        button->setIconSize(QSize(kIconSize, kIconSize));
        button->setFixedSize(24, 24);
        button->setToolTip(borderStyleName(styles.at(index)));
        const Qt::PenStyle style = styles.at(index);
        connect(button, &QToolButton::clicked, this, [this, style]() { applyBorderStyle(style); });
        m_borderStyleButtons.append(button);
        styleLayout->addWidget(button);
    }
    styleLayout->addStretch(1);
    layout->addWidget(styleRow);

    auto* widthRow = new QWidget(body);
    auto* widthLayout = new QHBoxLayout(widthRow);
    widthLayout->setContentsMargins(0, 0, 0, 0);
    widthLayout->setSpacing(4);
    m_borderWidthSpin = new QDoubleSpinBox(widthRow);
    m_borderWidthSpin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_borderWidthSpin->setRange(0.5, 48.0);
    m_borderWidthSpin->setSingleStep(0.5);
    m_borderWidthSpin->setDecimals(1);
    m_borderWidthSpin->setSuffix(QStringLiteral(" px"));
    m_borderWidthSpin->setToolTip(tr("Width"));
    connect(m_borderWidthSpin, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (!m_refreshing) {
            applyBorderWidth(value);
        }
    });
    widthLayout->addWidget(m_borderWidthSpin);
    widthLayout->addStretch(1);
    layout->addWidget(widthRow);
}

void ItemDockPanel::buildOpacitySection(QWidget* body)
{
    auto* layout = new QHBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    QToolButton* icon = makeRowIcon(IconId::Opacity, body);
    icon->setToolTip(tr("Opacity"));
    layout->addWidget(icon);

    m_opacitySpin = new QSpinBox(body);
    m_opacitySpin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_opacitySpin->setRange(0, 100);
    m_opacitySpin->setSuffix(QStringLiteral(" %"));
    m_opacitySpin->setToolTip(tr("Opacity"));
    connect(m_opacitySpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_refreshing) {
            applyOpacity(value);
        }
    });
    layout->addWidget(m_opacitySpin, 1);
}

void ItemDockPanel::buildCornerSection(QWidget* body)
{
    auto* layout = new QHBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    QToolButton* icon = makeRowIcon(IconId::Corner, body);
    icon->setToolTip(tr("Corner radius"));
    layout->addWidget(icon);

    m_cornerSpin = new QSpinBox(body);
    m_cornerSpin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_cornerSpin->setRange(0, 400);
    m_cornerSpin->setSuffix(QStringLiteral(" px"));
    m_cornerSpin->setToolTip(tr("Corner radius"));
    connect(m_cornerSpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_refreshing) {
            applyCornerRadius(value);
        }
    });
    layout->addWidget(m_cornerSpin, 1);
}

void ItemDockPanel::buildAngleSection(QWidget* body)
{
    auto* layout = new QHBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    QToolButton* icon = makeRowIcon(IconId::RotateCcw, body);
    icon->setToolTip(tr("Sweep angle"));
    layout->addWidget(icon);

    m_angleSpin = new QSpinBox(body);
    m_angleSpin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_angleSpin->setRange(1, 359);
    m_angleSpin->setSuffix(QStringLiteral("°"));
    m_angleSpin->setToolTip(tr("Sweep angle"));
    connect(m_angleSpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_refreshing) {
            applySweepAngle(value);
        }
    });
    layout->addWidget(m_angleSpin, 1);
}

void ItemDockPanel::buildRadiusSection(QWidget* body)
{
    auto* layout = new QHBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    QToolButton* icon = makeRowIcon(IconId::Corner, body);
    icon->setToolTip(tr("Shape radius"));
    layout->addWidget(icon);

    m_radiusSpin = new QSpinBox(body);
    m_radiusSpin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_radiusSpin->setRange(1, 1000);
    m_radiusSpin->setSuffix(QStringLiteral(" px"));
    m_radiusSpin->setToolTip(tr("Shape radius"));
    connect(m_radiusSpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_refreshing) {
            applyShapeRadius(value);
        }
    });
    layout->addWidget(m_radiusSpin, 1);
}

void ItemDockPanel::buildArrangeSection(QWidget* body)
{
    auto* row = new QHBoxLayout(body);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(2);

    QToolButton* icon = makeRowIcon(IconId::OrderUp, body);
    icon->setToolTip(tr("Layer and height") + QLatin1Char('\n') +
                     tr("Priority: layer order first, then height within the same layer"));
    row->addWidget(icon);

    auto makeButton = [this, body](IconId id, const QString& toolTip, const QString& objectName, int delta,
                                   bool height) {
        auto* button = new QToolButton(body);
        button->setObjectName(objectName);
        button->setIcon(makeIcon(id, QColor(75, 85, 99), kIconSize));
        button->setIconSize(QSize(kIconSize, kIconSize));
        button->setFixedSize(24, 24);
        button->setToolTip(toolTip);
        button->setAutoRaise(true);
        connect(button, &QToolButton::clicked, this, [this, delta, height]() {
            if (height) {
                adjustHeight(delta);
            } else {
                adjustLayer(delta);
            }
        });
        return button;
    };

    m_layerUpButton = makeButton(IconId::LayerUp, tr("Layer +1"), QStringLiteral("ItemLayerUpButton"), 1, false);
    m_layerDownButton = makeButton(IconId::LayerDown, tr("Layer -1"), QStringLiteral("ItemLayerDownButton"), -1, false);
    m_heightUpButton = makeButton(IconId::OrderUp, tr("Height +1"), QStringLiteral("ItemHeightUpButton"), 1, true);
    m_heightDownButton =
        makeButton(IconId::OrderDown, tr("Height -1"), QStringLiteral("ItemHeightDownButton"), -1, true);
    row->addWidget(m_layerUpButton);
    row->addWidget(m_layerDownButton);
    row->addWidget(m_heightUpButton);
    row->addWidget(m_heightDownButton);
    row->addStretch(1);
}

QList<CanvasItem*> ItemDockPanel::selectedItems() const
{
    return m_scene ? m_scene->selectedCanvasItems() : QList<CanvasItem*>();
}

QList<ShapeItem*> ItemDockPanel::selectedShapes() const
{
    QList<ShapeItem*> shapes;
    for (CanvasItem* item : selectedItems()) {
        if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
            shapes.append(shape);
        }
    }
    return shapes;
}

void ItemDockPanel::scheduleRefresh()
{
    if (m_refreshing) {
        m_refreshPending = true;
        return;
    }
    QTimer::singleShot(0, this, &ItemDockPanel::refresh);
}

void ItemDockPanel::refresh()
{
    if (!m_scene) {
        return;
    }
    m_refreshing = true;
    updateSummary();
    updateSections();
    updateShapeButtons();
    updateColorSwatches();
    updateFillButtons();
    updateBorderButtons();
    updateValueEditors();
    updateArrangeButtons();
    m_refreshing = false;
    if (m_refreshPending) {
        m_refreshPending = false;
        scheduleRefresh();
    }
}

void ItemDockPanel::updateSummary()
{
    const QList<CanvasItem*> items = selectedItems();
    if (items.isEmpty()) {
        m_titleLabel->setText(tr("No item selected"));
        m_metaLabel->setText(tr("Select an item on the canvas to edit its style here."));
        return;
    }
    if (items.size() > 1) {
        m_titleLabel->setText(tr("%1 items selected").arg(items.size()));
        m_metaLabel->setText(tr("Changes apply to every selected shape."));
        return;
    }
    CanvasItem* item = items.first();
    m_titleLabel->setText(item->displayName());
    m_metaLabel->setText(
        tr("%1 · layer %2 · height %3").arg(item->typeDisplayName()).arg(item->layerId()).arg(item->height()));
}

void ItemDockPanel::updateSections()
{
    const QList<ShapeItem*> shapes = selectedShapes();
    bool hasCorner = false;
    bool hasArc = false;
    for (ShapeItem* shape : shapes) {
        if (shape->shapeType() == ShapeType::Rectangle || shape->shapeType() == ShapeType::RoundedRectangle) {
            hasCorner = true;
        }
        if (shape->shapeType() == ShapeType::Sector || shape->shapeType() == ShapeType::Arc) {
            hasArc = true;
        }
    }
    if (m_cornerSection) {
        m_cornerSection->setVisible(hasCorner);
    }
    if (m_angleSection) {
        m_angleSection->setVisible(hasArc);
    }
    if (m_radiusSection) {
        m_radiusSection->setVisible(hasArc);
    }
}

void ItemDockPanel::updateShapeButtons()
{
    const QList<ShapeItem*> shapes = selectedShapes();
    const ShapeType current = shapes.isEmpty() ? ShapeType::Rectangle : shapes.first()->shapeType();
    const bool hasCustom = !shapes.isEmpty() && shapes.first()->isCustomShape();
    for (QToolButton* button : m_shapeButtons) {
        const ShapeType type = static_cast<ShapeType>(m_shapeButtons.indexOf(button));
        button->setChecked(!hasCustom && !shapes.isEmpty() && type == current);
    }
    if (m_customShapeButton) {
        m_customShapeButton->setChecked(hasCustom);
        if (hasCustom) {
            m_customShapeButton->setToolTip(shapes.first()->customShapeName());
        } else {
            m_customShapeButton->setToolTip(tr("Custom shapes"));
        }
    }
}

void ItemDockPanel::updateColorSwatches()
{
    const QList<ShapeItem*> shapes = selectedShapes();
    const QColor stroke = shapes.isEmpty() ? QColor(QStringLiteral("#1E293B")) : shapes.first()->strokeColor();
    const QColor fill = shapes.isEmpty() ? QColor(QStringLiteral("#1E293B")) : shapes.first()->fillColor();
    for (QToolButton* swatch : m_strokeSwatches) {
        const QColor color = swatch->property("swatchColor").value<QColor>();
        swatch->setChecked(color.rgb() == stroke.rgb());
    }
    for (QToolButton* swatch : m_fillSwatches) {
        const QColor color = swatch->property("swatchColor").value<QColor>();
        swatch->setChecked(color.rgb() == fill.rgb());
    }
    if (m_strokeColorButton) {
        m_strokeColorButton->setToolTip(colorTip(stroke));
    }
    if (m_fillColorButton) {
        m_fillColorButton->setToolTip(colorTip(fill));
    }
}

void ItemDockPanel::updateFillButtons()
{
    const QList<ShapeItem*> shapes = selectedShapes();
    const FillStyle current = shapes.isEmpty() ? FillStyle::None : shapes.first()->fillStyle();
    for (QToolButton* button : m_fillStyleButtons) {
        const FillStyle style = static_cast<FillStyle>(button->property("fillStyle").toInt());
        button->setChecked(!shapes.isEmpty() && style == current);
    }
}

void ItemDockPanel::updateBorderButtons()
{
    const QList<ShapeItem*> shapes = selectedShapes();
    const Qt::PenStyle current = shapes.isEmpty() ? Qt::SolidLine : shapes.first()->strokeStyle();
    const QList<Qt::PenStyle> styles = {Qt::SolidLine, Qt::DashLine, Qt::DotLine, Qt::DashDotLine};
    for (int index = 0; index < m_borderStyleButtons.size(); ++index) {
        m_borderStyleButtons.at(index)->setChecked(!shapes.isEmpty() && styles.at(index) == current);
    }
    if (m_borderWidthSpin) {
        m_borderWidthSpin->setEnabled(!shapes.isEmpty());
        m_borderWidthSpin->setValue(shapes.isEmpty() ? 4.0 : shapes.first()->strokeWidth());
    }
}

void ItemDockPanel::updateValueEditors()
{
    const QList<ShapeItem*> shapes = selectedShapes();
    const bool enabled = !shapes.isEmpty();
    if (m_opacitySpin) {
        m_opacitySpin->setEnabled(enabled);
        m_opacitySpin->setValue(shapes.isEmpty() ? 100 : qRound(shapes.first()->opacity() * 100.0));
    }
    if (m_cornerSpin) {
        m_cornerSpin->setEnabled(enabled);
        const qreal radius = shapes.isEmpty() ? 0.0 : qMax<qreal>(0.0, shapes.first()->cornerRadius());
        m_cornerSpin->setValue(qRound(radius));
    }
    if (m_angleSpin) {
        m_angleSpin->setEnabled(enabled);
        m_angleSpin->setValue(shapes.isEmpty() ? 120 : qRound(shapes.first()->sweepAngle()));
    }
    if (m_radiusSpin) {
        m_radiusSpin->setEnabled(enabled);
        // 自动半径（<0）显示为实际生效的短边一半，用户改一下即成为显式半径。
        qreal radius = 100.0;
        if (!shapes.isEmpty()) {
            const qreal stored = shapes.first()->shapeRadius();
            radius = stored >= 0.0 ? stored : shapes.first()->contentSize().width() / 2.0;
        }
        m_radiusSpin->setValue(qMax(1, qRound(radius)));
    }
}

void ItemDockPanel::updateArrangeButtons()
{
    const bool enabled = !selectedItems().isEmpty();
    for (QToolButton* button : {m_layerDownButton, m_layerUpButton, m_heightDownButton, m_heightUpButton}) {
        if (button) {
            button->setEnabled(enabled);
        }
    }
}

void ItemDockPanel::changeSelectedShapes(const QString& text, const std::function<void(ShapeItem*)>& mutate)
{
    const QList<ShapeItem*> shapes = selectedShapes();
    if (shapes.isEmpty() || !m_scene || !m_scene->undoStack()) {
        return;
    }

    QList<ShapeItemState> before;
    QList<ShapeItemState> after;
    before.reserve(shapes.size());
    after.reserve(shapes.size());
    for (ShapeItem* shape : shapes) {
        before.append(shape->state());
    }
    m_refreshing = true;
    for (ShapeItem* shape : shapes) {
        mutate(shape);
        after.append(shape->state());
    }
    m_refreshing = false;

    m_scene->undoStack()->push(new ShapeStyleCommand(m_scene, shapes, before, after, text));
    refresh();
}

void ItemDockPanel::applyStrokeColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    changeSelectedShapes(tr("Change shape stroke color"), [color](ShapeItem* shape) {
        shape->setStyle(color, shape->fillColor(), shape->strokeWidth());
    });
}

void ItemDockPanel::applyFillColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    changeSelectedShapes(tr("Change shape fill color"), [color](ShapeItem* shape) {
        shape->setFill(color, shape->fillStyle() == FillStyle::None ? FillStyle::Solid : shape->fillStyle());
    });
}

void ItemDockPanel::applyFillStyle(FillStyle style)
{
    changeSelectedShapes(tr("Change shape fill style"), [style](ShapeItem* shape) { shape->setFillStyle(style); });
}

void ItemDockPanel::applyBorderStyle(Qt::PenStyle style)
{
    changeSelectedShapes(tr("Change shape border style"), [style](ShapeItem* shape) { shape->setStrokeStyle(style); });
}

void ItemDockPanel::applyBorderWidth(qreal width)
{
    changeSelectedShapes(tr("Change shape border width"), [width](ShapeItem* shape) {
        shape->setStyle(shape->strokeColor(), shape->fillColor(), width);
    });
}

void ItemDockPanel::applyOpacity(int percent)
{
    changeSelectedShapes(tr("Change shape opacity"),
                         [percent](ShapeItem* shape) { shape->setOpacity(percent / 100.0); });
}

void ItemDockPanel::applyCornerRadius(int radius)
{
    changeSelectedShapes(tr("Change rectangle corner radius"),
                         [radius](ShapeItem* shape) { shape->setCornerRadius(radius); });
}

void ItemDockPanel::applySweepAngle(int angle)
{
    changeSelectedShapes(tr("Change sector angle"),
                         [angle](ShapeItem* shape) { shape->setArcAngles(shape->startAngle(), angle); });
}

void ItemDockPanel::applyShapeRadius(int radius)
{
    changeSelectedShapes(tr("Change shape radius"), [radius](ShapeItem* shape) { shape->setShapeRadius(radius); });
}

void ItemDockPanel::insertOrChangeShape(ShapeType type)
{
    const QList<ShapeItem*> shapes = selectedShapes();
    if (shapes.isEmpty()) {
        emit insertShapeRequested(static_cast<int>(type));
        return;
    }

    QList<ShapeItemState> before;
    QList<ShapeItemState> after;
    for (ShapeItem* shape : shapes) {
        before.append(shape->state());
    }
    m_refreshing = true;
    for (ShapeItem* shape : shapes) {
        shape->setShapeType(type);
        after.append(shape->state());
    }
    m_refreshing = false;
    if (m_scene && m_scene->undoStack()) {
        m_scene->undoStack()->push(new ShapeStyleCommand(m_scene, shapes, before, after, tr("Change shape type")));
    }
    refresh();
}

void ItemDockPanel::adjustLayer(int delta)
{
    if (m_scene) {
        const int count = m_scene->moveSelectedItemsToAdjacentLayer(delta);
        statusMessage(count > 0 ? tr("Moved %1 item(s) to another layer.").arg(count)
                                : tr("No layer to move to in that direction."));
        refresh();
    }
}

void ItemDockPanel::adjustHeight(int delta)
{
    if (m_scene) {
        const int count = m_scene->adjustSelectedHeights(delta);
        statusMessage(count > 0 ? tr("Adjusted the height of %1 item(s).").arg(count)
                                : tr("The height did not change."));
        refresh();
    }
}

void ItemDockPanel::rebuildCustomShapeMenu()
{
    if (!m_customShapeMenu) {
        return;
    }
    m_customShapeMenu->clear();

    QStringList errors;
    const QList<CustomShapeTemplate> templates = loadCustomShapeTemplatesFromDirs(AppSettings::shapesDirs(), &errors);
    if (templates.isEmpty()) {
        QAction* empty = m_customShapeMenu->addAction(tr("(No custom shapes in the shapes folder)"));
        empty->setEnabled(false);
    } else {
        for (const CustomShapeTemplate& entry : templates) {
            QAction* action =
                m_customShapeMenu->addAction(makeCustomShapeIcon(entry.unitPath, QColor(58, 64, 80), 18), entry.name);
            const QString filePath = entry.filePath;
            connect(action, &QAction::triggered, this,
                    [this, filePath]() { emit insertCustomShapeRequested(filePath); });
        }
    }
    m_customShapeMenu->addSeparator();
    connect(m_customShapeMenu->addAction(tr("Open shapes folder…")), &QAction::triggered, this,
            &ItemDockPanel::openShapesFolderRequested);
    if (!errors.isEmpty()) {
        QAction* broken = m_customShapeMenu->addAction(tr("%1 shape file(s) could not be read").arg(errors.size()));
        broken->setEnabled(false);
        broken->setToolTip(errors.join(QLatin1Char('\n')));
    }
}
