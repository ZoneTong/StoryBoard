#include "ItemDockPanelSelectionCard.h"

#include "IconFactory.h"
#include "ItemDockPanel.h"

#include <QAction>
#include <QHBoxLayout>
#include <QMenu>
#include <QToolButton>

namespace {

constexpr int kSelectionButtonSize = 26;
constexpr int kSmallIconSize = 14;

} // namespace

ItemDockPanelSelectionCard::ItemDockPanelSelectionCard(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    m_alignButton = new QToolButton(this);
    m_alignButton->setObjectName(QStringLiteral("ItemAlignButton"));
    m_alignButton->setIcon(makeIcon(IconId::Align, QColor(58, 64, 80), kSmallIconSize));
    m_alignButton->setIconSize(QSize(kSmallIconSize, kSmallIconSize));
    m_alignButton->setFixedSize(kSelectionButtonSize, kSelectionButtonSize);
    m_alignButton->setPopupMode(QToolButton::InstantPopup);
    m_alignButton->setToolTip(ItemDockPanel::tr("Align"));
    m_alignMenu = new QMenu(m_alignButton);
    m_alignMenu->setObjectName(QStringLiteral("ItemAlignMenu"));
    m_alignButton->setMenu(m_alignMenu);
    layout->addWidget(m_alignButton);

    m_mergeButton = new QToolButton(this);
    m_mergeButton->setObjectName(QStringLiteral("ItemMergeButton"));
    m_mergeButton->setIcon(makeIcon(IconId::MergeUnion, QColor(58, 64, 80), kSmallIconSize));
    m_mergeButton->setIconSize(QSize(kSmallIconSize, kSmallIconSize));
    m_mergeButton->setFixedSize(kSelectionButtonSize, kSelectionButtonSize);
    m_mergeButton->setPopupMode(QToolButton::InstantPopup);
    m_mergeButton->setToolTip(ItemDockPanel::tr("Merge shapes"));
    m_mergeMenu = new QMenu(m_mergeButton);
    m_mergeMenu->setObjectName(QStringLiteral("ItemMergeMenu"));
    m_mergeButton->setMenu(m_mergeMenu);
    layout->addWidget(m_mergeButton);
    layout->addStretch(1);

    setVisible(false);
}

void ItemDockPanelSelectionCard::setSelectionActions(const QList<QAction*>& alignActions,
                                                     const QList<QAction*>& boolActions)
{
    if (m_alignMenu) {
        m_alignMenu->clear();
        for (QAction* action : alignActions) {
            m_alignMenu->addAction(action);
        }
    }
    if (m_mergeMenu) {
        m_mergeMenu->clear();
        for (QAction* action : boolActions) {
            m_mergeMenu->addAction(action);
        }
    }
    refreshButtonEnabled();
}

void ItemDockPanelSelectionCard::setMultiSelection(bool multi)
{
    setVisible(multi);
    refreshButtonEnabled();
}

void ItemDockPanelSelectionCard::refreshButtonEnabled()
{
    if (m_alignButton && m_alignMenu) {
        bool anyEnabled = false;
        for (QAction* action : m_alignMenu->actions()) {
            anyEnabled = anyEnabled || action->isEnabled();
        }
        m_alignButton->setEnabled(anyEnabled);
    }
    if (m_mergeButton && m_mergeMenu) {
        bool anyEnabled = false;
        for (QAction* action : m_mergeMenu->actions()) {
            anyEnabled = anyEnabled || action->isEnabled();
        }
        m_mergeButton->setEnabled(anyEnabled);
    }
}
