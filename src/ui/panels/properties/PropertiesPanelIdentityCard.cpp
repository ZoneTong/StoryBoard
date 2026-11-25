#include "ui/panels/properties/PropertiesPanelIdentityCard.h"

#include "items/CanvasItem.h"
#include "scene/CanvasScene.h"
#include "scene/ItemTypeConversion.h"
#include "ui/icons/IconFactory.h"
#include "ui/panels/properties/PropertiesPanel.h"
#include "ui/widgets/RowMetrics.h"
#include "ui/widgets/rows/ValueRows.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

// 三型互换的固定顺位：与右键「转换为」子菜单、虚线通用口径一致，
// 下拉里始终列出三项，当前项只是选中态，不因目标类型变化而增删。
const QStringList& ConvertibleKinds()
{
    static const QStringList KINDS{QStringLiteral("sector"), QStringLiteral("arc"), QStringLiteral("angle")};
    return KINDS;
}

} // namespace

PropertiesPanelIdentityCard::PropertiesPanelIdentityCard(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    // 类型行：扇形 / 弧线 / 角度线三者等价的几何，靠下拉直接换类，改动进撤销记录。
    m_typeRow = new QWidget(this);
    m_typeRow->setObjectName(QStringLiteral("ItemTypeRow"));
    RowMetrics::ApplyRow(m_typeRow);
    m_typeRow->setToolTip(PropertiesPanel::tr("Item type"));
    auto* typeLayout = new QHBoxLayout(m_typeRow);
    typeLayout->setContentsMargins(0, 0, 0, 0);
    typeLayout->setSpacing(4);
    auto* typeIcon = new QToolButton(m_typeRow);
    typeIcon->setObjectName(QStringLiteral("ItemRowIcon"));
    typeIcon->setIcon(IconFactory::Make(IconId::SECTOR, QColor(139, 144, 156), RowMetrics::GlyphSize()));
    typeIcon->setIconSize(QSize(RowMetrics::GlyphSize(), RowMetrics::GlyphSize()));
    typeIcon->setFixedSize(RowMetrics::IconButtonSize());
    typeIcon->setAutoRaise(true);
    typeIcon->setFocusPolicy(Qt::NoFocus);
    typeIcon->setToolTip(PropertiesPanel::tr("Item type"));
    typeLayout->addWidget(typeIcon);
    m_typeCombo = new QComboBox(m_typeRow);
    m_typeCombo->setObjectName(QStringLiteral("ItemTypeCombo"));
    m_typeCombo->setToolTip(PropertiesPanel::tr("Item type"));
    for (const QString& kind : ConvertibleKinds()) {
        m_typeCombo->addItem(ItemTypeConversion::DisplayNameOf(kind), kind);
    }
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (!m_refreshing && m_item) {
            emit TypeChosen(m_typeCombo->currentData().toString());
        }
    });
    typeLayout->addWidget(m_typeCombo, 1);
    m_typeRow->setVisible(false);
    layout->addWidget(m_typeRow);

    m_nameRow = new TextValueRow(PropertiesPanel::tr("Item name"), this);
    m_nameRow->Edit()->setToolTip(PropertiesPanel::tr("Item name"));
    layout->addWidget(m_nameRow);

    m_layerRow = new LayerHeightRow(this);
    layout->addWidget(m_layerRow);

    connect(m_nameRow, &TextValueRow::textChosen, this, [this](const QString& name) {
        if (!m_refreshing && m_item && name != m_item->Name()) {
            emit nameChosen(name);
        }
    });
    connect(m_layerRow, &LayerHeightRow::layerChosen, this, [this](int layerId) {
        if (!m_refreshing && m_item && layerId != m_item->LayerId()) {
            emit layerChosen(layerId);
        }
    });
    connect(m_layerRow, &LayerHeightRow::heightChosen, this, [this](int height) {
        if (!m_refreshing && m_item && height != m_item->ZOrder()) {
            emit heightChosen(height);
        }
    });
}

void PropertiesPanelIdentityCard::Refresh(CanvasScene* scene, CanvasItem* item)
{
    m_scene = scene;
    m_item = item;
    const bool enabled = scene && item;
    setEnabled(enabled);

    // 类型行只对可互换的三型开放；其它图元（含无选中）整行隐藏，
    // 不给一个选了也没用的下拉。
    const QString kind = item ? ItemTypeConversion::TypeOf(item) : QString();
    const bool convertible = enabled && !kind.isEmpty();
    m_typeRow->setVisible(convertible);

    if (!enabled) {
        m_refreshing = true;
        m_nameRow->Refresh(QString());
        m_layerRow->Refresh(nullptr, 0, 0, 0);
        m_refreshing = false;
        return;
    }

    m_refreshing = true;
    if (convertible) {
        const int index = m_typeCombo->findData(kind);
        if (index >= 0) {
            m_typeCombo->setCurrentIndex(index);
        }
    }
    m_nameRow->Refresh(item->Name());

    // 与属性窗一致：同层最高高度上再加一格；不改当前值，允许保留下来的空洞。
    int maximum = 0;
    for (CanvasItem* peer : scene->LayerItems(item->LayerId())) {
        maximum = qMax(maximum, peer->ZOrder());
    }
    m_layerRow->Refresh(scene, item->LayerId(), item->ZOrder(), qMax(maximum + 1, item->ZOrder()));
    m_refreshing = false;
}
