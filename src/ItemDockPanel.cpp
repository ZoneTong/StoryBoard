#include "ItemDockPanel.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "ItemDockPanelArrangeCard.h"
#include "ItemDockPanelColorsCard.h"
#include "ItemDockPanelFillBorderCard.h"
#include "ItemDockPanelSelectionCard.h"
#include "ItemDockPanelValueCard.h"
#include "ShapeItem.h"

#include <QLabel>
#include <QScrollArea>
#include <QTimer>
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
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // 紧凑布局：不再做带文字的折叠分组标题，各行内容直接平铺，
    // 行首用小图标标注用途（描边 / 填充 / 边框 / 透明度 / 圆角 / 图层 / 顺序）。
    m_selectionCard = new ItemDockPanelSelectionCard(content);
    layout->addWidget(m_selectionCard);

    m_colorsCard = new ItemDockPanelColorsCard(content);
    m_fillBorderCard = new ItemDockPanelFillBorderCard(content);

    // 各行按功能顺序平铺：填充样式 → 填充颜色 → 边框样式 → 边框颜色 → 边框宽度。
    layout->addWidget(m_fillBorderCard->fillStyleRow());
    layout->addWidget(m_colorsCard->fillColorRow());
    layout->addWidget(m_fillBorderCard->borderStyleRow());
    layout->addWidget(m_colorsCard->borderColorRow());
    layout->addWidget(m_fillBorderCard->borderWidthRow());

    m_valueCard = new ItemDockPanelValueCard(content);
    layout->addWidget(m_valueCard);

    m_arrangeCard = new ItemDockPanelArrangeCard(content);
    layout->addWidget(m_arrangeCard);
    layout->addStretch(1);

    scroll->setWidget(content);
    outer->addWidget(scroll, 1);

    connect(m_colorsCard, &ItemDockPanelColorsCard::strokeColorChosen, this, &ItemDockPanel::applyStrokeColor);
    connect(m_colorsCard, &ItemDockPanelColorsCard::fillColorChosen, this, &ItemDockPanel::applyFillColor);
    connect(m_fillBorderCard, &ItemDockPanelFillBorderCard::fillStyleChosen, this, &ItemDockPanel::applyFillStyle);
    connect(m_fillBorderCard, &ItemDockPanelFillBorderCard::borderStyleChosen, this, &ItemDockPanel::applyBorderStyle);
    connect(m_fillBorderCard, &ItemDockPanelFillBorderCard::borderWidthChosen, this, &ItemDockPanel::applyBorderWidth);
    connect(m_valueCard, &ItemDockPanelValueCard::opacityChosen, this, &ItemDockPanel::applyOpacity);
    connect(m_valueCard, &ItemDockPanelValueCard::cornerRadiusChosen, this, &ItemDockPanel::applyCornerRadius);
    connect(m_valueCard, &ItemDockPanelValueCard::sweepAngleChosen, this, &ItemDockPanel::applySweepAngle);
    connect(m_valueCard, &ItemDockPanelValueCard::shapeRadiusChosen, this, &ItemDockPanel::applyShapeRadius);
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

    const QList<CanvasItem*> items = selectedItems();
    const QList<ShapeItem*> shapes = selectedShapes();
    m_selectionCard->setSelectionState(!items.isEmpty(), selectionGroupUseful(items));

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
    m_valueCard->setCornerVisible(hasCorner);
    m_valueCard->setArcRadiusVisible(hasArc);

    // 选中的图元都不支持填充（直线 / 弧线 / 大括号 / 文字 / 图片 / 合并图形）时，
    // 填充样式与填充颜色两行没有可作用的对象，直接隐藏；无选中时保持原有布局。
    bool anyFillable = false;
    for (ShapeItem* shape : shapes) {
        if (shapeTypeIsFilled(shape->shapeType())) {
            anyFillable = true;
            break;
        }
    }
    const bool showFillRows = items.isEmpty() || anyFillable;
    m_fillBorderCard->fillStyleRow()->setVisible(showFillRows);
    m_colorsCard->fillColorRow()->setVisible(showFillRows);

    m_colorsCard->refresh(shapes);
    m_fillBorderCard->refresh(shapes);
    m_valueCard->refresh(shapes);
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
    if (!color.isValid()) {
        return;
    }
    emit strokeColorPicked(color);
    changeSelectedShapes(tr("Change shape border color"), [color](ShapeItem* shape) {
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
    emit borderWidthPicked(width);
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
