#include "Settings.h"

#include "I18n.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTextStream>
#include <QUrl>

#ifdef Q_OS_WIN
// clang-format off
#include <windows.h>
#include <ole2.h>
#include <oleauto.h>
#include <shldisp.h>
#include <shellapi.h>
#include <shlobj.h>
// clang-format on
#endif

namespace {

// 诊断：当前进程的完整性级别（0x1000 低 / 0x2000 中 / 0x3000 高）。外壳"open"通道
// 被 UIPI 挡住时，先看这个数就知道是不是级别不匹配，而不是把每种拒绝都当成权限问题。
int processIntegrityLevel()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return 0;
    }
    DWORD size = 0;
    GetTokenInformation(token, TokenIntegrityLevel, nullptr, 0, &size);
    QByteArray buffer(static_cast<int>(size), Qt::Uninitialized);
    int level = 0;
    if (GetTokenInformation(token, TokenIntegrityLevel, buffer.data(), size, &size)) {
        const TOKEN_MANDATORY_LABEL* label = reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(buffer.constData());
        const DWORD* rid = GetSidSubAuthority(label->Label.Sid, *GetSidSubAuthorityCount(label->Label.Sid) - 1);
        if (rid) {
            level = static_cast<int>(*rid);
        }
    }
    CloseHandle(token);
    return level;
}

// 每条外壳通道都回一个 HRESULT，失败原因（5 / E_ACCESSDENIED / 类未注册）才能落到诊断里。
struct ShellChannelResult {
    bool opened = false;
    HRESULT code = E_FAIL;
    HRESULT prepareCode = S_OK;
};

// 需求 31：走 Shell.Application（CLSID_Shell）的 Explore 动词，把开目录的活交给
// 已经在跑的外壳进程。提权（High IL）会话里 ShellExecuteW 会因为 UIPI 拿不到
// 中完整性外壳而返回 5，直接起 explorer.exe 子进程又会在部分会话里以
// 0xc0000142（STATUS_DLL_INIT_FAILED）退出并弹“应用程序错误”；COM 本地服务器
// 激活只是跨进程调用，不会新建进程，实测在提权上下文可正常开窗。
ShellChannelResult openFolderViaShellAutomation(const QString& directory)
{
    const std::wstring native = QDir::toNativeSeparators(directory).toStdWString();
    // 线程若已按套间模型初始化，CoInitializeEx 会返回 RPC_E_CHANGED_MODE；
    // 那时 COM 仍然可用，只是不需要（也不该）在结束时反初始化。
    const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool comInitialized = SUCCEEDED(initHr);

    ShellChannelResult result;
    result.prepareCode = initHr;
    IShellDispatch* shell = nullptr;
    const HRESULT createHr = CoCreateInstance(CLSID_Shell, nullptr, CLSCTX_LOCAL_SERVER, IID_IShellDispatch,
                                              reinterpret_cast<void**>(&shell));
    result.code = createHr;
    if (SUCCEEDED(createHr) && shell) {
        VARIANT folder;
        VariantInit(&folder);
        folder.vt = VT_BSTR;
        folder.bstrVal = SysAllocString(native.c_str());
        if (folder.bstrVal) {
            result.code = shell->Explore(folder);
            result.opened = SUCCEEDED(result.code);
            VariantClear(&folder);
        }
        shell->Release();
    }
    if (comInitialized) {
        CoUninitialize();
    }
    return result;
}

// 需求 19：在已有外壳进程里打开目录（COM 通道）。优先委托现有外壳，而不是直接
// 起新的 explorer.exe 进程——提权 / 受限会话里子进程可能以 0xc0000142 启动失败
// 并弹“应用程序错误”对话框。需求 24 实测部分进程上下文里 ShellExecute 与本通道
// 都会被系统拒绝（5 / E_ACCESSDENIED），需求 31 改为再走 Shell.Application。
ShellChannelResult openFolderViaShellItem(const QString& directory)
{
    const std::wstring native = QDir::toNativeSeparators(directory).toStdWString();
    // SHOpenFolderAndSelectItems 要求调用线程已初始化 COM；已初始化（含模式
    // 不同）时直接沿用现有初始化。
    const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool comInitialized = SUCCEEDED(initHr);
    ShellChannelResult result;
    result.prepareCode = initHr;
    PIDLIST_ABSOLUTE pidl = nullptr;
    const HRESULT parseHr = SHParseDisplayName(native.c_str(), nullptr, &pidl, 0, nullptr);
    result.code = parseHr;
    if (SUCCEEDED(parseHr) && pidl) {
        result.code = SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
        result.opened = SUCCEEDED(result.code);
        ILFree(pidl);
    }
    if (comInitialized) {
        CoUninitialize();
    }
    return result;
}

