#pragma once

#include <QHash>
#include <QKeySequence>
#include <QStringList>

class MainWindow;
class QAction;

// 快捷键管理：采集默认组合键、应用配置文件里的覆盖、改键冲突检测，
// 以及「键盘快捷键」对话框。动作本身仍归 MainWindow 所有，这里只负责
// 键位与提示文字的维护。
class ShortcutController {
  public:
    explicit ShortcutController(MainWindow* window);

    // 启动 / 恢复默认后调用：按配置覆盖键位，并刷新所有动作的提示文字。
    void ApplyShortcutOverrides();
    // 按当前键位重拼动作提示里的 “(Ctrl+X)”，改键后立即生效。
    void RefreshShortcutTips();
    // 改单个动作的键位；冲突时提示并返回 false。
    bool ChangeShortcut(QAction* action, const QKeySequence& sequence);
    // 「键盘快捷键」对话框：列出、就地改键、恢复默认。
    void ShowShortcuts();

  private:
    MainWindow* m_window = nullptr;
    // 动作 → 默认快捷键（便携文本，多键动作保留整组），作为配置里
    // shortcutOverrides 的键。只在启动时采集一次，改键后不覆盖，才能恢复默认。
    QHash<QAction*, QStringList> m_defaultShortcuts;
};
