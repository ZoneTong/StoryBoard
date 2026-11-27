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
#include <QKeyEvent>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>

namespace {

QString g_source = QStringLiteral("界面");
QMutex g_canvasLogMutex;

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

void PruneOldCanvasLogs()
{
    const QDir dir(CanvasLogDirectory());
    if (!dir.exists()) {
        return;
    }
    const QDate today = QDate::currentDate();
    const QFileInfoList files = dir.entryInfoList({QStringLiteral("canvas-*.log")}, QDir::Files);
    for (const QFileInfo& info : files) {
        if (info.lastModified().date().daysTo(today) > 14) {
            QFile::remove(info.absoluteFilePath());
        }
    }
}

QString CanvasName(const QString& name)
{
    const QString trimmed = name.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("未命名画布") : trimmed;
}

QString SourceSuffix()
{
    return g_source.isEmpty() || g_source == QStringLiteral("界面") ? QString()
                                                                    : QStringLiteral("（来源：%1）").arg(g_source);
}

void Write(const QString& canvasUid, const QString& line)
{
    QMutexLocker locker(&g_canvasLogMutex);
    const QString dirPath = CanvasLogDirectory();
    QDir().mkpath(dirPath);
    const QString path = CanvasLogPath(canvasUid);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }
    if (file.size() == 0) {
        const QString header = QStringLiteral("# StoryBoard canvas lifecycle log\n"
                                              "# canvas uid=%1\n")
                                   .arg(SafeCanvasUid(canvasUid));
        file.write(header.toUtf8());
    }
    const QDateTime now = QDateTime::currentDateTime();
    const QString stamp = now.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) + QLatin1Char('.') +
                          QString::number(now.time().msec()).rightJustified(3, QLatin1Char('0'));
    file.write(QStringLiteral("%1 [INFO ] storyboard.lifecycle: %2\n").arg(stamp, line).toUtf8());
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
            LifecycleLog::SetSource(shortcut ? QStringLiteral("快捷键") : QStringLiteral("键盘"));
            break;
        }
        case QEvent::Shortcut:
            LifecycleLog::SetSource(QStringLiteral("快捷键"));
            break;
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick:
        case QEvent::Wheel:
        case QEvent::ContextMenu:
            LifecycleLog::SetSource(QStringLiteral("鼠标"));
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
    PruneOldCanvasLogs();
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
    QString line = QStringLiteral("画布「%1」%2").arg(CanvasName(canvasName), action);
    if (!detail.isEmpty()) {
        line += QStringLiteral("：%1").arg(detail);
    }
    Write(canvasUid, line);
}

void LifecycleLog::LogItem(const QString& canvasUid, const QString& canvasName, const QString& action,
                           const CanvasItem& item)
{
    Write(canvasUid, QStringLiteral("画布「%1」图元 uid=%2 %3：%4「%5」 图层=%6 高度=%7 %8%9")
                         .arg(CanvasName(canvasName), item.Uid(), action, item.TypeDisplayName(), item.Name())
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
    Write(canvasUid, QStringLiteral("画布「%1」%2：%3%4").arg(CanvasName(canvasName), action, detail, SourceSuffix()));
}
