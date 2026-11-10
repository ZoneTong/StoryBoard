#include "Settings.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace {

constexpr int kMinCanvasSide = 200;
constexpr int kMaxCanvasSide = 12000;

QColor colorFromSetting(const QSettings& settings, const QString& key, const QColor& fallback)
{
    const QColor color(settings.value(key, fallback.name(QColor::HexArgb)).toString());
    return color.isValid() ? color : fallback;
}

} // namespace

void AppSettings::load()
{
    QSettings settings;
    brushColor = colorFromSetting(settings, QStringLiteral("brush/color"), brushColor);
    brushWidth = qBound(1, settings.value(QStringLiteral("brush/width"), brushWidth).toInt(), 64);
    strokeSmoothing = qBound(0, settings.value(QStringLiteral("brush/smoothing"), strokeSmoothing).toInt(), 30);

    const int canvasWidth = qBound(kMinCanvasSide,
                                  settings.value(QStringLiteral("canvas/width"), static_cast<int>(canvasSize.width())).toInt(),
                                  kMaxCanvasSide);
    const int canvasHeight = qBound(kMinCanvasSide,
                                   settings.value(QStringLiteral("canvas/height"), static_cast<int>(canvasSize.height())).toInt(),
                                   kMaxCanvasSide);
    canvasSize = QSizeF(canvasWidth, canvasHeight);

    showGrid = settings.value(QStringLiteral("view/grid"), showGrid).toBool();
    canvasColor = colorFromSetting(settings, QStringLiteral("view/canvasColor"), canvasColor);

    lastExportDir = settings.value(QStringLiteral("paths/export")).toString();
    lastImageDir = settings.value(QStringLiteral("paths/image")).toString();
    lastCanvasPath = settings.value(QStringLiteral("paths/canvas")).toString();
    canvasesDirectory = settings.value(QStringLiteral("paths/canvasesDirectory")).toString();
    windowGeometry = settings.value(QStringLiteral("window/geometry")).toByteArray();

    lineArtColorLevels = qBound(2, settings.value(QStringLiteral("lineArt/colorLevels"), lineArtColorLevels).toInt(), 12);
    lineArtPaletteMerge = qBound(0, settings.value(QStringLiteral("lineArt/paletteMerge"), lineArtPaletteMerge).toInt(), 160);
    lineArtAdjacentMerge = qBound(0, settings.value(QStringLiteral("lineArt/adjacentMerge"), lineArtAdjacentMerge).toInt(), 160);
    lineArtMinRegionArea = qBound(4, settings.value(QStringLiteral("lineArt/minRegionArea"), lineArtMinRegionArea).toInt(), 400);
}

void AppSettings::save() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("brush/color"), brushColor.name(QColor::HexArgb));
    settings.setValue(QStringLiteral("brush/width"), brushWidth);
    settings.setValue(QStringLiteral("brush/smoothing"), strokeSmoothing);
    settings.setValue(QStringLiteral("canvas/width"), static_cast<int>(canvasSize.width()));
    settings.setValue(QStringLiteral("canvas/height"), static_cast<int>(canvasSize.height()));
    settings.setValue(QStringLiteral("view/grid"), showGrid);
    settings.setValue(QStringLiteral("view/canvasColor"), canvasColor.name(QColor::HexArgb));
    settings.setValue(QStringLiteral("paths/export"), lastExportDir);
    settings.setValue(QStringLiteral("paths/image"), lastImageDir);
    settings.setValue(QStringLiteral("paths/canvas"), lastCanvasPath);
    settings.setValue(QStringLiteral("paths/canvasesDirectory"), canvasesDirectory);
    settings.setValue(QStringLiteral("window/geometry"), windowGeometry);
    settings.setValue(QStringLiteral("lineArt/colorLevels"), lineArtColorLevels);
    settings.setValue(QStringLiteral("lineArt/paletteMerge"), lineArtPaletteMerge);
    settings.setValue(QStringLiteral("lineArt/adjacentMerge"), lineArtAdjacentMerge);
    settings.setValue(QStringLiteral("lineArt/minRegionArea"), lineArtMinRegionArea);
}

QString AppSettings::defaultCanvasesDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(base).filePath(QStringLiteral("canvases"));
}

QString AppSettings::canvasesDir()
{
    QSettings settings;
    const QString configured = settings.value(QStringLiteral("paths/canvasesDirectory")).toString();
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
