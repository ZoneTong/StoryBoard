#include "ItemDockPanel.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "CustomShapeTemplates.h"
#include "Settings.h"

#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QScrollArea>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QToolButton* makeSectionToggle(QWidget* parent)
{
    auto* toggle = new QToolButton(parent);
    toggle->setObjectName(QStringLiteral("ItemSectionToggle"));
    toggle->setCheckable(true);
    toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toggle->setArrowType(Qt::RightArrow);
    toggle->setAutoRaise(true);
    return toggle;
}

QToolButton* makeSwatch(const QColor& color, QWidget* parent, const QString& objectName)
{
    auto* button = new QToolButton(parent);
    button->setObjectName(objectName);
    button->setFixedSize(22, 22);
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

} // namespace

ItemDockPanel::ItemDockPanel(CanvasScene* scene, QWidget* parent) : QWidget(parent), m_scene(scene)
{
    setObjectName(QStringLiteral("ItemPanel"));

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(10, 8, 10, 8);
    outer->setSpacing(6);

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

    QWidget* shapesBody = new QWidget(content);
    buildShapesSection(shapesBody);
    layout->addWidget(createSection(tr("Common shapes"), shapesBody, true));

    QWidget* colorsBody = new QWidget(content);
    buildColorsSection(colorsBody);
    layout->addWidget(createSection(tr("Common colors"), colorsBody, true));

    QWidget* fillBody = new QWidget(content);
    buildFillSection(fillBody);
    layout->addWidget(createSection(tr("Fill style"), fillBody, true));

    QWidget* borderBody = new QWidget(content);
    buildBorderSection(borderBody);
    layout->addWidget(createSection(tr("Border style"), borderBody, true));

    QWidget* opacityBody = new QWidget(content);
    buildOpacitySection(opacityBody);
    layout->addWidget(createSection(tr("Opacity"), opacityBody, false));

    QWidget* cornerBody = new QWidget(content);
    buildCornerSection(cornerBody);
    m_cornerSection = createSection(tr("Corner radius"), cornerBody, false);
    layout->addWidget(m_cornerSection);

    QWidget* arrangeBody = new QWidget(content);
    buildArrangeSection(arrangeBody);
    layout->addWidget(createSection(tr("Layer and order"), arrangeBody, true));
    layout->addStretch(1);

    scroll->setWidget(content);
    outer->addWidget(scroll, 1);

    if (m_scene) {
        connect(m_scene, &QGraphicsScene::selectionChanged, this, &ItemDockPanel::scheduleRefresh);
        connect(m_scene, &CanvasScene::contentChanged, this, &ItemDockPanel::scheduleRefresh);
    }

    refresh();
}

QWidget* ItemDockPanel::createSection(const QString& title, QWidget* body, bool expanded)
{
    auto* section = new QWidget(this);
    section->setObjectName(QStringLiteral("ItemSection"));
    auto* layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->setSpacing(2);

    QToolButton* toggle = makeSectionToggle(section);
    toggle->setText(title);
    toggle->setChecked(expanded);
    toggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    layout->addWidget(toggle);

    body->setObjectName(QStringLiteral("ItemSectionBody"));
    body->setVisible(expanded);
    layout->addWidget(body);

    connect(toggle, &QToolButton::toggled, this, [toggle, body](bool checked) {
        body->setVisible(checked);
        toggle->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
    });
    return section;
}

void ItemDockPanel::buildShapesSection(QWidget* body)
{
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(2, 0, 2, 0);
    layout->setSpacing(4);

    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(3);
    const QList<ShapeType> types = allShapeTypes();
    for (int index = 0; index < types.size(); ++index) {
        const ShapeType type = types.at(index);
        auto* button = new QToolButton(body);
        button->setObjectName(QStringLiteral("ItemShapeButton"));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setIcon(makeShapeIcon(type, QColor(58, 64, 80), 20));
        button->setIconSize(QSize(20, 20));
        button->setFixedSize(30, 30);
        button->setToolTip(shapeTypeName(type));
        connect(button, &QToolButton::clicked, this, [this, type]() { insertOrChangeShape(type); });
        m_shapeButtons.append(button);
        grid->addWidget(button, index / 6, index % 6);
    }
    layout->addLayout(grid);

    m_customShapeButton = new QToolButton(body);
    m_customShapeButton->setObjectName(QStringLiteral("ItemCustomShapeButton"));
    m_customShapeButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_customShapeButton->setPopupMode(QToolButton::InstantPopup);
    m_customShapeButton->setArrowType(Qt::DownArrow);
    m_customShapeButton->setText(tr("Custom shapes"));
    m_customShapeButton->setIcon(makeCustomShapeIcon(QPainterPath(), QColor(58, 64, 80), 18));
    m_customShapeMenu = new QMenu(m_customShapeButton);
    m_customShapeButton->setMenu(m_customShapeMenu);
    connect(m_customShapeMenu, &QMenu::aboutToShow, this, &ItemDockPanel::rebuildCustomShapeMenu);
    layout->addWidget(m_customShapeButton);
    rebuildCustomShapeMenu();
}

