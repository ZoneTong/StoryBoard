#include "ItemDockPanel.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "ArcItem.h"
#include "CompositeShapeItem.h"
#include "CurveItem.h"
#include "CurveParamCommand.h"
#include "ImagePickerDialog.h"
#include "ItemDockPanelArrangeCard.h"
#include "ItemDockPanelColorsCard.h"
#include "ItemDockPanelContentCard.h"
#include "ItemDockPanelFillBorderCard.h"
#include "ItemDockPanelSelectionCard.h"
#include "ItemDockPanelValueCard.h"
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
    m_contentWidget = content;
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // 紧凑布局：不再做带文字的折叠分组标题，各行内容直接平铺，
    // 行首用小图标标注用途（描边 / 填充 / 边框 / 透明度 / 圆角 / 图层 / 顺序）。
    // 选中的操作行（复制/删除/旋转、对齐、合并图元、分组）统一放在卡片底部；
    // 后加的文字 / 图片内容行与样式行排在「图层与高度」行下方。
    m_selectionCard = new ItemDockPanelSelectionCard(content);
    m_selectionCard->setObjectName(QStringLiteral("ItemSelectionRowsCard"));

    m_colorsCard = new ItemDockPanelColorsCard(content);
    m_fillBorderCard = new ItemDockPanelFillBorderCard(content);
    m_contentCard = new ItemDockPanelContentCard(content);

    // 需求 9c 行序定稿（与属性窗同序）：
    // 图层/高度 → 文字内容/颜色/字体、图片 → 填充样式 → 填充色 →
    // 描边颜色 → 描边样式 → 描边宽度 → 透明度/圆角/角度/三角形/箭头 → 选择操作行。
    m_arrangeCard = new ItemDockPanelArrangeCard(content);
    m_arrangeCard->setObjectName(QStringLiteral("ItemArrangeCard"));
    layout->addWidget(m_arrangeCard);

    layout->addWidget(m_contentCard->textContentRow());
    layout->addWidget(m_contentCard->textColorRow());
    layout->addWidget(m_contentCard->textFontRow());

    // 需求 1 行序（与菜单、工具栏、属性窗统一）：
    // 填充样式 → 填充颜色 → （需求 13 引入的背景图片） → 描线样式 → 描线颜色 → 描线宽度。
    layout->addWidget(m_fillBorderCard->fillStyleRow());
    layout->addWidget(m_colorsCard->fillColorRow());

    // 需求 13：背景图片行排在填充颜色之后、描线样式之前；多选时一次套到所有可填充图元。
    m_fillImageRow = new QWidget(content);
    m_fillImageRow->setObjectName(QStringLiteral("ItemFillImageRow"));
    auto* fillImageLayout = new QHBoxLayout(m_fillImageRow);
    fillImageLayout->setContentsMargins(0, 0, 0, 0);
    fillImageLayout->setSpacing(6);
    auto* fillImageIcon = new QLabel(m_fillImageRow);
    fillImageIcon->setPixmap(makeIcon(IconId::Image, QColor(58, 64, 80), 16).pixmap(16, 16));
    fillImageLayout->addWidget(fillImageIcon);
    auto* fillImageButton = new QToolButton(m_fillImageRow);
    fillImageButton->setObjectName(QStringLiteral("FillImageButton"));
    fillImageButton->setText(tr("Background image"));
    fillImageButton->setToolTip(tr("Replace the background image of the selected fillable items"));
    fillImageButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(fillImageButton, &QToolButton::clicked, this, &ItemDockPanel::chooseFillImage);
    fillImageLayout->addWidget(fillImageButton, 1);
    layout->addWidget(m_fillImageRow);

    layout->addWidget(m_fillBorderCard->borderStyleRow());
    layout->addWidget(m_colorsCard->borderColorRow());
    layout->addWidget(m_fillBorderCard->borderWidthRow());

    m_valueCard = new ItemDockPanelValueCard(content);
    layout->addWidget(m_valueCard);

    layout->addWidget(m_selectionCard);
    layout->addStretch(1);

    scroll->setWidget(content);
    outer->addWidget(scroll, 1);

    connect(m_colorsCard, &ItemDockPanelColorsCard::strokeColorChosen, this, &ItemDockPanel::applyStrokeColor);
    connect(m_colorsCard, &ItemDockPanelColorsCard::fillColorChosen, this, &ItemDockPanel::applyFillColor);
    connect(m_fillBorderCard, &ItemDockPanelFillBorderCard::fillStyleChosen, this, &ItemDockPanel::applyFillStyle);
    connect(m_fillBorderCard, &ItemDockPanelFillBorderCard::borderStyleChosen, this, &ItemDockPanel::applyBorderStyle);
    connect(m_fillBorderCard, &ItemDockPanelFillBorderCard::borderWidthChosen, this, &ItemDockPanel::applyBorderWidth);
    connect(m_contentCard, &ItemDockPanelContentCard::textContentChosen, this, &ItemDockPanel::applyTextContent);
    connect(m_contentCard, &ItemDockPanelContentCard::textFontChosen, this, &ItemDockPanel::applyTextFont);
    connect(m_contentCard, &ItemDockPanelContentCard::textColorChosen, this, &ItemDockPanel::applyTextColor);
    connect(m_valueCard, &ItemDockPanelValueCard::opacityChosen, this, &ItemDockPanel::applyOpacity);
    connect(m_valueCard, &ItemDockPanelValueCard::cornerRadiusChosen, this, &ItemDockPanel::applyCornerRadius);
    connect(m_valueCard, &ItemDockPanelValueCard::startAngleChosen, this, &ItemDockPanel::applyStartAngle);
    connect(m_valueCard, &ItemDockPanelValueCard::sweepAngleChosen, this, &ItemDockPanel::applySweepAngle);
    connect(m_valueCard, &ItemDockPanelValueCard::sineCyclesChosen, this, &ItemDockPanel::applySineCycles);
    connect(m_valueCard, &ItemDockPanelValueCard::sineAmplitudeChosen, this, &ItemDockPanel::applySineAmplitude);
    connect(m_valueCard, &ItemDockPanelValueCard::triangleBaseChosen, this, &ItemDockPanel::applyTriangleBase);
    connect(m_valueCard, &ItemDockPanelValueCard::triangleAngle1Chosen, this, &ItemDockPanel::applyTriangleAngle1);
    connect(m_valueCard, &ItemDockPanelValueCard::triangleAngle2Chosen, this, &ItemDockPanel::applyTriangleAngle2);
    connect(m_valueCard, &ItemDockPanelValueCard::arrowStartChosen, this, &ItemDockPanel::applyArrowStart);
    connect(m_valueCard, &ItemDockPanelValueCard::arrowEndChosen, this, &ItemDockPanel::applyArrowEnd);
    connect(m_arrangeCard, &ItemDockPanelArrangeCard::layerAdjusted, this, &ItemDockPanel::adjustLayer);
    connect(m_arrangeCard, &ItemDockPanelArrangeCard::heightAdjusted, this, &ItemDockPanel::adjustHeight);

    if (m_scene) {
        connect(m_scene, &QGraphicsScene::selectionChanged, this, &ItemDockPanel::scheduleRefresh);
        connect(m_scene, &CanvasScene::contentChanged, this, &ItemDockPanel::scheduleRefresh);
        // 模式切换会改写一批动作的可用状态，操作栏要跟着重算显隐。
        connect(m_scene, &CanvasScene::modeChanged, this, &ItemDockPanel::scheduleRefresh);
        if (m_scene->undoStack()) {
            // 撤销 / 重做后卡片里的数值也要回到图元的真实状态。
            connect(m_scene->undoStack(), &QUndoStack::indexChanged, this, &ItemDockPanel::scheduleRefresh);
        }
    }

    refresh();
}

