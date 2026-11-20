#include "PropertiesPanel.h"

#include "ArcItem.h"
#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "CurveParamCommand.h"
#include "ImagePickerDialog.h"
#include "LineItemAbstract.h"
#include "PolygonItem.h"
#include "PropertiesPanelArrangeCard.h"
#include "PropertiesPanelColorsCard.h"
#include "PropertiesPanelContentCard.h"
#include "PropertiesPanelFillBorderCard.h"
#include "PropertiesPanelSelectionCard.h"
#include "PropertiesPanelValueCard.h"
#include "ShapeItem.h"
#include "ShapeMeta.h"
#include "SineItem.h"
#include "TextItem.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QScrollArea>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>

PropertiesPanel::PropertiesPanel(CanvasScene* scene, QWidget* parent) : QWidget(parent), m_scene(scene)
{
    setObjectName(QStringLiteral("PropertiesPanel"));

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(6, 4, 6, 4);
    outer->setSpacing(3);

    m_titleLabel = new QLabel(tr("Selected item"), this);
    m_titleLabel->setObjectName(QStringLiteral("PropertiesPanelTitle"));
    m_metaLabel = new QLabel(this);
    m_metaLabel->setObjectName(QStringLiteral("PropertiesPanelMeta"));
    m_metaLabel->setWordWrap(true);
    outer->addWidget(m_titleLabel);
    outer->addWidget(m_metaLabel);

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("PropertiesPanelScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("PropertiesPanelContent"));
    m_contentWidget = content;
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // 紧凑布局：不再做带文字的折叠分组标题，各行内容直接平铺，
    // 行首用小图标标注用途（描边 / 填充 / 边框 / 透明度 / 圆角 / 图层 / 顺序）。
    // 选中的操作行（复制/删除/旋转、对齐、合并图元、分组）统一放在卡片底部；
    // 后加的文字 / 图片内容行与样式行排在「图层与高度」行下方。
    m_selectionCard = new PropertiesPanelSelectionCard(content);
    m_selectionCard->setObjectName(QStringLiteral("ItemSelectionRowsCard"));

    m_colorsCard = new PropertiesPanelColorsCard(content);
    m_fillBorderCard = new PropertiesPanelFillBorderCard(content);
    m_contentCard = new PropertiesPanelContentCard(content);

    // 需求 9c 行序定稿（与属性窗同序）：
    // 图层/高度 → 文字内容/颜色/字体、图片 → 填充样式 → 填充色 →
    // 描边颜色 → 描边样式 → 描边宽度 → 透明度/圆角/角度/三角形/箭头 → 选择操作行。
    m_arrangeCard = new PropertiesPanelArrangeCard(content);
    m_arrangeCard->setObjectName(QStringLiteral("ItemArrangeCard"));
    layout->addWidget(m_arrangeCard);

    layout->addWidget(m_contentCard->textContentRow());
    layout->addWidget(m_contentCard->textColorRow());
    layout->addWidget(m_contentCard->textFontRow());

    // 需求 1 行序（与菜单、工具栏、属性窗统一）：
    // 填充样式 → 填充颜色 → （需求 13 引入的背景图片） → 描线样式 → 描线颜色 → 描线宽度。
    layout->addWidget(m_fillBorderCard->fillStyleRow());
    layout->addWidget(m_colorsCard->fillColorRow());

    // 填充图片行排在填充颜色之后、描线样式之前；与其它行同一套图标与值按钮样式。
    m_fillImageRow = new QWidget(content);
    m_fillImageRow->setObjectName(QStringLiteral("ItemFillImageRow"));
    auto* fillImageLayout = new QHBoxLayout(m_fillImageRow);
    fillImageLayout->setContentsMargins(0, 0, 0, 0);
    fillImageLayout->setSpacing(2);
    auto* fillImageIcon = new QToolButton(m_fillImageRow);
    fillImageIcon->setObjectName(QStringLiteral("ItemRowIcon"));
    fillImageIcon->setIcon(makeIcon(IconId::Image, QColor(139, 144, 156), 20));
    fillImageIcon->setIconSize(QSize(20, 20));
    fillImageIcon->setFixedSize(32, 32);
    fillImageIcon->setAutoRaise(true);
    fillImageIcon->setFocusPolicy(Qt::NoFocus);
    fillImageLayout->addWidget(fillImageIcon);

    m_fillImageButton = new QToolButton(m_fillImageRow);
    m_fillImageButton->setObjectName(QStringLiteral("ItemFillImageButton"));
    m_fillImageButton->setIcon(makeIcon(IconId::Image, QColor(58, 64, 80), 16));
    m_fillImageButton->setIconSize(QSize(16, 16));
    m_fillImageButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_fillImageButton->setText(tr("Choose fill image"));
    m_fillImageButton->setToolTip(tr("Choose an image to fill the selected items"));
    m_fillImageButton->setMinimumHeight(30);
    connect(m_fillImageButton, &QToolButton::clicked, this, &PropertiesPanel::chooseFillImage);
    fillImageLayout->addWidget(m_fillImageButton, 1);

    m_clearFillImageButton = new QToolButton(m_fillImageRow);
    m_clearFillImageButton->setObjectName(QStringLiteral("ItemRowIcon"));
    m_clearFillImageButton->setIcon(makeIcon(IconId::Delete, QColor(139, 144, 156), 20));
    m_clearFillImageButton->setIconSize(QSize(20, 20));
    m_clearFillImageButton->setFixedSize(32, 32);
    m_clearFillImageButton->setAutoRaise(true);
    m_clearFillImageButton->setFocusPolicy(Qt::NoFocus);
    m_clearFillImageButton->setToolTip(tr("Clear fill image"));
    connect(m_clearFillImageButton, &QToolButton::clicked, this, &PropertiesPanel::clearFillImage);
    fillImageLayout->addWidget(m_clearFillImageButton);
    layout->addWidget(m_fillImageRow);

    layout->addWidget(m_fillBorderCard->borderStyleRow());
    layout->addWidget(m_colorsCard->borderColorRow());
    layout->addWidget(m_fillBorderCard->borderWidthRow());

    m_valueCard = new PropertiesPanelValueCard(content);
    layout->addWidget(m_valueCard);

    layout->addWidget(m_selectionCard);
    layout->addStretch(1);

    scroll->setWidget(content);
    outer->addWidget(scroll, 1);

    connect(m_colorsCard, &PropertiesPanelColorsCard::strokeColorChosen, this, &PropertiesPanel::applyStrokeColor);
    connect(m_colorsCard, &PropertiesPanelColorsCard::fillColorChosen, this, &PropertiesPanel::applyFillColor);
    connect(m_fillBorderCard, &PropertiesPanelFillBorderCard::fillStyleChosen, this, &PropertiesPanel::applyFillStyle);
    connect(m_fillBorderCard, &PropertiesPanelFillBorderCard::borderStyleChosen, this,
            &PropertiesPanel::applyBorderStyle);
    connect(m_fillBorderCard, &PropertiesPanelFillBorderCard::borderWidthChosen, this,
            &PropertiesPanel::applyBorderWidth);
    connect(m_contentCard, &PropertiesPanelContentCard::textContentChosen, this, &PropertiesPanel::applyTextContent);
    connect(m_contentCard, &PropertiesPanelContentCard::textFontChosen, this, &PropertiesPanel::applyTextFont);
    connect(m_contentCard, &PropertiesPanelContentCard::textColorChosen, this, &PropertiesPanel::applyTextColor);
    connect(m_valueCard, &PropertiesPanelValueCard::opacityChosen, this, &PropertiesPanel::applyOpacity);
    connect(m_valueCard, &PropertiesPanelValueCard::cornerRadiusChosen, this, &PropertiesPanel::applyCornerRadius);
    connect(m_valueCard, &PropertiesPanelValueCard::startAngleChosen, this, &PropertiesPanel::applyStartAngle);
    connect(m_valueCard, &PropertiesPanelValueCard::sweepAngleChosen, this, &PropertiesPanel::applySweepAngle);
    connect(m_valueCard, &PropertiesPanelValueCard::sineCyclesChosen, this, &PropertiesPanel::applySineCycles);
    connect(m_valueCard, &PropertiesPanelValueCard::sineAmplitudeChosen, this, &PropertiesPanel::applySineAmplitude);
    connect(m_valueCard, &PropertiesPanelValueCard::triangleBaseChosen, this, &PropertiesPanel::applyTriangleBase);
    connect(m_valueCard, &PropertiesPanelValueCard::triangleAngle1Chosen, this, &PropertiesPanel::applyTriangleAngle1);
    connect(m_valueCard, &PropertiesPanelValueCard::triangleAngle2Chosen, this, &PropertiesPanel::applyTriangleAngle2);
    connect(m_valueCard, &PropertiesPanelValueCard::arrowStartChosen, this, &PropertiesPanel::applyArrowStart);
    connect(m_valueCard, &PropertiesPanelValueCard::arrowEndChosen, this, &PropertiesPanel::applyArrowEnd);
    connect(m_arrangeCard, &PropertiesPanelArrangeCard::layerAdjusted, this, &PropertiesPanel::adjustLayer);
    connect(m_arrangeCard, &PropertiesPanelArrangeCard::heightAdjusted, this, &PropertiesPanel::adjustHeight);

    if (m_scene) {
        connect(m_scene, &QGraphicsScene::selectionChanged, this, &PropertiesPanel::scheduleRefresh);
        connect(m_scene, &CanvasScene::contentChanged, this, &PropertiesPanel::scheduleRefresh);
        // 模式切换会改写一批动作的可用状态，操作栏要跟着重算显隐。
        connect(m_scene, &CanvasScene::modeChanged, this, &PropertiesPanel::scheduleRefresh);
        if (m_scene->undoStack()) {
            // 撤销 / 重做后卡片里的数值也要回到图元的真实状态。
            connect(m_scene->undoStack(), &QUndoStack::indexChanged, this, &PropertiesPanel::scheduleRefresh);
        }
    }

    refresh();
}

void PropertiesPanel::setSelectionActions(const QList<QAction*>& alignActions, const QList<QAction*>& boolActions,
                                          QAction* copyAction, QAction* removeAction, QAction* rotateCwAction,
                                          QAction* rotateCcwAction, QAction* groupAction, QAction* ungroupAction)
{
    if (m_selectionCard) {
        m_selectionCard->setSelectionActions(alignActions, boolActions, copyAction, removeAction, rotateCwAction,
                                             rotateCcwAction, groupAction, ungroupAction);
    }
}

void PropertiesPanel::setItemActions(QAction* smoothAction, QAction* propertiesAction)
{
    if (m_selectionCard) {
        m_selectionCard->setItemActions(smoothAction, propertiesAction);
    }
}

QList<CanvasItem*> PropertiesPanel::selectedItems() const
{
    return m_scene ? m_scene->selectedCanvasItems() : QList<CanvasItem*>();
}

QList<ShapeItem*> PropertiesPanel::selectedShapes() const
{
    QList<ShapeItem*> shapes;
    for (CanvasItem* item : selectedItems()) {
        // 多边形派生自 ShapeItem，但它没有形状类型语义：圆角 / 角度等行只对普通图形开放。
        if (auto* shape = dynamic_cast<ShapeItem*>(item); shape && !shape->isPolygon()) {
            shapes.append(shape);
        }
    }
    return shapes;
}

QList<ShapeItem*> PropertiesPanel::selectedComposites() const
{
    QList<ShapeItem*> composites;
    for (CanvasItem* item : selectedItems()) {
        if (auto* shape = dynamic_cast<ShapeItem*>(item); shape && shape->isPolygon()) {
            composites.append(shape);
        }
    }
    return composites;
}

QList<LineItemAbstract*> PropertiesPanel::selectedStrokes() const
{
    QList<LineItemAbstract*> strokes;
    for (CanvasItem* item : selectedItems()) {
        if (auto* stroke = dynamic_cast<LineItemAbstract*>(item)) {
            strokes.append(stroke);
        }
    }
    return strokes;
}

QList<CanvasItem*> PropertiesPanel::selectedStrokeTargets() const
{
    QList<CanvasItem*> targets;
    for (CanvasItem* item : selectedItems()) {
        if (dynamic_cast<ShapeItem*>(item) || dynamic_cast<LineItemAbstract*>(item)) {
            targets.append(item);
        }
    }
    return targets;
}

QList<CanvasItem*> PropertiesPanel::selectedTexts() const
{
    QList<CanvasItem*> texts;
    for (CanvasItem* item : selectedItems()) {
        if (item->hasTextFacet()) {
            texts.append(item);
        }
    }
    return texts;
}

void PropertiesPanel::scheduleRefresh()
{
    if (m_refreshing) {
        m_refreshPending = true;
        return;
    }
    QTimer::singleShot(0, this, &PropertiesPanel::refresh);
}

int PropertiesPanel::preferredWidth() const
{
    if (!m_contentWidget || !m_contentWidget->layout()) {
        return 260;
    }

    int widestRow = 0;
    // 按卡片里所有行（包括当前隐藏的对齐 / 合并行）计算一次宽度：
    // 卡片宽度在启动时固定，不随选中项增减而跳动，也不会在多选时挤掉按钮。
    auto scanRows = [&widestRow](QLayout* layout) {
        if (!layout) {
            return;
        }
        for (int index = 0; index < layout->count(); ++index) {
            const QWidget* row = layout->itemAt(index)->widget();
            if (row) {
                widestRow = qMax(widestRow, row->sizeHint().width());
            }
        }
    };
    scanRows(m_contentWidget->layout());
    if (m_selectionCard && !m_selectionCard->isHidden()) {
        scanRows(m_selectionCard->layout());
    }

    // 两侧外边距 + 垂直滚动条；宽度贴合内容，上限 320px 避免右侧留白过多。
    const int chrome = 12 + style()->pixelMetric(QStyle::PM_ScrollBarExtent) + 2;
    return qBound(260, widestRow + chrome, 320);
}

void PropertiesPanel::refresh()
{
    if (!m_scene) {
        return;
    }
    m_refreshing = true;
    updateSummary();

    const QList<CanvasItem*> items = selectedItems();
    const QList<ShapeItem*> shapes = selectedShapes();
    const QList<ShapeItem*> composites = selectedComposites();
    const QList<LineItemAbstract*> strokes = selectedStrokes();
    const QList<CanvasItem*> texts = selectedTexts();
    m_selectionCard->setSelectionState(!items.isEmpty(), selectionGroupUseful(items));

    // 交集规则：只显示选中集合里每个图元都拥有的行；空选择时集合为空，全部行隐藏。
    // 值的回显仍取第一个选中图元（卡片内部的既有逻辑）。
    ItemPropertySet common;
    for (int index = 0; index < items.size(); ++index) {
        const ItemPropertySet properties = items.at(index)->properties();
        common = index == 0 ? properties : common.intersected(properties);
    }

    m_valueCard->setCornerVisible(common.contains(ItemProperty::CORNER_RADIUS));
    m_valueCard->setArcRadiusVisible(common.contains(ItemProperty::START_ANGLE));
    m_valueCard->setSineVisible(common.contains(ItemProperty::SINE_CYCLES));
    m_valueCard->setTriangleVisible(common.contains(ItemProperty::TRIANGLE_BASE));
    m_valueCard->setArrowVisible(common.contains(ItemProperty::ARROW_START));
    m_fillImageRow->setVisible(common.contains(ItemProperty::FILL_IMAGE));
    if (m_fillImageButton) {
        const bool hasImage = !items.isEmpty() &&
                              items.first()->propertyValue(ItemProperty::FILL_IMAGE).value<QPixmap>().isNull() == false;
        m_fillImageButton->setText(hasImage ? tr("Change fill image") : tr("Choose fill image"));
    }
    if (m_clearFillImageButton) {
        m_clearFillImageButton->setVisible(common.contains(ItemProperty::FILL_IMAGE));
    }

    const bool hasSelection = !items.isEmpty();
    m_fillBorderCard->fillStyleRow()->setVisible(common.contains(ItemProperty::FILL_STYLE));
    m_colorsCard->fillColorRow()->setVisible(common.contains(ItemProperty::FILL_COLOR));
    m_fillBorderCard->borderStyleRow()->setVisible(common.contains(ItemProperty::STROKE_STYLE));
    m_colorsCard->borderColorRow()->setVisible(common.contains(ItemProperty::STROKE_COLOR));
    m_fillBorderCard->borderWidthRow()->setVisible(common.contains(ItemProperty::STROKE_WIDTH));

    m_contentCard->setVisible(hasSelection);
    m_colorsCard->setVisible(hasSelection);
    m_fillBorderCard->setVisible(hasSelection);
    m_valueCard->setVisible(hasSelection);
    m_arrangeCard->setVisible(hasSelection);
    m_contentCard->refresh(texts);
    m_colorsCard->refresh(shapes, strokes, composites);
    m_fillBorderCard->refresh(shapes, strokes, composites);
    m_valueCard->refresh(shapes, strokes, items);
    m_arrangeCard->refresh(!selectedItems().isEmpty());

    m_refreshing = false;
    if (m_refreshPending) {
        m_refreshPending = false;
        scheduleRefresh();
    }
}

bool PropertiesPanel::selectionGroupUseful(const QList<CanvasItem*>& items) const
{
    if (!m_scene || items.size() < 2) {
        return false;
    }
    int outerGroup = 0;
    for (CanvasItem* item : items) {
        // 有组外图元参与时组合总能多套一层，值得提供。
        if (item->groupId() <= 0) {
            return true;
        }
        const int current = m_scene->outermostGroup(item->groupId());
        if (outerGroup == 0) {
            outerGroup = current;
        } else if (current != outerGroup) {
            // 选中了两个不同的组：组合成外层组有意义。
            return true;
        }
    }
    // 全部落在同一个组里：组合只会把原组整个再包一层，不再显示“组合”。
    return false;
}

void PropertiesPanel::updateSummary()
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

void PropertiesPanel::changeSelectedShapes(const QString& text, const std::function<void(ShapeItem*)>& mutate)
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

void PropertiesPanel::applyPropertyValues(const QMap<ItemProperty, QVariant>& values, const QString& text,
                                          const QList<CanvasItem*>& targets)
{
    if (!m_scene || !m_scene->undoStack()) {
        return;
    }
    const QList<CanvasItem*> items = targets.isEmpty() ? selectedItems() : targets;

    QList<CanvasItem*> applyItems;
    QList<QMap<ItemProperty, QVariant>> before;
    QList<QMap<ItemProperty, QVariant>> after;
    m_refreshing = true;
    for (CanvasItem* item : items) {
        const ItemPropertySet properties = item->properties();
        QMap<ItemProperty, QVariant> beforeValues;
        QMap<ItemProperty, QVariant> afterValues;
        for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
            if (!properties.contains(it.key())) {
                continue;
            }
            beforeValues.insert(it.key(), item->propertyValue(it.key()));
            afterValues.insert(it.key(), it.value());
        }
        if (afterValues.isEmpty()) {
            continue;
        }
        // 先写值，命令随后 push 时的 redo 幂等执行；撤销 / 重做都按快照回放。
        for (auto it = afterValues.constBegin(); it != afterValues.constEnd(); ++it) {
            item->setPropertyValue(it.key(), it.value());
        }
        applyItems.append(item);
        before.append(beforeValues);
        after.append(afterValues);
    }
    m_refreshing = false;

    if (applyItems.isEmpty()) {
        return;
    }
    m_scene->undoStack()->push(new ItemPropertyCommand(m_scene, applyItems, before, after, text));
    refresh();
}