void ItemDockPanel::buildColorsSection(QWidget* body)
{
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(2, 0, 2, 0);
    layout->setSpacing(4);

    const QStringList presets = {
        QStringLiteral("#1E293B"), QStringLiteral("#EF4444"), QStringLiteral("#F97316"), QStringLiteral("#EAB308"),
        QStringLiteral("#22C55E"), QStringLiteral("#14B8A6"), QStringLiteral("#3B82F6"), QStringLiteral("#8B5CF6")};

    auto makeRow = [this, body, &presets](const QString& labelText, bool stroke) {
        auto* row = new QWidget(body);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(3);

        auto* label = new QLabel(labelText, row);
        label->setObjectName(QStringLiteral("ItemColorLabel"));
        label->setFixedWidth(30);
        rowLayout->addWidget(label);

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
        custom->setFixedSize(22, 22);
        custom->setToolTip(tr("Custom color…"));
        custom->setText(QStringLiteral("…"));
        connect(custom, &QToolButton::clicked, this, [this, stroke]() {
            const QList<class ShapeItem*> shapes = selectedShapes();
            const QColor initial =
                shapes.isEmpty() ? QColor(QStringLiteral("#1E293B"))
                                 : (stroke ? shapes.first()->strokeColor() : shapes.first()->fillColor());
            const QColor chosen =
                QColorDialog::getColor(initial, this, stroke ? tr("Stroke color") : tr("Fill color"),
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

    layout->addWidget(makeRow(tr("Stroke"), true));
    layout->addWidget(makeRow(tr("Fill"), false));
    auto* hint = new QLabel(tr("Colors apply to the selected shape; transparency is kept."), body);
    hint->setObjectName(QStringLiteral("ItemColorHint"));
    hint->setWordWrap(true);
    layout->addWidget(hint);
}

void ItemDockPanel::buildFillSection(QWidget* body)
{
    auto* grid = new QGridLayout(body);
    grid->setContentsMargins(2, 0, 2, 0);
    grid->setSpacing(3);
    const QList<FillStyle> styles = allFillStyles();
    for (int index = 0; index < styles.size(); ++index) {
        const FillStyle style = styles.at(index);
        auto* button = new QToolButton(body);
        button->setObjectName(QStringLiteral("ItemFillStyleButton"));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setText(fillStyleName(style));
        button->setToolTip(fillStyleName(style));
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(button, &QToolButton::clicked, this, [this, style]() { applyFillStyle(style); });
        m_fillStyleButtons.append(button);
        grid->addWidget(button, index / 3, index % 3);
    }
}

void ItemDockPanel::buildBorderSection(QWidget* body)
{
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(2, 0, 2, 0);
    layout->setSpacing(4);

    auto* styleRow = new QWidget(body);
    auto* styleLayout = new QHBoxLayout(styleRow);
    styleLayout->setContentsMargins(0, 0, 0, 0);
    styleLayout->setSpacing(3);
    const QList<Qt::PenStyle> styles = {Qt::SolidLine, Qt::DashLine, Qt::DotLine, Qt::DashDotLine};
    const QStringList names = {tr("Solid"), tr("Dashed"), tr("Dotted"), tr("Dash-dot")};
    for (int index = 0; index < styles.size(); ++index) {
        auto* button = new QToolButton(styleRow);
        button->setObjectName(QStringLiteral("ItemBorderStyleButton"));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setText(names.at(index));
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        const Qt::PenStyle style = styles.at(index);
        connect(button, &QToolButton::clicked, this, [this, style]() { applyBorderStyle(style); });
        m_borderStyleButtons.append(button);
        styleLayout->addWidget(button);
    }
    layout->addWidget(styleRow);

    auto* widthRow = new QWidget(body);
    auto* widthLayout = new QHBoxLayout(widthRow);
    widthLayout->setContentsMargins(0, 0, 0, 0);
    widthLayout->setSpacing(6);
    auto* widthLabel = new QLabel(tr("Width"), widthRow);
    widthLabel->setObjectName(QStringLiteral("ItemColorLabel"));
    m_borderWidthSpin = new QDoubleSpinBox(widthRow);
    m_borderWidthSpin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_borderWidthSpin->setRange(0.5, 48.0);
    m_borderWidthSpin->setSingleStep(0.5);
    m_borderWidthSpin->setDecimals(1);
    m_borderWidthSpin->setSuffix(QStringLiteral(" px"));
    connect(m_borderWidthSpin, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (!m_refreshing) {
            applyBorderWidth(value);
        }
    });
    widthLayout->addWidget(widthLabel);
    widthLayout->addWidget(m_borderWidthSpin, 1);
    layout->addWidget(widthRow);
}

void ItemDockPanel::buildOpacitySection(QWidget* body)
{
    auto* layout = new QHBoxLayout(body);
    layout->setContentsMargins(2, 0, 2, 0);
    layout->setSpacing(6);
    auto* label = new QLabel(tr("Opacity"), body);
    label->setObjectName(QStringLiteral("ItemColorLabel"));
    m_opacitySpin = new QSpinBox(body);
    m_opacitySpin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_opacitySpin->setRange(0, 100);
    m_opacitySpin->setSuffix(QStringLiteral(" %"));
    connect(m_opacitySpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_refreshing) {
            applyOpacity(value);
        }
    });
    layout->addWidget(label);
    layout->addWidget(m_opacitySpin, 1);
}

void ItemDockPanel::buildCornerSection(QWidget* body)
{
    auto* layout = new QHBoxLayout(body);
    layout->setContentsMargins(2, 0, 2, 0);
    layout->setSpacing(6);
    auto* label = new QLabel(tr("Radius"), body);
    label->setObjectName(QStringLiteral("ItemColorLabel"));
    m_cornerSpin = new QSpinBox(body);
    m_cornerSpin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_cornerSpin->setRange(0, 400);
    m_cornerSpin->setSuffix(QStringLiteral(" px"));
    connect(m_cornerSpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_refreshing) {
            applyCornerRadius(value);
        }
    });
    layout->addWidget(label);
    layout->addWidget(m_cornerSpin, 1);
}

