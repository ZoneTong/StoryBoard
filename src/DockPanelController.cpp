#include "DockPanelController.h"

#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CanvasView.h"
#include "LayerPanel.h"
#include "MainWindow.h"
#include "PropertiesPanel.h"

#include <QColor>
#include <QDockWidget>
#include <QEvent>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>

DockPanelController::DockPanelController(MainWindow* window) : m_window(window)
{
    m_propertiesDockStateTimer = new QTimer(window);
    m_propertiesDockStateTimer->setSingleShot(true);
    m_propertiesDockStateTimer->setInterval(300);
    QObject::connect(m_propertiesDockStateTimer, &QTimer::timeout, window,
                     [this]() { RememberPropertiesPanelDockState(); });
}

void DockPanelController::CreateLayerDock()
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

void DockPanelController::CreatePropertiesPanelDock()
{
    m_propertiesDock = new QDockWidget(MainWindow::tr("Edit panel"), m_window);
    m_propertiesDock->setObjectName(QStringLiteral("PropertiesPanelDock"));
    m_propertiesDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_propertiesDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable |
                                  QDockWidget::DockWidgetClosable);

    m_propertiesPanel = new PropertiesPanel(m_window->m_scene, m_propertiesDock);
    m_propertiesDock->setWidget(m_propertiesPanel);
    m_window->addDockWidget(Qt::LeftDockWidgetArea, m_propertiesDock);
    // 编辑面板固定在左侧，高度限定在合适区间，避免单独占满整列。
    m_propertiesDock->setMinimumHeight(240);
    m_propertiesDock->setMaximumHeight(620);
    // 宽度按内容算一次作为启动宽度；之后不随选中项增减自动变化，用户可手动拖拽调整。
    ApplyPropertiesPanelDockWidth(m_propertiesPanel->PreferredWidth(), true);

    QObject::connect(m_propertiesPanel, &PropertiesPanel::statusMessage, m_window,
                     [this](const QString& message) { m_window->statusBar()->showMessage(message, 3000); });
    // 卡片宽度只在启动时确定一次，选中变化不再联动，避免卡片来回跳动。
    // 画笔与描边共用同一套颜色 / 宽度：卡片里改描边后，之后画出的笔画立即沿用新值。
    QObject::connect(m_propertiesPanel, &PropertiesPanel::strokeColorPicked, m_window,
                     [this](const QColor& color) { m_window->SetBrushColor(color, true); });
    QObject::connect(m_propertiesPanel, &PropertiesPanel::borderWidthPicked, m_window,
                     [this](qreal width) { m_window->SetBrushWidthUi(qBound(1, qRound(width), 64)); });

    // 编辑面板默认停靠左侧并常驻显示；首次启动没有保存位置时由 applyDockLayout 固定到左侧。
    m_propertiesDock->setVisible(true);
    m_propertiesDock->installEventFilter(m_window);
    QObject::connect(m_propertiesDock, &QDockWidget::topLevelChanged, m_window, [this](bool floating) {
        if (!floating) {
            // 悬浮卡拖回主窗口后重新固定，恢复常驻显示。
            m_propertiesDock->setVisible(true);
        }
        // 停靠 / 悬浮切换后只约束宽度范围，保持用户当前宽度。
        ApplyPropertiesPanelDockWidth(
            m_propertiesPanel ? m_propertiesPanel->PreferredWidth() : m_propertiesDock->minimumWidth(), false);
        SchedulePropertiesPanelDockStateSave();
    });
    QObject::connect(m_propertiesDock, &QDockWidget::dockLocationChanged, m_window,
                     [this](Qt::DockWidgetArea) { SchedulePropertiesPanelDockStateSave(); });
}