constexpr int MIN_CANVAS_SIDE = 200;
constexpr int MAX_CANVAS_SIDE = 12000;

using SettingsMap = QMap<QString, QString>;

const QString FILE_NAME = QStringLiteral("StoryBoard.ini");
constexpr const char* OVERRIDE_VARIABLE = "STORYBOARD_CONFIG";

QString applicationSettingsFile()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(FILE_NAME);
}

QString userConfigDirectory()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (base.isEmpty()) {
        base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    return base;
}

// 配置只有一个落点：
// 1. 显式指定（自检、调试用）；
// 2. 便携目录里已经有 ini 时沿用，整包拷贝可以带走配置；
// 3. 其余情况写用户配置目录，避免程序目录被写保护。
QString resolveSettingsFile()
{
    const QString override = qEnvironmentVariable(OVERRIDE_VARIABLE).trimmed();
    if (!override.isEmpty()) {
        return QDir::cleanPath(override);
    }

    const QString portable = applicationSettingsFile();
    if (QFileInfo::exists(portable)) {
        return portable;
    }

    const QString directory = userConfigDirectory();
    if (!directory.isEmpty()) {
        return QDir(directory).filePath(FILE_NAME);
    }

    return portable;
}

SettingsMap readSettingsFile(const QString& path)
{
    SettingsMap values;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return values;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    QString section;
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char(';')) || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            section = line.mid(1, line.size() - 2).trimmed();
            continue;
        }
        const int equals = line.indexOf(QLatin1Char('='));
        if (equals <= 0 || section.isEmpty()) {
            continue;
        }
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
    if (!QDir(info.absolutePath()).exists() && !QDir().mkpath(info.absolutePath())) {
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    QString section;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        const int slash = it.key().indexOf(QLatin1Char('/'));
        if (slash <= 0) {
            continue;
        }
        const QString next = it.key().left(slash);
        if (next != section) {
            if (!section.isEmpty()) {
                stream << '\n';
            }
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
    if (!values.contains(key)) {
        return fallback;
    }
    const QString value = values.value(key).toLower();
    return value == QLatin1String("true") || value == QLatin1String("1") || value == QLatin1String("yes");
}

int boundedFromMap(const SettingsMap& values, const QString& key, int fallback, int minimum, int maximum)
{
    bool ok = false;
    const int value = values.value(key).toInt(&ok);
    return qBound(minimum, ok ? value : fallback, maximum);
}

qreal boundedDoubleFromMap(const SettingsMap& values, const QString& key, qreal fallback, qreal minimum, qreal maximum)
{
    bool ok = false;
    const qreal value = values.value(key).toDouble(&ok);
    return qBound(minimum, ok ? value : fallback, maximum);
}

void mergeLineArtSettings(SettingsMap& values, const AppSettings& settings);
void mergeShortcutSettings(SettingsMap& values, const AppSettings& settings);

void mergeSettings(SettingsMap& values, const AppSettings& settings)
{
    values.insert(QStringLiteral("brush/color"), settings.brushColor.name(QColor::HexArgb));
    values.insert(QStringLiteral("brush/width"), QString::number(settings.brushWidth));
    values.insert(QStringLiteral("brush/smoothing"), QString::number(settings.strokeSmoothing));

    values.insert(QStringLiteral("canvas/width"), QString::number(qRound(settings.canvasSize.width())));
    values.insert(QStringLiteral("canvas/height"), QString::number(qRound(settings.canvasSize.height())));

    values.insert(QStringLiteral("view/grid"), settings.showGrid ? QStringLiteral("true") : QStringLiteral("false"));
    values.insert(QStringLiteral("view/canvasColor"), settings.canvasColor.name(QColor::HexArgb));

    values.insert(QStringLiteral("paths/export"), settings.lastExportDir);
    values.insert(QStringLiteral("paths/image"), settings.lastImageDir);
    values.insert(QStringLiteral("paths/canvas"), settings.lastCanvasPath);
    values.insert(QStringLiteral("paths/canvasesDirectory"), settings.canvasesDirectory);

    values.insert(QStringLiteral("window/geometryBase64"), QString::fromLatin1(settings.windowGeometry.toBase64()));

    // 卡片顺序：逗号分隔的 objectName 列表，空列表就不写这个键。
    if (!settings.dockOrder.isEmpty()) {
        values.insert(QStringLiteral("layout/dockOrder"), settings.dockOrder.join(QLatin1Char(',')));
    } else {
        values.remove(QStringLiteral("layout/dockOrder"));
    }

    // 属性编辑面板位置：是否已经由用户移动过、当前停靠侧、是否悬浮以及悬浮几何。
    values.insert(QStringLiteral("layout/itemDockSaved"),
                  settings.propertiesDockStateSaved ? QStringLiteral("true") : QStringLiteral("false"));
    values.insert(QStringLiteral("layout/itemDockFloating"),
                  settings.propertiesDockFloating ? QStringLiteral("true") : QStringLiteral("false"));
    values.insert(QStringLiteral("layout/itemDockArea"), settings.propertiesDockArea);
    if (!settings.propertiesDockGeometry.isEmpty()) {
        values.insert(QStringLiteral("layout/itemDockGeometryBase64"),
                      QString::fromLatin1(settings.propertiesDockGeometry.toBase64()));
    } else {
        values.remove(QStringLiteral("layout/itemDockGeometryBase64"));
    }

    // 快捷键整段重写：先清掉旧的 shortcuts/*，再按当前内存里的覆盖表写入，
    // 这样「恢复默认」删掉的条目才会真的从文件里消失。
    mergeShortcutSettings(values, settings);

    mergeLineArtSettings(values, settings);
}

void mergeShortcutSettings(SettingsMap& values, const AppSettings& settings)
{
    const QString prefix = QStringLiteral("shortcuts/");
    for (auto it = values.begin(); it != values.end();) {
        if (it.key().startsWith(prefix)) {
            it = values.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = settings.shortcutOverrides.constBegin(); it != settings.shortcutOverrides.constEnd(); ++it) {
        if (it.key().isEmpty() || it.value().isEmpty()) {
            continue;
        }
        values.insert(prefix + it.key(), it.value());
    }
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
    values.insert(QStringLiteral("lineArt/minThickness"), QString::number(settings.lineArtMinThickness, 'f', 2));
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
    strokeSmoothing = boundedDoubleFromMap(values, QStringLiteral("brush/smoothing"), strokeSmoothing, 0.0, 20.0);

    const int canvasWidth = boundedFromMap(values, QStringLiteral("canvas/width"), qRound(canvasSize.width()),
                                           MIN_CANVAS_SIDE, MAX_CANVAS_SIDE);
    const int canvasHeight = boundedFromMap(values, QStringLiteral("canvas/height"), qRound(canvasSize.height()),
                                            MIN_CANVAS_SIDE, MAX_CANVAS_SIDE);
    canvasSize = QSizeF(canvasWidth, canvasHeight);

    showGrid = boolFromMap(values, QStringLiteral("view/grid"), showGrid);
    canvasColor = colorFromMap(values, QStringLiteral("view/canvasColor"), canvasColor);

    lastExportDir = values.value(QStringLiteral("paths/export"), lastExportDir);
    lastImageDir = values.value(QStringLiteral("paths/image"), lastImageDir);
    lastCanvasPath = values.value(QStringLiteral("paths/canvas"), lastCanvasPath);
    canvasesDirectory = values.value(QStringLiteral("paths/canvasesDirectory"), canvasesDirectory);
    windowGeometry = QByteArray::fromBase64(values.value(QStringLiteral("window/geometryBase64")).toLatin1());
    dockOrder = values.value(QStringLiteral("layout/dockOrder")).split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (QString& name : dockOrder) {
        name = name.trimmed();
    }

    propertiesDockStateSaved = boolFromMap(values, QStringLiteral("layout/itemDockSaved"), propertiesDockStateSaved);
    propertiesDockFloating = boolFromMap(values, QStringLiteral("layout/itemDockFloating"), propertiesDockFloating);
    const QString savedPropertiesPanelDockArea =
        values.value(QStringLiteral("layout/itemDockArea")).trimmed().toLower();
    propertiesDockArea =
        savedPropertiesPanelDockArea == QLatin1String("right") ? QStringLiteral("right") : QStringLiteral("left");
    propertiesDockGeometry =
        QByteArray::fromBase64(values.value(QStringLiteral("layout/itemDockGeometryBase64")).toLatin1());

    shortcutOverrides.clear();
    const QString shortcutPrefix = QStringLiteral("shortcuts/");
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        if (!it.key().startsWith(shortcutPrefix) || it.value().isEmpty()) {
            continue;
        }
        shortcutOverrides.insert(it.key().mid(shortcutPrefix.size()), it.value());
    }

    lineArtColorLevels = boundedFromMap(values, QStringLiteral("lineArt/colorLevels"), lineArtColorLevels, 2, 12);
    lineArtPaletteMerge = boundedFromMap(values, QStringLiteral("lineArt/paletteMerge"), lineArtPaletteMerge, 0, 160);
    lineArtAdjacentMerge =
        boundedFromMap(values, QStringLiteral("lineArt/adjacentMerge"), lineArtAdjacentMerge, 0, 160);
    lineArtMinRegionArea =
        boundedFromMap(values, QStringLiteral("lineArt/minRegionArea"), lineArtMinRegionArea, 4, 400);
    lineArtAdaptiveDownscale =
        boolFromMap(values, QStringLiteral("lineArt/adaptiveDownscale"), lineArtAdaptiveDownscale);
    lineArtAdaptiveLimit =
        boundedFromMap(values, QStringLiteral("lineArt/adaptiveLimit"), lineArtAdaptiveLimit, 100, 20000);
    lineArtSimplifyTolerance =
        boundedDoubleFromMap(values, QStringLiteral("lineArt/simplifyTolerance"), lineArtSimplifyTolerance, 0.0, 4.0);
    lineArtMinThickness =
        boundedDoubleFromMap(values, QStringLiteral("lineArt/minThickness"), lineArtMinThickness, 0.0, 8.0);

    // 首次运行也立刻落盘，确保配置文件一定存在；已存在的文件只在读取成功时才规范化，
    // 读取失败（例如临时被占用）不做覆盖，避免把用户的配置写回默认值。
    if (!existed || !values.isEmpty()) {
        save();
    }
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
    if (base.isEmpty()) {
        return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("canvases"));
    }
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

QString AppSettings::shapesDir()
{
    // 与配置文件同级：便携版里配了 STORYBOARD_CONFIG 就是 exe 旁的 shapes 目录。
    const QString dir = QDir(QFileInfo(settingsFilePath()).absolutePath()).filePath(QStringLiteral("shapes"));
    QDir().mkpath(dir);
    return dir;
}

QString AppSettings::writableShapesDir()
{
    // 与扫描顺序一致：配置文件同级最优先，其次是部署目录旁的 shapes，
    // 最后退到用户配置目录——这样装在 Program Files 里也能正常打开模板目录。
    QStringList candidates{shapesDir(), bundledShapesDir()};
    const QString userBase = userConfigDirectory();
    if (!userBase.isEmpty()) {
        candidates.append(QDir(userBase).filePath(QStringLiteral("shapes")));
    }

    QStringList seen;
    for (const QString& candidate : candidates) {
        if (candidate.isEmpty()) {
            continue;
        }
        const QString dir = QDir::cleanPath(candidate);
        if (seen.contains(dir)) {
            continue;
        }
        seen.append(dir);
        // mkpath 的结果必须看：只读安装目录里建不出来时继续往下一个候选降级。
        const bool created = QDir().mkpath(dir);
        const QFileInfo info(dir);
        if (info.isDir() && info.isWritable()) {
            return dir;
        }
        if (!created) {
            qWarning("shapes directory candidate not usable [%s]: mkpath failed", qUtf8Printable(dir));
        }
    }
    return QString();
}

QString AppSettings::bundledShapesDir()
{
    // 部署目录旁的 shapes：便携包解压后把模板丢这里就能被扫到，不必去用户配置目录翻。
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("shapes"));
}

