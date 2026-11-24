#include "app/FullScreenController.h"

#include "app/DockPanelController.h"
#include "app/FullScreenBar.h"
#include "canvas/CanvasView.h"
#include "ui/mainwindow/MainWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QSet>
#include <QStatusBar>
#include <QStyle>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QWidget>

#include <utility>

namespace {

// 菜单里的动作才是主入口；子菜单要递归展开，否则二级键位在全屏期全部失效。
void CollectMenuActions(const QMenu* menu, QList<QAction*>& collected)
{
    if (!menu) {
        return;
    }
    for (QAction* action : menu->actions()) {
        if (action->menu()) {
            CollectMenuActions(action->menu(), collected);
        } else {
            collected.append(action);
        }
    }
}

} // namespace

FullScreenController::FullScreenController(MainWindow* window) : m_window(window) {}

// FullScreenBar 只有本文件看得到完整类型，析构必须放在这里，否则别处的 unique_ptr 无法实例化。
FullScreenController::~FullScreenController() = default;

QToolBar* FullScreenController::MainToolBar() const
{
    return m_window ? m_window->findChild<QToolBar*>(QStringLiteral("MainToolBar")) : nullptr;
}

QDockWidget* FullScreenController::ViewModeDock() const
{
    return m_window && m_window->m_docks ? m_window->m_docks->ViewModeDock() : nullptr;
}

void FullScreenController::Toggle()
{
    if (m_active) {
        Exit();
    } else {
        Enter();
    }
}

void FullScreenController::ExitIfActive()
{
    if (m_active) {
        Exit();
    }
}

void FullScreenController::Enter()
{
    if (m_active || !m_window || !m_window->m_view) {
        return;
    }
    // 模态窗开着时不进全屏：全屏后模态窗会盖满整屏，连 F11 都拿不到键盘。
    if (QApplication::activeModalWidget()) {
        return;
    }
    m_window->m_view->FinishCurrentStroke();
    RememberChrome();
    m_active = true;
    m_window->menuBar()->hide();
    m_window->statusBar()->hide();
    if (QToolBar* mainBar = MainToolBar()) {
        mainBar->hide();
    }
    if (QDockWidget* modeDock = ViewModeDock()) {
        modeDock->hide();
    }
    RegisterActionShortcuts();
    m_window->showFullScreen();
    if (!m_bar) {
        m_bar = std::make_unique<FullScreenBar>(m_window);
    }
    m_bar->SyncFromWindow();
    PlaceBar();
    m_bar->show();
    m_bar->raise();
    // 面板摘成悬浮卡要等窗口真正是全屏几何之后再摆位，否则按旧尺寸算出错误位置。
    if (m_window->m_docks) {
        m_window->m_docks->EnterFullScreenLayout();
    }
    QTimer::singleShot(0, m_window, [this]() {
        if (m_active) {
            PlaceBar();
            if (m_window->m_docks) {
                m_window->m_docks->PlaceFullScreenPanels();
            }
        }
    });
    if (m_window->m_fullScreenAct) {
        m_window->m_fullScreenAct->setChecked(true);
    }
}

void FullScreenController::Exit()
{
    if (!m_active) {
        return;
    }
    // 先落状态再动窗口：showNormal 会触发 WindowStateChange，避免从兜底路径再进来一次。
    m_active = false;
    UnregisterActionShortcuts();
    if (m_bar) {
        m_bar->hide();
    }
    if (m_window->m_docks) {
        m_window->m_docks->ExitFullScreenLayout();
    }
    RestoreChrome(true);
    if (m_window->m_fullScreenAct) {
        m_window->m_fullScreenAct->setChecked(false);
    }
}

void FullScreenController::HandleWindowStateChange()
{
    if (!m_active || !m_window || m_window->isFullScreen()) {
        return;
    }
    // 被窗口管理器踢出全屏：界面已经回到普通窗口，只还原被隐藏的控件，别再 showNormal。
    m_active = false;
    UnregisterActionShortcuts();
    if (m_bar) {
        m_bar->hide();
    }
    if (m_window->m_docks) {
        m_window->m_docks->ExitFullScreenLayout();
    }
    RestoreChrome(false);
    if (m_window->m_fullScreenAct) {
        m_window->m_fullScreenAct->setChecked(false);
    }
}