void PropertiesPanel::applyStrokeColor(const QColor& color)
{
    const QList<CanvasItem*> items = selectedStrokeTargets();
    if (!color.isValid() || items.isEmpty()) {
        return;
    }
    emit strokeColorPicked(color);
    applyPropertyValues({{ItemProperty::STROKE_COLOR, color}}, tr("Change border color"), items);
}

void PropertiesPanel::applyFillColor(const QColor& color)
{
    const QList<CanvasItem*> items = selectedItems();
    if (!color.isValid() || items.isEmpty() || !m_scene || !m_scene->undoStack()) {
        return;
    }

    QList<CanvasItem*> targets;
    QList<QMap<ItemProperty, QVariant>> before;
    QList<QMap<ItemProperty, QVariant>> after;
    m_refreshing = true;
    for (CanvasItem* item : items) {
        const ItemPropertySet properties = item->properties();
        if (!properties.contains(ItemProperty::FILL_COLOR)) {
            continue;
        }
        QMap<ItemProperty, QVariant> beforeValues;
        beforeValues.insert(ItemProperty::FILL_COLOR, item->propertyValue(ItemProperty::FILL_COLOR));
        if (properties.contains(ItemProperty::FILL_STYLE)) {
            beforeValues.insert(ItemProperty::FILL_STYLE, item->propertyValue(ItemProperty::FILL_STYLE));
        }
        QMap<ItemProperty, QVariant> afterValues = beforeValues;
        afterValues.insert(ItemProperty::FILL_COLOR, color);
        if (properties.contains(ItemProperty::FILL_STYLE) &&
            item->propertyValue(ItemProperty::FILL_STYLE).toInt() == static_cast<int>(FillStyle::None)) {
            afterValues.insert(ItemProperty::FILL_STYLE, static_cast<int>(FillStyle::Solid));
        }
        targets.append(item);
        before.append(beforeValues);
        after.append(afterValues);
    }
    // 先写值再入栈：命令的 redo 会被 push 重复执行一次，幂等即可，这里保持与旧实现一致。
    for (int index = 0; index < targets.size(); ++index) {
        for (auto it = after.at(index).constBegin(); it != after.at(index).constEnd(); ++it) {
            targets.at(index)->setPropertyValue(it.key(), it.value());
        }
    }
    m_refreshing = false;
    if (targets.isEmpty()) {
        return;
    }
    m_scene->undoStack()->push(new ItemPropertyCommand(m_scene, targets, before, after, tr("Change shape fill color")));
    refresh();
}

