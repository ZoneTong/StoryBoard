#include "app/CanvasTabsController.h"

#include "app/CanvasSession.h"
#include "app/LifecycleLog.h"
#include "app/Settings.h"
#include "canvas/CanvasView.h"
#include "items/CanvasItem.h"
#include "scene/CanvasScene.h"
#include "ui/mainwindow/MainWindow.h"

#include <QFileInfo>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QUndoStack>
#include <utility>

CanvasTabsController::CanvasTabsController(MainWindow* window) : m_window(window)
{
    m_tabs = new QTabWidget(m_window);
    m_tabs->setObjectName(QStringLiteral("CanvasTabs"));
    m_tabs->setDocumentMode(true);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setElideMode(Qt::ElideRight);
    m_tabs->setUsesScrollButtons(true);
    if (m_tabs->tabBar()) {
        m_tabs->tabBar()->setObjectName(QStringLiteral("CanvasTabBar"));
        m_tabs->tabBar()->setExpanding(false);
    }

    QObject::connect(m_tabs, &QTabWidget::currentChanged, m_window, [this](int index) { ActivateIndex(index); });
    QObject::connect(m_tabs, &QTabWidget::tabCloseRequested, m_window, [this](int index) {
        CanvasSession* session = SessionAt(index);
        if (session) {
            m_window->CloseCanvasSession(session);
        }
    });
}

void CanvasTabsController::AdoptSession(CanvasSession* session)
{
    if (!session) {
        return;
    }
    m_sessions.append(session);
    m_untitledCount = qMax(m_untitledCount, session->UntitledNo());
    m_canvasCount = qMax(m_canvasCount, session->CanvasNo());
    if (m_current == nullptr) {
        m_current = session;
    }
    m_tabs->addTab(session->View(), QString());
    UpdateTabLabel(session);
    if (m_window) {
        m_window->AttachSession(session);
    }
    if (m_sessions.size() == 1) {
        // addTab 会触发 currentChanged，此时 m_current 已指向会话，可直接完成首次绑定。
        m_tabs->setCurrentIndex(0);
    }
}

CanvasSession* CanvasTabsController::CreateSession(bool activate)
{
    CanvasScene* scene = new CanvasScene;
    scene->ApplyCanvasSize(m_window->Settings().canvasSize);
    scene->SetShowGrid(m_window->Settings().showGrid);

    QUndoStack* undo = new QUndoStack;
    undo->setUndoLimit(300);
    scene->SetUndoStack(undo);

    CanvasView* view = new CanvasView(scene);
    view->SetBrushColor(m_window->BrushColor());
    view->SetBrushWidth(m_window->BrushWidth());
    view->SetStrokeSmoothing(m_window->Settings().strokeSmoothing);
    view->SetCanvasColor(m_window->Settings().canvasColor);
    view->SetMode(ToolMode::VIEW);

    CanvasSession* session = new CanvasSession(m_window, scene, view, undo, ++m_untitledCount, ++m_canvasCount);
    AdoptSession(session);
    UpdateTabLabel(session);
    LifecycleLog::LogCanvas(session->LogUid(), session->LogLabel(), QStringLiteral("New"));
    if (activate) {
        m_tabs->setCurrentWidget(view);
    }
    return session;
}

CanvasSession* CanvasTabsController::SessionAt(int index) const
{
    if (index < 0 || index >= m_sessions.size()) {
        return nullptr;
    }
    return m_sessions.at(index);
}

int CanvasTabsController::IndexOf(CanvasSession* session) const
{
    return m_sessions.indexOf(session);
}

CanvasSession* CanvasTabsController::FindByPath(const QString& path) const
{
    if (path.isEmpty()) {
        return nullptr;
    }
    for (CanvasSession* session : m_sessions) {
        if (session && session->FilePath() == path) {
            return session;
        }
    }
    return nullptr;
}

void CanvasTabsController::SetCurrent(CanvasSession* session)
{
    if (!session) {
        return;
    }
    const int index = m_sessions.indexOf(session);
    if (index >= 0) {
        m_tabs->setCurrentIndex(index);
    }
}

void CanvasTabsController::ActivateIndex(int index)
{
    if (m_switching) {
        return;
    }
    CanvasSession* session = SessionAt(index);
    if (!session || session == m_current) {
        return;
    }
    m_switching = true;
    m_current = session;
    if (m_window) {
        m_window->ActivateSession(session);
    }
    LifecycleLog::LogCanvas(session->LogUid(), session->LogLabel(), QStringLiteral("Switch"));
    m_switching = false;
}

