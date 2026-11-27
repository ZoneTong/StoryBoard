#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QThreadPool>
#include <QTimer>

class CanvasSession;
class MainWindow;

// 未保存内容的临时备份（断电 / 崩溃兜底）。
// 计时只在「脏且自上次备份后内容变过」的会话存在时走，一个单次 5 分钟定时器
// 服务全部画布；备份写到 recovery/<会话uuid>.sbcanvas + .json，绝不碰原文件。
class AutoSaveService : public QObject {
    Q_OBJECT
  public:
    explicit AutoSaveService(MainWindow* window);
    ~AutoSaveService() override;

    // 会话建立时登记：监听脏状态与内容修订号，按需启动计时。
    void Watch(CanvasSession* session);
    // 保存成功 / 关闭标签（保存或丢弃）/ 恢复完成后调用：删掉该会话的备份并停止跟踪。
    void Forget(CanvasSession* session);
    void ForgetAll();
    // 刚保存成功：删掉备份、把修订号记为已备份；之后再有改动才重新计时。
    void MarkSaved(CanvasSession* session);

    // 启动扫描：清理 7 天前的残留，其余询问用户是否恢复；自检模式直接跳过。
    void ScanOnStartup();

    bool IsArmed() const { return m_timer.isActive(); }
    int Interval() const { return m_timer.interval(); }
    // 自检入口：把间隔改成 0 毫秒，下一轮事件循环立即到点。
    void SetIntervalForTest(int milliseconds);
    void TriggerNowForTest() { Fire(); }

  private:
    struct State {
        CanvasSession* session = nullptr;
        quint64 contentRevision = 0;
        quint64 backedUpRevision = 0;
        quint64 generation = 0;
        bool dirty = false;
        bool writing = false;
    };

    void Fire();
    void Evaluate();
    bool NeedsBackup(const State& state) const;
    void WriteSnapshot(const QString& id, const State& state);
    void OnWritten(const QString& id, quint64 generation, quint64 revision);
    void RemoveFiles(const QString& id);
    // 返回需要恢复的条目；同时清理过期文件。
    QList<QJsonObject> CollectRecoverable() const;
    void RestoreOne(const QJsonObject& meta);

    MainWindow* m_window = nullptr;
    QHash<QString, State> m_states;
    QTimer m_timer;
    QThreadPool m_pool;
    int m_intervalMs = 5 * 60 * 1000;
};
