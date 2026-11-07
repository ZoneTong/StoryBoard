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
    lastTemplatePath = settings.value(QStringLiteral("paths/template")).toString();
    templatesDirectory = settings.value(QStringLiteral("paths/templatesDirectory")).toString();
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
    settings.setValue(QStringLiteral("paths/template"), lastTemplatePath);
    settings.setValue(QStringLiteral("paths/templatesDirectory"), templatesDirectory);
    settings.setValue(QStringLiteral("window/geometry"), windowGeometry);
}

QString AppSettings::defaultTemplatesDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(base).filePath(QStringLiteral("templates"));
}

QString AppSettings::templatesDir()
{
    const QString configured = QSettings().value(QStringLiteral("paths/templatesDirectory")).toString();
    const QString dir = configured.isEmpty() ? defaultTemplatesDir() : configured;
    QDir().mkpath(dir);
    return dir;
}

void AppSettings::ensureTemplatesDir()
{
    QDir().mkpath(templatesDir());
}