void PropertiesPanel::applyFillStyle(FillStyle style)
{
    if (selectedItems().isEmpty() || !m_scene || !m_scene->undoStack()) {
        return;
    }
    applyPropertyValues({{ItemProperty::FILL_STYLE, static_cast<int>(style)}}, tr("Change shape fill style"));
}

void PropertiesPanel::applyBorderStyle(Qt::PenStyle style)
{
    const QList<CanvasItem*> items = selectedStrokeTargets();
    if (items.isEmpty()) {
        return;
    }
    applyPropertyValues({{ItemProperty::STROKE_STYLE, static_cast<int>(style)}}, tr("Change shape border style"),
                        items);
}

void PropertiesPanel::applyBorderWidth(qreal width)
{
    const QList<CanvasItem*> items = selectedStrokeTargets();
    if (items.isEmpty()) {
        return;
    }
    emit borderWidthPicked(width);
    applyPropertyValues({{ItemProperty::STROKE_WIDTH, width}}, tr("Change border width"), items);
}

void PropertiesPanel::applyOpacity(int percent)
{
    if (selectedItems().isEmpty()) {
        return;
    }
    applyPropertyValues({{ItemProperty::OPACITY, percent / 100.0}}, tr("Change opacity"));
}

void PropertiesPanel::changeSelectedTexts(const QString& text, const std::function<void(TextItemState&)>& mutate)
{
    const QList<CanvasItem*> texts = selectedTexts();
    if (texts.isEmpty() || !m_scene || !m_scene->undoStack()) {
        return;
    }

    QList<TextItemState> beforeStates;
    QList<TextItemState> afterStates;
    bool changed = false;
    for (CanvasItem* item : texts) {
        const TextItemState before = item->textState();
        TextItemState after = before;
        mutate(after);
        beforeStates.append(before);
        afterStates.append(after);
        changed = changed || after != before;
    }
    if (!changed) {
        return;
    }

    QUndoStack* undo = m_scene->undoStack();
    undo->beginMacro(text);
    for (int i = 0; i < texts.size(); ++i) {
        undo->push(new TextItemCommand(m_scene, texts.at(i), beforeStates.at(i), afterStates.at(i), text));
    }
    undo->endMacro();
    refresh();
}

