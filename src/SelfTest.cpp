#include "SelfTest.h"

#include "BoardScene.h"
#include "BoardTypes.h"
#include "BoardView.h"
#include "ImageItem.h"
#include "LayerPanel.h"
#include "MainWindow.h"
#include "ShapeItem.h"
#include "StrokeItem.h"
#include "TextDialog.h"
#include "TextItem.h"

#include <QApplication>
#include <QAction>
#include <QBuffer>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QImage>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineF>
#include <QLinearGradient>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMarginsF>
#include <QMenu>
#include <QMouseEvent>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPixmap>
#include <QPolygonF>
#include <QSet>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QUndoStack>
#include <QWheelEvent>
#include <QtMath>

namespace {

QString geometryText(const QPointF& pos, const QSizeF& size)
{
    return QStringLiteral("pos=(%1,%2) size=(%3,%4)")
        .arg(pos.x(), 0, 'f', 1)
        .arg(pos.y(), 0, 'f', 1)
        .arg(size.width(), 0, 'f', 1)
        .arg(size.height(), 0, 'f', 1);
}

QPixmap makeShotImage(int width, int height, const QString& label,
                      const QColor& start, const QColor& end)
{
    QPixmap pixmap(width, height);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

    QPainterPath clip;
    clip.addRoundedRect(QRectF(0, 0, width, height), 18, 18);
    painter.setClipPath(clip);

    QLinearGradient gradient(0, 0, 0, height);
    gradient.setColorAt(0.0, start);
    gradient.setColorAt(1.0, end);
    painter.fillRect(QRectF(0, 0, width, height), gradient);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 70));
    painter.drawEllipse(QPointF(width * 0.76, height * 0.28), height * 0.16, height * 0.16);
    painter.setBrush(QColor(24, 40, 72, 55));
    QPainterPath hills;
    hills.moveTo(0, height);
    hills.lineTo(width * 0.28, height * 0.52);
    hills.lineTo(width * 0.48, height * 0.72);
    hills.lineTo(width * 0.68, height * 0.45);
    hills.lineTo(width, height * 0.78);
    hills.lineTo(width, height);
    hills.closeSubpath();
    painter.drawPath(hills);

    painter.setPen(QColor(255, 255, 255, 235));
    QFont font(QStringLiteral("Microsoft YaHei UI"));
    font.setPointSizeF(height * 0.17);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(QRectF(18, 12, width - 36, height * 0.4),
                     Qt::AlignLeft | Qt::AlignTop, label);
    painter.end();
    return pixmap;
}

QPixmap makeTextImage(const QString& text, const QColor& color, const QFont& font,
                      int width, int height)
{
    QPixmap pixmap(width, height);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setPen(color);
    painter.setFont(font);
    painter.drawText(QRectF(0, 0, width, height), Qt::AlignLeft | Qt::AlignVCenter, text);
    painter.end();
    return pixmap;
}

void addStroke(BoardScene& scene, const QVector<QPointF>& points, const QColor& color,
               qreal width, int layerId = -1)
{
    auto* stroke = new StrokeItem();
    stroke->setLayerId(layerId);
    stroke->beginStroke(points.constFirst(), color, width);
    for (int i = 1; i < points.size(); ++i)
        stroke->appendScenePoint(points.at(i));
    stroke->endStroke();
    scene.attachItem(stroke);
}

void addLine(BoardScene& scene, const QPointF& from, const QPointF& to, const QColor& color,
             qreal width, int layerId = -1)
{
    addStroke(scene, { from, to }, color, width, layerId);
}

void sendMouseEvent(QWidget* target, QEvent::Type type, const QPoint& position,
                    Qt::MouseButton button, Qt::MouseButtons buttons,
                    Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    QMouseEvent event(type, QPointF(position), QPointF(target->mapToGlobal(position)),
                      button, buttons, modifiers);
    QApplication::sendEvent(target, &event);
}

} // namespace

