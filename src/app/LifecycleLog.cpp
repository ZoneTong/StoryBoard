#include "app/LifecycleLog.h"

#include "items/CanvasItem.h"

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QLoggingCategory>
#include <QObject>

Q_LOGGING_CATEGORY(g_lifecycleLog, "storyboard.lifecycle")

namespace {

QString g_source = QStringLiteral("界面");

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

void Write(const QString& line)
{
    qCInfo(g_lifecycleLog).noquote() << line;
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

void LifecycleLog::LogCanvas(const QString& canvasName, const QString& action, const QString& detail)
{
    QString line = QStringLiteral("画布「%1」%2").arg(CanvasName(canvasName), action);
    if (!detail.isEmpty()) {
        line += QStringLiteral("：%1").arg(detail);
    }
    Write(line);
}

void LifecycleLog::LogItem(const QString& canvasName, const QString& action, const CanvasItem& item)
{
    Write(QStringLiteral("画布「%1」图元 uid=%2 %3：%4「%5」 图层=%6 高度=%7%8")
              .arg(CanvasName(canvasName), item.Uid(), action, item.TypeDisplayName(), item.Name())
              .arg(item.LayerId())
              .arg(item.ZOrder())
              .arg(SourceSuffix()));
}

void LifecycleLog::LogEdit(const QString& canvasName, const QString& action, const QString& detail)
{
    if (detail.isEmpty()) {
        return;
    }
    Write(QStringLiteral("画布「%1」%2：%3%4").arg(CanvasName(canvasName), action, detail, SourceSuffix()));
}