void FullScreenController::RememberChrome()
{
    m_menuBarVisible = m_window->menuBar()->isVisible();
    m_statusBarVisible = m_window->statusBar()->isVisible();
    if (QToolBar* mainBar = MainToolBar()) {
        m_mainToolBarVisible = mainBar->isVisible();
    }
    if (QDockWidget* modeDock = ViewModeDock()) {
        m_viewModeDockVisible = modeDock->isVisible();
    }
    m_wasMaximized = m_window->isMaximized();
    m_geometry = m_window->geometry();
    m_chromeValid = true;
}

void FullScreenController::RestoreChrome(bool restoreWindowGeometry)
{
    if (!m_chromeValid) {
        return;
    }
    if (restoreWindowGeometry) {
        if (m_wasMaximized) {
            m_window->showMaximized();
        } else {
            m_window->showNormal();
            m_window->setGeometry(m_geometry);
        }
    }
    m_window->menuBar()->setVisible(m_menuBarVisible);
    m_window->statusBar()->setVisible(m_statusBarVisible);
    if (QToolBar* mainBar = MainToolBar()) {
        mainBar->setVisible(m_mainToolBarVisible);
    }
    if (QDockWidget* modeDock = ViewModeDock()) {
        modeDock->setVisible(m_viewModeDockVisible);
    }
}

void FullScreenController::PlaceBar()
{
    if (!m_bar || !m_window) {
        return;
    }
    // 顶部留白至少一个标题栏高度：贴到屏幕最上沿会跟窗口管理器的手势 / 悬浮标题条打架。
    const int titleBar = m_window->style()->pixelMetric(QStyle::PM_TitleBarHeight, nullptr, m_window);
    const int top = qMax(32, titleBar + 8);
    m_bar->PlaceAt(m_window->width() / 2, top);
}

void FullScreenController::HandleWindowEvent(QObject* watched, QEvent* event)
{
    if (!m_active || watched != m_window) {
        return;
    }
    if (event->type() == QEvent::Resize) {
        PlaceBar();
        if (m_window->m_docks) {
            m_window->m_docks->PlaceFullScreenPanels();
        }
        if (m_bar) {
            m_bar->raise();
        }
        return;
    }
    // 数字键 0–9 只在全屏期落到悬浮工具栏的图形按钮上；输入框聚焦时让位给输入。
    if (event->type() == QEvent::KeyPress) {
        HandleKeyPress(static_cast<QKeyEvent*>(event));
    }
}

void FullScreenController::HandleKeyPress(QKeyEvent* event)
{
    if (!event || !m_bar || !m_bar->isVisible() || event->modifiers() != Qt::NoModifier) {
        return;
    }
    const int key = event->key();
    if (key < Qt::Key_0 || key > Qt::Key_9) {
        return;
    }
    // 正在输入内容（描线宽度等输入框）时数字键属于输入，不能抢去切图形。
    // 全屏窗口不一定处于激活态，先看窗口自己的焦点链，再退回应用级焦点。
    QWidget* focus = m_window ? m_window->focusWidget() : nullptr;
    if (!focus) {
        focus = QApplication::focusWidget();
    }
    if (qobject_cast<QAbstractSpinBox*>(focus) || qobject_cast<QLineEdit*>(focus) ||
        qobject_cast<QTextEdit*>(focus) || qobject_cast<QPlainTextEdit*>(focus) ||
        qobject_cast<QComboBox*>(focus)) {
        return;
    }
    if (m_bar->TriggerDigit(key - Qt::Key_0)) {
        event->accept();
    }
}

void FullScreenController::RegisterActionShortcuts()
{
    m_extraShortcutActions.clear();
    QList<QAction*> candidates;
    for (QAction* action : m_window->menuBar()->actions()) {
        if (action->menu()) {
            CollectMenuActions(action->menu(), candidates);
        }
    }
    for (QToolBar* bar : m_window->findChildren<QToolBar*>()) {
        for (QAction* action : bar->actions()) {
            candidates.append(action);
        }
    }
    if (m_window->m_modeGroup) {
        candidates.append(m_window->m_modeGroup->actions());
    }

    QSet<QAction*> seen;
    for (QAction* action : std::as_const(candidates)) {
        if (!action || action->shortcut().isEmpty() || action->shortcutContext() == Qt::WidgetShortcut) {
            continue;
        }
        if (seen.contains(action)) {
            continue;
        }
        seen.insert(action);
        m_window->m_view->addAction(action);
        m_extraShortcutActions.append(action);
    }
}

void FullScreenController::UnregisterActionShortcuts()
{
    for (QAction* action : std::as_const(m_extraShortcutActions)) {
        m_window->m_view->removeAction(action);
    }
    m_extraShortcutActions.clear();
}
