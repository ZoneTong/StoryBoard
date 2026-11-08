#include "Settings.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace {

constexpr int kMinBoardSide = 200;
constexpr int kMaxBoardSide = 12000;

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

    const int boardWidth = qBound(kMinBoardSide,
                                  settings.value(QStringLiteral("board/width"), static_cast<int>(boardSize.width())).toInt(),
                                  kMaxBoardSide);
    const int boardHeight = qBound(kMinBoardSide,
                                   settings.value(QStringLiteral("board/height"), static_cast<int>(boardSize.height())).toInt(),
                                   kMaxBoardSide);
    boardSize = QSizeF(boardWidth, boardHeight);

    showGrid = settings.value(QStringLiteral("view/grid"), showGrid).toBool();
    canvasColor = colorFromSetting(settings, QStringLiteral("view/canvasColor"), canvasColor);

    lastExportDir = settings.value(QStringLiteral("paths/export")).toString();
    lastImageDir = settings.value(QStringLiteral("paths/image")).toString();
    lastCanvasPath = settings.value(QStringLiteral("paths/canvas")).toString();
    canvasesDirectory = settings.value(QStringLiteral("paths/canvasesDirectory")).toString();
    windowGeometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
}

void AppSettings::save() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("brush/color"), brushColor.name(QColor::HexArgb));
    settings.setValue(QStringLiteral("brush/width"), brushWidth);
    settings.setValue(QStringLiteral("board/width"), static_cast<int>(boardSize.width()));
    settings.setValue(QStringLiteral("board/height"), static_cast<int>(boardSize.height()));
    settings.setValue(QStringLiteral("view/grid"), showGrid);
    settings.setValue(QStringLiteral("view/canvasColor"), canvasColor.name(QColor::HexArgb));
    settings.setValue(QStringLiteral("paths/export"), lastExportDir);
    settings.setValue(QStringLiteral("paths/image"), lastImageDir);
    settings.setValue(QStringLiteral("paths/canvas"), lastCanvasPath);
    settings.setValue(QStringLiteral("paths/canvasesDirectory"), canvasesDirectory);
    settings.setValue(QStringLiteral("window/geometry"), windowGeometry);
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
