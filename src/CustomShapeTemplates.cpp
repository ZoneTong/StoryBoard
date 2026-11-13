#include "CustomShapeTemplates.h"

#include "CompositeShapeItem.h"
#include "ShapeItem.h"
#include "StrokeItem.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QSet>
#include <QTransform>
#include <memory>

namespace {

// 画布文件里的图元坐标是场景坐标；模板只取轮廓，按 pos 平移后再归一化。
QPainterPath itemOutlinePath(CanvasItem* item)
{
    if (!item) {
        return QPainterPath();
    }

    QPainterPath path = item->outlineShape();
    if (path.isEmpty()) {
        return path;
    }
    QTransform translate;
    translate.translate(item->pos().x(), item->pos().y());
    return translate.map(path);
}

CanvasItem* createTemplateItem(const QString& kind)
{
    if (kind == QStringLiteral("shape")) {
        return new ShapeItem();
    }
    if (kind == QStringLiteral("stroke")) {
        return new StrokeItem();
    }
    if (kind == QStringLiteral("composite")) {
        return new CompositeShapeItem();
    }
    // 图片与文字不适合做矢量轮廓模板：图片会丢掉色彩层次，文字转轮廓还会依赖字体。
    return nullptr;
}

} // namespace

bool loadCustomShapeTemplate(const QString& filePath, CustomShapeTemplate* result, QString* error)
{
    if (!result) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Could not read the file.");
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) {
            *error = QStringLiteral("Invalid JSON: %1").arg(parseError.errorString());
        }
        return false;
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("format")).toString() != QStringLiteral("StoryBoardCanvas")) {
        if (error) {
            *error = QStringLiteral("Not a StoryBoard canvas file.");
        }
        return false;
    }

    QPainterPath combined;
    const QJsonArray items = root.value(QStringLiteral("items")).toArray();
    for (const QJsonValue& value : items) {
        const QJsonObject object = value.toObject();
        std::unique_ptr<CanvasItem> item(createTemplateItem(object.value(QStringLiteral("type")).toString()));
        if (!item) {
            continue;
        }
        item->fromJson(object);
        combined.addPath(itemOutlinePath(item.get()));
    }

    if (combined.isEmpty()) {
        if (error) {
            *error = QStringLiteral("No shape or stroke outlines were found.");
        }
        return false;
    }

    const QRectF bounds = combined.boundingRect();
    if (bounds.width() <= 0.0 || bounds.height() <= 0.0) {
        if (error) {
            *error = QStringLiteral("The outline is empty.");
        }
        return false;
    }

    QTransform normalize;
    // QTransform 的组合顺序是“后写的先作用于点”，先缩放再平移才能把轮廓
    // 精确压进 0–1 的单位框（左上角对齐）。
    normalize.scale(1.0 / bounds.width(), 1.0 / bounds.height());
    normalize.translate(-bounds.left(), -bounds.top());

    result->name = QFileInfo(filePath).completeBaseName().trimmed();
    if (result->name.isEmpty()) {
        result->name = QFileInfo(filePath).fileName();
    }
    result->filePath = QFileInfo(filePath).absoluteFilePath();
    result->unitPath = normalize.map(combined);
    result->aspectRatio = bounds.width() / bounds.height();
    return true;
}

QList<CustomShapeTemplate> loadCustomShapeTemplates(const QString& directory, QStringList* errors)
{
    QList<CustomShapeTemplate> templates;
    QDir dir(directory);
    if (!dir.exists()) {
        return templates;
    }

    const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.sbcanvas")}, QDir::Files, QDir::Name);
    for (const QFileInfo& info : files) {
        CustomShapeTemplate entry;
        QString error;
        if (loadCustomShapeTemplate(info.absoluteFilePath(), &entry, &error)) {
            templates.append(entry);
        } else if (errors) {
            errors->append(QStringLiteral("%1: %2").arg(info.fileName(), error));
        }
    }
    return templates;
}

QList<CustomShapeTemplate> loadCustomShapeTemplatesFromDirs(const QStringList& directories, QStringList* errors)
{
    QList<CustomShapeTemplate> templates;
    QSet<QString> seenNames;
    for (const QString& directory : directories) {
        const QList<CustomShapeTemplate> found = loadCustomShapeTemplates(directory, errors);
        for (const CustomShapeTemplate& entry : found) {
            // 同名模板按目录顺序优先：配置文件同级的覆盖部署目录里的。
            if (seenNames.contains(entry.name)) {
                continue;
            }
            seenNames.insert(entry.name);
            templates.append(entry);
        }
    }
    return templates;
}

QIcon makeCustomShapeIcon(const QPainterPath& unitPath, const QColor& color, int logicalSize)
{
    constexpr qreal scale = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * scale), static_cast<int>(logicalSize * scale));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(scale);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color, 1.4));
    painter.setBrush(Qt::NoBrush);

    // 图标留 2px 边距，单位路径按自身宽高比铺进去，避免小图标被拉扁。
    const QRectF box(2.0, 2.0, logicalSize - 4.0, logicalSize - 4.0);
    const QRectF bounds = unitPath.boundingRect();
    if (bounds.width() > 0.0 && bounds.height() > 0.0) {
        const qreal fit = qMin(box.width() / bounds.width(), box.height() / bounds.height());
        QTransform transform;
        transform.translate(box.center().x(), box.center().y());
        transform.scale(fit, fit);
        transform.translate(-bounds.center().x(), -bounds.center().y());
        painter.drawPath(transform.map(unitPath));
    }
    painter.end();
    return QIcon(pixmap);
}