void PropertiesPanel::applyTextContent(const QString& text)
{
    changeSelectedTexts(tr("Change text content"), [&text](TextItemState& state) {
        if (state.text == text) {
            return;
        }
        state.text = text;
        state.baseSize = TextItem::measure(state.text, state.font);
        state.size = state.baseSize;
    });
}

void PropertiesPanel::applyTextFont(const QFont& font)
{
    changeSelectedTexts(tr("Change text font"), [&font](TextItemState& state) {
        if (state.font == font) {
            return;
        }
        state.font = font;
        state.baseSize = TextItem::measure(state.text, state.font);
        state.size = state.baseSize;
    });
}

void PropertiesPanel::applyTextColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    changeSelectedTexts(tr("Change text color"), [&color](TextItemState& state) { state.color = color; });
}

void PropertiesPanel::changeSelectedArcs(const QString& text, const std::function<void(LineItemAbstract*)>& mutate)
{
    QList<LineItemAbstract*> arcs;
    for (LineItemAbstract* stroke : selectedStrokes()) {
        if (stroke->supportsArcAngles()) {
            arcs.append(stroke);
        }
    }
    if (arcs.isEmpty() || !m_scene || !m_scene->undoStack()) {
        return;
    }

    QList<CurveParamState> paramsBefore;
    QList<LineItemAbstract::GeometryState> geometryBefore;
    QList<CurveParamState> paramsAfter;
    QList<LineItemAbstract::GeometryState> geometryAfter;
    QList<LineItemAbstract*> targets;
    for (LineItemAbstract* arc : arcs) {
        targets.append(arc);
        paramsBefore.append(curveParamState(arc));
        geometryBefore.append(arc->geometryState());
    }
    m_refreshing = true;
    for (LineItemAbstract* arc : arcs) {
        mutate(arc);
        paramsAfter.append(curveParamState(arc));
        geometryAfter.append(arc->geometryState());
    }
    m_refreshing = false;

    m_scene->undoStack()->push(
        new CurveParamCommand(m_scene, targets, paramsBefore, geometryBefore, paramsAfter, geometryAfter, text));
    refresh();
}