void ItemDockPanel::buildArrangeSection(QWidget* body)
{
    auto* grid = new QGridLayout(body);
    grid->setContentsMargins(2, 0, 2, 0);
    grid->setSpacing(3);

    auto makeButton = [this, body](const QString& text, const QString& objectName, int delta, bool height) {
        auto* button = new QToolButton(body);
        button->setObjectName(objectName);
        button->setText(text);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(button, &QToolButton::clicked, this, [this, delta, height]() {
            if (height) {
                adjustHeight(delta);
            } else {
                adjustLayer(delta);
            }
        });
        return button;
    };

    m_layerUpButton = makeButton(tr("Layer +1"), QStringLiteral("ItemLayerUpButton"), 1, false);
    m_layerDownButton = makeButton(tr("Layer -1"), QStringLiteral("ItemLayerDownButton"), -1, false);
    m_heightUpButton = makeButton(tr("Order +1"), QStringLiteral("ItemHeightUpButton"), 1, true);
    m_heightDownButton = makeButton(tr("Order -1"), QStringLiteral("ItemHeightDownButton"), -1, true);
    grid->addWidget(m_layerUpButton, 0, 0);
    grid->addWidget(m_layerDownButton, 0, 1);
    grid->addWidget(m_heightUpButton, 1, 0);
    grid->addWidget(m_heightDownButton, 1, 1);

    auto* hint = new QLabel(tr("Layer moves across layers; order changes stacking inside the layer."), body);
    hint->setObjectName(QStringLiteral("ItemColorHint"));
    hint->setWordWrap(true);
    grid->addWidget(hint, 2, 0, 1, 2);
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
    m_metaLabel->setText(tr("%1 · layer %2 · order %3")
                             .arg(item->typeDisplayName())
                             .arg(item->layerId())
                             .arg(item->height()));
}

void ItemDockPanel::updateSections()
{
    const QList<ShapeItem*> shapes = selectedShapes();
    bool hasCorner = false;
    for (ShapeItem* shape : shapes) {
        if (shape->shapeType() == ShapeType::Rectangle || shape->shapeType() == ShapeType::RoundedRectangle) {
            hasCorner = true;
            break;
        }
    }
    if (m_cornerSection) {
        m_cornerSection->setVisible(hasCorner);
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
            m_customShapeButton->setText(shapes.first()->customShapeName());
        } else {
            m_customShapeButton->setText(tr("Custom shapes"));
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
        const FillStyle style = static_cast<FillStyle>(m_fillStyleButtons.indexOf(button));
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

void ItemDockPanel::changeSelectedShapes(const QString& text,
                                         const std::function<void(ShapeItem*)>& mutate)
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
    changeSelectedShapes(tr("Change shape border style"),
                         [style](ShapeItem* shape) { shape->setStrokeStyle(style); });
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
        statusMessage(count > 0 ? tr("Adjusted the stacking order of %1 item(s).").arg(count)
                                : tr("The stacking order did not change."));
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
    const QList<CustomShapeTemplate> templates = loadCustomShapeTemplates(AppSettings::shapesDir(), &errors);
    if (templates.isEmpty()) {
        QAction* empty = m_customShapeMenu->addAction(tr("(No custom shapes in the shapes folder)"));
        empty->setEnabled(false);
    } else {
        for (const CustomShapeTemplate& entry : templates) {
            QAction* action = m_customShapeMenu->addAction(makeCustomShapeIcon(entry.unitPath, QColor(58, 64, 80), 18),
                                                           entry.name);
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