QStringList AppSettings::shapesDirs()
{
    const QString userDir = shapesDir();
    const QString bundledDir = bundledShapesDir();
    QStringList dirs{userDir};
    // 便携包里两者常常是同一个目录，去重后只扫一次。
    if (QDir::cleanPath(bundledDir) != QDir::cleanPath(userDir)) {
        dirs.append(bundledDir);
    }
    return dirs;
}

void AppSettings::ensureShapesDir()
{
    QDir().mkpath(shapesDir());
}

AppSettings::ShapesFolderOpenResult AppSettings::openShapesFolder()
{
    ShapesFolderOpenResult result;
    // 先找一个「能建且可写」的目录：只读安装目录里配置同级目录建不出来时自动降级，
    // 不再像以前那样对一个不存在的目录发起打开，失败原因也就说不清了。
    const QString directory = writableShapesDir();
    result.directory = directory;
    if (directory.isEmpty()) {
        result.error = ShapesFolderError::CREATE_FAILED;
        qWarning("shapes folder unavailable: no writable candidate (settings=%s, bundled=%s)",
                 qUtf8Printable(shapesDir()), qUtf8Printable(bundledShapesDir()));
        return result;
    }
#ifdef Q_OS_WIN
    // 先走外壳"open"动词：正常会话里它会转发给已在运行的资源管理器。
    const std::wstring native = QDir::toNativeSeparators(directory).toStdWString();
    const HINSTANCE handle = ShellExecuteW(nullptr, L"open", native.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    const qintptr shellCode = reinterpret_cast<qintptr>(handle);
    const int integrity = processIntegrityLevel();
    if (shellCode > 32) {
        result.opened = true;
        return result;
    }
    // 诊断：把级别与各通道返回值攒进一句话，探针会原样写出来，省得再猜是哪条被拦。
    QString channelDiagnostics =
        QStringLiteral("integrity=0x%1 se=%2").arg(integrity, 4, 16, QLatin1Char('0')).arg(shellCode);
    qWarning("ShellExecute failed to open shapes folder [%s]: code=%d", qUtf8Printable(directory),
             static_cast<int>(shellCode));
    // 需求 31：提权会话里 ShellExecute 会被 UIPI 挡住（这里实测 5 / E_ACCESSDENIED）。
    // 改由 Shell.Application 的 Explore 委托已在运行的外壳进程开窗——不新建进程，
    // 也就不会再出现 0xc0000142 的“应用程序错误”弹窗。
    const ShellChannelResult automation = openFolderViaShellAutomation(directory);
    channelDiagnostics +=
        QStringLiteral(" auto=0x%1").arg(static_cast<quint32>(automation.code), 8, 16, QLatin1Char('0'));
    if (automation.opened) {
        result.opened = true;
        result.error = ShapesFolderError::NONE;
        return result;
    }
    qWarning("Shell.Application Explore failed to open shapes folder [%s]", qUtf8Printable(directory));
    // 需求 19：LocalLow 等目录 "open" 动词可能返回 5（拒绝访问）。再用 COM 的
    // SHOpenFolderAndSelectItems 兜一次底，同样不新建 explorer.exe 进程。
    const ShellChannelResult shellItem = openFolderViaShellItem(directory);
    channelDiagnostics +=
        QStringLiteral(" item=0x%1").arg(static_cast<quint32>(shellItem.code), 8, 16, QLatin1Char('0'));
    result.diagnostics = channelDiagnostics;
    if (shellItem.opened) {
        result.opened = true;
        result.error = ShapesFolderError::NONE;
        return result;
    }
    qWarning("SHOpenFolderAndSelectItems failed to open shapes folder [%s]", qUtf8Printable(directory));
    // 需求 24/31：直启 explorer.exe 的通道整体移除——它在提权 / 受限会话里正是
    // 0xc0000142 的来源，而上面的 COM 通道已覆盖同一场景。
    result.shellCode = static_cast<int>(shellCode);
#endif
    // Windows 外壳失败或非 Windows 平台：退到桌面服务再试一次，目录 URL 必须以分隔符结尾。
    if (QDesktopServices::openUrl(QUrl::fromLocalFile(directory + QLatin1Char('/')))) {
        result.opened = true;
        result.error = ShapesFolderError::NONE;
        return result;
    }
#ifdef Q_OS_WIN
    result.error = ShapesFolderError::SHELL_FAILED;
#else
    result.error = ShapesFolderError::DESKTOP_FAILED;
#endif
    qWarning("desktop service failed to open shapes folder [%s]", qUtf8Printable(directory));
    return result;
}

QString AppSettings::shapesFolderFailureText(const ShapesFolderOpenResult& result)
{
    const QString directory = result.directory.isEmpty() ? QStringLiteral("(unresolved directory)")
                                                         : QDir::toNativeSeparators(result.directory);
    switch (result.error) {
    case ShapesFolderError::CREATE_FAILED:
        return uiText(QStringLiteral(
                          "Could not open the shapes folder: %1 (cannot create the directory; check write permission)"))
            .arg(directory);
    case ShapesFolderError::SHELL_FAILED:
        return uiText(QStringLiteral("Could not open the shapes folder: %1 (Windows shell returned %2)"))
            .arg(directory)
            .arg(result.shellCode);
    case ShapesFolderError::DESKTOP_FAILED:
        return uiText(QStringLiteral("Could not open the shapes folder: %1 (no file manager available)"))
            .arg(directory);
    case ShapesFolderError::NONE:
        break;
    }
    return QString();
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