void PropertiesPanel::changeSelectedSines(const QString& text, const std::function<void(LineItemAbstract*)>& mutate)
{
    QList<LineItemAbstract*> sines;
    for (LineItemAbstract* stroke : selectedStrokes()) {
        if (stroke->supportsSineWave()) {
            sines.append(stroke);
        }
    }
    if (sines.isEmpty() || !m_scene || !m_scene->undoStack()) {
        return;
    }

    QList<CurveParamState> paramsBefore;
    QList<LineItemAbstract::GeometryState> geometryBefore;
    QList<CurveParamState> paramsAfter;
    QList<LineItemAbstract::GeometryState> geometryAfter;
    QList<LineItemAbstract*> targets;
    for (LineItemAbstract* sine : sines) {
        targets.append(sine);
        paramsBefore.append(curveParamState(sine));
        geometryBefore.append(sine->geometryState());
    }
    m_refreshing = true;
    for (LineItemAbstract* sine : sines) {
        mutate(sine);
        paramsAfter.append(curveParamState(sine));
        geometryAfter.append(sine->geometryState());
    }
    m_refreshing = false;

    m_scene->undoStack()->push(
        new CurveParamCommand(m_scene, targets, paramsBefore, geometryBefore, paramsAfter, geometryAfter, text));
    refresh();
}