void DockPanelController::CreateViewModeDock()
{
    // 卡片标题在构造时取当前模式，之后随模式切换刷新，不再固定写死「查看模式」。
    const ToolMode initialMode = m_window->m_view ? m_window->m_view->mode() : ToolMode::VIEW;
    m_viewModeDock = new QDockWidget(ToolModeName(initialMode), m_window);
    m_viewModeDock->setObjectName(QStringLiteral("ViewModeDock"));
    m_viewModeDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    // 不提供关闭按钮，避免用户误关后没有入口重新打开模式卡。
    m_viewModeDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    m_window->addDockWidget(Qt::RightDockWidgetArea, m_viewModeDock);
    m_viewModeDock->setMinimumHeight(90);
    m_viewModeDock->setMaximumHeight(130);
    if (m_window->m_view) {
        QObject::connect(m_window->m_view, &CanvasView::modeChanged, m_window, [this](ToolMode mode) {
            if (m_viewModeDock) {
                m_viewModeDock->setWindowTitle(ToolModeName(mode));
            }
        });
    }
}

void DockPanelController::AttachViewModeToolBar()
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

QStringList DockPanelController::CurrentDockOrder() const
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

void DockPanelController::ApplyDockLayout()
{
    if (!m_viewModeDock || !m_layerDock || !m_propertiesDock) {
        return;
    }
    m_applyingDockLayout = true;

    // 默认视图模式卡在上、图层卡在下；用户拖拽调整过则按设置文件恢复右侧顺序。
    QStringList order;
    for (const QString& name : m_window->m_settings.dockOrder) {
        if ((name == QStringLiteral("ViewModeDock") || name == QStringLiteral("LayerDock")) && !order.contains(name)) {
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

    // 编辑面板由 restorePropertiesPanelDockState 按设置恢复：首次启动没有保存位置时停靠左侧。
    m_window->removeDockWidget(m_propertiesDock);
    m_window->removeDockWidget(m_viewModeDock);
    m_window->removeDockWidget(m_layerDock);
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
        QTimer::singleShot(0, m_window, [this]() { RememberDockOrder(); });
    });
    QObject::connect(m_layerDock, &QDockWidget::dockLocationChanged, m_window, [this](Qt::DockWidgetArea) {
        QTimer::singleShot(0, m_window, [this]() { RememberDockOrder(); });
    });

    m_viewModeDock->setVisible(true);
    m_layerDock->setVisible(true);

    // 编辑面板：首次启动固定左侧；之后按设置恢复停靠侧或悬浮位置。
    RestorePropertiesPanelDockState();

    m_applyingDockLayout = false;
}

void DockPanelController::ApplyPropertiesPanelDockWidth(int width, bool resizeNow)
{
    if (!m_propertiesDock || !m_propertiesPanel) {
        return;
    }
    // 宽度只在启动时按内容确定一次；之后保留用户手动拖拽的结果，只约束上下限。
    const int bounded = qBound(260, width, 360);
    m_propertiesDock->setMinimumWidth(260);
    m_propertiesDock->setMaximumWidth(360);
    if (!resizeNow) {
        return;
    }
    if (m_propertiesDock->isFloating()) {
        if (m_propertiesDock->width() < bounded) {
            m_propertiesDock->resize(bounded, m_propertiesDock->height());
        }
        return;
    }
    if (m_propertiesDock->width() != bounded) {
        m_window->resizeDocks({m_propertiesDock}, {bounded}, Qt::Horizontal);
    }
}