void ItemDockPanel::setSelectionActions(const QList<QAction*>& alignActions, const QList<QAction*>& boolActions,
                                        QAction* copyAction, QAction* removeAction, QAction* rotateCwAction,
                                        QAction* rotateCcwAction, QAction* groupAction, QAction* ungroupAction)
{
    if (m_selectionCard) {
        m_selectionCard->setSelectionActions(alignActions, boolActions, copyAction, removeAction, rotateCwAction,
                                             rotateCcwAction, groupAction, ungroupAction);
    }
}

void ItemDockPanel::setItemActions(QAction* smoothAction, QAction* propertiesAction)
{
    if (m_selectionCard) {
        m_selectionCard->setItemActions(smoothAction, propertiesAction);
    }
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

QList<CompositeShapeItem*> ItemDockPanel::selectedComposites() const
{
    QList<CompositeShapeItem*> composites;
    for (CanvasItem* item : selectedItems()) {
        if (auto* composite = dynamic_cast<CompositeShapeItem*>(item)) {
            composites.append(composite);
        }
    }
    return composites;
}

QList<CurveItem*> ItemDockPanel::selectedStrokes() const
{
    QList<CurveItem*> strokes;
    for (CanvasItem* item : selectedItems()) {
        if (auto* stroke = dynamic_cast<CurveItem*>(item)) {
            strokes.append(stroke);
        }
    }
    return strokes;
}

QList<CanvasItem*> ItemDockPanel::selectedStrokeTargets() const
{
    QList<CanvasItem*> targets;
    for (CanvasItem* item : selectedItems()) {
        if (dynamic_cast<ShapeItem*>(item) || dynamic_cast<CurveItem*>(item) ||
            dynamic_cast<CompositeShapeItem*>(item)) {
            targets.append(item);
        }
    }
    return targets;
}

QList<TextItem*> ItemDockPanel::selectedTexts() const
{
    QList<TextItem*> texts;
    for (CanvasItem* item : selectedItems()) {
        if (auto* text = dynamic_cast<TextItem*>(item)) {
            texts.append(text);
        }
    }
    return texts;
}

void ItemDockPanel::scheduleRefresh()
{
    if (m_refreshing) {
        m_refreshPending = true;
        return;
    }
    QTimer::singleShot(0, this, &ItemDockPanel::refresh);
}

int ItemDockPanel::preferredWidth() const
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

void ItemDockPanel::refresh()
{
    if (!m_scene) {
        return;
    }
    m_refreshing = true;
    updateSummary();

    const QList<CanvasItem*> items = selectedItems();
    const QList<ShapeItem*> shapes = selectedShapes();
    const QList<CompositeShapeItem*> composites = selectedComposites();
    const QList<CurveItem*> strokes = selectedStrokes();
    const QList<TextItem*> texts = selectedTexts();
    m_selectionCard->setSelectionState(!items.isEmpty(), selectionGroupUseful(items));

    bool hasCorner = false;
    bool hasArc = false;
    bool hasSine = false;
    bool hasTriangle = false;
    bool hasArrow = false;
    bool hasFillable = false;
    for (ShapeItem* shape : shapes) {
        if (shape->shapeType() == ShapeType::Rectangle) {
            hasCorner = true;
        }
        if (ShapeMeta::isFilled(shape->shapeType()) && !shape->isCustomShape()) {
            hasFillable = true;
        }
        if (shape->shapeType() == ShapeType::Sector || shape->shapeType() == ShapeType::Arc) {
            hasArc = true;
        }
        if (shape->shapeType() == ShapeType::Triangle && !shape->isCustomShape()) {
            hasTriangle = true;
        }
        if (ShapeMeta::isLineLike(shape->shapeType()) && !shape->isCustomShape()) {
            hasArrow = true;
        }
    }
    // 需求 4：曲线与花括号同样提供起点 / 终点箭头；弧线与正弦另有角度 / 波形参数行。
    for (CurveItem* stroke : strokes) {
        if (stroke) {
            hasArrow = true;
            if (dynamic_cast<ArcItem*>(stroke)) {
                hasArc = true;
            }
            if (dynamic_cast<SineItem*>(stroke)) {
                hasSine = true;
            }
        }
    }
    m_valueCard->setCornerVisible(hasCorner);
    m_valueCard->setArcRadiusVisible(hasArc);
    m_valueCard->setSineVisible(hasSine);
    m_valueCard->setTriangleVisible(hasTriangle);
    m_valueCard->setArrowVisible(hasArrow);
    // 需求 13：背景图片入口只对可填充图元显示；多选时依旧可用（批量替换）。
    m_fillImageRow->setVisible(hasFillable);

    // 没有选中图元时隐藏全部图元编辑项，停靠卡片本身保持常驻。
    const bool hasSelection = !items.isEmpty();
    // 多边形自身可填充；直线 / 弧线 / 大括号 / 文字 / 图片仍不提供填充。
    bool anyFillable = !composites.isEmpty();
    for (ShapeItem* shape : shapes) {
        if (shapeTypeIsFilled(shape->shapeType())) {
            anyFillable = true;
            break;
        }
    }
    const bool showFillRows = hasSelection && anyFillable;
    m_fillBorderCard->fillStyleRow()->setVisible(showFillRows);
    m_colorsCard->fillColorRow()->setVisible(showFillRows);

    // 描线样式对图形、曲线与多边形都有效；描线颜色 / 宽度同样如此。
    const bool showBorderStyleRow = hasSelection && (!shapes.isEmpty() || !strokes.isEmpty() || !composites.isEmpty());
    const bool showStrokeRows = hasSelection && (!shapes.isEmpty() || !strokes.isEmpty() || !composites.isEmpty());
    m_fillBorderCard->borderStyleRow()->setVisible(showBorderStyleRow);
    m_colorsCard->borderColorRow()->setVisible(showStrokeRows);
    m_fillBorderCard->borderWidthRow()->setVisible(showStrokeRows);

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

bool ItemDockPanel::selectionGroupUseful(const QList<CanvasItem*>& items) const
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
    const QList<CanvasItem*> items = selectedStrokeTargets();
    if (!color.isValid() || items.isEmpty() || !m_scene || !m_scene->undoStack()) {
        return;
    }
    emit strokeColorPicked(color);

    QList<QColor> beforeColors;
    QList<qreal> beforeWidths;
    QList<QColor> afterColors;
    QList<qreal> afterWidths;
    m_refreshing = true;
    for (CanvasItem* item : items) {
        if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
            beforeColors.append(shape->strokeColor());
            beforeWidths.append(shape->strokeWidth());
            shape->setStyle(color, shape->fillColor(), shape->strokeWidth());
            afterColors.append(shape->strokeColor());
            afterWidths.append(shape->strokeWidth());
        } else if (auto* stroke = dynamic_cast<CurveItem*>(item)) {
            beforeColors.append(stroke->strokeColor());
            beforeWidths.append(stroke->strokeWidth());
            stroke->setStyle(color, stroke->strokeWidth());
            afterColors.append(stroke->strokeColor());
            afterWidths.append(stroke->strokeWidth());
        } else if (auto* composite = dynamic_cast<CompositeShapeItem*>(item)) {
            beforeColors.append(composite->strokeColor());
            beforeWidths.append(composite->strokeWidth());
            composite->setStroke(color, composite->strokeWidth());
            afterColors.append(composite->strokeColor());
            afterWidths.append(composite->strokeWidth());
        }
    }
    m_refreshing = false;

    m_scene->undoStack()->push(new ItemStrokeStyleCommand(m_scene, items, beforeColors, beforeWidths, afterColors,
                                                          afterWidths, tr("Change border color")));
    refresh();
}

