#include "ItemDockPanelShapesCard.h"

#include "CustomShapeTemplates.h"
#include "IconFactory.h"
#include "ItemDockPanel.h"
#include "Settings.h"
#include "ShapeItem.h"

#include <QGridLayout>
#include <QMenu>
#include <QPainterPath>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kIconSize = 16;

} // namespace

ItemDockPanelShapesCard::ItemDockPanelShapesCard(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(2);
    const QList<ShapeType> types = allShapeTypes();
    for (int index = 0; index < types.size(); ++index) {
        const ShapeType type = types.at(index);
        auto* button = new QToolButton(this);
        button->setObjectName(QStringLiteral("ItemShapeButton"));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setIcon(makeShapeIcon(type, QColor(58, 64, 80), 18));
        button->setIconSize(QSize(18, 18));
        button->setFixedSize(26, 26);
        button->setToolTip(ItemDockPanel::tr("Common shapes") + QStringLiteral(" · ") + shapeTypeName(type));
        connect(button, &QToolButton::clicked, this, [this, type]() { emit shapeTriggered(static_cast<int>(type)); });
        m_shapeButtons.append(button);
        grid->addWidget(button, index / 6, index % 6);
    }
    layout->addLayout(grid);

    m_customShapeButton = new QToolButton(this);
    m_customShapeButton->setObjectName(QStringLiteral("ItemCustomShapeButton"));
    m_customShapeButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_customShapeButton->setPopupMode(QToolButton::InstantPopup);
    m_customShapeButton->setArrowType(Qt::DownArrow);
    m_customShapeButton->setText(ItemDockPanel::tr("Custom shapes"));
    m_customShapeButton->setIcon(makeCustomShapeIcon(QPainterPath(), QColor(58, 64, 80), kIconSize));
    m_customShapeButton->setIconSize(QSize(kIconSize, kIconSize));
    m_customShapeButton->setFixedSize(26, 26);
    m_customShapeButton->setToolTip(ItemDockPanel::tr("Custom shapes"));
    m_customShapeMenu = new QMenu(m_customShapeButton);
    m_customShapeButton->setMenu(m_customShapeMenu);
    connect(m_customShapeMenu, &QMenu::aboutToShow, this, &ItemDockPanelShapesCard::rebuildCustomShapeMenu);
    grid->addWidget(m_customShapeButton, types.size() / 6, types.size() % 6);
    rebuildCustomShapeMenu();
}

void ItemDockPanelShapesCard::refresh(const QList<ShapeItem*>& shapes)
{
    const ShapeType current = shapes.isEmpty() ? ShapeType::Rectangle : shapes.first()->shapeType();
    const bool hasCustom = !shapes.isEmpty() && shapes.first()->isCustomShape();
    for (QToolButton* button : m_shapeButtons) {
        const ShapeType type = static_cast<ShapeType>(m_shapeButtons.indexOf(button));
        button->setChecked(!hasCustom && !shapes.isEmpty() && type == current);
    }
    if (m_customShapeButton) {
        m_customShapeButton->setChecked(hasCustom);
        if (hasCustom) {
            m_customShapeButton->setToolTip(shapes.first()->customShapeName());
        } else {
            m_customShapeButton->setToolTip(ItemDockPanel::tr("Custom shapes"));
        }
    }
}

void ItemDockPanelShapesCard::rebuildCustomShapeMenu()
{
    if (!m_customShapeMenu) {
        return;
    }
    m_customShapeMenu->clear();

    QStringList errors;
    const QList<CustomShapeTemplate> templates = loadCustomShapeTemplatesFromDirs(AppSettings::shapesDirs(), &errors);
    if (templates.isEmpty()) {
        QAction* empty = m_customShapeMenu->addAction(ItemDockPanel::tr("(No custom shapes in the shapes folder)"));
        empty->setEnabled(false);
    } else {
        for (const CustomShapeTemplate& entry : templates) {
            QAction* action =
                m_customShapeMenu->addAction(makeCustomShapeIcon(entry.unitPath, QColor(58, 64, 80), 18), entry.name);
            const QString filePath = entry.filePath;
            connect(action, &QAction::triggered, this,
                    [this, filePath]() { emit insertCustomShapeRequested(filePath); });
        }
    }
    m_customShapeMenu->addSeparator();
    connect(m_customShapeMenu->addAction(ItemDockPanel::tr("Open shapes folder…")), &QAction::triggered, this,
            &ItemDockPanelShapesCard::openShapesFolderRequested);
    if (!errors.isEmpty()) {
        QAction* broken =
            m_customShapeMenu->addAction(ItemDockPanel::tr("%1 shape file(s) could not be read").arg(errors.size()));
        broken->setEnabled(false);
        broken->setToolTip(errors.join(QLatin1Char('\n')));
    }
}
