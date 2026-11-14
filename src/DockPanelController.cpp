#include "DockPanelController.h"

#include "CanvasScene.h"
#include "ItemDockPanel.h"
#include "LayerPanel.h"
#include "MainWindow.h"

#include <QDockWidget>
#include <QEvent>
#include <QStatusBar>
#include <QTimer>

DockPanelController::DockPanelController(MainWindow* window) : m_window(window) {}

void DockPanelController::createLayerDock()
{
    m_layerDock = new QDockWidget(MainWindow::tr("Layers"), m_window);
    m_layerDock->setObjectName(QStringLiteral("LayerDock"));
    m_layerDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_layerDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable |
                             QDockWidget::DockWidgetClosable);

    m_layerPanel = new LayerPanel(m_window->m_scene, m_layerDock);
    m_layerDock->setWidget(m_layerPanel);
    m_window->addDockWidget(Qt::RightDockWidgetArea, m_layerDock);
    // 卡片不铺满右侧：给出合适的高度区间，避免单独显示时被拉到整列高度。
    m_layerDock->setMinimumHeight(200);
    m_layerDock->setMaximumHeight(420);

    QObject::connect(m_layerPanel, &LayerPanel::statusMessage, m_window,
                     [this](const QString& message) { m_window->statusBar()->showMessage(message, 3000); });
}

void DockPanelController::createItemDock()
{
    m_itemDock = new QDockWidget(MainWindow::tr("Edit item"), m_window);
    m_itemDock->setObjectName(QStringLiteral("ItemDock"));
    m_itemDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_itemDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable |
                            QDockWidget::DockWidgetClosable);

    m_itemPanel = new ItemDockPanel(m_window->m_scene, m_itemDock);
    m_itemDock->setWidget(m_itemPanel);
    m_window->addDockWidget(Qt::RightDockWidgetArea, m_itemDock);
    // 编辑图元卡同样限定高度区间，显示 / 隐藏时不会把另一张卡顶到整列高。
    m_itemDock->setMinimumHeight(240);
    m_itemDock->setMaximumHeight(620);

    QObject::connect(m_itemPanel, &ItemDockPanel::insertShapeRequested, m_window,
                     [this](int type) { m_window->insertShape(static_cast<ShapeType>(type)); });
    QObject::connect(m_itemPanel, &ItemDockPanel::insertCustomShapeRequested, m_window, &MainWindow::insertCustomShape);
    QObject::connect(m_itemPanel, &ItemDockPanel::openShapesFolderRequested, m_window, &MainWindow::openShapesFolder);
    QObject::connect(m_itemPanel, &ItemDockPanel::statusMessage, m_window,
                     [this](const QString& message) { m_window->statusBar()->showMessage(message, 3000); });

    // 需求：默认无选中就隐藏，选中图元时自动出现。
    m_itemDock->setVisible(false);
}

QStringList DockPanelController::currentDockOrder() const
{
    if (!m_layerDock || !m_itemDock) {
        return {};
    }
    // 两张卡都在右侧且都已显示时按实际上下位置排序；否则沿用上一次记录，
    // 避免编辑图元卡隐藏时无法判断它的位置而把顺序写丢。
    if (m_window->dockWidgetArea(m_layerDock) != Qt::RightDockWidgetArea ||
        m_window->dockWidgetArea(m_itemDock) != Qt::RightDockWidgetArea || !m_layerDock->isVisible() ||
        !m_itemDock->isVisible()) {
        return m_dockOrder;
    }
    const int layerTop = m_layerDock->mapTo(m_window, QPoint(0, 0)).y();
    const int itemTop = m_itemDock->mapTo(m_window, QPoint(0, 0)).y();
    if (layerTop == itemTop) {
        return m_dockOrder;
    }
    QStringList order;
    if (layerTop < itemTop) {
        order << QStringLiteral("LayerDock") << QStringLiteral("ItemDock");
    } else {
        order << QStringLiteral("ItemDock") << QStringLiteral("LayerDock");
    }
    return order;
}