void ItemDockPanel::applyFillColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    changeSelectedFills(
        tr("Change shape fill color"),
        [color](ShapeItem* shape) {
            shape->setFill(color, shape->fillStyle() == FillStyle::None ? FillStyle::Solid : shape->fillStyle());
        },
        [color](CompositeShapeItem* composite) {
            const FillStyle style =
                composite->fillStyle() == FillStyle::None ? FillStyle::Solid : composite->fillStyle();
            composite->setFill(color, style);
        });
}

void ItemDockPanel::applyFillStyle(FillStyle style)
{
    changeSelectedFills(
        tr("Change shape fill style"), [style](ShapeItem* shape) { shape->setFillStyle(style); },
        [style](CompositeShapeItem* composite) { composite->setFillStyle(style); });
}

void ItemDockPanel::applyBorderStyle(Qt::PenStyle style)
{
    const QList<CanvasItem*> items = selectedStrokeTargets();
    if (items.isEmpty() || !m_scene || !m_scene->undoStack()) {
        return;
    }

    QList<Qt::PenStyle> beforeStyles;
    QList<Qt::PenStyle> afterStyles;
    for (CanvasItem* item : items) {
        if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
            beforeStyles.append(shape->strokeStyle());
            afterStyles.append(style);
        } else if (auto* curve = dynamic_cast<CurveItem*>(item)) {
            beforeStyles.append(curve->strokeStyle());
            afterStyles.append(style);
        } else if (auto* composite = dynamic_cast<CompositeShapeItem*>(item)) {
            beforeStyles.append(composite->strokeStyle());
            afterStyles.append(style);
        }
    }
    m_scene->undoStack()->push(
        new ItemPenStyleCommand(m_scene, items, beforeStyles, afterStyles, tr("Change shape border style")));
    refresh();
}

