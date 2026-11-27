#pragma once

#include <QString>

class CanvasScene;
class CanvasView;
class MainWindow;
class QUndoStack;

// 一份画布文档：场景、视图、撤销栈与文件路径四件套。
// 多标签窗口里每个标签对应一个 CanvasSession，切换时由 MainWindow
// 把 m_scene / m_view / m_undo / m_filePath 重绑到当前会话。
class CanvasSession {
  public:
    // 由 MainWindow 创建首份文档时用：接管已构造好的场景 / 视图 / 撤销栈。
    CanvasSession(MainWindow* window, CanvasScene* scene, CanvasView* view, QUndoStack* undo, int untitledNo);
    ~CanvasSession();
    CanvasSession(const CanvasSession&) = delete;
    CanvasSession& operator=(const CanvasSession&) = delete;

    CanvasScene* Scene() const { return m_scene; }
    CanvasView* View() const { return m_view; }
    QUndoStack* Undo() const { return m_undo; }

    QString FilePath() const { return m_filePath; }
    void SetFilePath(const QString& path) { m_filePath = path; }

    int UntitledNo() const { return m_untitledNo; }
    // 自动备份用的稳定标识：每个会话一份，写 recovery/<id>.sbcanvas 与同名 .json。
    QString RecoveryId() const { return m_recoveryId; }
    // 从恢复文件打开时使用的显示名（「原名（恢复）」），不带文件路径。
    void SetDisplayNameOverride(const QString& name);
    QString DisplayNameOverride() const { return m_displayNameOverride; }
    // 标签名：文件基名；没保存过的画布按编号叫「未命名画布 N」。
    QString DisplayName() const;
    bool HasUnsavedChanges() const;
    // 干净、无图元、单图层、无背景、无文件路径：可以拿来装载用户要打开的文件。
    bool IsPristine() const;

  private:
    MainWindow* m_window = nullptr;
    CanvasScene* m_scene = nullptr;
    CanvasView* m_view = nullptr;
    QUndoStack* m_undo = nullptr;
    QString m_filePath;
    QString m_displayNameOverride;
    QString m_recoveryId;
    int m_untitledNo = 0;
};
