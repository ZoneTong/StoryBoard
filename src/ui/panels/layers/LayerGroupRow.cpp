#include "ui/panels/layers/LayerGroupRow.h"

#include "ui/icons/IconFactory.h"
#include "ui/panels/layers/LayerPanel.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>

LayerGroupRow::LayerGroupRow(int groupId, const QString& name, int memberCount, int depth, QWidget* parent)
    : QWidget(parent), m_groupId(groupId)
{
    setObjectName(QStringLiteral("LayerGroupRow"));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(18 + depth * 16, 0, 4, 0);
    layout->setSpacing(5);

    auto* iconLabel = new QLabel(this);
    iconLabel->setObjectName(QStringLiteral("LayerGroupRowIcon"));
    iconLabel->setPixmap(IconFactory::Make(IconId::GROUP, QColor(110, 118, 132), 16).pixmap(16, 16));
    layout->addWidget(iconLabel);

    m_nameLabel = new QLabel(this);
    m_nameLabel->setObjectName(QStringLiteral("LayerGroupRowLabel"));
    m_nameLabel->setText(LayerPanel::tr("%1 · %2 items").arg(name).arg(memberCount));
    m_nameLabel->setCursor(Qt::IBeamCursor);
    m_nameLabel->setToolTip(LayerPanel::tr("Double-click to rename group"));
    m_nameLabel->installEventFilter(this);
    layout->addWidget(m_nameLabel, 1);
}

void LayerGroupRow::ApplySelectedStyle(bool selected)
{
    setProperty("selected", selected);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

bool LayerGroupRow::eventFilter(QObject* watched, QEvent* event)
{
    if (event && event->type() == QEvent::MouseButtonDblClick && watched == m_nameLabel) {
        emit renameRequested(m_groupId);
        return true;
    }
    return QWidget::eventFilter(watched, event);
}