void DockPanelController::applyDockLayout()
{
    if (!m_layerDock || !m_itemDock) {
        return;
    }
    m_applyingDockLayout = true;

    // 默认图层卡在上、编辑图元卡在下；用户在界面上拖拽调整过则按设置文件恢复。
    QStringList order;
    for (const QString& name : m_window->m_settings.dockOrder) {
        if ((name == QStringLiteral("ItemDock") || name == QStringLiteral("LayerDock")) && !order.contains(name)) {
            order.append(name);
        }
    }
    if (order.size() != 2) {
        order = {QStringLiteral("LayerDock"), QStringLiteral("ItemDock")};
    }
    m_dockOrder = order;
    m_window->m_settings.dockOrder = order;

    QDockWidget* first = order.first() == QStringLiteral("LayerDock") ? m_layerDock : m_itemDock;
    QDockWidget* second = first == m_layerDock ? m_itemDock : m_layerDock;

    // 重新排布两张卡：同一右侧区域，竖直切分，first 在上、second 在下。
    m_window->removeDockWidget(m_layerDock);
    m_window->removeDockWidget(m_itemDock);
    m_window->addDockWidget(Qt::RightDockWidgetArea, first);
    m_window->addDockWidget(Qt::RightDockWidgetArea, second);
    m_window->splitDockWidget(first, second, Qt::Vertical);
    // 默认给一个合适高度，不占满右侧整列；上限已在建卡时限定。
    m_window->resizeDocks({m_layerDock, m_itemDock}, {300, 420}, Qt::Vertical);
    QTimer::singleShot(0, m_window, [this]() {
        // 窗口显示后再定一次初始高度：构造阶段窗口还没有实际尺寸，切分比例会失真。
        m_window->resizeDocks({m_layerDock, m_itemDock}, {300, 420}, Qt::Vertical);
    });

    // 拖动标题栏调整卡片相对顺序时记录新顺序（同一区域内换位不会有位置信号，
    // 用移动事件兜底，延迟到本轮布局结束后再读实际坐标）。
    m_layerDock->installEventFilter(m_window);
    m_itemDock->installEventFilter(m_window);
    QObject::connect(m_layerDock, &QDockWidget::dockLocationChanged, m_window, [this](Qt::DockWidgetArea) {
        QTimer::singleShot(0, m_window, [this]() { rememberDockOrder(); });
    });
    QObject::connect(m_itemDock, &QDockWidget::dockLocationChanged, m_window, [this](Qt::DockWidgetArea) {
        QTimer::singleShot(0, m_window, [this]() { rememberDockOrder(); });
    });

    m_layerDock->setVisible(true);
    m_itemDock->setVisible(false);
    m_applyingDockLayout = false;
}

void DockPanelController::rememberDockOrder()
{
    if (m_applyingDockLayout || !m_layerDock || !m_itemDock) {
        return;
    }
    const QStringList order = currentDockOrder();
    if (order.isEmpty() || order == m_dockOrder) {
        return;
    }
    m_dockOrder = order;
    m_window->m_settings.dockOrder = order;
    m_window->m_settings.save();
}

void DockPanelController::handleMoveEvent(QObject* watched, QEvent* event)
{
    if ((watched == m_layerDock || watched == m_itemDock) && event->type() == QEvent::Move) {
        // 卡片被拖到新位置后，等布局稳定再读上下关系并落盘。
        QTimer::singleShot(0, m_window, [this]() { rememberDockOrder(); });
    }
}

void DockPanelController::updateItemDockVisibility()
{
    if (!m_itemDock || !m_window->m_scene || !m_window->m_view) {
        return;
    }
    const bool hasSelection = !m_window->m_scene->selectedCanvasItems().isEmpty();
    if (m_itemDock->isVisible() != hasSelection) {
        m_itemDock->setVisible(hasSelection);
    }
    // 选中出现 / 清空隐藏时都重新按默认比例排一次，避免卡片被拉到整列高。
    if (!m_itemDock->isVisible() || m_itemDock->height() >= 300) {
        m_window->resizeDocks({m_layerDock, m_itemDock}, {300, 420}, Qt::Vertical);
    }
}