void ItemDockPanel::applyBorderWidth(qreal width)
{
    const QList<CanvasItem*> items = selectedStrokeTargets();
    if (items.isEmpty() || !m_scene || !m_scene->undoStack()) {
        return;
    }
    emit borderWidthPicked(width);

    QList<QColor> beforeColors;
    QList<qreal> beforeWidths;
    QList<QColor> afterColors;
    QList<qreal> afterWidths;
    m_refreshing = true;
    for (CanvasItem* item : items) {
        if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
            beforeColors.append(shape->strokeColor());
            beforeWidths.append(shape->strokeWidth());
            shape->setStyle(shape->strokeColor(), shape->fillColor(), width);
            afterColors.append(shape->strokeColor());
            afterWidths.append(shape->strokeWidth());
        } else if (auto* stroke = dynamic_cast<CurveItem*>(item)) {
            beforeColors.append(stroke->strokeColor());
            beforeWidths.append(stroke->strokeWidth());
            stroke->setStyle(stroke->strokeColor(), width);
            afterColors.append(stroke->strokeColor());
            afterWidths.append(stroke->strokeWidth());
        } else if (auto* composite = dynamic_cast<CompositeShapeItem*>(item)) {
            beforeColors.append(composite->strokeColor());
            beforeWidths.append(composite->strokeWidth());
            composite->setStroke(composite->strokeColor(), width);
            afterColors.append(composite->strokeColor());
            afterWidths.append(composite->strokeWidth());
        }
    }
    m_refreshing = false;

    m_scene->undoStack()->push(new ItemStrokeStyleCommand(m_scene, items, beforeColors, beforeWidths, afterColors,
                                                          afterWidths, tr("Change border width")));
    refresh();
}