void PropertiesPanel::chooseFillImage()
{
    // 需求 13：选中多个可填充图元时一次替换；图片从资源面板里选（也可以现场从本地加一张）。
    QList<ShapeItem*> shapes;
    for (CanvasItem* item : selectedItems()) {
        if (auto* shape = dynamic_cast<ShapeItem*>(item);
            shape && !shape->isCustomShape() && ShapeMeta::isFilled(shape->shapeType())) {
            shapes.append(shape);
        }
    }
    if (shapes.isEmpty()) {
        return;
    }
    const QPixmap pixmap = ImagePickerDialog::pick(this, collectCanvasImages(m_scene), tr("Fill image"));
    if (pixmap.isNull() || !m_scene || !m_scene->undoStack()) {
        return;
    }
    QList<ShapeItemState> before;
    QList<ShapeItemState> after;
    for (ShapeItem* shape : shapes) {
        const ShapeItemState state = shape->state();
        before.append(state);
        ShapeItemState next = state;
        next.hasFillImage = true;
        next.fillImage = pixmap;
        next.fillImageMode = ImageFill::Mode::Stretch;
        after.append(next);
    }
    m_scene->undoStack()->push(new ShapeStyleCommand(m_scene, shapes, before, after, tr("Replace fill image")));
    refresh();
}

