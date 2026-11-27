#include "app/CanvasSession.h"

#include "app/AutoSaveService.h"
#include "app/Settings.h"
#include "canvas/CanvasView.h"
#include "items/CanvasTypes.h"
#include "scene/CanvasScene.h"
#include "ui/mainwindow/MainWindow.h"

#include <QFileInfo>
#include <QUndoStack>
#include <QUuid>

CanvasSession::CanvasSession(MainWindow* window, CanvasScene* scene, CanvasView* view, QUndoStack* undo, int untitledNo,
                             int canvasNo)
    : m_window(window), m_scene(scene), m_view(view), m_undo(undo),
      m_recoveryId(QUuid::createUuid().toString(QUuid::WithoutBraces)), m_untitledNo(untitledNo), m_canvasNo(canvasNo)
{
    // 场景挂到窗口名下：既跟随窗口生命周期兜底，也保持「window.findChild<CanvasScene*>()」
    // 这一既有取用方式对自检与插件继续成立（会话析构时会先于窗口删除它）。
    if (m_window && m_scene) {
        m_scene->setParent(m_window);
    }
}

CanvasSession::~CanvasSession()
{
    // 会话被关闭：它的临时备份要么已随保存删除，要么由用户选择丢弃，这里统一收尾。
    if (m_window && m_window->AutoSave()) {
        m_window->AutoSave()->Forget(this);
    }
    // 先断开所有可能回到窗口的连接，再按「视图 → 撤销栈 → 场景」的顺序析构：
    // 场景析构会 clear() 并发出信号，此时面板与窗口都不该再收到回调。
    if (m_view) {
        // 只掐视图对外的四个业务信号；用 (sender, nullptr, nullptr, nullptr) 通配会连带
        // 断开 Qt 内部的 destroyed 连接，运行时刷出 “wildcard call disconnects…” 告警。
        QObject::disconnect(m_view, &CanvasView::zoomChanged, nullptr, nullptr);
        QObject::disconnect(m_view, &CanvasView::modeChanged, nullptr, nullptr);
        QObject::disconnect(m_view, &CanvasView::statusMessage, nullptr, nullptr);
        QObject::disconnect(m_view, &CanvasView::textPlacementRequested, nullptr, nullptr);
        m_view->setScene(nullptr);
    }
    if (m_undo) {
        QObject::disconnect(m_undo, nullptr, nullptr, nullptr);
        m_undo->clear();
    }
    if (m_scene) {
        QObject::disconnect(m_scene, nullptr, nullptr, nullptr);
        m_scene->SetUndoStack(nullptr);
    }
    delete m_view;
    delete m_undo;
    delete m_scene;
    m_view = nullptr;
    m_undo = nullptr;
    m_scene = nullptr;
}

void CanvasSession::SetDisplayNameOverride(const QString& name)
{
    m_displayNameOverride = name;
}

QString CanvasSession::DisplayName() const
{
    if (!m_displayNameOverride.isEmpty()) {
        return m_displayNameOverride;
    }
    if (!m_filePath.isEmpty()) {
        const QString name = QFileInfo(m_filePath).completeBaseName();
        if (!name.isEmpty()) {
            return name;
        }
    }
    return MainWindow::tr("Untitled canvas %1").arg(m_untitledNo);
}

QString CanvasSession::LogLabel(const QString& displayName) const
{
    QString name = displayName;
    if (name.isEmpty()) {
        name = m_displayNameOverride;
    }
    if (name.isEmpty() && !m_filePath.isEmpty()) {
        name = QFileInfo(m_filePath).completeBaseName();
    }
    if (name.isEmpty()) {
        // 日志固定英文，不取界面翻译，避免中文界面下写出「未命名画布」。
        name = QStringLiteral("Untitled canvas %1").arg(m_untitledNo);
    }
    if (m_canvasNo <= 0) {
        return name;
    }
    return QStringLiteral("#%1 %2").arg(m_canvasNo).arg(name);
}

QString CanvasSession::LogUid() const
{
    return m_scene ? m_scene->CanvasUid() : QString();
}

bool CanvasSession::HasUnsavedChanges() const
{
    return m_undo && !m_undo->isClean();
}

bool CanvasSession::IsPristine() const
{
    if (!m_scene || !m_undo || !m_undo->isClean() || !m_filePath.isEmpty()) {
        return false;
    }
    if (m_scene->CanvasItemCount() > 0 || m_scene->UserLayerCount() > 1) {
        return false;
    }
    return m_scene->canvasBackground().type == BackgroundType::NONE;
}
