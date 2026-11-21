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
    fillImageIcon->setIcon(makeIcon(IconId::IMAGE, QColor(139, 144, 156), 20));
    fillImageIcon->setIconSize(QSize(20, 20));
    fillImageIcon->setFixedSize(32, 32);
    fillImageIcon->setAutoRaise(true);
    fillImageIcon->setFocusPolicy(Qt::NoFocus);
    fillImageLayout->addWidget(fillImageIcon);

    m_fillImageButton = new QToolButton(m_fillImageRow);
    m_fillImageButton->setObjectName(QStringLiteral("ItemFillImageButton"));
    m_fillImageButton->setIcon(makeIcon(IconId::IMAGE, QColor(58, 64, 80), 16));
    m_fillImageButton->setIconSize(QSize(16, 16));
    m_fillImageButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_fillImageButton->setText(tr("Choose fill image"));
    m_fillImageButton->setToolTip(tr("Choose an image to fill the selected items"));
    m_fillImageButton->setMinimumHeight(30);
    connect(m_fillImageButton, &QToolButton::clicked, this, &PropertiesPanel::chooseFillImage);
    fillImageLayout->addWidget(m_fillImageButton, 1);

    m_clearFillImageButton = new QToolButton(m_fillImageRow);
    m_clearFillImageButton->setObjectName(QStringLiteral("ItemRowIcon"));
    m_clearFillImageButton->setIcon(makeIcon(IconId::DELETE, QColor(139, 144, 156), 20));
    m_clearFillImageButton->setIconSize(QSize(20, 20));
    m_clearFillImageButton->setFixedSize(32, 32);
    m_clearFillImageButton->setAutoRaise(true);
    m_clearFillImageButton->setFocusPolicy(Qt::NoFocus);
    m_clearFillImageButton->setToolTip(tr("Clear fill image"));
    connect(m_clearFillImageButton, &QToolButton::clicked, this, &PropertiesPanel::ClearFillImage);
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
        if (auto* shape = dynamic_cast<ShapeItem*>(item); shape && shape->ShapeLayerCount() == 0) {
            shapes.append(shape);
        }
    }
    return shapes;
}

QList<ShapeItem*> PropertiesPanel::selectedComposites() const
{
    QList<ShapeItem*> composites;
    for (CanvasItem* item : selectedItems()) {
        if (auto* shape = dynamic_cast<ShapeItem*>(item); shape && shape->ShapeLayerCount() > 0) {
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
        if (item->HasTextFacet()) {
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
        const ItemPropertySet properties = items.at(index)->Properties();
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
                              items.first()->PropertyValue(ItemProperty::FILL_IMAGE).value<QPixmap>().isNull() == false;
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
        if (item->GroupId() <= 0) {
            return true;
        }
        const int current = m_scene->outermostGroup(item->GroupId());
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
    m_titleLabel->setText(item->DisplayName());
    m_metaLabel->setText(
        tr("%1 · layer %2 · height %3").arg(item->TypeDisplayName()).arg(item->LayerId()).arg(item->ZOrder()));
}
