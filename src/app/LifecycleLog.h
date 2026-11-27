#pragma once

#include <QString>

class CanvasItem;

// 画布 / 图元的全生命周期日志：统一走 storyboard.lifecycle 分类，
// 日志行按「画布 → 图元 → 动作」组织，便于按一次操作复现问题。
namespace LifecycleLog {

// 记录最近一次用户输入来源（鼠标 / 快捷键 / 键盘 / 粘贴），供图元编辑日志标注。
void TrackInputSources();
void SetSource(const QString& source);
QString Source();

void LogCanvas(const QString& canvasName, const QString& action, const QString& detail = QString());
void LogItem(const QString& canvasName, const QString& action, const CanvasItem& item);
void LogEdit(const QString& canvasName, const QString& action, const QString& detail);

// 临时覆盖操作来源（如粘贴），析构后恢复原来源。
class EditSourceScope {
  public:
    explicit EditSourceScope(const QString& source);
    ~EditSourceScope();
    EditSourceScope(const EditSourceScope&) = delete;
    EditSourceScope& operator=(const EditSourceScope&) = delete;

  private:
    QString m_previous;
};

} // namespace LifecycleLog