void ItemDockPanel::applyOpacity(int percent)
{
    const QList<CanvasItem*> items = selectedItems();
    if (items.isEmpty() || !m_scene || !m_scene->undoStack()) {
        return;
    }

    QList<qreal> before;
    QList<qreal> after;
    m_refreshing = true;
    for (CanvasItem* item : items) {
        before.append(item->itemOpacity());
        item->setItemOpacity(percent / 100.0);
        after.append(item->itemOpacity());
    }
    m_refreshing = false;

    m_scene->undoStack()->push(new ItemOpacityCommand(m_scene, items, before, after, tr("Change opacity")));
    refresh();
}

void ItemDockPanel::changeSelectedFills(const QString& text, const std::function<void(ShapeItem*)>& mutateShape,
                                        const std::function<void(CompositeShapeItem*)>& mutateComposite)
{
    const QList<ShapeItem*> shapes = selectedShapes();
    const QList<CompositeShapeItem*> composites = selectedComposites();
    if ((shapes.isEmpty() && composites.isEmpty()) || !m_scene || !m_scene->undoStack()) {
        return;
    }

    QList<ShapeItemState> beforeShapes;
    QList<ShapeItemState> afterShapes;
    QList<QColor> beforeCompositeColors;
    QList<FillStyle> beforeCompositeStyles;
    QList<QColor> afterCompositeColors;
    QList<FillStyle> afterCompositeStyles;
    for (ShapeItem* shape : shapes) {
        beforeShapes.append(shape->state());
    }
    for (CompositeShapeItem* composite : composites) {
        beforeCompositeColors.append(composite->fillColor());
        beforeCompositeStyles.append(composite->fillStyle());
    }

    m_refreshing = true;
    for (ShapeItem* shape : shapes) {
        mutateShape(shape);
        afterShapes.append(shape->state());
    }
    for (CompositeShapeItem* composite : composites) {
        mutateComposite(composite);
        afterCompositeColors.append(composite->fillColor());
        afterCompositeStyles.append(composite->fillStyle());
    }
    m_refreshing = false;

    if (beforeShapes == afterShapes && beforeCompositeColors == afterCompositeColors &&
        beforeCompositeStyles == afterCompositeStyles) {
        return;
    }

    QUndoStack* undo = m_scene->undoStack();
    const bool mixed = !shapes.isEmpty() && !composites.isEmpty();
    if (mixed) {
        undo->beginMacro(text);
    }
    if (!shapes.isEmpty()) {
        undo->push(new ShapeStyleCommand(m_scene, shapes, beforeShapes, afterShapes, text));
    }
    if (!composites.isEmpty()) {
        QList<CanvasItem*> compositeItems;
        for (CompositeShapeItem* composite : composites) {
            compositeItems.append(composite);
        }
        undo->push(new CompositeFillStyleCommand(m_scene, compositeItems, beforeCompositeColors, beforeCompositeStyles,
                                                 afterCompositeColors, afterCompositeStyles, text));
    }
    if (mixed) {
        undo->endMacro();
    }
    refresh();
}