int runSelfTest(const QString& reportPath)
{
    QStringList lines;
    int failures = 0;

    auto check = [&lines, &failures](bool condition, const QString& name, const QString& detail = QString()) {
        if (condition) {
            lines.append(QStringLiteral("PASS  %1").arg(name));
        } else {
            ++failures;
            lines.append(QStringLiteral("FAIL  %1%2")
                             .arg(name, detail.isEmpty() ? QString() : QStringLiteral("  [") + detail + QLatin1Char(']')));
        }
    };

    const QString resolvedReportPath = reportPath.isEmpty()
        ? QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
              .filePath(QStringLiteral("storyboard_selftest.txt"))
        : reportPath;
    const QFileInfo reportInfo(resolvedReportPath);
    QDir().mkpath(reportInfo.absolutePath());
    const QString workDirectory = QDir(reportInfo.absolutePath())
                                      .filePath(QStringLiteral("storyboard-selftest-work"));
    QDir().mkpath(workDirectory);
    check(QDir(workDirectory).exists(), QStringLiteral("准备自检工作目录"), workDirectory);

    BoardScene scene;
    QUndoStack undo;
    undo.setUndoLimit(100);
    scene.setUndoStack(&undo);
    scene.applyBoardSize(QSizeF(800, 600));
    scene.setShowGrid(true);
    check(qFuzzyCompare(scene.boardSize().width(), 800.0)
              && qFuzzyCompare(scene.boardSize().height(), 600.0),
          QStringLiteral("设置画板尺寸"), geometryText(QPointF(), scene.boardSize()));

    BackgroundState background;
    background.type = BackgroundType::Color;
    background.color = QColor(QStringLiteral("#FEF3C7"));
    scene.replaceBackground(background);
    check(scene.background().type == BackgroundType::Color
              && scene.background().color == background.color,
          QStringLiteral("设置背景颜色"));
    check(undo.canUndo(), QStringLiteral("背景替换进入撤销栈"));

    undo.undo();
    check(scene.background().type == BackgroundType::None, QStringLiteral("撤销背景替换"));
    undo.redo();
    check(scene.background().color == background.color, QStringLiteral("重做背景替换"));

    auto* stroke = new StrokeItem();
    stroke->beginStroke(QPointF(90, 120), QColor(QStringLiteral("#EF4444")), 6.0);
    for (int i = 1; i <= 60; ++i) {
        const qreal x = 90.0 + i * 7.0;
        const qreal y = 220.0 + qSin(i * 0.32) * 80.0;
        stroke->appendScenePoint(QPointF(x, y));
    }
    stroke->endStroke();
    check(!stroke->isEmpty(), QStringLiteral("生成自由笔画"));
    scene.addItemsWithUndo({ stroke }, QStringLiteral("添加笔画"));
    check(scene.boardItemCount() == 1, QStringLiteral("笔画加入场景"));
    check(undo.canUndo(), QStringLiteral("添加笔画进入撤销栈"));

    const QPointF strokePos = stroke->pos();
    const QSizeF strokeSize = stroke->contentSize();

    undo.undo();
    check(scene.boardItemCount() == 0, QStringLiteral("撤销添加笔画"));
    undo.redo();
    check(scene.boardItemCount() == 1, QStringLiteral("重做添加笔画"));
    check(stroke->pos() == strokePos && stroke->contentSize() == strokeSize,
          QStringLiteral("重做后笔画几何一致"), geometryText(stroke->pos(), stroke->contentSize()));

    ItemGeometry beforeMove { stroke, stroke->pos(), stroke->contentSize() };
    stroke->setPos(stroke->pos() + QPointF(45.0, 30.0));
    ItemGeometry afterMove { stroke, stroke->pos(), stroke->contentSize() };
    scene.pushGeometry({ beforeMove }, { afterMove }, QStringLiteral("移动图元"));
    undo.undo();
    check(stroke->pos() == strokePos, QStringLiteral("撤销移动图元"));
    undo.redo();
    check(stroke->pos() == strokePos + QPointF(45.0, 30.0), QStringLiteral("重做移动图元"));

    ItemGeometry beforeResize { stroke, stroke->pos(), stroke->contentSize() };
    stroke->setContentSize(stroke->contentSize() * 1.35);
    ItemGeometry afterResize { stroke, stroke->pos(), stroke->contentSize() };
    scene.pushGeometry({ beforeResize }, { afterResize }, QStringLiteral("缩放图元"));
    const QSizeF resized = stroke->contentSize();
    undo.undo();
    check(stroke->contentSize() == strokeSize, QStringLiteral("撤销缩放图元"));
    undo.redo();
    check(stroke->contentSize() == resized, QStringLiteral("重做缩放图元"));

    const QPixmap shot = makeShotImage(320, 200, QStringLiteral("SHOT 01"),
                                       QColor(QStringLiteral("#60A5FA")),
                                       QColor(QStringLiteral("#1D4ED8")));
    auto* imageItem = new ImageItem();
    imageItem->setImage(shot, QStringLiteral("shot-01.png"));
    imageItem->setContentSize(QSizeF(320, 200));
    imageItem->setPos(QPointF(400, 260));
    scene.addItemsWithUndo({ imageItem }, QStringLiteral("导入图片"));
    check(scene.boardItemCount() == 2, QStringLiteral("导入图片图元"));

    const QJsonObject root = scene.toJson();
    const QByteArray jsonBytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    check(jsonBytes.contains("StoryBoard") && jsonBytes.contains("\"stroke\"")
              && jsonBytes.contains("\"image\""),
          QStringLiteral("模板序列化为 JSON"));

    BoardScene loaded;
    QUndoStack loadedUndo;
    loaded.setUndoStack(&loadedUndo);
    QString loadError;
    check(loaded.fromJson(root, &loadError), QStringLiteral("反序列化模板"), loadError);
    check(loaded.boardItemCount() == 2, QStringLiteral("模板图元数量一致"));
    check(loaded.background().type == BackgroundType::Color
              && loaded.background().color == background.color,
          QStringLiteral("模板背景恢复"));

    StrokeItem* loadedStroke = nullptr;
    ImageItem* loadedImage = nullptr;
    const QList<BoardItem*> loadedItems = loaded.boardItems();
    for (BoardItem* item : loadedItems) {
        if (item->kind() == QStringLiteral("stroke"))
            loadedStroke = static_cast<StrokeItem*>(item);
        else if (item->kind() == QStringLiteral("image"))
            loadedImage = static_cast<ImageItem*>(item);
    }
    check(loadedStroke != nullptr, QStringLiteral("模板包含笔画"));
    if (loadedStroke) {
        check(loadedStroke->strokeColor() == QColor(QStringLiteral("#EF4444")),
              QStringLiteral("笔画颜色恢复"));
        check(qAbs(loadedStroke->contentSize().width() - resized.width()) < 0.5
                  && qAbs(loadedStroke->contentSize().height() - resized.height()) < 0.5,
              QStringLiteral("笔画缩放状态恢复"));
    }
    check(loadedImage != nullptr && !loadedImage->image().isNull(),
          QStringLiteral("图片内容恢复"));

    const QImage rendered = loaded.renderImage(loaded.boardRect(), 1.0);
    check(rendered.size() == QSize(800, 600), QStringLiteral("导出图像尺寸正确"));
    check(!rendered.isNull() && rendered.pixelColor(8, 8) == background.color,
          QStringLiteral("导出包含背景颜色"));

    const QString jpegPath = QDir(workDirectory).filePath(QStringLiteral("selftest.jpg"));
    QImageWriter jpegWriter(jpegPath, "JPEG");
    jpegWriter.setQuality(90);
    check(jpegWriter.write(rendered), QStringLiteral("导出 JPEG"), jpegWriter.errorString());

    const QString pdfPath = QDir(workDirectory).filePath(QStringLiteral("selftest.pdf"));
    bool pdfOk = false;
    {
        QPdfWriter pdfWriter(pdfPath);
        pdfWriter.setResolution(96);
        pdfWriter.setTitle(QStringLiteral("StoryBoard 自检"));
        pdfWriter.setCreator(QStringLiteral("StoryBoard"));
        pdfWriter.setPageMargins(QMarginsF(0.0, 0.0, 0.0, 0.0), QPageLayout::Point);
        const qreal widthPoints = rendered.width() * 72.0 / 96.0;
        const qreal heightPoints = rendered.height() * 72.0 / 96.0;
        pdfWriter.setPageSize(QPageSize(QSizeF(widthPoints, heightPoints), QPageSize::Point,
                                        QStringLiteral("StoryBoard"), QPageSize::ExactMatch));
        QPainter pdfPainter(&pdfWriter);
        if (pdfPainter.isActive()) {
            pdfPainter.drawImage(QRectF(0.0, 0.0, pdfWriter.width(), pdfWriter.height()),
                                 rendered);
            pdfPainter.end();
            pdfOk = true;
        }
    }
    check(pdfOk && QFileInfo(pdfPath).size() > 0, QStringLiteral("导出 PDF"));

    BoardScene imageBackgroundScene;
    QPixmap backgroundPixmap(64, 48);
    backgroundPixmap.fill(QColor(QStringLiteral("#10B981")));
    BackgroundState imageBackground;
    imageBackground.type = BackgroundType::Image;
    imageBackground.image = backgroundPixmap;
    imageBackground.mode = BackgroundMode::Tile;
    imageBackgroundScene.replaceBackground(imageBackground);

    BoardScene imageBackgroundLoaded;
    QString imageBackgroundError;
    check(imageBackgroundLoaded.fromJson(imageBackgroundScene.toJson(), &imageBackgroundError),
          QStringLiteral("背景图片序列化"), imageBackgroundError);
    check(imageBackgroundLoaded.background().type == BackgroundType::Image
              && !imageBackgroundLoaded.background().image.isNull()
              && imageBackgroundLoaded.background().mode == BackgroundMode::Tile,
          QStringLiteral("反序列化背景图片与填充方式"));

    const QString templatePath = QDir(workDirectory).filePath(QStringLiteral("selftest.sbboard"));
    {
        QFile file(templatePath);
        if (file.open(QIODevice::WriteOnly))
            file.write(jsonBytes);
    }
    BoardScene reloaded;
    QUndoStack reloadedUndo;
    reloaded.setUndoStack(&reloadedUndo);
    QFile templateFile(templatePath);
    check(templateFile.open(QIODevice::ReadOnly), QStringLiteral("读取模板文件"));
    const QJsonDocument reloadedDocument = QJsonDocument::fromJson(templateFile.readAll());
    check(reloadedDocument.isObject(), QStringLiteral("模板文件 JSON 有效"));
    check(reloaded.fromJson(reloadedDocument.object()), QStringLiteral("从文件载入模板"));
    check(reloaded.boardItemCount() == 2, QStringLiteral("文件载入图元数量一致"));

    reloaded.setMode(ToolMode::Edit);
    const QList<BoardItem*> reloadedItems = reloaded.boardItems();
    if (!reloadedItems.isEmpty()) {
        reloadedItems.constFirst()->setSelected(true);
        reloaded.deleteSelected();
        check(reloaded.boardItemCount() == 1, QStringLiteral("删除选中图元"));
        reloadedUndo.undo();
        check(reloaded.boardItemCount() == 2, QStringLiteral("撤销删除图元"));
        reloadedUndo.redo();
        check(reloaded.boardItemCount() == 1, QStringLiteral("重做删除图元"));
    }

    scene.setBoardSizeWithUndo(QSizeF(1000, 700));
    check(qFuzzyCompare(scene.boardSize().width(), 1000.0), QStringLiteral("调整画板尺寸"));
    undo.undo();
    check(qFuzzyCompare(scene.boardSize().width(), 800.0), QStringLiteral("撤销调整画板尺寸"));

    // ---------------------------------------------------------------
    // Layer and per-layer height model.
    // ---------------------------------------------------------------
    BoardScene layerScene;
    QUndoStack layerUndo;
    layerUndo.setUndoLimit(100);
    layerScene.setUndoStack(&layerUndo);
    layerScene.applyBoardSize(QSizeF(400, 300));

    check(layerScene.layers().size() == 2
              && layerScene.layers().constFirst().id == BoardScene::BackgroundLayerId
              && layerScene.layerIndex(BoardScene::BackgroundLayerId) == 0,
          QStringLiteral("默认包含背景第 0 层"));
    check(layerScene.activeLayerId() == layerScene.layers().at(1).id,
          QStringLiteral("默认激活底层用户图层"));

    auto makeLineItem = [](const QPointF& from, const QPointF& to, const QColor& color) {
        auto* line = new StrokeItem();
        line->beginStroke(from, color, 4.0);
        line->appendScenePoint(to);
        line->endStroke();
        return line;
    };

    auto* lower = makeLineItem(QPointF(20, 20), QPointF(180, 120),
                               QColor(QStringLiteral("#DC2626")));
    layerScene.addItemsWithUndo({ lower }, QStringLiteral("添加底层图元"));
    check(lower->layerId() == layerScene.activeLayerId() && lower->height() == 1,
          QStringLiteral("新图元进入当前图层并从 height 1 开始"),
          QStringLiteral("layer=%1 height=%2").arg(lower->layerId()).arg(lower->height()));

    auto* upper = makeLineItem(QPointF(40, 40), QPointF(200, 160),
                               QColor(QStringLiteral("#2563EB")));
    layerScene.addItemsWithUndo({ upper }, QStringLiteral("添加同层图元"));
    check(upper->height() == 2 && upper->zValue() > lower->zValue(),
          QStringLiteral("同层新图元 height 递增且覆盖旧图元"));

    check(layerScene.addLayerWithUndo(QStringLiteral("顶层")),
          QStringLiteral("新建图层进入撤销栈"));
    const int topLayerId = layerScene.activeLayerId();
    check(layerScene.userLayerCount() == 2 && topLayerId > BoardScene::BackgroundLayerId,
          QStringLiteral("新图层位于最上层并成为当前图层"));

    auto* topLine = makeLineItem(QPointF(60, 60), QPointF(220, 180),
                                 QColor(QStringLiteral("#059669")));
    layerScene.addItemsWithUndo({ topLine }, QStringLiteral("添加上层图元"));
    check(topLine->layerId() == topLayerId && topLine->height() == 1,
          QStringLiteral("上层图元使用新图层"));
    check(topLine->zValue() > upper->zValue(),
          QStringLiteral("高图层的低 height 图元仍覆盖低图层图元"));

    check(layerScene.moveLayerWithUndo(topLayerId, -1), QStringLiteral("图层下移一层"));
    check(topLine->zValue() < lower->zValue(), QStringLiteral("图层下移后绘制顺序同步降低"));
    layerUndo.undo();
    check(topLine->zValue() > upper->zValue(), QStringLiteral("撤销图层下移恢复绘制顺序"));
    layerUndo.redo();
    check(topLine->zValue() < lower->zValue(), QStringLiteral("重做图层下移"));
    layerUndo.undo();

    check(!layerScene.moveLayerWithUndo(BoardScene::BackgroundLayerId, 1),
          QStringLiteral("背景第 0 层不能上移"));

    QString layerError;
    check(!layerScene.removeLayerWithUndo(topLayerId, &layerError) && !layerError.isEmpty(),
          QStringLiteral("非空图层不可直接删除"));

    check(layerScene.addLayerWithUndo(QStringLiteral("临时空层")),
          QStringLiteral("新建临时图层"));
    const int temporaryLayerId = layerScene.activeLayerId();
    check(layerScene.removeLayerWithUndo(temporaryLayerId),
          QStringLiteral("空图层可以删除"));
    check(!layerScene.hasLayer(temporaryLayerId) && layerScene.userLayerCount() == 2,
          QStringLiteral("删除空图层后图层数量减少"));
    layerUndo.undo();
    check(layerScene.hasLayer(temporaryLayerId) && layerScene.activeLayerId() == temporaryLayerId,
          QStringLiteral("撤销删除图层"));
    layerUndo.redo();
    check(!layerScene.hasLayer(temporaryLayerId), QStringLiteral("重做删除图层"));

    check(layerScene.adjustItemHeight(upper, 1),
          QStringLiteral("图元高度 +1 进入撤销栈"));
    check(upper->height() == 3 && upper->zValue() > lower->zValue(),
          QStringLiteral("height +1 后图元位于同层更高处"));
    layerUndo.undo();
    check(upper->height() == 2, QStringLiteral("撤销 height +1"));
    layerUndo.redo();
    check(upper->height() == 3, QStringLiteral("重做 height +1"));
    layerUndo.undo();

    check(layerScene.adjustItemHeight(lower, 1),
          QStringLiteral("height +1 遇到同高图元时交换"));
    check(lower->height() == 2 && upper->height() == 1 && lower->zValue() > upper->zValue(),
          QStringLiteral("height +1 跨过同层相邻图元"));
    layerUndo.undo();
    check(lower->height() == 1 && upper->height() == 2 && lower->zValue() < upper->zValue(),
          QStringLiteral("撤销 height 交换"));
    layerUndo.redo();
    check(lower->height() == 2 && upper->height() == 1,
          QStringLiteral("重做 height 交换"));
    layerUndo.undo();

    check(layerScene.adjustItemHeight(upper, -1),
          QStringLiteral("图元高度 -1 进入撤销栈"));
    check(upper->height() == 1 && lower->height() == 2 && upper->zValue() < lower->zValue(),
          QStringLiteral("height -1 后图元位于同层更低处"));
    layerUndo.undo();
    check(upper->height() == 2 && lower->height() == 1,
          QStringLiteral("撤销 height -1"));

    check(layerScene.adjustItemHeight(lower, -1) && lower->height() == 0,
          QStringLiteral("height 可以降到 0"));
    check(!layerScene.adjustItemHeight(lower, -1) && lower->height() == 0,
          QStringLiteral("height 不会低于 0"));
    layerUndo.undo();
    check(lower->height() == 1, QStringLiteral("撤销 height 降到 0"));

    check(layerScene.moveItemToAdjacentLayer(lower, 1),
          QStringLiteral("图元移到上一层图层"));
    check(lower->layerId() == topLayerId && lower->zValue() > topLine->zValue(),
          QStringLiteral("跨图层移动后覆盖目标层图元"));
    layerUndo.undo();
    check(lower->layerId() == layerScene.layers().at(1).id && lower->height() == 1,
          QStringLiteral("撤销图元跨图层移动"));
    layerUndo.redo();
    check(lower->layerId() == topLayerId,
          QStringLiteral("重做图元跨图层移动"));
    layerUndo.undo();

    const QJsonObject layerRoot = layerScene.toJson();
    check(layerRoot.value(QStringLiteral("version")).toInt() == 2
              && layerRoot.value(QStringLiteral("layers")).toArray().size() == 3
              && layerRoot.value(QStringLiteral("activeLayer")).toInt()
                     == layerScene.activeLayerId(),
          QStringLiteral("模板保存图层与当前图层"));
    const QByteArray layerBytes = QJsonDocument(layerRoot).toJson(QJsonDocument::Compact);
    check(layerBytes.contains("\"layer\":") && layerBytes.contains("\"height\":"),
          QStringLiteral("模板保存图元 layer 与 height"));

    BoardScene layerLoaded;
    QString layerLoadError;
    check(layerLoaded.fromJson(layerRoot, &layerLoadError),
          QStringLiteral("图层模板反序列化"), layerLoadError);
    check(layerLoaded.layers() == layerScene.layers()
              && layerLoaded.activeLayerId() == layerScene.activeLayerId(),
          QStringLiteral("图层顺序与当前图层恢复"));
    check(layerLoaded.boardItemCount() == 3, QStringLiteral("图层模板图元数量一致"));
    bool layerRoundTrip = layerLoaded.boardItemCount() == layerScene.boardItemCount();
    for (const BoardLayer& entry : layerScene.layers()) {
        if (layerLoaded.layerItemCount(entry.id) != layerScene.layerItemCount(entry.id)) {
            layerRoundTrip = false;
            break;
        }
    }
    check(layerRoundTrip, QStringLiteral("图层内图元归属恢复"));

    QJsonObject legacyRoot = layerRoot;
    legacyRoot.remove(QStringLiteral("layers"));
    legacyRoot.remove(QStringLiteral("activeLayer"));
    QJsonArray legacyItems = legacyRoot.value(QStringLiteral("items")).toArray();
    for (int i = 0; i < legacyItems.size(); ++i) {
        QJsonObject itemJson = legacyItems.at(i).toObject();
        itemJson.remove(QStringLiteral("layer"));
        itemJson.remove(QStringLiteral("height"));
        legacyItems.replace(i, itemJson);
    }
    legacyRoot.insert(QStringLiteral("items"), legacyItems);
    BoardScene legacyScene;
    QString legacyError;
    check(legacyScene.fromJson(legacyRoot, &legacyError),
          QStringLiteral("兼容旧版无图层模板"), legacyError);
    check(legacyScene.userLayerCount() == 1, QStringLiteral("旧版模板归入单一图层"));
    bool legacyHeights = true;
    QSet<int> legacyHeightSet;
    const QList<BoardItem*> legacyItemsInScene = legacyScene.boardItems();
    for (BoardItem* item : legacyItemsInScene) {
        if (item->layerId() != legacyScene.layers().at(1).id
            || item->height() <= 0 || legacyHeightSet.contains(item->height())) {
            legacyHeights = false;
            break;
        }
        legacyHeightSet.insert(item->height());
    }
    check(legacyHeights, QStringLiteral("旧版 z 顺序转换为唯一 height"));

    // ---------------------------------------------------------------
    // Layer visibility and active-layer-only editing.
    // ---------------------------------------------------------------
    const int lowerLayerId = layerScene.layers().at(1).id;
    layerScene.setMode(ToolMode::Edit);
    check(layerScene.isItemEditable(topLine)
              && !layerScene.isItemEditable(lower)
              && !layerScene.isItemEditable(upper),
          QStringLiteral("编辑模式仅当前图层图元可编辑"));
    check((topLine->flags() & QGraphicsItem::ItemIsSelectable) != 0
              && (lower->flags() & QGraphicsItem::ItemIsSelectable) == 0
              && (upper->flags() & QGraphicsItem::ItemIsSelectable) == 0,
          QStringLiteral("非当前图层图元不接收选择"));

    layerScene.selectAllItems();
    check(topLine->isSelected() && !lower->isSelected() && !upper->isSelected(),
          QStringLiteral("全选只选择当前图层图元"));

    layerScene.setActiveLayer(lowerLayerId);
    check(layerScene.activeLayerId() == lowerLayerId
              && layerScene.isLayerVisible(lowerLayerId),
          QStringLiteral("切换图层后当前图层可见"));
    check(layerScene.isItemEditable(lower) && layerScene.isItemEditable(upper)
              && !layerScene.isItemEditable(topLine),
          QStringLiteral("切换图层后仅新图层图元可编辑"));
    check(!topLine->isSelected(), QStringLiteral("切换图层清除旧图层选中状态"));

    check(layerScene.setLayerVisible(topLayerId, false),
          QStringLiteral("隐藏非当前图层"));
    check(!layerScene.isLayerVisible(topLayerId) && !topLine->isVisible(),
          QStringLiteral("隐藏图层后图元不可见"));
    check(layerScene.isItemEditable(lower),
          QStringLiteral("隐藏其他图层不影响当前图层编辑"));

    check(layerScene.setLayerVisible(topLayerId, true),
          QStringLiteral("显示非当前图层"));
    check(topLine->isVisible() && !layerScene.isItemEditable(topLine),
          QStringLiteral("显示非当前图层后仍不可编辑"));

    layerScene.setActiveLayer(topLayerId);
    check(layerScene.isItemEditable(topLine) && !layerScene.isItemEditable(lower),
          QStringLiteral("选中图层后仅该图层可编辑"));

    check(layerScene.setLayerVisible(topLayerId, false),
          QStringLiteral("隐藏当前图层"));
    check(layerScene.activeLayerId() == lowerLayerId
              && !layerScene.isLayerVisible(topLayerId)
              && layerScene.isLayerVisible(lowerLayerId),
          QStringLiteral("隐藏当前图层自动切换到其他可见图层"));
    check(layerScene.isItemEditable(lower) && !layerScene.isItemEditable(topLine),
          QStringLiteral("隐藏当前图层后新图层接管编辑"));

    const QRectF hiddenContent = layerScene.contentRect();
    check(hiddenContent.right() < topLine->sceneContentRect().right() - 1.0,
          QStringLiteral("隐藏图层不参与内容范围"),
          QStringLiteral("contentRight=%1 hiddenItemRight=%2")
              .arg(hiddenContent.right())
              .arg(topLine->sceneContentRect().right()));

    layerScene.setActiveLayer(topLayerId);
    check(layerScene.isLayerVisible(topLayerId) && topLine->isVisible()
              && layerScene.isItemEditable(topLine),
          QStringLiteral("选中隐藏图层时自动显示并允许编辑"));

    BackgroundState visibilityBackground;
    visibilityBackground.type = BackgroundType::Color;
    visibilityBackground.color = QColor(QStringLiteral("#FEF3C7"));
    layerScene.applyBackground(visibilityBackground);
    check(layerScene.setLayerVisible(BoardScene::BackgroundLayerId, false),
          QStringLiteral("隐藏背景层"));
    const QImage noBackgroundImage = layerScene.renderImage(layerScene.boardRect(), 1.0);
    check(noBackgroundImage.pixelColor(5, 5) == QColor(Qt::white),
          QStringLiteral("隐藏背景层后导出为白色底"));
    check(layerScene.setLayerVisible(BoardScene::BackgroundLayerId, true),
          QStringLiteral("显示背景层"));
    const QImage backgroundImage = layerScene.renderImage(layerScene.boardRect(), 1.0);
    check(backgroundImage.pixelColor(5, 5) == visibilityBackground.color,
          QStringLiteral("显示背景层后恢复背景颜色"));

    check(layerScene.setLayerVisible(lowerLayerId, false),
          QStringLiteral("隐藏带图元的下层"));
    check(layerScene.setLayerVisible(BoardScene::BackgroundLayerId, false),
          QStringLiteral("模板保存前隐藏背景"));
    const QJsonObject visibilityRoot = layerScene.toJson();
    const QJsonArray visibilityLayers = visibilityRoot.value(QStringLiteral("layers")).toArray();
    bool lowerHiddenInJson = false;
    bool backgroundHiddenInJson = false;
    for (const QJsonValue& value : visibilityLayers) {
        const QJsonObject layerJson = value.toObject();
        const int id = layerJson.value(QStringLiteral("id")).toInt();
        if (id == lowerLayerId)
            lowerHiddenInJson = !layerJson.value(QStringLiteral("visible")).toBool(true);
        if (id == BoardScene::BackgroundLayerId)
            backgroundHiddenInJson = !layerJson.value(QStringLiteral("visible")).toBool(true);
    }
    check(lowerHiddenInJson && backgroundHiddenInJson,
          QStringLiteral("模板保存图层显示/隐藏状态"));

    BoardScene visibilityLoaded;
    QString visibilityLoadError;
    check(visibilityLoaded.fromJson(visibilityRoot, &visibilityLoadError),
          QStringLiteral("图层显示状态模板反序列化"), visibilityLoadError);
    check(!visibilityLoaded.isLayerVisible(lowerLayerId)
              && !visibilityLoaded.isLayerVisible(BoardScene::BackgroundLayerId)
              && visibilityLoaded.isLayerVisible(topLayerId),
          QStringLiteral("模板恢复图层显示/隐藏状态"));

    QJsonObject hiddenActiveRoot = visibilityRoot;
    QJsonArray hiddenActiveLayers = hiddenActiveRoot.value(QStringLiteral("layers")).toArray();
    for (int i = 0; i < hiddenActiveLayers.size(); ++i) {
        QJsonObject layerJson = hiddenActiveLayers.at(i).toObject();
        if (layerJson.value(QStringLiteral("id")).toInt() == topLayerId) {
            layerJson.insert(QStringLiteral("visible"), false);
            hiddenActiveLayers.replace(i, layerJson);
        }
    }
    hiddenActiveRoot.insert(QStringLiteral("layers"), hiddenActiveLayers);
    BoardScene hiddenActiveScene;
    QString hiddenActiveError;
    check(hiddenActiveScene.fromJson(hiddenActiveRoot, &hiddenActiveError),
          QStringLiteral("载入当前图层被隐藏的模板"), hiddenActiveError);
    check(hiddenActiveScene.activeLayerId() == topLayerId
              && hiddenActiveScene.isLayerVisible(topLayerId),
          QStringLiteral("载入时自动显示当前图层"));

    // A hidden layer must also be absent from actual rendering.
    BoardScene hiddenPaintScene;
    hiddenPaintScene.applyBoardSize(QSizeF(120, 120));
    hiddenPaintScene.setShowGrid(false);
    hiddenPaintScene.setMode(ToolMode::Edit);
    QPixmap redPaint(80, 80);
    redPaint.fill(QColor(QStringLiteral("#DC2626")));
    QPixmap bluePaint(80, 80);
    bluePaint.fill(QColor(QStringLiteral("#2563EB")));

    auto* redImage = new ImageItem();
    redImage->setImage(redPaint, QStringLiteral("red"));
    redImage->setContentSize(QSizeF(80, 80));
    redImage->setPos(QPointF(10, 10));
    hiddenPaintScene.addItemsWithUndo({ redImage }, QStringLiteral("添加红色图元"));

    check(hiddenPaintScene.addLayerWithUndo(QStringLiteral("蓝色层")),
          QStringLiteral("隐藏渲染测试新建上层"));
    auto* blueImage = new ImageItem();
    blueImage->setImage(bluePaint, QStringLiteral("blue"));
    blueImage->setContentSize(QSizeF(80, 80));
    blueImage->setPos(QPointF(20, 20));
    hiddenPaintScene.addItemsWithUndo({ blueImage }, QStringLiteral("添加蓝色图元"));
    const int hiddenBlueLayerId = hiddenPaintScene.activeLayerId();

    check(hiddenPaintScene.setLayerVisible(hiddenBlueLayerId, false),
          QStringLiteral("隐藏蓝色图层"));
    check(hiddenPaintScene.activeLayerId() != hiddenBlueLayerId,
          QStringLiteral("隐藏当前图层后自动切换编辑图层"));
    const QImage paintedHidden = hiddenPaintScene.renderImage(hiddenPaintScene.boardRect(), 1.0);
    check(paintedHidden.pixelColor(30, 30) == QColor(QStringLiteral("#DC2626")),
          QStringLiteral("隐藏图层图元不参与渲染"));
    check(hiddenPaintScene.contentRect().right()
              < blueImage->sceneContentRect().right() - 1.0,
          QStringLiteral("隐藏图层图元不参与内容外接矩形"));

    check(hiddenPaintScene.setLayerVisible(hiddenBlueLayerId, true),
          QStringLiteral("显示蓝色图层"));
    const QImage paintedShown = hiddenPaintScene.renderImage(hiddenPaintScene.boardRect(), 1.0);
    check(paintedShown.pixelColor(30, 30) == QColor(QStringLiteral("#2563EB")),
          QStringLiteral("显示图层后图元重新参与渲染"));
    check(!hiddenPaintScene.isItemEditable(blueImage)
              && hiddenPaintScene.isItemEditable(redImage),
          QStringLiteral("显示非当前图层后仍只有当前图层可编辑"));
    hiddenPaintScene.setActiveLayer(hiddenBlueLayerId);
    check(hiddenPaintScene.isItemEditable(blueImage)
              && !hiddenPaintScene.isItemEditable(redImage),
          QStringLiteral("选中蓝色图层后其图元可编辑"));

    BoardScene singleLayerScene;
    singleLayerScene.setMode(ToolMode::Edit);
    const int singleLayerId = singleLayerScene.activeLayerId();
    check(!singleLayerScene.setLayerVisible(singleLayerId, false)
              && singleLayerScene.isLayerVisible(singleLayerId),
          QStringLiteral("最后一个可编辑图层不能被隐藏"));

    // ---------------------------------------------------------------
    // Text items and the per-layer unique-height invariant.
    // ---------------------------------------------------------------
    BoardScene textScene;
    QUndoStack textUndo;
    textUndo.setUndoLimit(100);
    textScene.setUndoStack(&textUndo);
    textScene.applyBoardSize(QSizeF(900, 600));
    textScene.setShowGrid(false);

    QFont headingFont(QStringLiteral("Microsoft YaHei UI"));
    headingFont.setPointSize(36);
    headingFont.setBold(true);
    auto* heading = new TextItem();
    heading->setTextFormat(QStringLiteral("第一行标题\n第二行文字"),
                           headingFont, QColor(QStringLiteral("#1D4ED8")));
    heading->setPos(QPointF(80, 70));
    const QSizeF headingNatural = heading->naturalSize();
    textScene.addItemsWithUndo({ heading }, QStringLiteral("添加文字"));

    check(heading->kind() == QStringLiteral("text")
              && textScene.boardItemCount() == 1,
          QStringLiteral("文字图元加入场景"));
    check(heading->text() == QStringLiteral("第一行标题\n第二行文字"),
          QStringLiteral("文字内容保存"));
    check(heading->textFont().bold()
              && qAbs(heading->textFont().pointSizeF() - 36.0) < 0.1,
          QStringLiteral("文字字体样式保存"));
    check(heading->textColor() == QColor(QStringLiteral("#1D4ED8")),
          QStringLiteral("文字颜色保存"));
    check(heading->contentSize().width() >= headingNatural.width() - 0.5
              && heading->contentSize().height() >= headingNatural.height() - 0.5,
          QStringLiteral("文字图元按内容计算尺寸"),
          geometryText(heading->pos(), heading->contentSize()));
    check(heading->layerId() == textScene.activeLayerId() && heading->height() == 1,
          QStringLiteral("文字进入当前图层并从 height 1 开始"));
    check(textScene.layerHeightsUnique(textScene.activeLayerId()),
          QStringLiteral("文字加入后层内 height 唯一"));

    const QImage textRendered = textScene.renderImage(textScene.boardRect(), 1.0);
    const QRect textArea = heading->sceneContentRect().toAlignedRect()
                               .intersected(textRendered.rect());
    int textInkPixels = 0;
    for (int y = textArea.top(); y <= textArea.bottom(); ++y) {
        for (int x = textArea.left(); x <= textArea.right(); ++x) {
            if (textRendered.pixelColor(x, y).lightness() < 240)
                ++textInkPixels;
        }
    }
    check(textInkPixels > 40, QStringLiteral("文字图元实际渲染到画板"),
          QStringLiteral("ink=%1").arg(textInkPixels));

    textUndo.undo();
    check(textScene.boardItemCount() == 0, QStringLiteral("撤销添加文字"));
    textUndo.redo();
    check(textScene.boardItemCount() == 1, QStringLiteral("重做添加文字"));

    QFont editedFont(QStringLiteral("Microsoft YaHei UI"));
    editedFont.setPointSize(24);
    editedFont.setItalic(true);
    check(textScene.changeTextWithUndo(heading, QStringLiteral("更新后的文字"),
                                       editedFont, QColor(QStringLiteral("#B91C1C")),
                                       QStringLiteral("编辑文字")),
          QStringLiteral("编辑文字进入撤销栈"));
    check(heading->text() == QStringLiteral("更新后的文字")
              && heading->textFont().italic()
              && heading->textColor() == QColor(QStringLiteral("#B91C1C")),
          QStringLiteral("编辑文字内容与样式生效"));
    textUndo.undo();
    check(heading->text() == QStringLiteral("第一行标题\n第二行文字")
              && heading->textFont().bold()
              && heading->textColor() == QColor(QStringLiteral("#1D4ED8")),
          QStringLiteral("撤销编辑文字"));
    textUndo.redo();
    check(heading->text() == QStringLiteral("更新后的文字"),
          QStringLiteral("重做编辑文字"));

    auto* secondText = new TextItem();
    QFont bodyFont(QStringLiteral("Microsoft YaHei UI"));
    bodyFont.setPointSize(22);
    secondText->setTextFormat(QStringLiteral("第二段文字"),
                              bodyFont, QColor(QStringLiteral("#0F766E")));
    secondText->setPos(QPointF(120, 260));
    textScene.addItemsWithUndo({ secondText }, QStringLiteral("添加第二段文字"));
    check(secondText->height() == 2 && textScene.layerHeightsUnique(textScene.activeLayerId()),
          QStringLiteral("同层文字高度继续递增且唯一"));

    check(textScene.adjustItemHeight(secondText, -1),
          QStringLiteral("文字图元支持高度 -1"));
    check(secondText->height() == 1 && heading->height() == 2
              && textScene.layerHeightsUnique(textScene.activeLayerId()),
          QStringLiteral("文字高度交换后仍然唯一"));
    textUndo.undo();
    check(secondText->height() == 2 && heading->height() == 1,
          QStringLiteral("撤销文字高度交换"));

    const QJsonObject textRoot = textScene.toJson();
    check(textRoot.value(QStringLiteral("version")).toInt() == 2
              && QJsonDocument(textRoot).toJson(QJsonDocument::Compact).contains("\"text\""),
          QStringLiteral("模板保存文字图元"));
    BoardScene textLoaded;
    QString textLoadError;
    check(textLoaded.fromJson(textRoot, &textLoadError),
          QStringLiteral("模板反序列化文字图元"), textLoadError);
    TextItem* loadedText = nullptr;
    for (BoardItem* item : textLoaded.boardItems()) {
        if (auto* text = dynamic_cast<TextItem*>(item)) {
            loadedText = text;
            break;
        }
    }
    check(loadedText != nullptr, QStringLiteral("模板恢复文字类型"));
    if (loadedText) {
        check(loadedText->text() == QStringLiteral("更新后的文字")
                  && loadedText->textFont().italic()
                  && loadedText->textColor() == QColor(QStringLiteral("#B91C1C")),
              QStringLiteral("模板恢复文字内容与样式"));
        check(loadedText->layerId() == textScene.activeLayerId()
                  && loadedText->height() == heading->height(),
              QStringLiteral("模板恢复文字图层与高度"));
        check(qAbs(loadedText->contentSize().width() - heading->contentSize().width()) < 0.5
                  && qAbs(loadedText->contentSize().height() - heading->contentSize().height()) < 0.5,
              QStringLiteral("模板恢复文字尺寸"));
    }
    check(textLoaded.layerHeightsUnique(textLoaded.activeLayerId()),
          QStringLiteral("模板载入后层内 height 唯一"));

    // A hand-edited template may contain duplicate heights. Loading it must
    // repair them to a stable, unique sequence while preserving item order.
    QJsonObject duplicateRoot = textScene.toJson();
    QJsonArray duplicateItems = duplicateRoot.value(QStringLiteral("items")).toArray();
    for (int i = 0; i < duplicateItems.size(); ++i) {
        QJsonObject itemJson = duplicateItems.at(i).toObject();
        itemJson.insert(QStringLiteral("height"), 1);
        duplicateItems.replace(i, itemJson);
    }
    duplicateRoot.insert(QStringLiteral("items"), duplicateItems);
    BoardScene repairedScene;
    QString repairedError;
    check(repairedScene.fromJson(duplicateRoot, &repairedError),
          QStringLiteral("载入含重复 height 的模板"), repairedError);
    check(repairedScene.layerHeightsUnique(repairedScene.activeLayerId())
              && repairedScene.boardItemCount() == 2,
          QStringLiteral("重复 height 载入后自动修复为唯一值"));
    QSet<int> repairedHeights;
    for (BoardItem* item : repairedScene.boardItems())
        repairedHeights.insert(item->height());
    check(repairedHeights == QSet<int>({ 1, 2 }),
          QStringLiteral("修复后的 height 序列连续且唯一"));

    QJsonObject oversizedRoot = textScene.toJson();
    QJsonArray oversizedItems = oversizedRoot.value(QStringLiteral("items")).toArray();
    for (int i = 0; i < oversizedItems.size(); ++i) {
        QJsonObject itemJson = oversizedItems.at(i).toObject();
        itemJson.insert(QStringLiteral("height"), 1000000 + i);
        oversizedItems.replace(i, itemJson);
    }
    oversizedRoot.insert(QStringLiteral("items"), oversizedItems);
    BoardScene oversizedScene;
    QString oversizedError;
    check(oversizedScene.fromJson(oversizedRoot, &oversizedError),
          QStringLiteral("载入超出高度上限的模板"), oversizedError);
    bool oversizedRepaired = oversizedScene.layerHeightsUnique(oversizedScene.activeLayerId());
    for (BoardItem* item : oversizedScene.boardItems())
        oversizedRepaired = oversizedRepaired && item->height() <= 999999;
    check(oversizedRepaired,
          QStringLiteral("超限 height 载入后修复到合法唯一范围"));

    // ---------------------------------------------------------------
    // Shape items: common diagrams, quick insertion geometry and rendering.
    // ---------------------------------------------------------------
    BoardScene shapeScene;
    QUndoStack shapeUndo;
    shapeUndo.setUndoLimit(100);
    shapeScene.setUndoStack(&shapeUndo);
    shapeScene.applyBoardSize(QSizeF(1000, 800));
    shapeScene.setShowGrid(false);

    const QList<ShapeType> shapeTypes = allShapeTypes();
    check(shapeTypes.size() == 12, QStringLiteral("提供 12 种常见图形"),
          QStringLiteral("count=%1").arg(shapeTypes.size()));
    check(shapeTypeFromString(QStringLiteral("circle")) == ShapeType::Ellipse
              && shapeTypeFromString(QStringLiteral("roundrect")) == ShapeType::RoundedRectangle
              && shapeTypeFromString(QStringLiteral("double_arrow")) == ShapeType::DoubleArrow
              && shapeTypeFromString(QStringLiteral("leftbrace")) == ShapeType::BraceLeft
              && shapeTypeFromString(QStringLiteral("speech")) == ShapeType::Callout,
          QStringLiteral("图形类型字符串兼容常见别名"));
    check(shapeTypeFromString(QStringLiteral("does-not-exist")) == ShapeType::Rectangle,
          QStringLiteral("未知图形类型回退为矩形"));

    bool shapePathsOk = true;
    bool shapeIconsOk = true;
    for (ShapeType type : shapeTypes) {
        if (ShapeItem::pathFor(type, QRectF(0, 0, 140, 90)).isEmpty())
            shapePathsOk = false;
        if (makeShapeIcon(type, QColor(58, 64, 80), 18).isNull())
            shapeIconsOk = false;
        if (shapeTypeName(type).isEmpty())
            shapePathsOk = false;
    }
    check(shapePathsOk, QStringLiteral("每种图形都能生成有效路径与名称"));
    check(shapeIconsOk, QStringLiteral("每种图形都能生成菜单图标"));

    QList<ShapeItem*> shapeItems;
    const QColor shapeStroke(QStringLiteral("#1D4ED8"));
    const QColor shapeFill(29, 78, 216, 40);
    for (int i = 0; i < shapeTypes.size(); ++i) {
        const ShapeType type = shapeTypes.at(i);
        auto* item = new ShapeItem(type);

        QSizeF size(150, 90);
        if (type == ShapeType::BraceLeft || type == ShapeType::BraceRight)
            size = QSizeF(60, 160);
        else if (type == ShapeType::Line)
            size = QSizeF(160, 50);
        else if (type == ShapeType::Arrow || type == ShapeType::DoubleArrow)
            size = QSizeF(180, 80);
        else if (type == ShapeType::Star || type == ShapeType::Triangle
                 || type == ShapeType::Diamond) {
            size = QSizeF(120, 110);
        }

        const int column = i % 4;
        const int row = i / 4;
        const QPointF cell(40.0 + column * 240.0, 40.0 + row * 240.0);
        item->setPos(cell + QPointF((220.0 - size.width()) / 2.0,
                                    (220.0 - size.height()) / 2.0));
        item->setContentSize(size);
        item->setStyle(shapeStroke,
                       shapeTypeIsFilled(type) ? shapeFill : QColor(Qt::transparent),
                       4.0);
        shapeScene.addItemsWithUndo({ item }, QStringLiteral("插入%1").arg(shapeTypeName(type)));
        shapeItems.append(item);

        check(item->kind() == QStringLiteral("shape")
                  && item->shapeType() == type
                  && item->layerId() == shapeScene.activeLayerId()
                  && item->height() == i + 1,
              QStringLiteral("图形图元 %1 进入当前图层且高度唯一").arg(shapeTypeName(type)),
              QStringLiteral("layer=%1 height=%2").arg(item->layerId()).arg(item->height()));
    }
    check(shapeScene.boardItemCount() == shapeTypes.size(),
          QStringLiteral("所有常见图形加入场景"));
    check(shapeScene.layerHeightsUnique(shapeScene.activeLayerId()),
          QStringLiteral("图形同层 height 唯一"));

    const QImage shapesRendered = shapeScene.renderImage(shapeScene.boardRect(), 1.0);
    bool everyShapeVisible = !shapesRendered.isNull();
    QString invisibleShape;
    for (ShapeItem* item : shapeItems) {
        const QRect area = item->sceneContentRect().toAlignedRect()
                               .adjusted(-3, -3, 3, 3)
                               .intersected(shapesRendered.rect());
        int inkPixels = 0;
        for (int y = area.top(); y <= area.bottom(); ++y) {
            for (int x = area.left(); x <= area.right(); ++x) {
                if (shapesRendered.pixelColor(x, y) != QColor(Qt::white))
                    ++inkPixels;
            }
        }
        if (inkPixels < 20) {
            everyShapeVisible = false;
            invisibleShape = shapeTypeName(item->shapeType());
            break;
        }
    }
    check(everyShapeVisible, QStringLiteral("每种图形都实际渲染到画板"),
          invisibleShape);

    ShapeItem* lastShape = shapeItems.constLast();
    shapeUndo.undo();
    check(shapeScene.boardItemCount() == shapeTypes.size() - 1,
          QStringLiteral("撤销插入图形"));
    shapeUndo.redo();
    check(shapeScene.boardItemCount() == shapeTypes.size() && lastShape->scene() == &shapeScene,
          QStringLiteral("重做插入图形"));

    ShapeItem* firstShape = shapeItems.constFirst();
    const QPointF shapePosBefore = firstShape->pos();
    const QSizeF shapeSizeBefore = firstShape->contentSize();
    const ItemGeometry shapeBefore { firstShape, shapePosBefore, shapeSizeBefore };
    firstShape->setPos(shapePosBefore + QPointF(28, 20));
    firstShape->setContentSize(shapeSizeBefore + QSizeF(24, 16));
    const ItemGeometry shapeAfter { firstShape, firstShape->pos(), firstShape->contentSize() };
    shapeScene.pushGeometry({ shapeBefore }, { shapeAfter }, QStringLiteral("缩放图形"));
    shapeUndo.undo();
    check(firstShape->pos() == shapePosBefore && firstShape->contentSize() == shapeSizeBefore,
          QStringLiteral("撤销图形移动与缩放"));
    shapeUndo.redo();
    check(firstShape->pos() != shapePosBefore
              && firstShape->contentSize() != shapeSizeBefore,
          QStringLiteral("重做图形移动与缩放"));

    check(shapeScene.adjustItemHeight(firstShape, 1),
          QStringLiteral("图形支持层内高度调整"));
    check(firstShape->height() == 2 && shapeScene.layerHeightsUnique(firstShape->layerId()),
          QStringLiteral("图形高度交换后仍然唯一"));
    shapeUndo.undo();
    check(firstShape->height() == 1, QStringLiteral("撤销图形高度调整"));

    const QJsonObject shapeRoot = shapeScene.toJson();
    const QByteArray shapeBytes = QJsonDocument(shapeRoot).toJson(QJsonDocument::Compact);
    check(shapeBytes.contains("\"type\":\"shape\"")
              && shapeBytes.contains("\"shapeType\":\"rectangle\"")
              && shapeBytes.contains("\"strokeColor\"")
              && shapeBytes.contains("\"fillColor\""),
          QStringLiteral("模板保存图形类型、描边与填充"));

    BoardScene shapeLoaded;
    QUndoStack shapeLoadedUndo;
    shapeLoaded.setUndoStack(&shapeLoadedUndo);
    QString shapeLoadError;
    check(shapeLoaded.fromJson(shapeRoot, &shapeLoadError),
          QStringLiteral("模板反序列化图形图元"), shapeLoadError);
    check(shapeLoaded.boardItemCount() == shapeItems.size(),
          QStringLiteral("模板恢复全部图形"));
    check(shapeLoaded.layerHeightsUnique(shapeLoaded.activeLayerId()),
          QStringLiteral("模板恢复图形后 height 唯一"));
    QSet<QString> loadedShapeTypes;
    ShapeItem* loadedRectangle = nullptr;
    for (BoardItem* item : shapeLoaded.boardItems()) {
        if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
            loadedShapeTypes.insert(shapeTypeToString(shape->shapeType()));
            if (shape->shapeType() == ShapeType::Rectangle && !loadedRectangle)
                loadedRectangle = shape;
        }
    }
    check(loadedShapeTypes.size() == shapeTypes.size(),
          QStringLiteral("模板恢复全部 12 种图形类型"));
    check(loadedRectangle
              && loadedRectangle->strokeColor() == shapeStroke
              && loadedRectangle->fillColor().alpha() == shapeFill.alpha()
              && qAbs(loadedRectangle->strokeWidth() - 4.0) < 0.01,
          QStringLiteral("模板恢复图形描边、填充和线宽"));

    TextDialog textDialog;
    textDialog.setTextValue(QStringLiteral("对话框文字\n第二行"));
    QFont dialogFont(QStringLiteral("Microsoft YaHei UI"));
    dialogFont.setPointSize(22);
    dialogFont.setBold(true);
    dialogFont.setItalic(true);
    textDialog.setTextFont(dialogFont);
    textDialog.setTextColor(QColor(QStringLiteral("#7C3AED")));
    check(textDialog.textValue() == QStringLiteral("对话框文字\n第二行"),
          QStringLiteral("文字对话框保留换行内容"));
    check(qAbs(textDialog.textFont().pointSizeF() - 22.0) < 0.1
              && textDialog.textFont().bold()
              && textDialog.textFont().italic(),
          QStringLiteral("文字对话框返回字体样式"));
    check(textDialog.textColor() == QColor(QStringLiteral("#7C3AED")),
          QStringLiteral("文字对话框返回颜色"));

    // Exercise the actual layer panel buttons, not only the scene API.
    BoardScene panelScene;
    QUndoStack panelUndo;
    panelUndo.setUndoLimit(50);
    panelScene.setUndoStack(&panelUndo);
    LayerPanel panel(&panelScene);
    panel.resize(240, 360);

    auto* panelList = panel.findChild<QListWidget*>(QStringLiteral("LayerList"));
    QToolButton* panelAdd = nullptr;
    QToolButton* panelRemove = nullptr;
    QToolButton* panelUp = nullptr;
    QToolButton* panelDown = nullptr;
    const QList<QToolButton*> panelButtons = panel.findChildren<QToolButton*>();
    for (QToolButton* button : panelButtons) {
        if (button->text() == QStringLiteral("+"))
            panelAdd = button;
        else if (button->text() == QStringLiteral("−"))
            panelRemove = button;
        else if (button->text() == QStringLiteral("↑"))
            panelUp = button;
        else if (button->text() == QStringLiteral("↓"))
            panelDown = button;
    }
    check(panelList && panelAdd && panelRemove && panelUp && panelDown,
          QStringLiteral("图层面板创建列表与按钮"));
    check(panelList && panelList->count() == 2,
          QStringLiteral("图层面板显示背景与一个用户图元"));

    if (panelList && panelAdd && panelRemove && panelUp && panelDown) {
        panelAdd->click();
        QApplication::processEvents();
        const int panelTopId = panelScene.activeLayerId();
        check(panelScene.userLayerCount() == 2 && panelList->count() == 3
                  && panelScene.layerIndex(panelTopId) == 2,
              QStringLiteral("面板新建图层位于最上层"));

        panelDown->click();
        QApplication::processEvents();
        check(panelScene.layerIndex(panelTopId) == 1,
              QStringLiteral("面板按钮下移图层"));

        panelUp->click();
        QApplication::processEvents();
        check(panelScene.layerIndex(panelTopId) == 2,
              QStringLiteral("面板按钮上移图层"));

        auto* panelItem = makeLineItem(QPointF(10, 10), QPointF(80, 80),
                                       QColor(QStringLiteral("#7C3AED")));
        panelScene.addItemsWithUndo({ panelItem }, QStringLiteral("面板测试图元"));
        QApplication::processEvents();
        check(!panelRemove->isEnabled(),
              QStringLiteral("非空图层禁用面板删除按钮"));

        panelUndo.undo();
        QApplication::processEvents();
        check(panelRemove->isEnabled(),
              QStringLiteral("空图层启用面板删除按钮"));

        panelRemove->click();
        QApplication::processEvents();
        check(panelScene.userLayerCount() == 1 && panelList->count() == 2,
              QStringLiteral("面板按钮删除空图层"));

        panelUndo.undo();
        QApplication::processEvents();
        check(panelScene.userLayerCount() == 2 && panelScene.hasLayer(panelTopId),
              QStringLiteral("面板删除图层支持撤销"));
    }

    {
        BoardScene visibilityPanelScene;
        QUndoStack visibilityPanelUndo;
        visibilityPanelScene.setUndoStack(&visibilityPanelUndo);
        auto* lowerItem = makeLineItem(QPointF(8, 8), QPointF(60, 60),
                                       QColor(QStringLiteral("#0F766E")));
        visibilityPanelScene.addItemsWithUndo({ lowerItem }, QStringLiteral("面板下层图元"));
        const int lowerId = visibilityPanelScene.activeLayerId();

        visibilityPanelScene.addLayerWithUndo(QStringLiteral("面板上层"));
        const int upperId = visibilityPanelScene.activeLayerId();
        auto* upperItem = makeLineItem(QPointF(20, 20), QPointF(80, 80),
                                       QColor(QStringLiteral("#EA580C")));
        visibilityPanelScene.addItemsWithUndo({ upperItem }, QStringLiteral("面板上层图元"));

        LayerPanel visibilityPanel(&visibilityPanelScene);
        auto* visibilityList = visibilityPanel.findChild<QListWidget*>(
            QStringLiteral("LayerList"));
        auto findRow = [visibilityList](int layerId) -> QListWidgetItem* {
            if (!visibilityList)
                return nullptr;
            for (int row = 0; row < visibilityList->count(); ++row) {
                QListWidgetItem* item = visibilityList->item(row);
                if (item->data(Qt::UserRole).toInt() == layerId)
                    return item;
            }
            return nullptr;
        };

        QListWidgetItem* lowerRow = findRow(lowerId);
        check(lowerRow && lowerRow->checkState() == Qt::Checked,
              QStringLiteral("图层面板显示可见勾选状态"));
        if (lowerRow) {
            lowerRow->setCheckState(Qt::Unchecked);
            QApplication::processEvents();
            check(!visibilityPanelScene.isLayerVisible(lowerId),
                  QStringLiteral("图层面板取消勾选隐藏图层"));
            QListWidgetItem* hiddenRow = findRow(lowerId);
            check(hiddenRow && hiddenRow->checkState() == Qt::Unchecked
                      && hiddenRow->text().contains(QStringLiteral("隐藏")),
                  QStringLiteral("图层面板标记隐藏图层"));

            if (hiddenRow) {
                visibilityList->setCurrentItem(hiddenRow);
                QApplication::processEvents();
                check(visibilityPanelScene.activeLayerId() == lowerId
                          && visibilityPanelScene.isLayerVisible(lowerId),
                      QStringLiteral("选中隐藏图层自动显示"));
                QListWidgetItem* shownRow = findRow(lowerId);
                check(shownRow && shownRow->checkState() == Qt::Checked,
                      QStringLiteral("自动显示后同步勾选状态"));
                if (shownRow) {
                    shownRow->setCheckState(Qt::Unchecked);
                    QApplication::processEvents();
                    check(!visibilityPanelScene.isLayerVisible(lowerId)
                              && visibilityPanelScene.activeLayerId() == upperId,
                          QStringLiteral("隐藏当前图层后自动选择其他图层"));
                }
            }
        }

        QListWidgetItem* backgroundRow = findRow(BoardScene::BackgroundLayerId);
        check(backgroundRow && backgroundRow->checkState() == Qt::Checked,
              QStringLiteral("图层面板显示背景层勾选"));
        if (backgroundRow) {
            backgroundRow->setCheckState(Qt::Unchecked);
            QApplication::processEvents();
            check(!visibilityPanelScene.isLayerVisible(BoardScene::BackgroundLayerId),
                  QStringLiteral("图层面板可隐藏背景层"));
            QListWidgetItem* hiddenBackgroundRow = findRow(BoardScene::BackgroundLayerId);
            if (hiddenBackgroundRow) {
                hiddenBackgroundRow->setCheckState(Qt::Checked);
                QApplication::processEvents();
            }
            check(visibilityPanelScene.isLayerVisible(BoardScene::BackgroundLayerId),
                  QStringLiteral("图层面板可重新显示背景层"));
        }
    }

    // Verify the actual paint order, not just the numeric z values.
    BoardScene paintScene;
    paintScene.applyBoardSize(QSizeF(100, 100));
    QPixmap redPixmap(80, 80);
    redPixmap.fill(QColor(QStringLiteral("#DC2626")));
    QPixmap bluePixmap(80, 80);
    bluePixmap.fill(QColor(QStringLiteral("#2563EB")));

    auto* redItem = new ImageItem();
    redItem->setImage(redPixmap, QStringLiteral("red"));
    redItem->setContentSize(QSizeF(80, 80));
    redItem->setPos(QPointF(10, 10));
    paintScene.addItemsWithUndo({ redItem }, QStringLiteral("添加红色图元"));

    check(paintScene.addLayerWithUndo(QStringLiteral("蓝色层")),
          QStringLiteral("渲染测试新建上层"));
    auto* blueItem = new ImageItem();
    blueItem->setImage(bluePixmap, QStringLiteral("blue"));
    blueItem->setContentSize(QSizeF(80, 80));
    blueItem->setPos(QPointF(20, 20));
    paintScene.addItemsWithUndo({ blueItem }, QStringLiteral("添加蓝色图元"));
    const int blueLayerId = paintScene.activeLayerId();

    const QImage painted = paintScene.renderImage(paintScene.boardRect(), 1.0);
    check(painted.pixelColor(30, 30) == QColor(QStringLiteral("#2563EB")),
          QStringLiteral("上层图元实际遮盖下层图元"));
    check(painted.pixelColor(15, 15) == QColor(QStringLiteral("#DC2626")),
          QStringLiteral("未重叠区域保留下层图元"));

    paintScene.moveLayerWithUndo(blueLayerId, -1);
    const QImage paintedLower = paintScene.renderImage(paintScene.boardRect(), 1.0);
    check(paintedLower.pixelColor(30, 30) == QColor(QStringLiteral("#DC2626")),
          QStringLiteral("图层下移后实际遮盖关系反转"));

    const QString demoPath = QDir(QCoreApplication::applicationDirPath())
                                 .filePath(QStringLiteral("../../examples/demo-storyboard.sbboard"));
    if (QFileInfo::exists(demoPath)) {
        QFile demoFile(demoPath);
        check(demoFile.open(QIODevice::ReadOnly), QStringLiteral("读取示例模板文件"));
        const QJsonDocument demoDocument = QJsonDocument::fromJson(demoFile.readAll());
        BoardScene demoLoaded;
        QString demoError;
        check(demoLoaded.fromJson(demoDocument.object(), &demoError),
              QStringLiteral("加载示例模板"), demoError);
        check(demoLoaded.userLayerCount() == 3
                  && demoLoaded.activeLayerId() == demoLoaded.layers().last().id
                  && demoLoaded.boardItemCount() == 27,
              QStringLiteral("示例模板图层与图元完整"),
              QStringLiteral("layers=%1 items=%2")
                  .arg(demoLoaded.userLayerCount())
                  .arg(demoLoaded.boardItemCount()));
    }

    // ---------------------------------------------------------------
    // Interaction test: real mouse events through BoardView.
    // ---------------------------------------------------------------
    BoardScene viewScene;
    QUndoStack viewUndo;
    viewScene.setUndoStack(&viewUndo);
    viewScene.applyBoardSize(QSizeF(1200, 800));
    viewScene.setShowGrid(false);

    BoardView view(&viewScene);
    view.resize(1000, 700);
    view.show();
    QApplication::processEvents();

    check(view.mode() == ToolMode::View, QStringLiteral("默认模式为查看模式"));
    view.fitAll();
    QApplication::processEvents();

    view.setBrushColor(QColor(QStringLiteral("#2563EB")));
    view.setBrushWidth(7.0);
    view.setMode(ToolMode::Brush);

    const QPoint brushStart = view.mapFromScene(QPointF(300, 300));
    const QPoint brushEnd = view.mapFromScene(QPointF(700, 500));
    sendMouseEvent(view.viewport(), QEvent::MouseButtonPress, brushStart,
                   Qt::LeftButton, Qt::LeftButton);
    for (int i = 1; i <= 12; ++i) {
        const QPoint point(brushStart.x() + (brushEnd.x() - brushStart.x()) * i / 12,
                           brushStart.y() + (brushEnd.y() - brushStart.y()) * i / 12);
        sendMouseEvent(view.viewport(), QEvent::MouseMove, point, Qt::NoButton, Qt::LeftButton);
    }
    sendMouseEvent(view.viewport(), QEvent::MouseButtonRelease, brushEnd,
                   Qt::LeftButton, Qt::NoButton);
    QApplication::processEvents();

    check(viewScene.boardItemCount() == 1, QStringLiteral("鼠标绘制生成笔画"));
    BoardItem* drawnItem = viewScene.boardItems().value(0);
    auto* drawnStroke = dynamic_cast<StrokeItem*>(drawnItem);
    check(drawnStroke != nullptr, QStringLiteral("绘制图元类型为笔画"));
    if (drawnStroke) {
        check(drawnStroke->strokeColor() == QColor(QStringLiteral("#2563EB")),
              QStringLiteral("画笔颜色应用到图元"));
        check(qAbs(drawnStroke->strokeWidth() - 7.0) < 0.01,
              QStringLiteral("画笔粗细应用到图元"));
        check(drawnStroke->layerId() == viewScene.activeLayerId()
                  && drawnStroke->height() == 1,
              QStringLiteral("鼠标绘制落入当前图层"));
    }
    check(viewUndo.count() == 1, QStringLiteral("鼠标绘制进入撤销栈"));
    viewUndo.undo();
    check(viewScene.boardItemCount() == 0, QStringLiteral("撤销鼠标绘制"));
    viewUndo.redo();
    check(viewScene.boardItemCount() == 1, QStringLiteral("重做鼠标绘制"));

    int textPlacementRequests = 0;
    QPointF requestedTextPosition;
    QObject::connect(&view, &BoardView::textPlacementRequested, &view,
                     [&textPlacementRequests, &requestedTextPosition](const QPointF& scenePos) {
                         ++textPlacementRequests;
                         requestedTextPosition = scenePos;
                     });
    view.setMode(ToolMode::Text);
    const QPointF textClickScene(620.0, 330.0);
    const QPoint textClick = view.mapFromScene(textClickScene);
    sendMouseEvent(view.viewport(), QEvent::MouseButtonPress, textClick,
                   Qt::LeftButton, Qt::LeftButton);
    sendMouseEvent(view.viewport(), QEvent::MouseButtonRelease, textClick,
                   Qt::LeftButton, Qt::NoButton);
    QApplication::processEvents();
    check(textPlacementRequests == 1, QStringLiteral("文字模式点击画板请求放置文字"));
    if (textPlacementRequests == 1) {
        check(QLineF(requestedTextPosition, textClickScene).length() < 2.0,
              QStringLiteral("文字放置位置对应画板坐标"),
              QStringLiteral("requested=%1,%2")
                  .arg(requestedTextPosition.x()).arg(requestedTextPosition.y()));
    }
    check(viewScene.boardItemCount() == 1, QStringLiteral("文字模式点击不会直接创建图元"));
    view.setMode(ToolMode::View);
    QApplication::processEvents();

    const QPointF viewPosBefore = drawnStroke ? drawnStroke->pos() : QPointF();
    const QSizeF viewSizeBefore = drawnStroke ? drawnStroke->contentSize() : QSizeF();

    view.setMode(ToolMode::View);
    QApplication::processEvents();
    const QRectF visibleScene = view.mapToScene(view.viewport()->rect()).boundingRect();
    check(visibleScene.contains(viewScene.contentRect()),
          QStringLiteral("查看模式自动包含全部图元"),
          QStringLiteral("visible=%1,%2 %3x%4 content=%5,%6 %7x%8")
              .arg(visibleScene.x()).arg(visibleScene.y())
              .arg(visibleScene.width()).arg(visibleScene.height())
              .arg(viewScene.contentRect().x()).arg(viewScene.contentRect().y())
              .arg(viewScene.contentRect().width()).arg(viewScene.contentRect().height()));

    const QPointF centerBefore = view.mapToScene(view.viewport()->rect().center());
    const QPoint panStart = view.viewport()->rect().center();
    sendMouseEvent(view.viewport(), QEvent::MouseButtonPress, panStart,
                   Qt::LeftButton, Qt::LeftButton);
    sendMouseEvent(view.viewport(), QEvent::MouseMove, panStart + QPoint(60, 40),
                   Qt::NoButton, Qt::LeftButton);
    sendMouseEvent(view.viewport(), QEvent::MouseButtonRelease, panStart + QPoint(60, 40),
                   Qt::LeftButton, Qt::NoButton);
    const QPointF centerAfter = view.mapToScene(view.viewport()->rect().center());
    check(QLineF(centerBefore, centerAfter).length() > 1.0,
          QStringLiteral("查看模式拖拽平移整体视图"));
    if (drawnStroke) {
        check(drawnStroke->pos() == viewPosBefore && drawnStroke->contentSize() == viewSizeBefore,
              QStringLiteral("查看模式平移不修改图元坐标"));
    }

    const qreal zoomBefore = view.zoom();
    QWheelEvent wheel(QPointF(view.viewport()->rect().center()),
                      QPointF(view.viewport()->mapToGlobal(view.viewport()->rect().center())),
                      QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                      Qt::ScrollUpdate, false);
    QApplication::sendEvent(view.viewport(), &wheel);
    check(view.zoom() > zoomBefore, QStringLiteral("滚轮缩放整体视图"),
          QStringLiteral("before=%1 after=%2").arg(zoomBefore).arg(view.zoom()));
    if (drawnStroke) {
        check(drawnStroke->pos() == viewPosBefore && drawnStroke->contentSize() == viewSizeBefore,
              QStringLiteral("查看模式缩放不修改图元坐标"));
    }

    view.setMode(ToolMode::Edit);
    view.fitAll();
    QApplication::processEvents();

    const QPoint midStroke = view.mapFromScene(QPointF(500, 400));
    sendMouseEvent(view.viewport(), QEvent::MouseButtonPress, midStroke,
                   Qt::LeftButton, Qt::LeftButton);
    sendMouseEvent(view.viewport(), QEvent::MouseMove, midStroke + QPoint(55, 35),
                   Qt::NoButton, Qt::LeftButton);
    sendMouseEvent(view.viewport(), QEvent::MouseButtonRelease, midStroke + QPoint(55, 35),
                   Qt::LeftButton, Qt::NoButton);
    QApplication::processEvents();
    check(drawnStroke && drawnStroke->isSelected(), QStringLiteral("编辑模式点选图元"));
    if (drawnStroke) {
        check(drawnStroke->pos() != viewPosBefore, QStringLiteral("编辑模式拖动移动图元"),
              geometryText(drawnStroke->pos(), drawnStroke->contentSize()));
        viewUndo.undo();
        check(drawnStroke->pos() == viewPosBefore, QStringLiteral("撤销拖动移动图元"));
        viewUndo.redo();
        check(drawnStroke->pos() != viewPosBefore, QStringLiteral("重做拖动移动图元"));
    }

    if (drawnStroke) {
        const QSizeF sizeBeforeResize = drawnStroke->contentSize();
        const QPoint handleScreen = view.mapFromScene(
            drawnStroke->mapToScene(QPointF(0.0, 0.0)));
        sendMouseEvent(view.viewport(), QEvent::MouseButtonPress, handleScreen,
                       Qt::LeftButton, Qt::LeftButton);
        sendMouseEvent(view.viewport(), QEvent::MouseMove, handleScreen + QPoint(45, 35),
                       Qt::NoButton, Qt::LeftButton);
        sendMouseEvent(view.viewport(), QEvent::MouseButtonRelease, handleScreen + QPoint(45, 35),
                       Qt::LeftButton, Qt::NoButton);
        QApplication::processEvents();
        check(drawnStroke->contentSize().width() < sizeBeforeResize.width()
                  && drawnStroke->contentSize().height() < sizeBeforeResize.height(),
              QStringLiteral("编辑模式拖动控制点缩放图元"),
              geometryText(drawnStroke->pos(), drawnStroke->contentSize()));
        viewUndo.undo();
        check(drawnStroke->contentSize() == sizeBeforeResize,
              QStringLiteral("撤销缩放图元（鼠标交互）"));
        viewUndo.redo();
        check(drawnStroke->contentSize() != sizeBeforeResize,
              QStringLiteral("重做缩放图元（鼠标交互）"));

        auto* secondStroke = new StrokeItem();
        secondStroke->beginStroke(QPointF(900, 160), QColor(QStringLiteral("#0F766E")), 5.0);
        secondStroke->appendScenePoint(QPointF(1000, 220));
        secondStroke->appendScenePoint(QPointF(1100, 170));
        secondStroke->endStroke();
        viewScene.addItemsWithUndo({ secondStroke }, QStringLiteral("添加笔画"));
        view.fitAll();
        QApplication::processEvents();
        viewScene.selectAllItems();
        check(viewScene.selectedItems().size() == 2, QStringLiteral("编辑模式多选图元"));

        const QPointF firstBefore = drawnStroke->pos();
        const QPointF secondBefore = secondStroke->pos();
        const QPoint multiDragStart = view.mapFromScene(QPointF(900, 160));
        sendMouseEvent(view.viewport(), QEvent::MouseButtonPress, multiDragStart,
                       Qt::LeftButton, Qt::LeftButton);
        sendMouseEvent(view.viewport(), QEvent::MouseMove, multiDragStart + QPoint(45, 30),
                       Qt::NoButton, Qt::LeftButton);
        sendMouseEvent(view.viewport(), QEvent::MouseButtonRelease, multiDragStart + QPoint(45, 30),
                       Qt::LeftButton, Qt::NoButton);
        QApplication::processEvents();
        const QPointF firstDelta = drawnStroke->pos() - firstBefore;
        const QPointF secondDelta = secondStroke->pos() - secondBefore;
        check(QLineF(firstDelta, QPointF()).length() > 1.0,
              QStringLiteral("多选拖动移动图元"));
        check(QLineF(firstDelta, secondDelta).length() < 0.01,
              QStringLiteral("多选图元位移一致"));
        viewUndo.undo();
        check(drawnStroke->pos() == firstBefore && secondStroke->pos() == secondBefore,
              QStringLiteral("撤销多选移动"));
        viewUndo.redo();
        check(drawnStroke->pos() != firstBefore && secondStroke->pos() != secondBefore,
              QStringLiteral("重做多选移动"));
    }

    QFont doubleClickFont(QStringLiteral("Microsoft YaHei UI"));
    doubleClickFont.setPointSize(28);
    auto* doubleClickText = new TextItem();
    doubleClickText->setTextFormat(QStringLiteral("双击编辑"),
                                   doubleClickFont, QColor(QStringLiteral("#7C3AED")));
    doubleClickText->setPos(QPointF(180, 620));
    viewScene.addItemsWithUndo({ doubleClickText }, QStringLiteral("添加文字"));
    view.fitAll();
    QApplication::processEvents();
    int textEditRequests = 0;
    QObject::connect(&view, &BoardView::editTextRequested, &view,
                     [&textEditRequests](TextItem*) { ++textEditRequests; });
    const QPoint doubleClickPoint = view.mapFromScene(
        doubleClickText->sceneContentRect().center());
    QMouseEvent doubleClickEvent(
        QEvent::MouseButtonDblClick, QPointF(doubleClickPoint),
        QPointF(view.viewport()->mapToGlobal(doubleClickPoint)),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &doubleClickEvent);
    QApplication::processEvents();
    check(textEditRequests == 1, QStringLiteral("双击文字请求编辑"));

    const int baseLayerId = viewScene.activeLayerId();
    check(viewScene.addLayerWithUndo(QStringLiteral("锁定的注释")),
          QStringLiteral("创建锁定测试图层"));
    const int lockedLayerId = viewScene.activeLayerId();
    QFont lockedFont(QStringLiteral("Microsoft YaHei UI"));
    lockedFont.setPointSize(22);
    auto* lockedText = new TextItem();
    lockedText->setTextFormat(QStringLiteral("锁定图层文字"),
                              lockedFont, QColor(QStringLiteral("#0F766E")));
    lockedText->setPos(QPointF(980, 600));
    viewScene.addItemsWithUndo({ lockedText }, QStringLiteral("添加锁定图层文字"));
    viewScene.setActiveLayer(baseLayerId);
    view.fitAll();
    QApplication::processEvents();

    check(lockedLayerId != baseLayerId
              && !lockedText->isEditable()
              && (lockedText->flags() & QGraphicsItem::ItemIsSelectable) == 0,
          QStringLiteral("非当前图层文字不可编辑和选择"));
    viewScene.selectAllItems();
    check(!lockedText->isSelected(), QStringLiteral("全选不包含非当前图层图元"));
    viewScene.clearSelection();

    const int requestsBeforeLockedClick = textEditRequests;
    const QPoint lockedPoint = view.mapFromScene(lockedText->sceneContentRect().center());
    QMouseEvent lockedDoubleClick(
        QEvent::MouseButtonDblClick, QPointF(lockedPoint),
        QPointF(view.viewport()->mapToGlobal(lockedPoint)),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &lockedDoubleClick);
    QApplication::processEvents();
    check(textEditRequests == requestsBeforeLockedClick,
          QStringLiteral("双击非当前图层文字不会进入编辑"));

    sendMouseEvent(view.viewport(), QEvent::MouseButtonPress, lockedPoint,
                   Qt::LeftButton, Qt::LeftButton);
    sendMouseEvent(view.viewport(), QEvent::MouseButtonRelease, lockedPoint,
                   Qt::LeftButton, Qt::NoButton);
    QApplication::processEvents();
    check(!lockedText->isSelected(), QStringLiteral("非当前图层图元不能被鼠标选中"));

    int lockedLayerMessages = 0;
    QObject::connect(&view, &BoardView::statusMessage, &view,
                     [&lockedLayerMessages](const QString& message) {
                         if (message.contains(QStringLiteral("图层")))
                             ++lockedLayerMessages;
                     });
    const QPoint lockedGlobal = view.viewport()->mapToGlobal(lockedPoint);
    QContextMenuEvent lockedContext(QContextMenuEvent::Mouse, lockedPoint, lockedGlobal);
    QApplication::sendEvent(view.viewport(), &lockedContext);
    QApplication::processEvents();
    check(lockedLayerMessages > 0,
          QStringLiteral("右键非当前图层图元提示先选择图层"));

    view.close();
    QApplication::processEvents();

    // End-to-end check of the actual MainWindow text flow. The modal text
    // dialog is filled and accepted by a timer while insertTextAt() is running.
    {
        MainWindow window;
        window.resize(1200, 800);
        window.show();
        QApplication::processEvents();
        window.view()->setMode(ToolMode::Text);
        QTimer::singleShot(120, &window, []() {
            auto* dialog = qobject_cast<TextDialog*>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            dialog->setTextValue(QStringLiteral("端到端文字图元"));
            QFont font(QStringLiteral("Microsoft YaHei UI"));
            font.setPointSize(26);
            dialog->setTextFont(font);
            dialog->setTextColor(QColor(QStringLiteral("#EA580C")));
            dialog->accept();
        });
        const QPoint clickPosition = window.view()->mapFromScene(
            window.scene()->boardRect().center());
        sendMouseEvent(window.view()->viewport(), QEvent::MouseButtonPress, clickPosition,
                       Qt::LeftButton, Qt::LeftButton);
        sendMouseEvent(window.view()->viewport(), QEvent::MouseButtonRelease, clickPosition,
                       Qt::LeftButton, Qt::NoButton);
        QApplication::processEvents();

        check(window.scene()->boardItemCount() == 1,
              QStringLiteral("主窗口文字模式端到端创建图元"));
        TextItem* createdText = nullptr;
        if (window.scene()->boardItemCount() == 1)
            createdText = dynamic_cast<TextItem*>(window.scene()->boardItems().constFirst());
        check(createdText && createdText->text() == QStringLiteral("端到端文字图元"),
              QStringLiteral("主窗口创建的文字内容正确"));
        check(createdText && createdText->textColor() == QColor(QStringLiteral("#EA580C"))
                  && createdText->layerId() == window.scene()->activeLayerId()
                  && window.scene()->layerHeightsUnique(createdText->layerId()),
              QStringLiteral("主窗口文字使用所选颜色且高度唯一"));
        check(window.view()->mode() == ToolMode::Edit,
              QStringLiteral("主窗口添加文字后切回编辑模式"));
    }

    const QString shapeTemplatePath = QDir(workDirectory)
                                          .filePath(QStringLiteral("shapes.sbboard"));
    {
        QFile shapeTemplateFile(shapeTemplatePath);
        check(shapeTemplateFile.open(QIODevice::WriteOnly),
              QStringLiteral("创建图形模板测试文件"));
        shapeTemplateFile.write(QJsonDocument(shapeRoot).toJson(QJsonDocument::Indented));
    }

    {
        MainWindow shapeWindow;
        shapeWindow.resize(1200, 800);
        shapeWindow.show();
        QApplication::processEvents();

        QMenu* shapeMenu = shapeWindow.findChild<QMenu*>(QStringLiteral("ShapeMenu"));
        check(shapeMenu != nullptr, QStringLiteral("主窗口创建常见图形菜单"));
        check(shapeMenu && shapeMenu->actions().size() == shapeTypes.size(),
              QStringLiteral("图形菜单包含全部图形"),
              QStringLiteral("actions=%1").arg(shapeMenu ? shapeMenu->actions().size() : 0));
        QToolButton* shapeButton = shapeWindow.findChild<QToolButton*>(
            QStringLiteral("ShapeButton"));
        check(shapeButton && shapeButton->menu() == shapeMenu
                  && shapeButton->popupMode() == QToolButton::InstantPopup,
              QStringLiteral("主工具栏图形按钮绑定快速插入菜单"));

        QAction* ellipseAction = shapeMenu
            ? shapeMenu->findChild<QAction*>(QStringLiteral("ShapeAction_ellipse"))
            : nullptr;
        check(ellipseAction != nullptr, QStringLiteral("图形菜单包含圆形插入动作"));
        if (ellipseAction) {
            ellipseAction->trigger();
            QApplication::processEvents();

            check(shapeWindow.scene()->boardItemCount() == 1,
                  QStringLiteral("快速插入图形到当前画板"));
            ShapeItem* inserted = shapeWindow.scene()->boardItemCount() == 1
                ? dynamic_cast<ShapeItem*>(shapeWindow.scene()->boardItems().constFirst())
                : nullptr;
            check(inserted && inserted->shapeType() == ShapeType::Ellipse,
                  QStringLiteral("快速插入的图元类型为圆形"));
            check(inserted && inserted->isSelected()
                      && shapeWindow.view()->mode() == ToolMode::Edit,
                  QStringLiteral("快速插入后自动进入编辑模式并选中图形"));
            check(inserted
                      && inserted->layerId() == shapeWindow.scene()->activeLayerId()
                      && shapeWindow.scene()->layerHeightsUnique(inserted->layerId()),
                  QStringLiteral("快速插入图形进入当前图层且高度唯一"));
            check(inserted
                      && shapeWindow.scene()->boardRect().contains(inserted->sceneContentRect()),
                  QStringLiteral("快速插入图形位于画板范围内"),
                  inserted ? geometryText(inserted->pos(), inserted->contentSize()) : QString());
            check(inserted && inserted->fillColor().alpha() > 0
                      && inserted->strokeWidth() > 0.0,
                  QStringLiteral("快速插入图形带有默认描边与填充"));
        }
    }

    {
        MainWindow shapeTemplateWindow;
        check(shapeTemplateWindow.openTemplateFile(shapeTemplatePath),
              QStringLiteral("主窗口打开含图形的模板"));
        check(shapeTemplateWindow.scene()->boardItemCount() == shapeTypes.size(),
              QStringLiteral("主窗口载入含图形模板完整内容"));
        int loadedShapeCount = 0;
        for (BoardItem* item : shapeTemplateWindow.scene()->boardItems()) {
            if (dynamic_cast<ShapeItem*>(item))
                ++loadedShapeCount;
        }
        check(loadedShapeCount == shapeTypes.size(),
              QStringLiteral("主窗口识别模板中的图形图元"));
    }

    if (QFileInfo::exists(demoPath)) {
        MainWindow templateWindow;
        check(templateWindow.openTemplateFile(demoPath),
              QStringLiteral("主窗口打开含文字的示例模板"));
        check(templateWindow.scene()->boardItemCount() == 27
                  && templateWindow.scene()->userLayerCount() == 3,
              QStringLiteral("主窗口载入示例模板完整内容"));
        bool hasTextItem = false;
        for (BoardItem* item : templateWindow.scene()->boardItems()) {
            if (dynamic_cast<TextItem*>(item)) {
                hasTextItem = true;
                break;
            }
        }
        check(hasTextItem, QStringLiteral("主窗口载入模板中的文字图元"));
    }

    int passCount = 0;
    for (const QString& line : lines) {
        if (line.startsWith(QStringLiteral("PASS")))
            ++passCount;
    }
    lines.append(QString());
    lines.append(QStringLiteral("SUMMARY  %1 passed, %2 failed").arg(passCount).arg(failures));

    const QString path = resolvedReportPath;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile report(path);
    if (report.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&report);
        stream.setEncoding(QStringConverter::Utf8);
        for (const QString& line : lines)
            stream << line << '\n';
    }

    return failures == 0 ? 0 : 1;
}

