#include "ui/panels/properties/PropertiesPanel.h"

#include "items/CanvasItem.h"
#include "items/geometry/ShapeMeta.h"
#include "items/lines/ArcItem.h"
#include "items/lines/LineAbstractItem.h"
#include "items/lines/SineItem.h"
#include "items/polygon/PolygonAccess.h"
#include "items/polygon/PolygonItem.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "items/text/TextItem.h"
#include "scene/CanvasScene.h"
#include "scene/Commands.h"
#include "ui/dialogs/ImagePickerDialog.h"
#include "ui/panels/properties/PropertiesPanelArrangeCard.h"
#include "ui/panels/properties/PropertiesPanelColorsCard.h"
#include "ui/panels/properties/PropertiesPanelContentCard.h"
#include "ui/panels/properties/PropertiesPanelFillBorderCard.h"
#include "ui/panels/properties/PropertiesPanelGeometryCard.h"
#include "ui/panels/properties/PropertiesPanelIdentityCard.h"
#include "ui/panels/properties/PropertiesPanelSelectionCard.h"
#include "ui/panels/properties/PropertiesPanelValueCard.h"
#include "ui/panels/properties/PropertyRowSpec.h"
#include "ui/widgets/rows/FillStrokeRows.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QScrollArea>
#include <QSpinBox>
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
    // 面板本身跟着主窗口高度伸展；只有窗口矮到内容放不下时才出现竖向卷动轴。
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
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
    // 名称/图层/高度 → 图层与高度微调 → 文字内容/颜色/字体、图片 → 填充样式 → 填充色 →
    // 描边颜色 → 描边样式 → 描边宽度 → 透明度/圆角/角度/三角形/箭头 → 位置/尺寸/旋转 →
    // 选择操作行。
    m_identityCard = new PropertiesPanelIdentityCard(content);
    m_identityCard->setObjectName(QStringLiteral("ItemIdentityCard"));
    layout->addWidget(m_identityCard);

    m_arrangeCard = new PropertiesPanelArrangeCard(content);
    m_arrangeCard->setObjectName(QStringLiteral("ItemArrangeCard"));
    layout->addWidget(m_arrangeCard);

    layout->addWidget(m_contentCard->TextContentRow());
    layout->addWidget(m_contentCard->TextColorRow());
    layout->addWidget(m_contentCard->TextFontRow());

    // 需求 1 行序（与菜单、工具栏、属性窗统一）：
    // 填充样式 → 填充颜色 → （需求 13 引入的背景图片） → 描线样式 → 描线颜色 → 描线宽度。
    layout->addWidget(m_fillBorderCard->FillStyleWidget());
    layout->addWidget(m_colorsCard->FillColorWidget());

    // 填充图片行与属性窗共用同一个属性行控件：图标、更换图片、摆放方式、清除四件，
    // 值直接读 FillAttributes，改动仍交回面板走撤销命令。
    m_fillImageRow = new FillImageRow(content);
    connect(m_fillImageRow, &FillImageRow::chooseFillImageClicked, this, &PropertiesPanel::ChooseFillImage);
    connect(m_fillImageRow, &FillImageRow::fillImageModeChosen, this, &PropertiesPanel::ApplyFillImageMode);
    connect(m_fillImageRow, &FillImageRow::clearFillImageClicked, this, &PropertiesPanel::ClearFillImage);
    layout->addWidget(m_fillImageRow);

    layout->addWidget(m_fillBorderCard->BorderStyleWidget());
    layout->addWidget(m_colorsCard->BorderColorRow());
    layout->addWidget(m_fillBorderCard->BorderWidthRow());

    m_valueCard = new PropertiesPanelValueCard(content);
    layout->addWidget(m_valueCard);

    m_geometryCard = new PropertiesPanelGeometryCard(content);
    m_geometryCard->setObjectName(QStringLiteral("ItemPlacementCard"));
    layout->addWidget(m_geometryCard);

    m_dynamicCard = new QWidget(content);
    m_dynamicCard->setObjectName(QStringLiteral("ItemDynamicPropertyCard"));
    m_dynamicForm = new QFormLayout(m_dynamicCard);
    m_dynamicForm->setContentsMargins(0, 0, 0, 0);
    m_dynamicForm->setSpacing(4);
    m_dynamicCard->setVisible(false);
    layout->addWidget(m_dynamicCard);

    layout->addWidget(m_selectionCard);
    layout->addStretch(1);

    scroll->setWidget(content);
    outer->addWidget(scroll, 1);

    connect(m_colorsCard, &PropertiesPanelColorsCard::strokeColorChosen, this, &PropertiesPanel::ApplyStrokeColor);
    connect(m_colorsCard, &PropertiesPanelColorsCard::fillColorChosen, this, &PropertiesPanel::ApplyFillColor);
    connect(m_fillBorderCard, &PropertiesPanelFillBorderCard::fillStyleChosen, this, &PropertiesPanel::ApplyFillStyle);
    connect(m_fillBorderCard, &PropertiesPanelFillBorderCard::borderStyleChosen, this,
            &PropertiesPanel::ApplyBorderStyle);
    connect(m_fillBorderCard, &PropertiesPanelFillBorderCard::borderWidthChosen, this,
            &PropertiesPanel::ApplyBorderWidth);
    connect(m_contentCard, &PropertiesPanelContentCard::textContentChosen, this, &PropertiesPanel::ApplyTextContent);
    connect(m_contentCard, &PropertiesPanelContentCard::textFontChosen, this, &PropertiesPanel::ApplyTextFont);
    connect(m_contentCard, &PropertiesPanelContentCard::textColorChosen, this, &PropertiesPanel::ApplyTextColor);
    connect(m_valueCard, &PropertiesPanelValueCard::opacityChosen, this, &PropertiesPanel::ApplyOpacity);
    connect(m_valueCard, &PropertiesPanelValueCard::cornerRadiusChosen, this, &PropertiesPanel::ApplyCornerRadius);
    connect(m_valueCard, &PropertiesPanelValueCard::arcAnglesChosen, this, &PropertiesPanel::ApplyArcAngles);
    connect(m_valueCard, &PropertiesPanelValueCard::axesChosen, this, &PropertiesPanel::ApplyItemAxes);
    connect(m_valueCard, &PropertiesPanelValueCard::interiorAngleChosen, this, &PropertiesPanel::ApplyInteriorAngle);
    connect(m_valueCard, &PropertiesPanelValueCard::interiorAngleBChosen, this, &PropertiesPanel::ApplyInteriorAngleB);
    connect(m_valueCard, &PropertiesPanelValueCard::sineCyclesChosen, this, &PropertiesPanel::ApplySineCycles);
    connect(m_valueCard, &PropertiesPanelValueCard::sineAmplitudeChosen, this, &PropertiesPanel::ApplySineAmplitude);
    connect(m_valueCard, &PropertiesPanelValueCard::arrowStartChosen, this, &PropertiesPanel::ApplyArrowStart);
    connect(m_valueCard, &PropertiesPanelValueCard::arrowEndChosen, this, &PropertiesPanel::ApplyArrowEnd);
    connect(m_arrangeCard, &PropertiesPanelArrangeCard::layerAdjusted, this, &PropertiesPanel::AdjustLayer);
    connect(m_arrangeCard, &PropertiesPanelArrangeCard::heightAdjusted, this, &PropertiesPanel::AdjustHeight);
    connect(m_identityCard, &PropertiesPanelIdentityCard::TypeChosen, this, &PropertiesPanel::ApplyItemType);
    connect(m_identityCard, &PropertiesPanelIdentityCard::nameChosen, this, &PropertiesPanel::ApplyItemName);
    connect(m_identityCard, &PropertiesPanelIdentityCard::layerChosen, this, &PropertiesPanel::ApplyItemLayer);
    connect(m_identityCard, &PropertiesPanelIdentityCard::heightChosen, this, &PropertiesPanel::ApplyItemHeight);
    connect(m_geometryCard, &PropertiesPanelGeometryCard::geometryChosen, this, &PropertiesPanel::ApplyItemPlacement);
    connect(m_geometryCard, &PropertiesPanelGeometryCard::rotationChosen, this, &PropertiesPanel::ApplyItemRotation);
    connect(m_geometryCard, &PropertiesPanelGeometryCard::centerChosen, this, &PropertiesPanel::ApplyItemCenter);

    if (m_scene) {
        connect(m_scene, &QGraphicsScene::selectionChanged, this, &PropertiesPanel::ScheduleRefresh);
        connect(m_scene, &CanvasScene::contentChanged, this, &PropertiesPanel::ScheduleRefresh);
        // 模式切换会改写一批动作的可用状态，操作栏要跟着重算显隐。
        connect(m_scene, &CanvasScene::modeChanged, this, &PropertiesPanel::ScheduleRefresh);
        if (m_scene->UndoStack()) {
            // 撤销 / 重做后卡片里的数值也要回到图元的真实状态。
            connect(m_scene->UndoStack(), &QUndoStack::indexChanged, this, &PropertiesPanel::ScheduleRefresh);
        }
    }

    Refresh();
}

