#include "Settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTextStream>

namespace {

constexpr int kMinCanvasSide = 200;
constexpr int kMaxCanvasSide = 12000;

using SettingsMap = QMap<QString, QString>;

const QString kFileName = QStringLiteral("StoryBoard.ini");
constexpr const char* kOverrideVariable = "STORYBOARD_CONFIG";

QString applicationSettingsFile()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(kFileName);
}

QString userConfigDirectory()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (base.isEmpty())
        base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return base;
}

// 配置只有一个落点：
// 1. 显式指定（自检、调试用）；
// 2. 便携目录里已经有 ini 时沿用，整包拷贝可以带走配置；
// 3. 其余情况写用户配置目录，避免程序目录被写保护。
QString resolveSettingsFile()
{
    const QString override = qEnvironmentVariable(kOverrideVariable).trimmed();
    if (!override.isEmpty())
        return QDir::cleanPath(override);

    const QString portable = applicationSettingsFile();
    if (QFileInfo::exists(portable))
        return portable;

    const QString directory = userConfigDirectory();
    if (!directory.isEmpty())
        return QDir(directory).filePath(kFileName);

    return portable;
}

SettingsMap readSettingsFile(const QString& path)
{
    SettingsMap values;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return values;

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    QString section;
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char(';')) || line.startsWith(QLatin1Char('#')))
            continue;
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            section = line.mid(1, line.size() - 2).trimmed();
            continue;
        }
        const int equals = line.indexOf(QLatin1Char('='));
        if (equals <= 0 || section.isEmpty())
            continue;
        const QString key = line.left(equals).trimmed();
        const QString value = line.mid(equals + 1).trimmed();
        values.insert(section + QLatin1Char('/') + key, value);
    }
    return values;
}

// 用 QSaveFile 提交：要么保留旧文件，要么换成完整的新文件，不会留下截断的半个配置。
bool writeSettingsFile(const QString& path, const SettingsMap& values)
{
    const QFileInfo info(path);
    if (!QDir(info.absolutePath()).exists() && !QDir().mkpath(info.absolutePath()))
        return false;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    QString section;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        const int slash = it.key().indexOf(QLatin1Char('/'));
        if (slash <= 0)
            continue;
        const QString next = it.key().left(slash);
        if (next != section) {
            if (!section.isEmpty())
                stream << '\n';
            stream << '[' << next << "]\n";
            section = next;
        }
        stream << it.key().mid(slash + 1) << '=' << it.value() << '\n';
    }
    stream.flush();
    return file.commit();
}

QColor colorFromMap(const SettingsMap& values, const QString& key, const QColor& fallback)
{
    const QColor color(values.value(key));
    return color.isValid() ? color : fallback;
}

bool boolFromMap(const SettingsMap& values, const QString& key, bool fallback)
{
    if (!values.contains(key))
        return fallback;
    const QString value = values.value(key).toLower();
    return value == QLatin1String("true") || value == QLatin1String("1") || value == QLatin1String("yes");
}

int boundedFromMap(const SettingsMap& values, const QString& key, int fallback, int minimum, int maximum)
{
    bool ok = false;
    const int value = values.value(key).toInt(&ok);
    return qBound(minimum, ok ? value : fallback, maximum);
}

qreal boundedDoubleFromMap(const SettingsMap& values, const QString& key, qreal fallback, qreal minimum,
                          qreal maximum)
{
    bool ok = false;
    const qreal value = values.value(key).toDouble(&ok);
    return qBound(minimum, ok ? value : fallback, maximum);
}

void mergeLineArtSettings(SettingsMap& values, const AppSettings& settings);