void DockPanelController::RestorePropertiesPanelDockState()
{
    if (!m_propertiesDock) {
        return;
    }
    const AppSettings& settings = m_window->m_settings;
    const bool saved = settings.propertiesDockStateSaved;
    const bool floating = saved && settings.propertiesDockFloating;
    const Qt::DockWidgetArea area = saved && settings.propertiesDockArea == QLatin1String("right")
                                        ? Qt::RightDockWidgetArea
                                        : Qt::LeftDockWidgetArea;

    // 先固定回停靠区，再决定是否摘成悬浮窗口；首次没有保存记录时这就是最终状态。
    m_propertiesDock->setFloating(false);
    m_window->addDockWidget(area, m_propertiesDock);

    if (!floating) {
        m_propertiesDock->setVisible(true);
        ApplyPropertiesPanelDockWidth(
            m_propertiesPanel ? m_propertiesPanel->PreferredWidth() : m_propertiesDock->width(), false);
        return;
    }

    m_propertiesDock->setFloating(true);
    const bool geometryRestored = !settings.propertiesDockGeometry.isEmpty() &&
                                  m_propertiesDock->restoreGeometry(settings.propertiesDockGeometry);
    // 恢复的悬浮几何只做宽度范围约束，保留用户上次的宽度。
    ApplyPropertiesPanelDockWidth(m_propertiesPanel ? m_propertiesPanel->PreferredWidth() : m_propertiesDock->width(),
                                  false);
    QTimer::singleShot(0, m_window, [this, geometryRestored]() {
        if (!m_propertiesDock || !m_propertiesDock->isFloating() || geometryRestored) {
            return;
        }
        // 没有可用的悬浮几何时落在画布左上角，尺寸限制在卡片自身区间。
        const QRect frame = m_window->frameGeometry();
        const int width = qBound(260, frame.width() / 4, 360);
        const int height = qBound(320, frame.height() - 200, 560);
        m_propertiesDock->resize(width, height);
        m_propertiesDock->move(frame.left() + 24, frame.top() + 88);
    });
    m_propertiesDock->setVisible(true);
    UpdatePropertiesPanelDockVisibility();
}

void DockPanelController::RememberPropertiesPanelDockState()
{
    if (!m_propertiesDock || m_applyingDockLayout) {
        return;
    }
    AppSettings& settings = m_window->m_settings;
    settings.propertiesDockStateSaved = true;
    settings.propertiesDockFloating = m_propertiesDock->isFloating();
    settings.propertiesDockArea = m_window->dockWidgetArea(m_propertiesDock) == Qt::RightDockWidgetArea
                                      ? QStringLiteral("right")
                                      : QStringLiteral("left");
    settings.propertiesDockGeometry = m_propertiesDock->saveGeometry();
    settings.Save();
}

void DockPanelController::SchedulePropertiesPanelDockStateSave()
{
    if (!m_propertiesDockStateTimer || m_applyingDockLayout) {
        return;
    }
    m_propertiesDockStateTimer->start();
}

void DockPanelController::RememberDockOrder()
{
    if (m_applyingDockLayout || !m_layerDock || !m_propertiesDock) {
        return;
    }
    const QStringList order = CurrentDockOrder();
    if (order.isEmpty() || order == m_dockOrder) {
        return;
    }
    m_dockOrder = order;
    m_window->m_settings.dockOrder = order;
    m_window->m_settings.Save();
}

void DockPanelController::HandleMoveEvent(QObject* watched, QEvent* event)
{
    if ((watched == m_viewModeDock || watched == m_layerDock) && event->type() == QEvent::Move) {
        // 卡片被拖到新位置后，等布局稳定再读上下关系并落盘。
        QTimer::singleShot(0, m_window, [this]() { RememberDockOrder(); });
    } else if (watched == m_propertiesDock && (event->type() == QEvent::Move || event->type() == QEvent::Resize) &&
               m_propertiesDock->isFloating()) {
        // 悬浮卡拖动或缩放后延迟保存位置，避免拖动期间反复写文件。
        SchedulePropertiesPanelDockStateSave();
    }
}

void DockPanelController::UpdatePropertiesPanelDockVisibility()
{
    if (!m_propertiesDock || !m_window->m_scene || !m_window->m_view) {
        return;
    }
    // 停靠在主窗口里时卡片常驻，选中状态变化不动整张卡片；
    // 只有悬浮窗口才跟随选中状态：有选中显示、清空选中隐藏。
    if (!m_propertiesDock->isFloating()) {
        return;
    }
    const bool hasSelection = !m_window->m_scene->SelectedCanvasItems().isEmpty();
    if (m_propertiesDock->isVisible() != hasSelection) {
        m_propertiesDock->setVisible(hasSelection);
    }
}
