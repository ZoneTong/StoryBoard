#include "app/LogService.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMessageLogContext>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QThread>

namespace {

QMutex g_mutex;
QFile g_file;
QDate g_fileDate;
QString g_directory;
int g_retentionDays = 14;

const char* LevelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return "DEBUG";
    case QtInfoMsg:
        return "INFO ";
    case QtWarningMsg:
        return "WARN ";
    case QtCriticalMsg:
        return "CRIT ";
    case QtFatalMsg:
        return "FATAL";
    }
    return "INFO ";
}

QString ResolveDirectory()
{
    if (!g_directory.isEmpty()) {
        return g_directory;
    }
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (base.isEmpty()) {
        base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    if (base.isEmpty()) {
        base = QCoreApplication::applicationDirPath();
    }
    return QDir(base).filePath(QStringLiteral("logs"));
}

// 调用方必须持有 g_mutex：同一天内只开一次文件，跨天自动换新文件。
void EnsureFileOpen(const QDate& date)
{
    if (g_file.isOpen() && g_fileDate == date) {
        return;
    }
    if (g_file.isOpen()) {
        g_file.close();
    }
    const QString dirPath = ResolveDirectory();
    QDir().mkpath(dirPath);
    const QString fileName = QStringLiteral("storyboard-%1.log").arg(date.toString(QStringLiteral("yyyy-MM-dd")));
    g_file.setFileName(QDir(dirPath).filePath(fileName));
    if (!g_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        // 打不开日志文件时不能再 qWarning（会回到本处理器造成递归），静默放弃。
        return;
    }
    g_fileDate = date;
}

// offscreen 是自检专用的无窗口平台，Qt 会反复提示 raise / propagateSizeHints
// 这类“插件不支持”的能力缺失；它们不是应用缺陷，也不该淹没日志里的真实告警。
bool IsOffscreenPlatformNoise(const QString& message)
{
    static const bool offscreen = QGuiApplication::platformName() == QLatin1String("offscreen");
    return offscreen && message.startsWith(QStringLiteral("This plugin does not support"));
}

void MessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (IsOffscreenPlatformNoise(message)) {
        return;
    }
    QMutexLocker locker(&g_mutex);
    const QDateTime now = QDateTime::currentDateTime();
    EnsureFileOpen(now.date());
    if (!g_file.isOpen()) {
        return;
    }
    const QString stamp = now.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) + QLatin1Char('.') +
                          QString::number(now.time().msec()).rightJustified(3, QLatin1Char('0'));
    QString line = QStringLiteral("%1 [%2] %3: %4")
                       .arg(stamp, QString::fromLatin1(LevelName(type)),
                            QString::fromUtf8(context.category ? context.category : "default"), message);
    if (context.file && (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg)) {
        line += QStringLiteral(" (%1:%2)").arg(QString::fromUtf8(context.file)).arg(context.line);
    }
    line += QLatin1Char('\n');
    g_file.write(line.toUtf8());
    g_file.flush();
}

} // namespace

QString LogService::LogDirectory()
{
    return ResolveDirectory();
}

void LogService::SetRetentionDaysForTest(int days)
{
    QMutexLocker locker(&g_mutex);
    g_retentionDays = qMax(1, days);
}

int LogService::PruneOldLogs()
{
    QMutexLocker locker(&g_mutex);
    const QDir dir(ResolveDirectory());
    if (!dir.exists()) {
        return 0;
    }
    const QDate today = QDate::currentDate();
    int removed = 0;
    const QFileInfoList files = dir.entryInfoList({QStringLiteral("storyboard-*.log")}, QDir::Files);
    for (const QFileInfo& info : files) {
        if (info.lastModified().date().daysTo(today) > g_retentionDays) {
            if (QFile::remove(info.absoluteFilePath())) {
                ++removed;
            }
        }
    }
    return removed;
}

void LogService::Install()
{
    {
        QMutexLocker locker(&g_mutex);
        QDir().mkpath(ResolveDirectory());
    }
    PruneOldLogs();
    qInstallMessageHandler(MessageHandler);
}