void PropertiesPanel::SetSelectionActions(const QList<QAction*>& alignActions, const QList<QAction*>& boolActions,
                                          QAction* copyAction, QAction* removeAction, QAction* rotateCwAction,
                                          QAction* rotateCcwAction, QAction* groupAction, QAction* ungroupAction)
{
    if (m_selectionCard) {
        m_selectionCard->SetSelectionActions(alignActions, boolActions, copyAction, removeAction, rotateCwAction,
                                             rotateCcwAction, groupAction, ungroupAction);
    }
}

void PropertiesPanel::SetItemActions(QAction* smoothAction)
{
    if (m_selectionCard) {
        m_selectionCard->SetItemActions(smoothAction);
    }
}

QList<CanvasItem*> PropertiesPanel::SelectedItems() const
{
    return m_scene ? m_scene->SelectedCanvasItems() : QList<CanvasItem*>();
}

QList<ShapeAbstractItem*> PropertiesPanel::SelectedShapes() const
{
    QList<ShapeAbstractItem*> shapes;
    for (CanvasItem* item : SelectedItems()) {
        // 多边形派生自 ShapeAbstractItem，但它没有形状类型语义：圆角 / 角度等行只对普通图形开放。
        if (auto* shape = dynamic_cast<ShapeAbstractItem*>(item); shape && !PolygonAccess::Is(shape)) {
            shapes.append(shape);
        }
    }
    return shapes;
}