void PropertiesPanel::clearFillImage()
{
    if (selectedItems().isEmpty()) {
        return;
    }
    applyPropertyValues({{ItemProperty::FILL_IMAGE, QVariant()}}, tr("Clear fill image"));
}

void PropertiesPanel::applyCornerRadius(int radius)
{
    changeSelectedShapes(tr("Change rectangle corner radius"),
                         [radius](ShapeItem* shape) { shape->setCornerRadius(radius); });
}

void PropertiesPanel::applyStartAngle(int angle)
{
    changeSelectedShapes(tr("Change start angle"),
                         [angle](ShapeItem* shape) { shape->setArcAngles(angle, shape->sweepAngle()); });
    changeSelectedArcs(tr("Change start angle"), [angle](LineItemAbstract* arc) {
        LineItemAbstract::ParamState params = arc->paramState();
        params.first = angle;
        arc->applyParamState(params);
    });
}

void PropertiesPanel::applySweepAngle(int angle)
{
    changeSelectedShapes(tr("Change sector angle"),
                         [angle](ShapeItem* shape) { shape->setArcAngles(shape->startAngle(), angle); });
    changeSelectedArcs(tr("Change arc sweep angle"), [angle](LineItemAbstract* arc) {
        LineItemAbstract::ParamState params = arc->paramState();
        params.second = angle;
        arc->applyParamState(params);
    });
}

