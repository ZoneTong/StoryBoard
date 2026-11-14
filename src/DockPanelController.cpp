#include "DockPanelController.h"

#include "CanvasScene.h"
#include "ItemDockPanel.h"
#include "LayerPanel.h"
#include "MainWindow.h"

#include <QDockWidget>
#include <QEvent>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>

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
    // 卡片不铺满右侧：给出最小高度，实际高度由上方模式卡之外的剩余空间决定。
    m_layerDock->setMinimumHeight(200);

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
    m_window->addDockWidget(Qt::LeftDockWidgetArea, m_itemDock);
    // 编辑图元卡固定在左侧，高度限定在合适区间，避免单独占满整列。
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

void DockPanelController::createViewModeDock()
{
    m_viewModeDock = new QDockWidget(MainWindow::tr("View mode"), m_window);
    m_viewModeDock->setObjectName(QStringLiteral("ViewModeDock"));
    m_viewModeDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    // 不提供关闭按钮，避免用户误关后没有入口重新打开模式卡。
    m_viewModeDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    m_window->addDockWidget(Qt::RightDockWidgetArea, m_viewModeDock);
    m_viewModeDock->setMinimumHeight(90);
    m_viewModeDock->setMaximumHeight(130);
}

void DockPanelController::attachViewModeToolBar()
{
    if (!m_viewModeDock) {
        return;
    }
    auto* modeBar = m_window->findChild<QToolBar*>(QStringLiteral("ModeToolBar"));
    if (!modeBar) {
        return;
    }
    // 模式栏原先由 UiBuilderController 建在左侧工具条；这里把它从工具条区域摘下，
    // 作为右上卡片的唯一内容，保留原动作、图标和翻译，不改动大文件的构建。
    m_window->removeToolBar(modeBar);
    modeBar->setOrientation(Qt::Horizontal);
    m_viewModeDock->setWidget(modeBar);
}

QStringList DockPanelController::currentDockOrder() const
{
    if (!m_viewModeDock || !m_layerDock) {
        return {};
    }
    // 右侧两张卡都显示时按实际上下位置排序；否则沿用上一次记录，
    // 避免其中一张被隐藏时无法判断位置而把顺序写丢。
    if (m_window->dockWidgetArea(m_viewModeDock) != Qt::RightDockWidgetArea ||
        m_window->dockWidgetArea(m_layerDock) != Qt::RightDockWidgetArea || !m_viewModeDock->isVisible() ||
        !m_layerDock->isVisible()) {
        return m_dockOrder;
    }
    const int viewTop = m_viewModeDock->mapTo(m_window, QPoint(0, 0)).y();
    const int layerTop = m_layerDock->mapTo(m_window, QPoint(0, 0)).y();
    if (viewTop == layerTop) {
        return m_dockOrder;
    }
    QStringList order;
    if (viewTop < layerTop) {
        order << QStringLiteral("ViewModeDock") << QStringLiteral("LayerDock");
    } else {
        order << QStringLiteral("LayerDock") << QStringLiteral("ViewModeDock");
    }
    return order;
}

void DockPanelController::applyDockLayout()
{
    if (!m_viewModeDock || !m_layerDock || !m_itemDock) {
        return;
    }
    m_applyingDockLayout = true;

    // 默认视图模式卡在上、图层卡在下；用户拖拽调整过则按设置文件恢复右侧顺序。
    QStringList order;
    for (const QString& name : m_window->m_settings.dockOrder) {
        if ((name == QStringLiteral("ViewModeDock") || name == QStringLiteral("LayerDock")) &&
            !order.contains(name)) {
            order.append(name);
        }
    }
    if (order.size() != 2) {
        order = {QStringLiteral("ViewModeDock"), QStringLiteral("LayerDock")};
    }
    m_dockOrder = order;
    m_window->m_settings.dockOrder = order;

    QDockWidget* first = order.first() == QStringLiteral("ViewModeDock") ? m_viewModeDock : m_layerDock;
    QDockWidget* second = first == m_viewModeDock ? m_layerDock : m_viewModeDock;

    // 编辑图元卡固定左侧；视图模式 / 图层卡在右侧竖直切分，first 在上、second 在下。
    m_window->removeDockWidget(m_itemDock);
    m_window->removeDockWidget(m_viewModeDock);
    m_window->removeDockWidget(m_layerDock);
    m_window->addDockWidget(Qt::LeftDockWidgetArea, m_itemDock);
    m_window->addDockWidget(Qt::RightDockWidgetArea, first);
    m_window->addDockWidget(Qt::RightDockWidgetArea, second);
    m_window->splitDockWidget(first, second, Qt::Vertical);
    // 右上模式卡保持紧凑，右下图层卡吃掉剩余高度，落在右下角。
    m_window->resizeDocks({m_viewModeDock, m_layerDock}, {100, 1000}, Qt::Vertical);
    QTimer::singleShot(0, m_window, [this]() {
        // 窗口显示后再定一次初始高度：构造阶段窗口还没有实际尺寸，切分比例会失真。
        m_window->resizeDocks({m_viewModeDock, m_layerDock}, {100, 1000}, Qt::Vertical);
    });

    // 拖动标题栏调整右侧卡片相对顺序时记录新顺序（同一区域内换位不会有位置信号，
    // 用移动事件兜底，延迟到本轮布局结束后再读实际坐标）。
    m_viewModeDock->installEventFilter(m_window);
    m_layerDock->installEventFilter(m_window);
    QObject::connect(m_viewModeDock, &QDockWidget::dockLocationChanged, m_window, [this](Qt::DockWidgetArea) {
        QTimer::singleShot(0, m_window, [this]() { rememberDockOrder(); });
    });
    QObject::connect(m_layerDock, &QDockWidget::dockLocationChanged, m_window, [this](Qt::DockWidgetArea) {
        QTimer::singleShot(0, m_window, [this]() { rememberDockOrder(); });
    });

    m_viewModeDock->setVisible(true);
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
    if ((watched == m_viewModeDock || watched == m_layerDock) && event->type() == QEvent::Move) {
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
}