QList<LineAbstractItem*> PropertiesPanel::SelectedStrokes() const
{
    QList<LineAbstractItem*> strokes;
    for (CanvasItem* item : SelectedItems()) {
        if (auto* stroke = dynamic_cast<LineAbstractItem*>(item)) {
            strokes.append(stroke);
        }
    }
    return strokes;
}

QList<TextItem*> PropertiesPanel::SelectedTexts() const
{
    QList<TextItem*> texts;
    for (CanvasItem* item : SelectedItems()) {
        if (auto* text = dynamic_cast<TextItem*>(item)) {
            texts.append(text);
        }
    }
    return texts;
}

void PropertiesPanel::ScheduleRefresh()
{
    if (m_refreshing) {
        m_refreshPending = true;
        return;
    }
    QTimer::singleShot(0, this, &PropertiesPanel::Refresh);
}

int PropertiesPanel::PreferredWidth() const
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

void PropertiesPanel::Refresh()
{
    if (!m_scene) {
        return;
    }
    m_refreshing = true;
    UpdateSummary();

    const QList<CanvasItem*> items = SelectedItems();
    const QList<ShapeAbstractItem*> shapes = SelectedShapes();
    const QList<LineAbstractItem*> strokes = SelectedStrokes();
    const QList<TextItem*> texts = SelectedTexts();

    // 属性行按基类绑定：填充行只看 FillAttributes，描线行只看 StrokeAttributes，
    // 不再按叶子类分别拼目标；多边形等特殊取色由基类虚函数处理。
    QList<FillAttributes*> fillTargets;
    QList<StrokeAttributes*> strokeTargets;
    for (CanvasItem* item : items) {
        if (auto* fill = dynamic_cast<FillAttributes*>(item)) {
            fillTargets.append(fill);
        }
        if (auto* stroke = dynamic_cast<StrokeAttributes*>(item)) {
            strokeTargets.append(stroke);
        }
    }
    m_selectionCard->SetSelectionState(!items.isEmpty(), SelectionGroupUseful(items));

    // 交集规则：只显示选中集合里每个图元都拥有的行；空选择时集合为空，全部行隐藏。
    // 值的回显仍取第一个选中图元（卡片内部的既有逻辑）。
    ItemPropertySet common;
    for (int index = 0; index < items.size(); ++index) {
        const ItemPropertySet properties = items.at(index)->Properties();
        common = index == 0 ? properties : common.Intersected(properties);
    }
    // 行显隐统一走行描述的可接受集合：内角行同时接受三角形底角与平行四边形内角，
    // 箭头行同时接受起止箭头，避免面板再按单个枚举值各写一份判断。
    auto rowVisible = [&common](ItemProperty property) {
        const PropertyRowSpec* spec = PropertyRowSpecs::For(property);
        return spec && spec->Matches(common);
    };

    // 形状成对属性行统一在数值卡片里，行序：半轴 → 起止角 → 内角 → 圆角。
    m_valueCard->SetAxisVisible(rowVisible(ItemProperty::MAJOR_AXIS));
    m_valueCard->SetArcVisible(rowVisible(ItemProperty::START_ANGLE));
    // 内角行同时接受三角形底角与平行四边形内角（行描述里两个键已合并）。
    m_valueCard->SetInteriorAngleVisible(rowVisible(ItemProperty::BOTTOM_ANGLE1));
    m_valueCard->SetCornerVisible(rowVisible(ItemProperty::CORNER_RADIUS));
    m_valueCard->SetSineVisible(rowVisible(ItemProperty::SINE_CYCLES));
    m_valueCard->SetArrowVisible(rowVisible(ItemProperty::ARROW_START));
    m_fillImageRow->setVisible(rowVisible(ItemProperty::FILL_IMAGE));
    m_fillImageRow->SetTargets(fillTargets);

    // 身份行（名称 / 图层 / 高度）只在单选时开放：把多个图元改成同一个名称或
    // 同一个层内高度没有意义。几何行多选也要开放，位置与尺寸按选中集合的首个
    // 图元回显，改动按增量套到其余图元，避免所有图元被叠到同一坐标。
    const bool singleItem = items.size() == 1;
    const bool hasSelection = !items.isEmpty();
    m_identityCard->setVisible(singleItem);
    m_geometryCard->setVisible(hasSelection);
    m_identityCard->Refresh(m_scene, singleItem ? items.first() : nullptr);
    m_geometryCard->Refresh(hasSelection ? items.first() : nullptr);

    // 中心点行按属性交集显隐：多选时也开放，写入侧只对上报了该属性的成员生效。
    m_geometryCard->SetCenterVisible(rowVisible(ItemProperty::CENTER));
    RefreshDynamicRows(common, items);

    m_fillBorderCard->FillStyleWidget()->setVisible(rowVisible(ItemProperty::FILL_STYLE));
    m_colorsCard->FillColorWidget()->setVisible(rowVisible(ItemProperty::FILL_COLOR));
    m_fillBorderCard->BorderStyleWidget()->setVisible(rowVisible(ItemProperty::STROKE_STYLE));
    m_colorsCard->BorderColorRow()->setVisible(rowVisible(ItemProperty::STROKE_COLOR));
    m_fillBorderCard->BorderWidthRow()->setVisible(rowVisible(ItemProperty::STROKE_WIDTH));

    m_contentCard->setVisible(hasSelection);
    m_colorsCard->setVisible(hasSelection);
    m_fillBorderCard->setVisible(hasSelection);
    m_valueCard->setVisible(hasSelection);
    m_arrangeCard->setVisible(hasSelection);
    m_contentCard->Refresh(texts);
    m_colorsCard->Refresh(fillTargets, strokeTargets);
    m_fillBorderCard->Refresh(fillTargets, strokeTargets);
    m_valueCard->Refresh(shapes, strokes, items);
    m_arrangeCard->Refresh(!SelectedItems().isEmpty());

    m_refreshing = false;
    if (m_refreshPending) {
        m_refreshPending = false;
        ScheduleRefresh();
    }
}