void ItemDockPanel::changeSelectedTexts(const QString& text, const std::function<void(TextItemState&)>& mutate)
{
    const QList<TextItem*> texts = selectedTexts();
    if (texts.isEmpty() || !m_scene || !m_scene->undoStack()) {
        return;
    }

    QList<TextItemState> beforeStates;
    QList<TextItemState> afterStates;
    bool changed = false;
    for (TextItem* item : texts) {
        const TextItemState before = item->state();
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

void ItemDockPanel::applyTextContent(const QString& text)
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

void ItemDockPanel::applyTextFont(const QFont& font)
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

void ItemDockPanel::applyTextColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    changeSelectedTexts(tr("Change text color"), [&color](TextItemState& state) { state.color = color; });
}

void ItemDockPanel::changeSelectedArcs(const QString& text, const std::function<void(ArcItem*)>& mutate)
{
    QList<ArcItem*> arcs;
    for (CurveItem* stroke : selectedStrokes()) {
        if (auto* arc = dynamic_cast<ArcItem*>(stroke)) {
            arcs.append(arc);
        }
    }
    if (arcs.isEmpty() || !m_scene || !m_scene->undoStack()) {
        return;
    }

    QList<CurveParamState> paramsBefore;
    QList<CurveItem::GeometryState> geometryBefore;
    QList<CurveParamState> paramsAfter;
    QList<CurveItem::GeometryState> geometryAfter;
    QList<CurveItem*> targets;
    for (ArcItem* arc : arcs) {
        targets.append(arc);
        paramsBefore.append(curveParamState(arc));
        geometryBefore.append(arc->geometryState());
    }
    m_refreshing = true;
    for (ArcItem* arc : arcs) {
        mutate(arc);
        paramsAfter.append(curveParamState(arc));
        geometryAfter.append(arc->geometryState());
    }
    m_refreshing = false;

    m_scene->undoStack()->push(new CurveParamCommand(m_scene, targets, paramsBefore, geometryBefore, paramsAfter,
                                                    geometryAfter, text));
    refresh();
}

void ItemDockPanel::changeSelectedSines(const QString& text, const std::function<void(SineItem*)>& mutate)
{
    QList<SineItem*> sines;
    for (CurveItem* stroke : selectedStrokes()) {
        if (auto* sine = dynamic_cast<SineItem*>(stroke)) {
            sines.append(sine);
        }
    }
    if (sines.isEmpty() || !m_scene || !m_scene->undoStack()) {
        return;
    }

    QList<CurveParamState> paramsBefore;
    QList<CurveItem::GeometryState> geometryBefore;
    QList<CurveParamState> paramsAfter;
    QList<CurveItem::GeometryState> geometryAfter;
    QList<CurveItem*> targets;
    for (SineItem* sine : sines) {
        targets.append(sine);
        paramsBefore.append(curveParamState(sine));
        geometryBefore.append(sine->geometryState());
    }
    m_refreshing = true;
    for (SineItem* sine : sines) {
        mutate(sine);
        paramsAfter.append(curveParamState(sine));
        geometryAfter.append(sine->geometryState());
    }
    m_refreshing = false;

    m_scene->undoStack()->push(new CurveParamCommand(m_scene, targets, paramsBefore, geometryBefore, paramsAfter,
                                                    geometryAfter, text));
    refresh();
}

void ItemDockPanel::chooseFillImage()
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
    const QPixmap pixmap = ImagePickerDialog::pick(this, collectCanvasImages(m_scene), tr("Background image"));
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
    m_scene->undoStack()->push(new ShapeStyleCommand(m_scene, shapes, before, after, tr("Replace background image")));
    refresh();
}

