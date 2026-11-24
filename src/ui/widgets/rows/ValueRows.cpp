#include "ui/widgets/rows/ValueRows.h"

#include "items/properties/ItemPropertyCatalog.h"
#include "scene/CanvasScene.h"
#include "ui/widgets/RowMetrics.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>

namespace {

// 行标签与图层描述沿用属性 i18n 上下文（手工维护的 storyboard_zh.ts 已按该上下文收录）。
QString TrRow(const char* text)
{
    return QCoreApplication::translate("ItemPropertiesDialog", text);
}

QString LayerDescriptionText(const CanvasScene* scene, int layerId)
{
    if (layerId == CanvasScene::BackgroundLayerId) {
        return TrRow("Background layer");
    }
    const int index = scene ? scene->LayerIndex(layerId) : layerId;
    QString layerName;
    if (scene) {
        for (const CanvasLayer& layer : scene->Layers()) {
            if (layer.id == layerId) {
                layerName = layer.name;
                break;
            }
        }
    }
    const QString defaultName = TrRow("Layer %1").arg(index);
    if (layerName.isEmpty() || layerName == defaultName) {
        return defaultName;
    }
    return TrRow("Layer %1 · %2").arg(index).arg(layerName);
}

const ItemPropertyDescriptor& DescriptorOf(ItemProperty property)
{
    const ItemPropertyDescriptor* descriptor = ItemPropertyCatalog::Find(property);
    static const ItemPropertyDescriptor FALLBACK{};
    return descriptor ? *descriptor : FALLBACK;
}

QToolButton* AddRowIcon(QHBoxLayout* layout, IconId id, QWidget* parent, const QString& toolTip)
{
    auto* button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("ItemRowIcon"));
    const int glyph = RowMetrics::GlyphSize();
    button->setIcon(IconFactory::Make(id, QColor(139, 144, 156), glyph));
    button->setIconSize(QSize(glyph, glyph));
    // 与输入框等高：属性窗没有图标，这里一旦比输入框高，就会把整行撑起来。
    button->setFixedSize(RowMetrics::IconButtonSize());
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setToolTip(toolTip);
    layout->addWidget(button);
    return button;
}

QDoubleSpinBox* MakeSpin(QWidget* parent, const QString& objectName, int decimals, qreal minimum, qreal maximum,
                         const QString& suffix)
{
    auto* spin = new QDoubleSpinBox(parent);
    spin->setObjectName(objectName);
    spin->setDecimals(decimals);
    spin->setRange(minimum, maximum);
    spin->setSuffix(suffix);
    spin->setKeyboardTracking(false);
    spin->setMinimumWidth(0);
    spin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    RowMetrics::ApplyFieldHeight(spin);
    return spin;
}

} // namespace

TextValueRow::TextValueRow(const QString& placeholder, QWidget* parent) : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    m_edit = new QLineEdit(this);
    m_edit->setObjectName(QStringLiteral("ItemNameEdit"));
    m_edit->setPlaceholderText(placeholder);
    m_edit->setClearButtonEnabled(true);
    RowMetrics::ApplyFieldHeight(m_edit);
    layout->addWidget(m_edit, 1);

    connect(m_edit, &QLineEdit::editingFinished, this, [this]() {
        // 只在用户真的改动文本后发信号，避免仅点一下输入框就产生一条改名记录。
        if (m_edit->text() != m_edit->property("lastText").toString()) {
            m_edit->setProperty("lastText", m_edit->text());
            emit textChosen(m_edit->text());
        }
    });
}

void TextValueRow::Refresh(const QString& text)
{
    const QSignalBlocker blocker(m_edit);
    m_edit->setText(text);
    m_edit->setProperty("lastText", text);
}

QString TextValueRow::Text() const
{
    return m_edit ? m_edit->text() : QString();
}

LayerHeightRow::LayerHeightRow(QWidget* parent, bool showIcon) : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    if (showIcon) {
        AddRowIcon(layout, IconId::LAYER_UP, this, TrRow("Layer") + QStringLiteral(" / ") + TrRow("Height"));
    }

    m_combo = new QComboBox(this);
    m_combo->setObjectName(QStringLiteral("ItemLayerCombo"));
    m_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_combo->setMinimumWidth(0);
    m_combo->setToolTip(TrRow("Layer"));
    RowMetrics::ApplyFieldHeight(m_combo);
    layout->addWidget(m_combo, 1);

    m_spin = new QSpinBox(this);
    m_spin->setObjectName(QStringLiteral("ItemHeightSpin"));
    m_spin->setRange(0, 999999);
    m_spin->setKeyboardTracking(false);
    // 图层下拉吃掉剩余宽度，高度框只占一格数字：同一行的两个语义不互相挤压。
    m_spin->setFixedWidth(RowMetrics::IconButtonSize().width() * 2);
    m_spin->setToolTip(TrRow("Height"));
    RowMetrics::ApplyFieldHeight(m_spin);
    layout->addWidget(m_spin, 0);

    setToolTip(TrRow("Layer") + QStringLiteral(" / ") + TrRow("Height"));

    connect(m_combo, &QComboBox::currentIndexChanged, this, [this](int) {
        if (!m_refreshing) {
            emit layerChosen(LayerId());
        }
    });
    connect(m_spin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_refreshing) {
            emit heightChosen(value);
        }
    });
}

