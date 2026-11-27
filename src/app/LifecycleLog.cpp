#include "app/LifecycleLog.h"

#include "app/LogService.h"
#include "items/CanvasItem.h"

#include <QApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QKeyEvent>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QSaveFile>

namespace {

QString g_source = QStringLiteral("interface");
QMutex g_canvasLogMutex;
QDate g_lastPruneDate;
QHash<QString, QDate> g_lastTrimDates;

QString SafeCanvasUid(const QString& uid)
{
    QString safe = uid.trimmed();
    for (QChar& ch : safe) {
        if (!ch.isLetterOrNumber() && ch != QLatin1Char('-') && ch != QLatin1Char('_')) {
            ch = QLatin1Char('_');
        }
    }
    return safe.isEmpty() ? QStringLiteral("unassigned") : safe;
}

QString CanvasLogDirectory()
{
    return QDir(LogService::LogDirectory()).filePath(QStringLiteral("canvases"));
}

QString CanvasLogPath(const QString& uid)
{
    return QDir(CanvasLogDirectory()).filePath(QStringLiteral("canvas-%1.log").arg(SafeCanvasUid(uid)));
}

// 单份画布日志可能持续追加很多天：按时间戳保留最近 14 天，文件头的 uid 不参与裁剪。
bool TrimCanvasLog(const QString& path, const QDateTime& now)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
    file.close();

    // 时间戳固定为 yyyy-MM-dd HH:mm:ss.zzz，可直接按字符串先后比较，不必逐行解析。
    const QString cutoff = now.addDays(-14).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    QStringList kept;
    bool removed = false;
    for (const QString& line : lines) {
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            if (!line.isEmpty()) {
                kept.append(line);
            }
            continue;
        }
        if (line.size() >= cutoff.size() && line.left(cutoff.size()) >= cutoff) {
            kept.append(line);
        } else {
            removed = true;
        }
    }
    if (!removed) {
        return false;
    }

    QString data = kept.join(QLatin1Char('\n'));
    if (!data.isEmpty()) {
        data += QLatin1Char('\n');
    }
    QSaveFile rewritten(path);
    if (!rewritten.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    rewritten.write(data.toUtf8());
    return rewritten.commit();
}

void PruneOldCanvasLogs(const QDateTime& now)
{
    const QDir dir(CanvasLogDirectory());
    if (!dir.exists()) {
        g_lastPruneDate = now.date();
        return;
    }
    const QDate today = now.date();
    const QDateTime cutoff = now.addDays(-14);
    const QFileInfoList files = dir.entryInfoList({QStringLiteral("canvas-*.log")}, QDir::Files);
    for (const QFileInfo& info : files) {
        if (info.lastModified() < cutoff) {
            QFile::remove(info.absoluteFilePath());
            continue;
        }
        const QString path = info.absoluteFilePath();
        TrimCanvasLog(path, now);
        g_lastTrimDates.insert(path, today);
    }
    g_lastPruneDate = today;
}

void EnsureCanvasLogTrimmed(const QString& path, const QDateTime& now)
{
    const QDate today = now.date();
    if (g_lastTrimDates.value(path) == today) {
        return;
    }
    TrimCanvasLog(path, now);
    g_lastTrimDates.insert(path, today);
}

QString CanvasName(const QString& name)
{
    QString trimmed = name.trimmed();
    trimmed.replace(QLatin1Char('\n'), QLatin1Char(' '));
    trimmed.replace(QLatin1Char('\r'), QLatin1Char(' '));
    return trimmed.isEmpty() ? QStringLiteral("Untitled canvas") : trimmed;
}

QString SourceSuffix()
{
    return g_source.isEmpty() || g_source == QStringLiteral("interface") ? QString()
                                                                         : QStringLiteral(" (%1)").arg(g_source);
}

QString HeaderCanvasName(const QString& name)
{
    return CanvasName(name).replace(QLatin1Char('"'), QLatin1Char('\''));
}