void PropertiesPanel::applySineCycles(int cycles)
{
    const int bounded = qBound(1, cycles, 12);
    changeSelectedSines(tr("Change sine cycles"), [bounded](LineItemAbstract* sine) {
        LineItemAbstract::ParamState params = sine->paramState();
        params.first = bounded;
        sine->applyParamState(params);
    });
}

void PropertiesPanel::applySineAmplitude(int pixels)
{
    const qreal amplitude = qBound<qreal>(1.0, pixels, 200.0);
    changeSelectedSines(tr("Change sine amplitude"), [amplitude](LineItemAbstract* sine) {
        LineItemAbstract::ParamState params = sine->paramState();
        params.second = amplitude;
        sine->applyParamState(params);
    });
}

void PropertiesPanel::applyTriangleBase(int base)
{
    // 底边即 A 边；改底边后高度由两个底角派生（contentResized 内完成）。
    changeSelectedShapes(tr("Change triangle base"), [base](ShapeItem* shape) {
        if (!shape || shape->shapeType() != ShapeType::Triangle || shape->isCustomShape()) {
            return;
        }
        const QSizeF current = shape->contentSize();
        shape->setContentSize(QSizeF(qMax<qreal>(8.0, base), current.height()));
    });
}

void PropertiesPanel::applyTriangleAngle1(int degrees)
{
    changeSelectedShapes(tr("Change triangle base angle"), [degrees](ShapeItem* shape) {
        if (!shape || shape->shapeType() != ShapeType::Triangle || shape->isCustomShape()) {
            return;
        }
        shape->setTriangleAngles(degrees, shape->bottomAngle2());
    });
}

void PropertiesPanel::applyTriangleAngle2(int degrees)
{
    changeSelectedShapes(tr("Change triangle base angle"), [degrees](ShapeItem* shape) {
        if (!shape || shape->shapeType() != ShapeType::Triangle || shape->isCustomShape()) {
            return;
        }
        shape->setTriangleAngles(shape->bottomAngle1(), degrees);
    });
}

void PropertiesPanel::applyArrowStart(int style)
{
    // 需求 9b/9d：直线与曲线共用一条箭头撤销记录，只改箭头样式。
    const QList<LineItemAbstract*> curves = selectedStrokes();
    QList<CanvasItem*> targets;
    QList<QPair<ArrowStyle, ArrowStyle>> before;
    QList<QPair<ArrowStyle, ArrowStyle>> after;
    for (LineItemAbstract* curve : curves) {
        if (!curve) {
            continue;
        }
        targets.append(curve);
        before.append(qMakePair(curve->startArrow(), curve->endArrow()));
        after.append(qMakePair(static_cast<ArrowStyle>(style), curve->endArrow()));
    }
    if (targets.isEmpty() || !m_scene || !m_scene->undoStack()) {
        return;
    }
    m_scene->undoStack()->push(new ArrowStyleCommand(m_scene, targets, before, after, tr("Change line start arrow")));
    refresh();
}

void PropertiesPanel::applyArrowEnd(int style)
{
    const QList<LineItemAbstract*> curves = selectedStrokes();
    QList<CanvasItem*> targets;
    QList<QPair<ArrowStyle, ArrowStyle>> before;
    QList<QPair<ArrowStyle, ArrowStyle>> after;
    for (LineItemAbstract* curve : curves) {
        if (!curve) {
            continue;
        }
        targets.append(curve);
        before.append(qMakePair(curve->startArrow(), curve->endArrow()));
        after.append(qMakePair(curve->startArrow(), static_cast<ArrowStyle>(style)));
    }
    if (targets.isEmpty() || !m_scene || !m_scene->undoStack()) {
        return;
    }
    m_scene->undoStack()->push(new ArrowStyleCommand(m_scene, targets, before, after, tr("Change line end arrow")));
    refresh();
}

void PropertiesPanel::adjustLayer(int delta)
{
    if (m_scene) {
        const int count = m_scene->moveSelectedItemsToAdjacentLayer(delta);
        statusMessage(count > 0 ? tr("Moved %1 item(s) to another layer.").arg(count)
                                : tr("No layer to move to in that direction."));
        refresh();
    }
}

void PropertiesPanel::adjustHeight(int delta)
{
    if (m_scene) {
        const int count = m_scene->adjustSelectedHeights(delta);
        statusMessage(count > 0 ? tr("Adjusted the height of %1 item(s).").arg(count)
                                : tr("The height did not change."));
        refresh();
    }
}