void LayerHeightRow::Refresh(CanvasScene* scene, int layerId, int height, int maximum)
{
    m_refreshing = true;
    m_combo->clear();
    if (scene) {
        for (const CanvasLayer& layer : scene->UserLayers()) {
            m_combo->addItem(LayerDescriptionText(scene, layer.id), layer.id);
        }
    }
    const int index = m_combo->findData(layerId);
    if (index >= 0) {
        m_combo->setCurrentIndex(index);
    }
    // 上限只往上留，不改当前值：允许保留下来的空洞。
    m_spin->setMaximum(qMax(maximum, height));
    m_spin->setValue(height);
    m_refreshing = false;
}

int LayerHeightRow::LayerId() const
{
    return m_combo && m_combo->currentIndex() >= 0 ? m_combo->currentData().toInt() : 0;
}

int LayerHeightRow::Height() const
{
    return m_spin ? m_spin->value() : 0;
}

NumberValueRow::NumberValueRow(IconId icon, bool showIcon, int decimals, qreal minimum, qreal maximum,
                               const QString& suffix, const QString& objectName, QWidget* parent)
    : QWidget(parent)
{
    RowMetrics::ApplyRow(this);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    if (showIcon) {
        m_icon = AddRowIcon(layout, icon, this, objectName);
    }

    m_spin = MakeSpin(this, objectName, decimals, minimum, maximum, suffix);
    layout->addWidget(m_spin, 1);

    connect(m_spin, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (!m_refreshing) {
            emit valueChosen(value);
        }
    });
}

NumberValueRow::NumberValueRow(IconId icon, bool showIcon, ItemProperty property, const QString& objectName,
                               QWidget* parent)
    : NumberValueRow(icon, showIcon, DescriptorOf(property).decimals, DescriptorOf(property).minimum,
                     DescriptorOf(property).maximum, DescriptorOf(property).suffix, objectName, parent)
{
    SetSingleStep(DescriptorOf(property).step);
}

void NumberValueRow::SetIconToolTip(const QString& toolTip)
{
    if (m_icon) {
        m_icon->setToolTip(toolTip);
    }
}

void NumberValueRow::Refresh(qreal value)
{
    m_refreshing = true;
    m_spin->setValue(value);
    m_refreshing = false;
}

qreal NumberValueRow::Value() const
{
    return m_spin ? m_spin->value() : 0.0;
}

void NumberValueRow::SetSingleStep(qreal step)
{
    m_spin->setSingleStep(step);
}

PairValueRow::PairValueRow(IconId icon, bool showIcon, const QString& firstName, const QString& secondName,
                           int decimals, qreal minimum, qreal maximum, const QString& suffix, const QString& separator,
                           QWidget* parent)
    : QWidget(parent)
{
    RowMetrics::ApplyRow(this);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    if (showIcon) {
        m_icon = AddRowIcon(layout, icon, this, firstName);
    }

    m_first = MakeSpin(this, firstName, decimals, minimum, maximum, suffix);
    m_second = MakeSpin(this, secondName, decimals, minimum, maximum, suffix);
    layout->addWidget(m_first, 1);

    auto* label = new QLabel(separator, this);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);

    layout->addWidget(m_second, 1);

    for (QDoubleSpinBox* spin : {m_first, m_second}) {
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this](double) {
            if (!m_refreshing) {
                emit valuesChosen(m_first->value(), m_second->value());
            }
        });
    }
}

PairValueRow::PairValueRow(IconId icon, bool showIcon, const QString& firstName, const QString& secondName,
                           ItemProperty property, const QString& separator, QWidget* parent)
    : PairValueRow(icon, showIcon, firstName, secondName, DescriptorOf(property).decimals,
                   DescriptorOf(property).minimum, DescriptorOf(property).maximum, DescriptorOf(property).suffix,
                   separator, parent)
{
}

void PairValueRow::SetIconToolTip(const QString& toolTip)
{
    if (m_icon) {
        m_icon->setToolTip(toolTip);
    }
}

void PairValueRow::Refresh(qreal first, qreal second)
{
    m_refreshing = true;
    m_first->setValue(first);
    m_second->setValue(second);
    m_refreshing = false;
}

qreal PairValueRow::First() const
{
    return m_first ? m_first->value() : 0.0;
}

qreal PairValueRow::Second() const
{
    return m_second ? m_second->value() : 0.0;
}

void ApplyItemPropertyRange(QSpinBox* spin, ItemProperty property)
{
    if (!spin) {
        return;
    }
    const ItemPropertyDescriptor& descriptor = DescriptorOf(property);
    spin->setRange(qRound(descriptor.minimum), qRound(descriptor.maximum));
    spin->setSingleStep(qMax(1, qRound(descriptor.step)));
    spin->setSuffix(descriptor.suffix);
}