void Write(LogLevel level, const QString& canvasUid, const QString& canvasName, const QString& line)
{
    if (!LogService::ShouldLog(level)) {
        return;
    }
    QMutexLocker locker(&g_canvasLogMutex);
    const QDateTime now = QDateTime::currentDateTime();
    if (g_lastPruneDate != now.date()) {
        PruneOldCanvasLogs(now);
    }
    const QString dirPath = CanvasLogDirectory();
    QDir().mkpath(dirPath);
    const QString path = CanvasLogPath(canvasUid);
    EnsureCanvasLogTrimmed(path, now);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }
    if (file.size() == 0) {
        const QString header = QStringLiteral("# StoryBoard canvas lifecycle log\n"
                                              "# canvas uid=%1\n"
                                              "# canvas name=\"%2\"\n")
                                   .arg(SafeCanvasUid(canvasUid), HeaderCanvasName(canvasName));
        file.write(header.toUtf8());
    }
    const QString stamp = now.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) + QLatin1Char('.') +
                          QString::number(now.time().msec()).rightJustified(3, QLatin1Char('0'));
    const QString tag = level == LogLevel::LOG_EVENT ? QStringLiteral("[EVENT]") : QStringLiteral("[INFO]");
    file.write(QStringLiteral("%1 %2 %3\n").arg(stamp, tag, line).toUtf8());
}

// 应用级事件过滤器：先于控件记录输入方式，后续命令落日志时按“最近一次输入”归类。
class InputSourceFilter : public QObject {
  public:
    explicit InputSourceFilter(QObject* parent = nullptr) : QObject(parent) {}

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        Q_UNUSED(watched);
        switch (event->type()) {
        case QEvent::KeyPress: {
            const QKeyEvent* key = static_cast<QKeyEvent*>(event);
            const bool shortcut = key->modifiers().testFlag(Qt::ControlModifier) ||
                                  key->modifiers().testFlag(Qt::AltModifier) ||
                                  key->modifiers().testFlag(Qt::MetaModifier);
            LifecycleLog::SetSource(shortcut ? QStringLiteral("shortcut") : QStringLiteral("keyboard"));
            break;
        }
        case QEvent::Shortcut:
            LifecycleLog::SetSource(QStringLiteral("shortcut"));
            break;
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick:
        case QEvent::Wheel:
        case QEvent::ContextMenu:
            LifecycleLog::SetSource(QStringLiteral("mouse"));
            break;
        default:
            break;
        }
        return false;
    }
};

} // namespace

void LifecycleLog::TrackInputSources()
{
    static bool installed = false;
    if (installed || !qApp) {
        return;
    }
    installed = true;
    PruneOldCanvasLogs(QDateTime::currentDateTime());
    qApp->installEventFilter(new InputSourceFilter(qApp));
}

void LifecycleLog::SetSource(const QString& source)
{
    g_source = source;
}

QString LifecycleLog::Source()
{
    return g_source;
}

LifecycleLog::EditSourceScope::EditSourceScope(const QString& source) : m_previous(Source())
{
    SetSource(source);
}

LifecycleLog::EditSourceScope::~EditSourceScope()
{
    SetSource(m_previous);
}

void LifecycleLog::LogCanvas(const QString& canvasUid, const QString& canvasName, const QString& action,
                             const QString& detail)
{
    QString line = QStringLiteral("Canvas \"%1\" %2").arg(CanvasName(canvasName), action);
    if (!detail.isEmpty()) {
        line += QStringLiteral(": %1").arg(detail);
    }
    Write(LogLevel::LOG_INFO, canvasUid, canvasName, line);
}

void LifecycleLog::LogItem(const QString& canvasUid, const QString& canvasName, const QString& action,
                           const CanvasItem& item)
{
    QString label = item.LogTypeName();
    if (item.HasCustomName()) {
        label += QStringLiteral(" name=\"%1\"").arg(item.Name());
    }
    if (action.startsWith(QStringLiteral("Destroy"))) {
        // 销毁只留身份，不再重复创建时已经写过的完整状态。
        Write(LogLevel::LOG_EVENT, canvasUid, canvasName,
              QStringLiteral("item uid=%1 %2 type=%3").arg(item.Uid(), action, label));
        return;
    }
    Write(LogLevel::LOG_EVENT, canvasUid, canvasName,
          QStringLiteral("item uid=%1 %2 type=%3 layer=%4 z=%5 %6%7")
              .arg(item.Uid(), action, label)
              .arg(item.LayerId())
              .arg(item.ZOrder())
              .arg(item.LogStateSummary(), SourceSuffix()));
}

void LifecycleLog::LogEdit(const QString& canvasUid, const QString& canvasName, const QString& action,
                           const QString& detail)
{
    if (detail.isEmpty()) {
        return;
    }
    Write(LogLevel::LOG_EVENT, canvasUid, canvasName, QStringLiteral("%1: %2%3").arg(action, detail, SourceSuffix()));
}