void mergeSettings(SettingsMap& values, const AppSettings& settings)
{
    values.insert(QStringLiteral("brush/color"), settings.brushColor.name(QColor::HexArgb));
    values.insert(QStringLiteral("brush/width"), QString::number(settings.brushWidth));
    values.insert(QStringLiteral("brush/smoothing"), QString::number(settings.strokeSmoothing));

    values.insert(QStringLiteral("canvas/width"),
                  QString::number(qRound(settings.canvasSize.width())));
    values.insert(QStringLiteral("canvas/height"),
                  QString::number(qRound(settings.canvasSize.height())));

    values.insert(QStringLiteral("view/grid"),
                  settings.showGrid ? QStringLiteral("true") : QStringLiteral("false"));
    values.insert(QStringLiteral("view/canvasColor"), settings.canvasColor.name(QColor::HexArgb));

    values.insert(QStringLiteral("paths/export"), settings.lastExportDir);
    values.insert(QStringLiteral("paths/image"), settings.lastImageDir);
    values.insert(QStringLiteral("paths/canvas"), settings.lastCanvasPath);
    values.insert(QStringLiteral("paths/canvasesDirectory"), settings.canvasesDirectory);

    values.insert(QStringLiteral("window/geometryBase64"),
                  QString::fromLatin1(settings.windowGeometry.toBase64()));

    mergeLineArtSettings(values, settings);
}

void mergeLineArtSettings(SettingsMap& values, const AppSettings& settings)
{
    values.insert(QStringLiteral("lineArt/colorLevels"), QString::number(settings.lineArtColorLevels));
    values.insert(QStringLiteral("lineArt/paletteMerge"), QString::number(settings.lineArtPaletteMerge));
    values.insert(QStringLiteral("lineArt/adjacentMerge"), QString::number(settings.lineArtAdjacentMerge));
    values.insert(QStringLiteral("lineArt/minRegionArea"), QString::number(settings.lineArtMinRegionArea));
    values.insert(QStringLiteral("lineArt/adaptiveDownscale"),
                  settings.lineArtAdaptiveDownscale ? QStringLiteral("true") : QStringLiteral("false"));
    values.insert(QStringLiteral("lineArt/adaptiveLimit"), QString::number(settings.lineArtAdaptiveLimit));
    values.insert(QStringLiteral("lineArt/simplifyTolerance"),
                  QString::number(settings.lineArtSimplifyTolerance, 'f', 2));
    values.insert(QStringLiteral("lineArt/minThickness"),
                  QString::number(settings.lineArtMinThickness, 'f', 2));
}

} // namespace

QString AppSettings::settingsFilePath()
{
    return resolveSettingsFile();
}