void CanvasTabsController::Next()
{
    if (m_sessions.size() < 2) {
        return;
    }
    const int index = m_sessions.indexOf(m_current);
    m_tabs->setCurrentIndex((index + 1) % m_sessions.size());
}

void CanvasTabsController::Previous()
{
    if (m_sessions.size() < 2) {
        return;
    }
    const int index = m_sessions.indexOf(m_current);
    m_tabs->setCurrentIndex((index - 1 + m_sessions.size()) % m_sessions.size());
}

void CanvasTabsController::SwitchToNumber(int oneBasedNumber)
{
    const int index = oneBasedNumber - 1;
    if (index >= 0 && index < m_sessions.size()) {
        m_tabs->setCurrentIndex(index);
    }
}

void CanvasTabsController::RemoveSession(CanvasSession* session)
{
    const int index = m_sessions.indexOf(session);
    if (index < 0) {
        return;
    }
    const bool wasCurrent = session == m_current;
    // 先把「当前场景 / 视图」挪到接替者，再删旧会话：否则窗口的 m_view 会指向
    // 已释放的视图，构造期排队的 FitAll 之类延迟调用就会踩空。
    if (wasCurrent && m_sessions.size() == 1) {
        CreateSession(true);
    } else if (wasCurrent) {
        CanvasSession* next = m_sessions.at(qBound(0, index - 1, m_sessions.size() - 2));
        m_current = next;
        if (m_window) {
            m_window->ActivateSession(next);
        }
    }

    m_switching = true;
    m_tabs->removeTab(index);
    m_sessions.removeAt(index);
    m_switching = false;
    if (session == m_current) {
        m_current = nullptr;
    }
    const QString logLabel = session->LogLabel();
    const QString logUid = session->LogUid();
    LifecycleLog::LogCanvas(logUid, logLabel, QStringLiteral("Close"), session->FilePath());
    if (session->Scene()) {
        // 关闭时只为「当前状态还没进文件」的图元记销毁：已保存图元的销毁属于
        // 正常卸载，写进日志只会淹没真正需要复现的未落盘改动。
        for (CanvasItem* item : session->Scene()->CanvasItems()) {
            if (!session->Scene()->IsItemSavedState(item)) {
                LifecycleLog::LogItem(logUid, logLabel, QStringLiteral("Destroy (unsaved)"), *item);
            }
        }
    }
    delete session;

    if (m_sessions.isEmpty()) {
        CreateSession(true);
        return;
    }
    if (wasCurrent) {
        const int targetIndex = qBound(0, index - 1, m_sessions.size() - 1);
        m_tabs->setCurrentIndex(targetIndex);
    } else if (m_current) {
        m_tabs->setCurrentWidget(m_current->View());
    }
}

void CanvasTabsController::ClearAll()
{
    m_switching = true;
    for (CanvasSession* session : std::as_const(m_sessions)) {
        if (!session) {
            continue;
        }
        const int index = m_tabs->indexOf(session->View());
        if (index >= 0) {
            m_tabs->removeTab(index);
        }
        // 窗口关闭也会销毁全部会话：同样只为未落盘的图元留失效记录。
        const QString logLabel = session->LogLabel();
        const QString logUid = session->LogUid();
        LifecycleLog::LogCanvas(logUid, logLabel, QStringLiteral("Close"), session->FilePath());
        if (session->Scene()) {
            for (CanvasItem* item : session->Scene()->CanvasItems()) {
                if (!session->Scene()->IsItemSavedState(item)) {
                    LifecycleLog::LogItem(logUid, logLabel, QStringLiteral("Destroy (unsaved)"), *item);
                }
            }
        }
        delete session;
    }
    m_sessions.clear();
    m_current = nullptr;
    m_switching = false;
}

void CanvasTabsController::UpdateTabLabel(CanvasSession* session)
{
    if (!session || !m_tabs) {
        return;
    }
    const int index = m_tabs->indexOf(session->View());
    if (index < 0) {
        return;
    }
    QString label = session->DisplayName();
    if (session->Scene()) {
        session->Scene()->SetLogLabel(session->LogLabel());
        session->Scene()->SetLogUid(session->LogUid());
    }
    if (session->HasUnsavedChanges()) {
        label.prepend(QStringLiteral("• "));
    }
    m_tabs->setTabText(index, label);
    const QString path = session->FilePath();
    m_tabs->setTabToolTip(index, path.isEmpty() ? label : QFileInfo(path).absoluteFilePath());
}

void CanvasTabsController::RefreshLabels()
{
    for (CanvasSession* session : std::as_const(m_sessions)) {
        UpdateTabLabel(session);
    }
}

void CanvasTabsController::SetTabBarVisible(bool visible)
{
    m_tabs->tabBar()->setVisible(visible);
}