void ItemDockPanel::applyCornerRadius(int radius)
{
    changeSelectedShapes(tr("Change rectangle corner radius"),
                         [radius](ShapeItem* shape) { shape->setCornerRadius(radius); });
}

void ItemDockPanel::applyStartAngle(int angle)
{
    changeSelectedShapes(tr("Change start angle"),
                         [angle](ShapeItem* shape) { shape->setArcAngles(angle, shape->sweepAngle()); });
    changeSelectedArcs(tr("Change start angle"),
                       [angle](ArcItem* arc) { arc->setAngles(angle, arc->sweepAngle()); });
}

void ItemDockPanel::applySweepAngle(int angle)
{
    changeSelectedShapes(tr("Change sector angle"),
                         [angle](ShapeItem* shape) { shape->setArcAngles(shape->startAngle(), angle); });
    changeSelectedArcs(tr("Change arc sweep angle"),
                       [angle](ArcItem* arc) { arc->setAngles(arc->startAngle(), angle); });
}

void ItemDockPanel::applySineCycles(int cycles)
{
    const int bounded = qBound(1, cycles, 12);
    changeSelectedSines(tr("Change sine cycles"), [bounded](SineItem* sine) {
        SineItem::Params params = sine->params();
        params.cycles = bounded;
        sine->setParams(params);
    });
}

void ItemDockPanel::applySineAmplitude(int percent)
{
    const qreal amplitude = qBound(0.05, percent / 100.0, 0.5);
    changeSelectedSines(tr("Change sine amplitude"), [amplitude](SineItem* sine) {
        SineItem::Params params = sine->params();
        params.amplitude = amplitude;
        sine->setParams(params);
    });
}

void ItemDockPanel::applyTriangleBase(int base)
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

void ItemDockPanel::applyTriangleAngle1(int degrees)
{
    changeSelectedShapes(tr("Change triangle base angle"), [degrees](ShapeItem* shape) {
        if (!shape || shape->shapeType() != ShapeType::Triangle || shape->isCustomShape()) {
            return;
        }
        shape->setTriangleAngles(degrees, shape->bottomAngle2());
    });
}

void ItemDockPanel::applyTriangleAngle2(int degrees)
{
    changeSelectedShapes(tr("Change triangle base angle"), [degrees](ShapeItem* shape) {
        if (!shape || shape->shapeType() != ShapeType::Triangle || shape->isCustomShape()) {
            return;
        }
        shape->setTriangleAngles(shape->bottomAngle1(), degrees);
    });
}

void ItemDockPanel::applyArrowStart(int style)
{
    // 需求 9b/9d：直线与曲线共用一条箭头撤销记录，只改箭头样式。
    const QList<ShapeItem*> shapes = selectedShapes();
    const QList<CurveItem*> curves = selectedStrokes();
    QList<CanvasItem*> targets;
    QList<QPair<ArrowStyle, ArrowStyle>> before;
    QList<QPair<ArrowStyle, ArrowStyle>> after;
    for (ShapeItem* shape : shapes) {
        if (!shape || !ShapeMeta::isLineLike(shape->shapeType()) || shape->isCustomShape()) {
            continue;
        }
        targets.append(shape);
        before.append(qMakePair(shape->startArrow(), shape->endArrow()));
        after.append(qMakePair(static_cast<ArrowStyle>(style), shape->endArrow()));
    }
    for (CurveItem* curve : curves) {
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

void ItemDockPanel::applyArrowEnd(int style)
{
    const QList<ShapeItem*> shapes = selectedShapes();
    const QList<CurveItem*> curves = selectedStrokes();
    QList<CanvasItem*> targets;
    QList<QPair<ArrowStyle, ArrowStyle>> before;
    QList<QPair<ArrowStyle, ArrowStyle>> after;
    for (ShapeItem* shape : shapes) {
        if (!shape || !ShapeMeta::isLineLike(shape->shapeType()) || shape->isCustomShape()) {
            continue;
        }
        targets.append(shape);
        before.append(qMakePair(shape->startArrow(), shape->endArrow()));
        after.append(qMakePair(shape->startArrow(), static_cast<ArrowStyle>(style)));
    }
    for (CurveItem* curve : curves) {
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