void AppSettings::load()
{
    const QString path = settingsFilePath();
    const bool existed = QFileInfo::exists(path);
    const SettingsMap values = readSettingsFile(path);

    brushColor = colorFromMap(values, QStringLiteral("brush/color"), brushColor);
    brushWidth = boundedFromMap(values, QStringLiteral("brush/width"), brushWidth, 1, 64);
    strokeSmoothing = boundedFromMap(values, QStringLiteral("brush/smoothing"), strokeSmoothing, 0, 30);

    const int canvasWidth = boundedFromMap(values, QStringLiteral("canvas/width"),
                                           qRound(canvasSize.width()), kMinCanvasSide, kMaxCanvasSide);
    const int canvasHeight = boundedFromMap(values, QStringLiteral("canvas/height"),
                                            qRound(canvasSize.height()), kMinCanvasSide, kMaxCanvasSide);
    canvasSize = QSizeF(canvasWidth, canvasHeight);

    showGrid = boolFromMap(values, QStringLiteral("view/grid"), showGrid);
    canvasColor = colorFromMap(values, QStringLiteral("view/canvasColor"), canvasColor);

    lastExportDir = values.value(QStringLiteral("paths/export"), lastExportDir);
    lastImageDir = values.value(QStringLiteral("paths/image"), lastImageDir);
    lastCanvasPath = values.value(QStringLiteral("paths/canvas"), lastCanvasPath);
    canvasesDirectory = values.value(QStringLiteral("paths/canvasesDirectory"), canvasesDirectory);
    windowGeometry = QByteArray::fromBase64(values.value(QStringLiteral("window/geometryBase64")).toLatin1());

    lineArtColorLevels = boundedFromMap(values, QStringLiteral("lineArt/colorLevels"),
                                       lineArtColorLevels, 2, 12);
    lineArtPaletteMerge = boundedFromMap(values, QStringLiteral("lineArt/paletteMerge"),
                                         lineArtPaletteMerge, 0, 160);
    lineArtAdjacentMerge = boundedFromMap(values, QStringLiteral("lineArt/adjacentMerge"),
                                          lineArtAdjacentMerge, 0, 160);
    lineArtMinRegionArea = boundedFromMap(values, QStringLiteral("lineArt/minRegionArea"),
                                          lineArtMinRegionArea, 4, 400);
    lineArtAdaptiveDownscale = boolFromMap(values, QStringLiteral("lineArt/adaptiveDownscale"),
                                           lineArtAdaptiveDownscale);
    lineArtAdaptiveLimit = boundedFromMap(values, QStringLiteral("lineArt/adaptiveLimit"),
                                          lineArtAdaptiveLimit, 100, 20000);
    lineArtSimplifyTolerance = boundedDoubleFromMap(values, QStringLiteral("lineArt/simplifyTolerance"),
                                                   lineArtSimplifyTolerance, 0.0, 4.0);
    lineArtMinThickness = boundedDoubleFromMap(values, QStringLiteral("lineArt/minThickness"),
                                              lineArtMinThickness, 0.0, 8.0);

    // 首次运行也立刻落盘，确保配置文件一定存在；已存在的文件只在读取成功时才规范化，
    // 读取失败（例如临时被占用）不做覆盖，避免把用户的配置写回默认值。
    if (!existed || !values.isEmpty())
        save();
}

bool AppSettings::save() const
{
    const QString path = settingsFilePath();
    SettingsMap values = readSettingsFile(path);
    mergeSettings(values, *this);
    return writeSettingsFile(path, values);
}

bool AppSettings::saveLineArt() const
{
    const QString path = settingsFilePath();
    SettingsMap values = readSettingsFile(path);
    mergeLineArtSettings(values, *this);
    return writeSettingsFile(path, values);
}

void AppSettings::copyLineArtFrom(const AppSettings& other)
{
    lineArtColorLevels = other.lineArtColorLevels;
    lineArtPaletteMerge = other.lineArtPaletteMerge;
    lineArtAdjacentMerge = other.lineArtAdjacentMerge;
    lineArtMinRegionArea = other.lineArtMinRegionArea;
    lineArtAdaptiveDownscale = other.lineArtAdaptiveDownscale;
    lineArtAdaptiveLimit = other.lineArtAdaptiveLimit;
    lineArtSimplifyTolerance = other.lineArtSimplifyTolerance;
    lineArtMinThickness = other.lineArtMinThickness;
}

QString AppSettings::defaultCanvasesDir()
{
    const QString base = userConfigDirectory();
    if (base.isEmpty())
        return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("canvases"));
    return QDir(base).filePath(QStringLiteral("canvases"));
}

QString AppSettings::canvasesDir()
{
    const SettingsMap values = readSettingsFile(settingsFilePath());
    const QString configured = values.value(QStringLiteral("paths/canvasesDirectory"));
    if (!configured.isEmpty()) {
        QDir().mkpath(configured);
        return configured;
    }

    const QString dir = defaultCanvasesDir();
    QDir().mkpath(dir);
    return dir;
}

void AppSettings::ensureCanvasesDir()
{
    QDir().mkpath(canvasesDir());
}

QString AppSettings::languageSetting()
{
    return readSettingsFile(settingsFilePath()).value(QStringLiteral("ui/language"));
}

bool AppSettings::setLanguageSetting(const QString& stored)
{
    const QString path = settingsFilePath();
    SettingsMap values = readSettingsFile(path);
    values.insert(QStringLiteral("ui/language"), stored);
    return writeSettingsFile(path, values);
}