void PropertiesPanel::RefreshDynamicRows(const ItemPropertySet& common, const QList<CanvasItem*>& items)
{
    if (!m_dynamicForm) {
        return;
    }
    while (m_dynamicForm->rowCount() > 0) {
        auto row = m_dynamicForm->takeRow(0);
        delete row.labelItem;
        if (row.fieldItem) {
            delete row.fieldItem->widget();
            delete row.fieldItem;
        }
    }
    if (items.isEmpty()) {
        m_dynamicCard->setVisible(false);
        return;
    }

    CanvasItem* source = items.first();
    for (PropertyKey property : common.Keys()) {
        if (property.IsCommon()) {
            continue;
        }
        const ItemPropertyDescriptor* descriptor = ItemPropertyCatalog::Find(property);
        const PropertyRowSpec* spec = PropertyRowSpecs::For(property);
        if (!descriptor || !spec) {
            continue;
        }
        const QVariant current = source->PropertyValueByKey(property);
        QWidget* editor = nullptr;
        if (descriptor->kind == ItemPropertyKind::TEXT) {
            auto* line = new QLineEdit(m_dynamicCard);
            line->setText(current.toString());
            connect(line, &QLineEdit::editingFinished, this,
                    [this, property, line]() { ApplyDynamicProperty(property, line->text()); });
            editor = line;
        } else if (descriptor->kind == ItemPropertyKind::INTEGER) {
            auto* spin = new QSpinBox(m_dynamicCard);
            spin->setRange(qRound(descriptor->minimum), qRound(descriptor->maximum));
            spin->setSingleStep(qMax(1, qRound(descriptor->step)));
            spin->setSuffix(descriptor->suffix);
            spin->setValue(current.toInt());
            connect(spin, &QSpinBox::editingFinished, this,
                    [this, property, spin]() { ApplyDynamicProperty(property, spin->value()); });
            editor = spin;
        } else if (descriptor->kind == ItemPropertyKind::REAL) {
            auto* spin = new QDoubleSpinBox(m_dynamicCard);
            spin->setRange(descriptor->minimum, descriptor->maximum);
            spin->setSingleStep(descriptor->step);
            spin->setDecimals(descriptor->decimals);
            spin->setSuffix(descriptor->suffix);
            spin->setValue(current.toDouble());
            connect(spin, &QDoubleSpinBox::editingFinished, this,
                    [this, property, spin]() { ApplyDynamicProperty(property, spin->value()); });
            editor = spin;
        } else {
            auto* label = new QLabel(current.toString(), m_dynamicCard);
            editor = label;
        }
        m_dynamicForm->addRow(spec->Label(), editor);
    }
    m_dynamicCard->setVisible(m_dynamicForm->rowCount() > 0);
}

void PropertiesPanel::ApplyDynamicProperty(PropertyKey property, const QVariant& value)
{
    ApplyPropertyValues(QMap<PropertyKey, QVariant>{{property, value}}, ItemPropertyCatalog::Label(property));
}

bool PropertiesPanel::SelectionGroupUseful(const QList<CanvasItem*>& items) const
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
        const int current = m_scene->OutermostGroup(item->GroupId());
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

void PropertiesPanel::UpdateSummary()
{
    const QList<CanvasItem*> items = SelectedItems();
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