bool renderDemoBoard(const QString& outputPath)
{
    BoardScene scene;
    populateDemoScene(scene);

    const QImage image = scene.renderImage(scene.boardRect(), 1.0);
    if (image.isNull())
        return false;
    return image.save(outputPath);
}

bool writeDemoTemplate(const QString& outputPath)
{
    BoardScene scene;
    populateDemoScene(scene);

    QDir().mkpath(QFileInfo(outputPath).absolutePath());
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const QJsonDocument document(scene.toJson());
    const QByteArray data = document.toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size()) {
        file.close();
        return false;
    }
    file.close();
    return file.error() == QFileDevice::NoError;
}

void populateDemoScene(BoardScene& scene)
{
    scene.applyBoardSize(QSizeF(1600, 1000));

    BackgroundState background;
    background.type = BackgroundType::Color;
    background.color = QColor(QStringLiteral("#F7F3EA"));
    scene.applyBackground(background);

    const int frameLayer = scene.createLayer(QStringLiteral("分镜框"));
    const int shotLayer = scene.createLayer(QStringLiteral("镜头画面"));
    const int inkLayer = scene.createLayer(QStringLiteral("注释与标题"));
    scene.removeLayerWithUndo(BoardScene::BackgroundLayerId + 1);
    scene.setActiveLayer(inkLayer);

    QFont titleFont(QStringLiteral("Microsoft YaHei UI"));
    titleFont.setPointSize(52);
    titleFont.setBold(true);
    auto* titleItem = new TextItem();
    titleItem->setLayerId(inkLayer);
    titleItem->setTextFormat(QStringLiteral("STORYBOARD  /  分镜画板"),
                             titleFont, QColor(QStringLiteral("#1F2937")));
    titleItem->setContentSize(QSizeF(980, 80));
    titleItem->setPos(QPointF(78, 48));
    scene.attachItem(titleItem);

    QFont labelFont(QStringLiteral("Microsoft YaHei UI"));
    labelFont.setPointSize(20);
    labelFont.setBold(true);
    const QPixmap subtitle = makeTextImage(QStringLiteral("三个镜头 · 一个下午的故事"),
                                           QColor(QStringLiteral("#78716C")), labelFont, 720, 40);
    auto* subtitleItem = new ImageItem();
    subtitleItem->setLayerId(inkLayer);
    subtitleItem->setImage(subtitle, QStringLiteral("subtitle"));
    subtitleItem->setContentSize(subtitle.size());
    subtitleItem->setPos(QPointF(82, 126));
    scene.attachItem(subtitleItem);

    const QColor ink(QStringLiteral("#1F2937"));
    const QColor accent(QStringLiteral("#EA580C"));
    const QColor teal(QStringLiteral("#0F766E"));

    const QVector<QRectF> panels = {
        QRectF(80, 210, 440, 660),
        QRectF(580, 210, 440, 660),
        QRectF(1080, 210, 440, 660)
    };
    const QList<QColor> shotStarts = { QColor(QStringLiteral("#93C5FD")),
                                       QColor(QStringLiteral("#FCA5A5")),
                                       QColor(QStringLiteral("#86EFAC")) };
    const QList<QColor> shotEnds = { QColor(QStringLiteral("#1D4ED8")),
                                     QColor(QStringLiteral("#B91C1C")),
                                     QColor(QStringLiteral("#047857")) };
    const QStringList shotLabels = { QStringLiteral("SHOT 01"),
                                     QStringLiteral("SHOT 02"),
                                     QStringLiteral("SHOT 03") };

    for (int i = 0; i < panels.size(); ++i) {
        const QRectF& panel = panels.at(i);
        auto* frame = new ShapeItem(ShapeType::Rectangle);
        frame->setLayerId(frameLayer);
        frame->setStyle(ink, QColor(Qt::transparent), 5.0);
        frame->setPos(panel.topLeft());
        frame->setContentSize(panel.size());
        scene.attachItem(frame);

        const QPixmap shot = makeShotImage(380, 230, shotLabels.at(i),
                                           shotStarts.at(i), shotEnds.at(i));
        auto* shotItem = new ImageItem();
        shotItem->setLayerId(shotLayer);
        shotItem->setImage(shot, QStringLiteral("shot-%1.png").arg(i + 1));
        shotItem->setContentSize(QSizeF(380, 230));
        shotItem->setPos(panel.left() + 30, panel.top() + 24);
        scene.attachItem(shotItem);

        QFont numberFont(QStringLiteral("Microsoft YaHei UI"));
        numberFont.setPointSize(26);
        numberFont.setBold(true);
        const QPixmap number = makeTextImage(QStringLiteral("0%1").arg(i + 1),
                                              ink, numberFont, 90, 44);
        auto* numberItem = new ImageItem();
        numberItem->setLayerId(shotLayer);
        numberItem->setImage(number, QStringLiteral("number"));
        numberItem->setContentSize(QSizeF(70, 40));
        numberItem->setPos(panel.left() + 32, panel.bottom() - 84);
        scene.attachItem(numberItem);

        QFont captionFont(QStringLiteral("Microsoft YaHei UI"));
        captionFont.setPointSize(16);
        const QStringList captions = {
            QStringLiteral("主角推门进入，光从背后打来。"),
            QStringLiteral("发现桌上的旧照片，定格。"),
            QStringLiteral("转身离开，门缓缓合上。")
        };
        const QPixmap caption = makeTextImage(captions.at(i), QColor(QStringLiteral("#57534E")),
                                              captionFont, 420, 34);
        auto* captionItem = new ImageItem();
        captionItem->setLayerId(shotLayer);
        captionItem->setImage(caption, QStringLiteral("caption"));
        captionItem->setContentSize(QSizeF(376, 30));
        captionItem->setPos(panel.left() + 32, panel.bottom() - 40);
        scene.attachItem(captionItem);
    }

    // Hand-drawn annotations: stick figure, arrows and a few accents.
    auto* firstArrow = new ShapeItem(ShapeType::Arrow);
    firstArrow->setLayerId(inkLayer);
    firstArrow->setStyle(accent, QColor(Qt::transparent), 5.0);
    firstArrow->setPos(QPointF(520, 522));
    firstArrow->setContentSize(QSizeF(66, 36));
    scene.attachItem(firstArrow);

    auto* secondArrow = new ShapeItem(ShapeType::Arrow);
    secondArrow->setLayerId(inkLayer);
    secondArrow->setStyle(accent, QColor(Qt::transparent), 5.0);
    secondArrow->setPos(QPointF(1020, 522));
    secondArrow->setContentSize(QSizeF(66, 36));
    scene.attachItem(secondArrow);

    auto* headRing = new ShapeItem(ShapeType::Ellipse);
    headRing->setLayerId(inkLayer);
    headRing->setStyle(teal, QColor(Qt::transparent), 3.0);
    headRing->setPos(QPointF(258, 510));
    headRing->setContentSize(QSizeF(84, 84));
    scene.attachItem(headRing);

    auto* star = new ShapeItem(ShapeType::Star);
    star->setLayerId(inkLayer);
    QColor starFill = accent;
    starFill.setAlpha(62);
    star->setStyle(accent, starFill, 3.0);
    star->setPos(QPointF(1538, 306));
    star->setContentSize(QSizeF(54, 54));
    scene.attachItem(star);

    auto* brace = new ShapeItem(ShapeType::BraceLeft);
    brace->setLayerId(inkLayer);
    brace->setStyle(accent, QColor(Qt::transparent), 5.0);
    brace->setPos(QPointF(1040, 884));
    brace->setContentSize(QSizeF(44, 92));
    scene.attachItem(brace);

    QVector<QPointF> figure;
    const QPointF headCenter(300, 552);
    constexpr qreal pi = 3.14159265358979323846;
    for (int i = 0; i <= 24; ++i) {
        const qreal angle = 2.0 * pi * i / 24.0;
        figure.append(headCenter + QPointF(qCos(angle) * 26.0, qSin(angle) * 26.0));
    }
    addStroke(scene, figure, teal, 4.0, inkLayer);
    addLine(scene, QPointF(300, 578), QPointF(300, 650), teal, 4.0, inkLayer);
    addLine(scene, QPointF(300, 604), QPointF(260, 628), teal, 4.0, inkLayer);
    addLine(scene, QPointF(300, 604), QPointF(342, 626), teal, 4.0, inkLayer);
    addLine(scene, QPointF(300, 650), QPointF(270, 696), teal, 4.0, inkLayer);
    addLine(scene, QPointF(300, 650), QPointF(330, 696), teal, 4.0, inkLayer);

    QVector<QPointF> underline;
    for (int i = 0; i <= 30; ++i) {
        const qreal x = 1080 + i * 13.0;
        underline.append(QPointF(x, 890 + qSin(i * 0.45) * 4.0));
    }
    addStroke(scene, underline, accent, 5.0, inkLayer);

    QFont noteFont(QStringLiteral("Microsoft YaHei UI"));
    noteFont.setPointSize(18);
    noteFont.setBold(true);
    const QPixmap note = makeTextImage(QStringLiteral("KEEP THE MOMENT"),
                                       QColor(QStringLiteral("#9A3412")), noteFont, 330, 40);
    auto* noteItem = new ImageItem();
    noteItem->setLayerId(inkLayer);
    noteItem->setImage(note, QStringLiteral("note"));
    noteItem->setContentSize(QSizeF(300, 36));
    noteItem->setPos(QPointF(1098, 920));
    scene.attachItem(noteItem);
}

