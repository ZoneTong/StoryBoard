#include "SelfTest.h"

#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CanvasView.h"
#include "Commands.h"
#include "ExportDialog.h"
#include "ImageItem.h"
#include "I18n.h"
#include "ItemPropertiesDialog.h"
#include "LayerPanel.h"
#include "LineArtConverter.h"
#include "LineArtDialog.h"
#include "MainWindow.h"
#include "SettingsDialog.h"
#include "ShapeItem.h"
#include "StrokeItem.h"
#include "TextItem.h"

#include <QApplication>
#include <QAction>
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QImage>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLineF>
#include <QLinearGradient>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QListWidgetItem>
#include <QMarginsF>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>
#include <QWidgetAction>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPixmap>
#include <QPolygonF>
#include <QSet>
#include <QSize>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
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

void addStroke(CanvasScene& scene, const QVector<QPointF>& points, const QColor& color,
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

void addLine(CanvasScene& scene, const QPointF& from, const QPointF& to, const QColor& color,
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
    // 自检里的界面文案断言都按中文原文写，固定成中文，避免受机器语言影响。
    uiOverrideLanguage(UiLanguage::Chinese);

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

    // Qt 内置字体对话框的标签来自 qtbase 翻译，而不是应用自己的 StoryBoard 上下文。
    check(QCoreApplication::translate("QFontDialog", "Effects") == QStringLiteral("效果"),
          QStringLiteral("Qt 内置字体对话框已装载中文翻译"));

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

    CanvasScene scene;
    QUndoStack undo;
    undo.setUndoLimit(100);
    scene.setUndoStack(&undo);
    scene.applyCanvasSize(QSizeF(800, 600));
    scene.setShowGrid(true);
    check(qFuzzyCompare(scene.canvasSize().width(), 800.0)
              && qFuzzyCompare(scene.canvasSize().height(), 600.0),
          QStringLiteral("设置画板尺寸"), geometryText(QPointF(), scene.canvasSize()));

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
    check(scene.canvasItemCount() == 1, QStringLiteral("笔画加入场景"));
    check(undo.canUndo(), QStringLiteral("添加笔画进入撤销栈"));

    const QPointF strokePos = stroke->pos();
    const QSizeF strokeSize = stroke->contentSize();

    undo.undo();
    check(scene.canvasItemCount() == 0, QStringLiteral("撤销添加笔画"));
    undo.redo();
    check(scene.canvasItemCount() == 1, QStringLiteral("重做添加笔画"));
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
    scene.addItemsWithUndo({ imageItem }, QStringLiteral("插入图片"));
    check(scene.canvasItemCount() == 2, QStringLiteral("插入图片图元"));

    const QJsonObject root = scene.toJson();
    const QByteArray jsonBytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    check(jsonBytes.contains("StoryBoard") && jsonBytes.contains("\"stroke\"")
              && jsonBytes.contains("\"image\""),
          QStringLiteral("画布序列化为 JSON"));

    CanvasScene loaded;
    QUndoStack loadedUndo;
    loaded.setUndoStack(&loadedUndo);
    QString loadError;
    check(loaded.fromJson(root, &loadError), QStringLiteral("反序列化画布"), loadError);
    check(loaded.canvasItemCount() == 2, QStringLiteral("画布图元数量一致"));
    check(loaded.background().type == BackgroundType::Color
              && loaded.background().color == background.color,
          QStringLiteral("画布背景恢复"));

    StrokeItem* loadedStroke = nullptr;
    ImageItem* loadedImage = nullptr;
    const QList<CanvasItem*> loadedItems = loaded.canvasItems();
    for (CanvasItem* item : loadedItems) {
        if (item->kind() == QStringLiteral("stroke"))
            loadedStroke = static_cast<StrokeItem*>(item);
        else if (item->kind() == QStringLiteral("image"))
            loadedImage = static_cast<ImageItem*>(item);
    }
    check(loadedStroke != nullptr, QStringLiteral("画布包含笔画"));
    if (loadedStroke) {
        check(loadedStroke->strokeColor() == QColor(QStringLiteral("#EF4444")),
              QStringLiteral("笔画颜色恢复"));
        check(qAbs(loadedStroke->contentSize().width() - resized.width()) < 0.5
                  && qAbs(loadedStroke->contentSize().height() - resized.height()) < 0.5,
              QStringLiteral("笔画缩放状态恢复"));
    }
    check(loadedImage != nullptr && !loadedImage->image().isNull(),
          QStringLiteral("图片内容恢复"));

    const QImage rendered = loaded.renderImage(loaded.canvasRect(), 1.0);
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

    CanvasScene imageBackgroundScene;
    QPixmap backgroundPixmap(64, 48);
    backgroundPixmap.fill(QColor(QStringLiteral("#10B981")));
    BackgroundState imageBackground;
    imageBackground.type = BackgroundType::Image;
    imageBackground.image = backgroundPixmap;
    imageBackground.mode = BackgroundMode::Tile;
    imageBackgroundScene.replaceBackground(imageBackground);

    CanvasScene imageBackgroundLoaded;
    QString imageBackgroundError;
    check(imageBackgroundLoaded.fromJson(imageBackgroundScene.toJson(), &imageBackgroundError),
          QStringLiteral("背景图片序列化"), imageBackgroundError);
    check(imageBackgroundLoaded.background().type == BackgroundType::Image
              && !imageBackgroundLoaded.background().image.isNull()
              && imageBackgroundLoaded.background().mode == BackgroundMode::Tile,
          QStringLiteral("反序列化背景图片与填充方式"));

    const QString canvasPath = QDir(workDirectory).filePath(QStringLiteral("selftest.sbcanvas"));
    {
        QFile file(canvasPath);
        if (file.open(QIODevice::WriteOnly))
            file.write(jsonBytes);
    }
    CanvasScene reloaded;
    QUndoStack reloadedUndo;
    reloaded.setUndoStack(&reloadedUndo);
    QFile canvasFile(canvasPath);
    check(canvasFile.open(QIODevice::ReadOnly), QStringLiteral("读取画布文件"));
    const QJsonDocument reloadedDocument = QJsonDocument::fromJson(canvasFile.readAll());
    check(reloadedDocument.isObject(), QStringLiteral("画布文件 JSON 有效"));
    check(reloaded.fromJson(reloadedDocument.object()), QStringLiteral("从文件载入画布"));
    check(reloaded.canvasItemCount() == 2, QStringLiteral("文件载入图元数量一致"));

    reloaded.setMode(ToolMode::Edit);
    const QList<CanvasItem*> reloadedItems = reloaded.canvasItems();
    if (!reloadedItems.isEmpty()) {
        reloadedItems.constFirst()->setSelected(true);
        reloaded.deleteSelected();
        check(reloaded.canvasItemCount() == 1, QStringLiteral("删除选中图元"));
        reloadedUndo.undo();
        check(reloaded.canvasItemCount() == 2, QStringLiteral("撤销删除图元"));
        reloadedUndo.redo();
        check(reloaded.canvasItemCount() == 1, QStringLiteral("重做删除图元"));
    }

    scene.setCanvasSizeWithUndo(QSizeF(1000, 700));
    check(qFuzzyCompare(scene.canvasSize().width(), 1000.0), QStringLiteral("调整画板尺寸"));
    undo.undo();
    check(qFuzzyCompare(scene.canvasSize().width(), 800.0), QStringLiteral("撤销调整画板尺寸"));

    // ---------------------------------------------------------------
    // Layer and per-layer height model.
    // ---------------------------------------------------------------
    CanvasScene layerScene;
    QUndoStack layerUndo;
    layerUndo.setUndoLimit(100);
    layerScene.setUndoStack(&layerUndo);
    layerScene.applyCanvasSize(QSizeF(400, 300));

    check(layerScene.layers().size() == 2
              && layerScene.layers().constFirst().id == CanvasScene::BackgroundLayerId
              && layerScene.layerIndex(CanvasScene::BackgroundLayerId) == 0,
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
    check(layerScene.userLayerCount() == 2 && topLayerId > CanvasScene::BackgroundLayerId,
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

    check(!layerScene.moveLayerWithUndo(CanvasScene::BackgroundLayerId, 1),
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

    // 需求 24：height 合法值包含 0；打开属性窗时不能把 0 夹成 1，也不能
    // 因为层内存在 height 空洞而把当前值收紧。
    lower->setHeight(0);
    const int zeroHeightUndoBefore = layerUndo.count();
    {
        ItemPropertiesDialog zeroHeightDialog(&layerScene, lower);
        zeroHeightDialog.show();
        QApplication::processEvents();
        QSpinBox* zeroHeightValue = zeroHeightDialog.findChild<QSpinBox*>(
            QStringLiteral("ItemPropertiesHeightValue"));
        check(zeroHeightValue && zeroHeightValue->value() == 0,
              QStringLiteral("属性窗显示 height 0 且不夹紧"),
              zeroHeightValue ? QString::number(zeroHeightValue->value()) : QString());
        check(zeroHeightValue && zeroHeightValue->maximum() >= 3,
              QStringLiteral("属性窗 height 上限允许层内空洞"));
        zeroHeightDialog.accept();
        QApplication::processEvents();
    }
    check(layerUndo.count() == zeroHeightUndoBefore,
          QStringLiteral("height 0 属性窗未修改时不产生撤销记录"));
    lower->setHeight(1);

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
          QStringLiteral("画布保存图层与当前图层"));
    const QByteArray layerBytes = QJsonDocument(layerRoot).toJson(QJsonDocument::Compact);
    check(layerBytes.contains("\"layer\":") && layerBytes.contains("\"height\":"),
          QStringLiteral("画布保存图元 layer 与 height"));

    CanvasScene layerLoaded;
    QString layerLoadError;
    check(layerLoaded.fromJson(layerRoot, &layerLoadError),
          QStringLiteral("图层画布反序列化"), layerLoadError);
    check(layerLoaded.layers() == layerScene.layers()
              && layerLoaded.activeLayerId() == layerScene.activeLayerId(),
          QStringLiteral("图层顺序与当前图层恢复"));
    check(layerLoaded.canvasItemCount() == 3, QStringLiteral("图层画布图元数量一致"));
    bool layerRoundTrip = layerLoaded.canvasItemCount() == layerScene.canvasItemCount();
    for (const CanvasLayer& entry : layerScene.layers()) {
        if (layerLoaded.layerItemCount(entry.id) != layerScene.layerItemCount(entry.id)) {
            layerRoundTrip = false;
            break;
        }
    }
    check(layerRoundTrip, QStringLiteral("图层内图元归属恢复"));

    QJsonObject missingLayersRoot = layerRoot;
    missingLayersRoot.remove(QStringLiteral("layers"));
    missingLayersRoot.remove(QStringLiteral("activeLayer"));
    CanvasScene missingLayersScene;
    QString missingLayersError;
    check(!missingLayersScene.fromJson(missingLayersRoot, &missingLayersError),
          QStringLiteral("缺少图层数组的画布被拒绝"));

    // ---------------------------------------------------------------
    // 叠放优先级（layer / height）与被遮盖图元的轮廓提示。
    // ---------------------------------------------------------------
    CanvasScene priorityScene;
    priorityScene.applyCanvasSize(QSizeF(600, 400));

    auto addPriorityRect = [&priorityScene](const QRectF& rect, const QColor& color) {
        auto* shape = new ShapeItem(ShapeType::Rectangle);
        shape->setPos(rect.topLeft());
        shape->setContentSize(rect.size());
        shape->setStyle(color, color, 3.0);
        shape->setFill(color, FillStyle::Solid);
        priorityScene.attachItem(shape);
        return shape;
    };

    auto* coveredRect = addPriorityRect(QRectF(120.0, 120.0, 240.0, 140.0),
                                        QColor(QStringLiteral("#DC2626")));
    auto* coverRect = addPriorityRect(QRectF(120.0, 120.0, 240.0, 140.0),
                                      QColor(QStringLiteral("#2563EB")));
    check(coverRect->zValue() > coveredRect->zValue()
              && coverRect->height() > coveredRect->height(),
          QStringLiteral("同层后加入的图元叠放优先级更高"));

    priorityScene.setMode(ToolMode::Edit);
    QList<CanvasItem*> obscured = priorityScene.obscuredItems();
    check(obscured.size() == 1 && obscured.constFirst() == coveredRect,
          QStringLiteral("非查看模式下完全遮盖的低优先级图元会描出轮廓"),
          QStringLiteral("count=%1").arg(obscured.size()));

    auto renderSceneImage = [](CanvasScene& target) {
        QImage image(600, 400, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        QPainter imagePainter(&image);
        target.render(&imagePainter, QRectF(0.0, 0.0, 600.0, 400.0),
                      QRectF(0.0, 0.0, 600.0, 400.0));
        imagePainter.end();
        return image;
    };
    const QImage editRender = renderSceneImage(priorityScene);
    priorityScene.setMode(ToolMode::View);
    const QImage viewRender = renderSceneImage(priorityScene);
    int changedPixels = 0;
    for (int y = 0; y < editRender.height(); ++y) {
        const QRgb* editLine = reinterpret_cast<const QRgb*>(editRender.constScanLine(y));
        const QRgb* viewLine = reinterpret_cast<const QRgb*>(viewRender.constScanLine(y));
        for (int x = 0; x < editRender.width(); ++x) {
            if (editLine[x] != viewLine[x])
                ++changedPixels;
        }
    }
    check(changedPixels > 0,
          QStringLiteral("查看模式完全遮盖，编辑模式能看到被遮盖图元的轮廓"),
          QStringLiteral("changed=%1").arg(changedPixels));
    check(priorityScene.obscuredItems().isEmpty(),
          QStringLiteral("查看模式下不再描被遮盖图元的轮廓"));

    priorityScene.setMode(ToolMode::Edit);
    check(priorityScene.adjustItemHeight(coveredRect, 1),
          QStringLiteral("调整被遮盖图元的 height"));
    obscured = priorityScene.obscuredItems();
    check(coveredRect->zValue() > coverRect->zValue()
              && obscured.size() == 1 && obscured.constFirst() == coverRect,
          QStringLiteral("同层 height 更大的图元优先级更高，轮廓提示随之转移"),
          QStringLiteral("count=%1").arg(obscured.size()));

    check(priorityScene.addLayerWithUndo(QStringLiteral("遮挡层")),
          QStringLiteral("新建更高的遮挡图层"));
    const int overlayLayerId = priorityScene.activeLayerId();
    auto* layerCover = addPriorityRect(QRectF(120.0, 120.0, 240.0, 140.0),
                                       QColor(QStringLiteral("#059669")));
    check(layerCover->layerId() == overlayLayerId
              && layerCover->zValue() > coveredRect->zValue(),
          QStringLiteral("更高图层的低 height 图元优先级仍然更高"));
    obscured = priorityScene.obscuredItems();
    check(obscured.contains(coveredRect) && obscured.contains(coverRect)
              && !obscured.contains(layerCover),
          QStringLiteral("轮廓提示只覆盖被更高优先级图元遮住的图元"),
          QStringLiteral("count=%1").arg(obscured.size()));

    const int coveredLayerId = coveredRect->layerId();
    check(priorityScene.setLayerVisible(coveredLayerId, false),
          QStringLiteral("隐藏低优先级图元所在图层"));
    check(priorityScene.obscuredItems().isEmpty(),
          QStringLiteral("隐藏图层上的图元不参与轮廓提示"));

    CanvasScene partialScene;
    partialScene.applyCanvasSize(QSizeF(500, 300));
    auto addPartialRect = [&partialScene](const QRectF& rect, const QColor& color) {
        auto* shape = new ShapeItem(ShapeType::Rectangle);
        shape->setPos(rect.topLeft());
        shape->setContentSize(rect.size());
        shape->setStyle(color, color, 3.0);
        shape->setFill(color, FillStyle::Solid);
        partialScene.attachItem(shape);
        return shape;
    };
    auto* partialLower =
        addPartialRect(QRectF(100.0, 100.0, 200.0, 100.0), QColor(QStringLiteral("#DC2626")));
    addPartialRect(QRectF(240.0, 140.0, 200.0, 100.0), QColor(QStringLiteral("#2563EB")));
    partialScene.setMode(ToolMode::Edit);
    obscured = partialScene.obscuredItems();
    check(obscured.size() == 1 && obscured.constFirst() == partialLower,
          QStringLiteral("只要有任何遮挡就描出被遮图元的轮廓"),
          QStringLiteral("count=%1").arg(obscured.size()));
    partialScene.setMode(ToolMode::View);
    check(partialScene.obscuredItems().isEmpty(),
          QStringLiteral("查看模式下小面积遮挡也不描轮廓"));

    // ---------------------------------------------------------------
    // Layer visibility, locking and per-layer editing permissions.
    // ---------------------------------------------------------------
    const int lowerLayerId = layerScene.layers().at(1).id;
    layerScene.setMode(ToolMode::Edit);
    check(layerScene.isItemEditable(topLine) && layerScene.isItemEditable(lower)
              && layerScene.isItemEditable(upper),
          QStringLiteral("编辑模式下可见且未锁定图层的图元都可编辑"));
    check((topLine->flags() & QGraphicsItem::ItemIsSelectable) != 0
              && (lower->flags() & QGraphicsItem::ItemIsSelectable) != 0
              && (upper->flags() & QGraphicsItem::ItemIsSelectable) != 0,
          QStringLiteral("可见且未锁定图层的图元都可选中"));

    layerScene.selectAllItems();
    check(topLine->isSelected() && lower->isSelected() && upper->isSelected(),
          QStringLiteral("全选包含所有可见且未锁定图层的图元"));
    layerScene.clearSelection();

    check(layerScene.setLayerLocked(lowerLayerId, true)
              && layerScene.isLayerLocked(lowerLayerId),
          QStringLiteral("锁定图层"));
    check(!layerScene.isItemEditable(lower)
              && (lower->flags() & QGraphicsItem::ItemIsSelectable) == 0,
          QStringLiteral("锁定图层上的图元不可编辑也不可选中"));
    check(layerScene.isItemEditable(topLine),
          QStringLiteral("锁定一个图层不影响其他图层编辑"));
    check(layerScene.isLayerLocked(CanvasScene::BackgroundLayerId),
          QStringLiteral("背景层默认锁定"));
    check(!layerScene.setLayerLocked(CanvasScene::BackgroundLayerId, false)
              && layerScene.isLayerLocked(CanvasScene::BackgroundLayerId),
          QStringLiteral("背景层锁定状态不可更改"));
    check(layerScene.setLayerLocked(lowerLayerId, false) && layerScene.isItemEditable(lower),
          QStringLiteral("解锁图层后图元恢复可编辑"));

    layerScene.setActiveLayer(lowerLayerId);
    check(layerScene.activeLayerId() == lowerLayerId
              && layerScene.isLayerVisible(lowerLayerId),
          QStringLiteral("切换当前图层后该图层可见"));
    check(layerScene.isItemEditable(topLine) && layerScene.isItemEditable(lower),
          QStringLiteral("切换当前图层不改变其他可见图层的编辑权限"));

    check(layerScene.setLayerVisible(topLayerId, false),
          QStringLiteral("隐藏非当前图层"));
    check(!layerScene.isLayerVisible(topLayerId) && !topLine->isVisible(),
          QStringLiteral("隐藏图层后图元不可见"));
    check(!layerScene.isItemEditable(topLine) && !topLine->isSelected(),
          QStringLiteral("隐藏图层上的图元不可编辑"));
    check(layerScene.isItemEditable(lower),
          QStringLiteral("隐藏其他图层不影响当前图层编辑"));

    check(layerScene.setLayerVisible(topLayerId, true),
          QStringLiteral("显示非当前图层"));
    check(topLine->isVisible() && layerScene.isItemEditable(topLine),
          QStringLiteral("重新显示图层后图元恢复可编辑"));

    layerScene.setActiveLayer(topLayerId);
    check(layerScene.setLayerVisible(topLayerId, false),
          QStringLiteral("隐藏当前图层"));
    check(layerScene.activeLayerId() != topLayerId
              && !layerScene.isLayerVisible(topLayerId)
              && layerScene.isLayerVisible(layerScene.activeLayerId()),
          QStringLiteral("隐藏当前图层自动切换到其他可见图层"));
    check(!layerScene.isItemEditable(topLine),
          QStringLiteral("隐藏后原当前图层图元不可编辑"));

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
    check(layerScene.setLayerVisible(CanvasScene::BackgroundLayerId, false),
          QStringLiteral("隐藏背景层"));
    const QImage noBackgroundImage = layerScene.renderImage(layerScene.canvasRect(), 1.0);
    check(noBackgroundImage.pixelColor(5, 5) == QColor(Qt::white),
          QStringLiteral("隐藏背景层后导出为白色底"));
    check(layerScene.setLayerVisible(CanvasScene::BackgroundLayerId, true),
          QStringLiteral("显示背景层"));
    const QImage backgroundImage = layerScene.renderImage(layerScene.canvasRect(), 1.0);
    check(backgroundImage.pixelColor(5, 5) == visibilityBackground.color,
          QStringLiteral("显示背景层后恢复背景颜色"));

    check(layerScene.setLayerVisible(lowerLayerId, false),
          QStringLiteral("隐藏带图元的下层"));
    check(layerScene.setLayerVisible(CanvasScene::BackgroundLayerId, false),
          QStringLiteral("画布保存前隐藏背景"));
    const QJsonObject visibilityRoot = layerScene.toJson();
    const QJsonArray visibilityLayers = visibilityRoot.value(QStringLiteral("layers")).toArray();
    bool lowerHiddenInJson = false;
    bool backgroundHiddenInJson = false;
    for (const QJsonValue& value : visibilityLayers) {
        const QJsonObject layerJson = value.toObject();
        const int id = layerJson.value(QStringLiteral("id")).toInt();
        if (id == lowerLayerId)
            lowerHiddenInJson = !layerJson.value(QStringLiteral("visible")).toBool(true);
        if (id == CanvasScene::BackgroundLayerId)
            backgroundHiddenInJson = !layerJson.value(QStringLiteral("visible")).toBool(true);
    }
    check(lowerHiddenInJson && backgroundHiddenInJson,
          QStringLiteral("画布保存图层显示/隐藏状态"));

    CanvasScene visibilityLoaded;
    QString visibilityLoadError;
    check(visibilityLoaded.fromJson(visibilityRoot, &visibilityLoadError),
          QStringLiteral("图层显示状态画布反序列化"), visibilityLoadError);
    check(!visibilityLoaded.isLayerVisible(lowerLayerId)
              && !visibilityLoaded.isLayerVisible(CanvasScene::BackgroundLayerId)
              && visibilityLoaded.isLayerVisible(topLayerId),
          QStringLiteral("画布恢复图层显示/隐藏状态"));

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
    CanvasScene hiddenActiveScene;
    QString hiddenActiveError;
    check(hiddenActiveScene.fromJson(hiddenActiveRoot, &hiddenActiveError),
          QStringLiteral("载入当前图层被隐藏的画布"), hiddenActiveError);
    check(hiddenActiveScene.activeLayerId() == topLayerId
              && hiddenActiveScene.isLayerVisible(topLayerId),
          QStringLiteral("载入时自动显示当前图层"));

    // A hidden layer must also be absent from actual rendering.
    CanvasScene hiddenPaintScene;
    hiddenPaintScene.applyCanvasSize(QSizeF(120, 120));
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
    const QImage paintedHidden = hiddenPaintScene.renderImage(hiddenPaintScene.canvasRect(), 1.0);
    check(paintedHidden.pixelColor(30, 30) == QColor(QStringLiteral("#DC2626")),
          QStringLiteral("隐藏图层图元不参与渲染"));
    check(hiddenPaintScene.contentRect().right()
              < blueImage->sceneContentRect().right() - 1.0,
          QStringLiteral("隐藏图层图元不参与内容外接矩形"));

    check(hiddenPaintScene.setLayerVisible(hiddenBlueLayerId, true),
          QStringLiteral("显示蓝色图层"));
    const QImage paintedShown = hiddenPaintScene.renderImage(hiddenPaintScene.canvasRect(), 1.0);
    check(paintedShown.pixelColor(30, 30) == QColor(QStringLiteral("#2563EB")),
          QStringLiteral("显示图层后图元重新参与渲染"));
    check(hiddenPaintScene.isItemEditable(blueImage)
              && hiddenPaintScene.isItemEditable(redImage),
          QStringLiteral("显示非当前图层后其图元同样可编辑"));
    check(hiddenPaintScene.setLayerLocked(hiddenBlueLayerId, true),
          QStringLiteral("锁定蓝色图层"));
    check(!hiddenPaintScene.isItemEditable(blueImage)
              && hiddenPaintScene.isItemEditable(redImage),
          QStringLiteral("锁定蓝色图层后仅该层图元不可编辑"));
    check(hiddenPaintScene.setLayerLocked(hiddenBlueLayerId, false),
          QStringLiteral("解锁蓝色图层"));
    check(hiddenPaintScene.isItemEditable(blueImage),
          QStringLiteral("解锁后图元恢复可编辑"));

    CanvasScene singleLayerScene;
    singleLayerScene.setMode(ToolMode::Edit);
    const int singleLayerId = singleLayerScene.activeLayerId();
    check(!singleLayerScene.setLayerVisible(singleLayerId, false)
              && singleLayerScene.isLayerVisible(singleLayerId),
          QStringLiteral("最后一个可编辑图层不能被隐藏"));

    // ---------------------------------------------------------------
    // Text items and the per-layer unique-height invariant.
    // ---------------------------------------------------------------
    CanvasScene textScene;
    QUndoStack textUndo;
    textUndo.setUndoLimit(100);
    textScene.setUndoStack(&textUndo);
    textScene.applyCanvasSize(QSizeF(900, 600));
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
              && textScene.canvasItemCount() == 1,
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

    const QImage textRendered = textScene.renderImage(textScene.canvasRect(), 1.0);
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
    check(textScene.canvasItemCount() == 0, QStringLiteral("撤销添加文字"));
    textUndo.redo();
    check(textScene.canvasItemCount() == 1, QStringLiteral("重做添加文字"));

    QFont editedFont(QStringLiteral("Microsoft YaHei UI"));
    editedFont.setPointSize(24);
    editedFont.setItalic(true);
    editedFont.setUnderline(true);
    editedFont.setStrikeOut(true);
    check(textScene.changeTextWithUndo(heading, QStringLiteral("更新后的文字"),
                                       editedFont, QColor(QStringLiteral("#B91C1C")),
                                       QStringLiteral("编辑文字")),
          QStringLiteral("编辑文字进入撤销栈"));
    check(heading->text() == QStringLiteral("更新后的文字")
              && heading->textFont().italic()
              && heading->textFont().underline()
              && heading->textFont().strikeOut()
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
          QStringLiteral("画布保存文字图元"));
    CanvasScene textLoaded;
    QString textLoadError;
    check(textLoaded.fromJson(textRoot, &textLoadError),
          QStringLiteral("画布反序列化文字图元"), textLoadError);
    TextItem* loadedText = nullptr;
    for (CanvasItem* item : textLoaded.canvasItems()) {
        if (auto* text = dynamic_cast<TextItem*>(item)) {
            loadedText = text;
            break;
        }
    }
    check(loadedText != nullptr, QStringLiteral("画布恢复文字类型"));
    if (loadedText) {
        check(loadedText->text() == QStringLiteral("更新后的文字")
                  && loadedText->textFont().italic()
                  && loadedText->textFont().underline()
                  && loadedText->textFont().strikeOut()
                  && loadedText->textColor() == QColor(QStringLiteral("#B91C1C")),
              QStringLiteral("画布恢复文字内容与样式"));
        check(loadedText->layerId() == textScene.activeLayerId()
                  && loadedText->height() == heading->height(),
              QStringLiteral("画布恢复文字图层与高度"));
        check(qAbs(loadedText->contentSize().width() - heading->contentSize().width()) < 0.5
                  && qAbs(loadedText->contentSize().height() - heading->contentSize().height()) < 0.5,
              QStringLiteral("画布恢复文字尺寸"));
    }
    check(textLoaded.layerHeightsUnique(textLoaded.activeLayerId()),
          QStringLiteral("画布载入后层内 height 唯一"));

    // A hand-edited canvas may contain duplicate heights. Loading it must
    // repair them to a stable, unique sequence while preserving item order.
    QJsonObject duplicateRoot = textScene.toJson();
    QJsonArray duplicateItems = duplicateRoot.value(QStringLiteral("items")).toArray();
    for (int i = 0; i < duplicateItems.size(); ++i) {
        QJsonObject itemJson = duplicateItems.at(i).toObject();
        itemJson.insert(QStringLiteral("height"), 1);
        duplicateItems.replace(i, itemJson);
    }
    duplicateRoot.insert(QStringLiteral("items"), duplicateItems);
    CanvasScene repairedScene;
    QString repairedError;
    check(repairedScene.fromJson(duplicateRoot, &repairedError),
          QStringLiteral("载入含重复 height 的画布"), repairedError);
    check(repairedScene.layerHeightsUnique(repairedScene.activeLayerId())
              && repairedScene.canvasItemCount() == 2,
          QStringLiteral("重复 height 载入后自动修复为唯一值"));
    QSet<int> repairedHeights;
    for (CanvasItem* item : repairedScene.canvasItems())
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
    CanvasScene oversizedScene;
    QString oversizedError;
    check(oversizedScene.fromJson(oversizedRoot, &oversizedError),
          QStringLiteral("载入超出高度上限的画布"), oversizedError);
    bool oversizedRepaired = oversizedScene.layerHeightsUnique(oversizedScene.activeLayerId());
    for (CanvasItem* item : oversizedScene.canvasItems())
        oversizedRepaired = oversizedRepaired && item->height() <= 999999;
    check(oversizedRepaired,
          QStringLiteral("超限 height 载入后修复到合法唯一范围"));

    // ---------------------------------------------------------------
    // Shape items: common diagrams, quick insertion geometry and rendering.
    // ---------------------------------------------------------------
    CanvasScene shapeScene;
    QUndoStack shapeUndo;
    shapeUndo.setUndoLimit(100);
    shapeScene.setUndoStack(&shapeUndo);
    shapeScene.applyCanvasSize(QSizeF(1000, 800));
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
    check(shapeScene.canvasItemCount() == shapeTypes.size(),
          QStringLiteral("所有常见图形加入场景"));
    check(shapeScene.layerHeightsUnique(shapeScene.activeLayerId()),
          QStringLiteral("图形同层 height 唯一"));

    const QImage shapesRendered = shapeScene.renderImage(shapeScene.canvasRect(), 1.0);
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
    check(shapeScene.canvasItemCount() == shapeTypes.size() - 1,
          QStringLiteral("撤销插入图形"));
    shapeUndo.redo();
    check(shapeScene.canvasItemCount() == shapeTypes.size() && lastShape->scene() == &shapeScene,
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
          QStringLiteral("画布保存图形类型、描边与填充"));

    CanvasScene shapeLoaded;
    QUndoStack shapeLoadedUndo;
    shapeLoaded.setUndoStack(&shapeLoadedUndo);
    QString shapeLoadError;
    check(shapeLoaded.fromJson(shapeRoot, &shapeLoadError),
          QStringLiteral("画布反序列化图形图元"), shapeLoadError);
    check(shapeLoaded.canvasItemCount() == shapeItems.size(),
          QStringLiteral("画布恢复全部图形"));
    check(shapeLoaded.layerHeightsUnique(shapeLoaded.activeLayerId()),
          QStringLiteral("画布恢复图形后 height 唯一"));
    QSet<QString> loadedShapeTypes;
    ShapeItem* loadedRectangle = nullptr;
    for (CanvasItem* item : shapeLoaded.canvasItems()) {
        if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
            loadedShapeTypes.insert(shapeTypeToString(shape->shapeType()));
            if (shape->shapeType() == ShapeType::Rectangle && !loadedRectangle)
                loadedRectangle = shape;
        }
    }
    check(loadedShapeTypes.size() == shapeTypes.size(),
          QStringLiteral("画布恢复全部 12 种图形类型"));
    check(loadedRectangle
              && loadedRectangle->strokeColor() == shapeStroke
              && loadedRectangle->fillColor().alpha() == shapeFill.alpha()
              && qAbs(loadedRectangle->strokeWidth() - 4.0) < 0.01,
          QStringLiteral("画布恢复图形描边、填充和线宽"));

    // Exercise the actual layer panel buttons, not only the scene API.
    CanvasScene panelScene;
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
        CanvasScene visibilityPanelScene;
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

        auto rowCheckBox = [visibilityList](int layerId) -> QCheckBox* {
            if (!visibilityList)
                return nullptr;
            for (int row = 0; row < visibilityList->count(); ++row) {
                QListWidgetItem* item = visibilityList->item(row);
                if (item->data(Qt::UserRole).toInt() != layerId)
                    continue;
                QWidget* widget = visibilityList->itemWidget(item);
                return widget ? widget->findChild<QCheckBox*>(QStringLiteral("LayerVisibleBox"))
                              : nullptr;
            }
            return nullptr;
        };
        auto rowLockButton = [visibilityList](int layerId) -> QToolButton* {
            if (!visibilityList)
                return nullptr;
            for (int row = 0; row < visibilityList->count(); ++row) {
                QListWidgetItem* item = visibilityList->item(row);
                if (item->data(Qt::UserRole).toInt() != layerId)
                    continue;
                QWidget* widget = visibilityList->itemWidget(item);
                return widget ? widget->findChild<QToolButton*>(QStringLiteral("LayerLockButton"))
                              : nullptr;
            }
            return nullptr;
        };

        QListWidgetItem* lowerRow = findRow(lowerId);
        QCheckBox* lowerBox = rowCheckBox(lowerId);
        check(lowerRow && lowerBox && lowerBox->isChecked(),
              QStringLiteral("图层面板显示可见勾选状态"));
        if (lowerBox) {
            lowerBox->setChecked(false);
            QApplication::processEvents();
            check(!visibilityPanelScene.isLayerVisible(lowerId),
                  QStringLiteral("图层面板取消勾选隐藏图层"));
            QCheckBox* hiddenBox = rowCheckBox(lowerId);
            QLabel* hiddenLabel = hiddenBox && hiddenBox->parentWidget()
                ? hiddenBox->parentWidget()->findChild<QLabel*>(QStringLiteral("LayerRowLabel"))
                : nullptr;
            check(hiddenBox && !hiddenBox->isChecked() && hiddenLabel
                      && hiddenLabel->text().contains(QStringLiteral("隐藏")),
                  QStringLiteral("图层面板标记隐藏图层"));

            QListWidgetItem* hiddenRow = findRow(lowerId);
            if (hiddenRow) {
                visibilityList->setCurrentItem(hiddenRow);
                QApplication::processEvents();
                check(visibilityPanelScene.activeLayerId() == lowerId
                          && visibilityPanelScene.isLayerVisible(lowerId),
                      QStringLiteral("选中隐藏图层自动显示"));
                QCheckBox* shownBox = rowCheckBox(lowerId);
                check(shownBox && shownBox->isChecked(),
                      QStringLiteral("自动显示后同步勾选状态"));
                if (shownBox) {
                    shownBox->setChecked(false);
                    QApplication::processEvents();
                    check(!visibilityPanelScene.isLayerVisible(lowerId)
                              && visibilityPanelScene.activeLayerId() == upperId,
                          QStringLiteral("隐藏当前图层后自动选择其他图层"));
                }
            }
        }

        QCheckBox* backgroundBox = rowCheckBox(CanvasScene::BackgroundLayerId);
        check(backgroundBox && backgroundBox->isChecked(),
              QStringLiteral("图层面板显示背景层勾选"));
        if (backgroundBox) {
            backgroundBox->setChecked(false);
            QApplication::processEvents();
            check(!visibilityPanelScene.isLayerVisible(CanvasScene::BackgroundLayerId),
                  QStringLiteral("图层面板可隐藏背景层"));
            QCheckBox* hiddenBackgroundBox = rowCheckBox(CanvasScene::BackgroundLayerId);
            if (hiddenBackgroundBox) {
                hiddenBackgroundBox->setChecked(true);
                QApplication::processEvents();
            }
            check(visibilityPanelScene.isLayerVisible(CanvasScene::BackgroundLayerId),
                  QStringLiteral("图层面板可重新显示背景层"));
        }

        QCheckBox* allVisibleBox = visibilityPanel.findChild<QCheckBox*>(
            QStringLiteral("LayerVisibleAllBox"));
        check(allVisibleBox && allVisibleBox->isTristate(),
              QStringLiteral("图层面板表头提供三态勾选框"));
        check(allVisibleBox && allVisibleBox->checkState() == Qt::PartiallyChecked,
              QStringLiteral("表头勾选框反映部分图层隐藏"));
        if (allVisibleBox) {
            allVisibleBox->click();
            QApplication::processEvents();
            bool everyLayerVisible = true;
            for (const CanvasLayer& layer : visibilityPanelScene.layers()) {
                if (!layer.visible)
                    everyLayerVisible = false;
            }
            check(everyLayerVisible && allVisibleBox->checkState() == Qt::Checked,
                  QStringLiteral("表头勾选框可全选显示所有图层"));

            allVisibleBox->click();
            QApplication::processEvents();
            bool everyLayerHidden = true;
            for (const CanvasLayer& layer : visibilityPanelScene.layers()) {
                if (layer.visible)
                    everyLayerHidden = false;
            }
            check(everyLayerHidden && allVisibleBox->checkState() == Qt::Unchecked,
                  QStringLiteral("表头勾选框可全不选隐藏所有图层"));

            allVisibleBox->click();
            QApplication::processEvents();
            bool restoredAllVisible = true;
            for (const CanvasLayer& layer : visibilityPanelScene.layers()) {
                if (!layer.visible)
                    restoredAllVisible = false;
            }
            check(restoredAllVisible && allVisibleBox->checkState() == Qt::Checked,
                  QStringLiteral("表头勾选框可再次显示所有图层"));
        }

        QToolButton* upperLock = rowLockButton(upperId);
        check(upperLock != nullptr, QStringLiteral("图层面板提供图层锁定按钮"));
        if (upperLock) {
            upperLock->click();
            QApplication::processEvents();
            check(visibilityPanelScene.isLayerLocked(upperId)
                      && !visibilityPanelScene.isItemEditable(upperItem),
                  QStringLiteral("锁定按钮锁定图层并禁止编辑其图元"));
            QToolButton* restoredLock = rowLockButton(upperId);
            if (restoredLock)
                restoredLock->click();
            QApplication::processEvents();
            check(!visibilityPanelScene.isLayerLocked(upperId),
                  QStringLiteral("再次点击解锁图层"));
        }

        visibilityList->clearSelection();
        for (int row = 0; row < visibilityList->count(); ++row) {
            QListWidgetItem* item = visibilityList->item(row);
            if (item->data(Qt::UserRole).toInt() == upperId)
                item->setSelected(true);
        }
        check(visibilityPanel.selectedLayerIds().size() == 1
                  && visibilityPanel.selectedLayerIds().first() == upperId,
              QStringLiteral("图层面板可读取选中的图层集合"));

        upperItem->setSelected(true);
        QApplication::processEvents();
        check(visibilityPanelScene.activeLayerId() == upperId,
              QStringLiteral("选中图元时自动选中其所属图层"));
    }

    // Verify the actual paint order, not just the numeric z values.
    CanvasScene paintScene;
    paintScene.applyCanvasSize(QSizeF(100, 100));
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

    const QImage painted = paintScene.renderImage(paintScene.canvasRect(), 1.0);
    check(painted.pixelColor(30, 30) == QColor(QStringLiteral("#2563EB")),
          QStringLiteral("上层图元实际遮盖下层图元"));
    check(painted.pixelColor(15, 15) == QColor(QStringLiteral("#DC2626")),
          QStringLiteral("未重叠区域保留下层图元"));

    paintScene.moveLayerWithUndo(blueLayerId, -1);
    const QImage paintedLower = paintScene.renderImage(paintScene.canvasRect(), 1.0);
    check(paintedLower.pixelColor(30, 30) == QColor(QStringLiteral("#DC2626")),
          QStringLiteral("图层下移后实际遮盖关系反转"));

    const QString demoPath = []() {
        const QString appDir = QCoreApplication::applicationDirPath();
        const QString adjacent = QDir(appDir).filePath(
            QStringLiteral("examples/demo-canvas.sbcanvas"));
        if (QFileInfo::exists(adjacent))
            return adjacent;
        return QDir(appDir).filePath(
            QStringLiteral("../../examples/demo-canvas.sbcanvas"));
    }();
    if (QFileInfo::exists(demoPath)) {
        QFile demoFile(demoPath);
        check(demoFile.open(QIODevice::ReadOnly), QStringLiteral("读取示例画布文件"));
        const QJsonDocument demoDocument = QJsonDocument::fromJson(demoFile.readAll());
        CanvasScene demoLoaded;
        QString demoError;
        check(demoLoaded.fromJson(demoDocument.object(), &demoError),
              QStringLiteral("加载示例画布"), demoError);
        check(demoLoaded.userLayerCount() == 3
                  && demoLoaded.activeLayerId() == demoLoaded.layers().last().id
                  && demoLoaded.canvasItemCount() == 27,
              QStringLiteral("示例画布图层与图元完整"),
              QStringLiteral("layers=%1 items=%2")
                  .arg(demoLoaded.userLayerCount())
                  .arg(demoLoaded.canvasItemCount()));
    }

    // ---------------------------------------------------------------
    // Interaction test: real mouse events through CanvasView.
    // ---------------------------------------------------------------
    CanvasScene viewScene;
    QUndoStack viewUndo;
    viewScene.setUndoStack(&viewUndo);
    viewScene.applyCanvasSize(QSizeF(1200, 800));
    viewScene.setShowGrid(false);

    CanvasView view(&viewScene);
    view.resize(1000, 700);
    view.show();
    QApplication::processEvents();

    check(view.metaObject()->indexOfSignal("editTextRequested(TextItem*)") < 0,
          QStringLiteral("右键独立编辑文字入口信号已移除"));
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

    check(viewScene.canvasItemCount() == 1, QStringLiteral("鼠标绘制生成笔画"));
    CanvasItem* drawnItem = viewScene.canvasItems().value(0);
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
    check(viewScene.canvasItemCount() == 0, QStringLiteral("撤销鼠标绘制"));
    viewUndo.redo();
    check(viewScene.canvasItemCount() == 1, QStringLiteral("重做鼠标绘制"));

    {
        // 画笔收尾自动平滑：轻微手抖的直线收敛成两点，曲线保留形状但点集被简化。
        StrokeItem jitterLine;
        jitterLine.beginStroke(QPointF(100, 100), QColor(QStringLiteral("#111827")), 4.0);
        for (int i = 0; i < 40; ++i) {
            const qreal x = 100.0 + i * 5.0;
            const qreal y = 100.0 + (i % 2 == 0 ? 1.5 : -1.5);
            jitterLine.appendScenePoint(QPointF(x, y));
        }
        jitterLine.endStroke(4.0);
        check(jitterLine.strokePointCount() == 2,
              QStringLiteral("自动平滑将手绘直线收敛为两点"),
              QString::number(jitterLine.strokePointCount()));

        StrokeItem rawLine;
        rawLine.beginStroke(QPointF(100, 100), QColor(QStringLiteral("#111827")), 4.0);
        for (int i = 0; i < 40; ++i) {
            const qreal x = 100.0 + i * 5.0;
            const qreal y = 100.0 + (i % 2 == 0 ? 1.5 : -1.5);
            rawLine.appendScenePoint(QPointF(x, y));
        }
        rawLine.endStroke(0.0);
        check(rawLine.strokePointCount() == 41,
              QStringLiteral("平滑参数为 0 时关闭自动平滑"),
              QString::number(rawLine.strokePointCount()));

        StrokeItem rawCurve;
        rawCurve.beginStroke(QPointF(100, 200), QColor(QStringLiteral("#111827")), 4.0);
        StrokeItem smoothCurve;
        smoothCurve.beginStroke(QPointF(100, 200), QColor(QStringLiteral("#111827")), 4.0);
        for (int i = 0; i < 60; ++i) {
            const qreal x = 100.0 + i * 4.0;
            const qreal y = 200.0 + 24.0 * qSin(i * 0.35) + (i % 2 == 0 ? 1.2 : -1.2);
            rawCurve.appendScenePoint(QPointF(x, y));
            smoothCurve.appendScenePoint(QPointF(x, y));
        }
        rawCurve.endStroke(0.0);
        smoothCurve.endStroke(4.0);
        check(smoothCurve.strokePointCount() >= 3
                  && (smoothCurve.strokePointCount() != rawCurve.strokePointCount()
                      || smoothCurve.strokePoints() != rawCurve.strokePoints()),
              QStringLiteral("自动平滑保留曲线并简化采样点"),
              QStringLiteral("raw=%1 smooth=%2")
                  .arg(rawCurve.strokePointCount())
                  .arg(smoothCurve.strokePointCount()));

        CanvasScene manualScene;
        manualScene.applyCanvasSize(QSizeF(800, 600));
        manualScene.setShowGrid(false);
        manualScene.setMode(ToolMode::Edit);
        QUndoStack manualUndo;
        manualScene.setUndoStack(&manualUndo);
        auto* manualStroke = new StrokeItem();
        manualStroke->setLayerId(manualScene.activeLayerId());
        manualStroke->beginStroke(QPointF(120, 240), QColor(QStringLiteral("#111827")), 4.0);
        for (int i = 0; i < 50; ++i) {
            const qreal x = 120.0 + i * 4.0;
            const qreal y = 240.0 + 18.0 * qSin(i * 0.4);
            manualStroke->appendScenePoint(QPointF(x, y));
        }
        manualStroke->endStroke(0.0);
        manualScene.attachItem(manualStroke);
        manualStroke->setSelected(true);
        const QVector<QPointF> beforeSmooth = manualStroke->strokePoints();
        const int manuallySmoothed = manualScene.smoothSelectedStrokes(4.0);
        const QVector<QPointF> afterSmooth = manualStroke->strokePoints();
        check(manuallySmoothed == 1 && manualUndo.count() == 1,
              QStringLiteral("选中笔画可手动平滑并进入撤销栈"),
              QStringLiteral("changed=%1 undo=%2").arg(manuallySmoothed).arg(manualUndo.count()));
        manualUndo.undo();
        check(manualStroke->strokePoints() == beforeSmooth,
              QStringLiteral("撤销手动平滑恢复笔画采样点"));
        manualUndo.redo();
        check(manualStroke->strokePoints() == afterSmooth,
              QStringLiteral("重做手动平滑恢复平滑结果"));
    }

    int textPlacementRequests = 0;
    QPointF requestedTextPosition;
    QObject::connect(&view, &CanvasView::textPlacementRequested, &view,
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
    check(viewScene.canvasItemCount() == 1, QStringLiteral("文字模式点击不会直接创建图元"));
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
    const QPointF panAnchor = view.mapToScene(panStart);
    sendMouseEvent(view.viewport(), QEvent::MouseButtonPress, panStart,
                   Qt::LeftButton, Qt::LeftButton);

    // 连续小幅拖动：按下点在场景里的位置必须始终留在鼠标下方。若平移用“按下时的
    // 场景锚点”去和随视图移动后的 mapToScene 相减，位移会被反复累加，画面抖动。
    qreal maxPanDrift = 0.0;
    for (int step = 1; step <= 6; ++step) {
        const QPoint cursor = panStart + QPoint(step * 10, step * 6);
        sendMouseEvent(view.viewport(), QEvent::MouseMove, cursor, Qt::NoButton, Qt::LeftButton);
        QApplication::processEvents();
        maxPanDrift = qMax(maxPanDrift, QLineF(view.mapToScene(cursor), panAnchor).length());
    }
    check(maxPanDrift < 3.0,
          QStringLiteral("拖拽平移时图元紧跟鼠标，不会来回抖动"),
          QStringLiteral("maxDrift=%1").arg(maxPanDrift));

    // 拖回起点：视图应回到原位，不残留位移。
    sendMouseEvent(view.viewport(), QEvent::MouseMove, panStart, Qt::NoButton, Qt::LeftButton);
    QApplication::processEvents();
    check(QLineF(view.mapToScene(view.viewport()->rect().center()), centerBefore).length() < 3.0,
          QStringLiteral("来回拖拽后视图回到原位，不累积漂移"));

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

    {
        // 编辑模式按住 Ctrl：左键拖动平移整体视图，不移动图元、也不清空已有选择。
        const QPointF ctrlDragItemBefore = drawnStroke ? drawnStroke->pos() : QPointF();
        const int ctrlDragSelectionBefore = viewScene.selectedItems().size();
        const QPointF ctrlDragCenterBefore = view.mapToScene(view.viewport()->rect().center());
        const QPoint ctrlDragStart = view.mapFromScene(
            drawnStroke ? drawnStroke->sceneContentRect().center() : QPointF(500.0, 400.0));
        sendMouseEvent(view.viewport(), QEvent::MouseButtonPress, ctrlDragStart,
                       Qt::LeftButton, Qt::LeftButton, Qt::ControlModifier);
        sendMouseEvent(view.viewport(), QEvent::MouseMove, ctrlDragStart + QPoint(70, 45),
                       Qt::NoButton, Qt::LeftButton, Qt::ControlModifier);
        sendMouseEvent(view.viewport(), QEvent::MouseButtonRelease, ctrlDragStart + QPoint(70, 45),
                       Qt::LeftButton, Qt::NoButton, Qt::ControlModifier);
        QApplication::processEvents();
        const QPointF ctrlDragCenterAfter = view.mapToScene(view.viewport()->rect().center());
        check(QLineF(ctrlDragCenterBefore, ctrlDragCenterAfter).length() > 1.0,
              QStringLiteral("编辑模式按住 Ctrl 拖动平移整体视图"),
              QStringLiteral("before=%1,%2 after=%3,%4")
                  .arg(ctrlDragCenterBefore.x()).arg(ctrlDragCenterBefore.y())
                  .arg(ctrlDragCenterAfter.x()).arg(ctrlDragCenterAfter.y()));
        check(drawnStroke && drawnStroke->pos() == ctrlDragItemBefore,
              QStringLiteral("编辑模式 Ctrl 拖动不移动图元"));
        check(viewScene.selectedItems().size() == ctrlDragSelectionBefore,
              QStringLiteral("编辑模式 Ctrl 拖动不清空已有选择"));
    }

    if (drawnStroke) {
        // Ctrl 只是点击（没有拖动）时仍按点选处理，保留原有的“Ctrl 点击切换选中”。
        const bool ctrlClickSelectedBefore = drawnStroke->isSelected();
        const QPoint ctrlClickPoint = view.mapFromScene(drawnStroke->sceneContentRect().center());
        sendMouseEvent(view.viewport(), QEvent::MouseButtonPress, ctrlClickPoint,
                       Qt::LeftButton, Qt::LeftButton, Qt::ControlModifier);
        sendMouseEvent(view.viewport(), QEvent::MouseButtonRelease, ctrlClickPoint,
                       Qt::LeftButton, Qt::NoButton, Qt::ControlModifier);
        QApplication::processEvents();
        check(drawnStroke->isSelected() != ctrlClickSelectedBefore,
              QStringLiteral("编辑模式 Ctrl 点击切换图元选中"));
        sendMouseEvent(view.viewport(), QEvent::MouseButtonPress, ctrlClickPoint,
                       Qt::LeftButton, Qt::LeftButton, Qt::ControlModifier);
        sendMouseEvent(view.viewport(), QEvent::MouseButtonRelease, ctrlClickPoint,
                       Qt::LeftButton, Qt::NoButton, Qt::ControlModifier);
        QApplication::processEvents();
        check(drawnStroke->isSelected() == ctrlClickSelectedBefore,
              QStringLiteral("编辑模式再次 Ctrl 点击恢复图元选中状态"));

        // Ctrl + 滚轮同样缩放整体视图，且尺寸不随缩放改变。
        const qreal ctrlWheelZoomBefore = view.zoom();
        const QSizeF ctrlWheelSizeBefore = drawnStroke->contentSize();
        const QPoint wheelCenter = view.viewport()->rect().center();
        QWheelEvent ctrlWheel(QPointF(wheelCenter),
                              QPointF(view.viewport()->mapToGlobal(wheelCenter)),
                              QPoint(), QPoint(0, 120), Qt::NoButton, Qt::ControlModifier,
                              Qt::ScrollUpdate, false);
        QApplication::sendEvent(view.viewport(), &ctrlWheel);
        QApplication::processEvents();
        check(view.zoom() > ctrlWheelZoomBefore,
              QStringLiteral("编辑模式 Ctrl 滚轮缩放整体视图"),
              QStringLiteral("before=%1 after=%2").arg(ctrlWheelZoomBefore).arg(view.zoom()));
        check(drawnStroke->contentSize() == ctrlWheelSizeBefore,
              QStringLiteral("编辑模式 Ctrl 滚轮缩放不修改图元尺寸"));
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
    int itemPropertyRequests = 0;
    QObject::connect(&view, &CanvasView::itemPropertiesRequested, &view,
                     [&itemPropertyRequests](CanvasItem*) { ++itemPropertyRequests; });
    const QPoint doubleClickPoint = view.mapFromScene(
        doubleClickText->sceneContentRect().center());
    QMouseEvent doubleClickEvent(
        QEvent::MouseButtonDblClick, QPointF(doubleClickPoint),
        QPointF(view.viewport()->mapToGlobal(doubleClickPoint)),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &doubleClickEvent);
    QApplication::processEvents();
    check(itemPropertyRequests == 1, QStringLiteral("双击文字请求属性弹窗"));

    // 需求 42：查看模式下双击图元也可以打开（只读）属性窗。
    view.setMode(ToolMode::View);
    view.fitAll();
    QApplication::processEvents();
    const QPoint viewDoubleClickPoint = view.mapFromScene(
        doubleClickText->sceneContentRect().center());
    QMouseEvent viewDoubleClickEvent(
        QEvent::MouseButtonDblClick, QPointF(viewDoubleClickPoint),
        QPointF(view.viewport()->mapToGlobal(viewDoubleClickPoint)),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &viewDoubleClickEvent);
    QApplication::processEvents();
    check(itemPropertyRequests == 2,
          QStringLiteral("查看模式双击文字仍请求属性弹窗"));
    view.setMode(ToolMode::Edit);

    {
        ItemPropertiesDialog dialog(&viewScene, doubleClickText, &view);
        dialog.show();
        QApplication::processEvents();
        bool hasEditText = false;
        for (QPushButton* button : dialog.findChildren<QPushButton*>()) {
            if (button->text() == uiText(QStringLiteral("Edit text…"))) {
                hasEditText = true;
                break;
            }
        }
        check(!hasEditText, QStringLiteral("文字属性窗不再提供编辑文字按钮"));
        QPlainTextEdit* inlineText = dialog.findChild<QPlainTextEdit*>(
            QStringLiteral("ItemPropertiesTextValue"));
        check(inlineText && inlineText->isEnabled(),
              QStringLiteral("文字属性窗保留直接编辑正文"));
        QToolButton* textColorValue = dialog.findChild<QToolButton*>(
            QStringLiteral("ItemPropertiesTextColorValue"));
        QPushButton* fontValue = dialog.findChild<QPushButton*>(
            QStringLiteral("ItemPropertiesFontValue"));
        check(textColorValue && textColorValue->isEnabled(),
              QStringLiteral("文字属性窗文字颜色可编辑"));
        check(fontValue && fontValue->isEnabled(),
              QStringLiteral("文字属性窗字体可编辑"));
        dialog.close();
        QApplication::processEvents();
    }

    {
        ItemPropertiesDialog readOnlyDialog(&viewScene, doubleClickText, &view, false);
        readOnlyDialog.show();
        QApplication::processEvents();
        check(!readOnlyDialog.findChild<QCheckBox*>(
                  QStringLiteral("ItemPropertiesBoldValue"))
                  && !readOnlyDialog.findChild<QCheckBox*>(
                      QStringLiteral("ItemPropertiesItalicValue"))
                  && !readOnlyDialog.findChild<QCheckBox*>(
                      QStringLiteral("ItemPropertiesUnderlineValue"))
                  && !readOnlyDialog.findChild<QCheckBox*>(
                      QStringLiteral("ItemPropertiesStrikeOutValue")),
              QStringLiteral("只读文字属性窗不显示文字样式选项"));
        readOnlyDialog.close();
        QApplication::processEvents();
    }

    // 需求：选中图元后按 Q 或双击图元，弹窗显示 layer / height / 填充样式 / 颜色等属性。
    auto* propertyShape = new ShapeItem(ShapeType::RoundedRectangle);
    propertyShape->setStyle(QColor(QStringLiteral("#1D4ED8")),
                            QColor(QStringLiteral("#BFDBFE")), 6.0);
    propertyShape->setFill(QColor(QStringLiteral("#BFDBFE")), FillStyle::GridHatch);
    propertyShape->setContentSize(QSizeF(180.0, 120.0));
    propertyShape->setPos(QPointF(360, 620));
    viewScene.addItemsWithUndo({ propertyShape }, QStringLiteral("Add property shape"));
    view.fitAll();
    QApplication::processEvents();

    const int requestsBeforeShape = itemPropertyRequests;
    const QPoint shapePoint = view.mapFromScene(propertyShape->sceneContentRect().center());
    QMouseEvent shapeDoubleClick(
        QEvent::MouseButtonDblClick, QPointF(shapePoint),
        QPointF(view.viewport()->mapToGlobal(shapePoint)),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &shapeDoubleClick);
    QApplication::processEvents();
    check(itemPropertyRequests == requestsBeforeShape + 1,
          QStringLiteral("双击图形请求属性弹窗"));

    {
        ItemPropertiesDialog dialog(&viewScene, propertyShape, &view);
        dialog.show();
        QApplication::processEvents();
        QLabel* typeValue = dialog.findChild<QLabel*>(
            QStringLiteral("ItemPropertiesTypeValue"));
        QLineEdit* nameValue = dialog.findChild<QLineEdit*>(
            QStringLiteral("ItemPropertiesNameValue"));
        QComboBox* layerValue = dialog.findChild<QComboBox*>(
            QStringLiteral("ItemPropertiesLayerValue"));
        QSpinBox* heightValue = dialog.findChild<QSpinBox*>(
            QStringLiteral("ItemPropertiesHeightValue"));
        QComboBox* fillStyleValue = dialog.findChild<QComboBox*>(
            QStringLiteral("ItemPropertiesFillStyleValue"));
        QToolButton* fillColorValue = dialog.findChild<QToolButton*>(
            QStringLiteral("ItemPropertiesFillColorValue"));
        QToolButton* strokeColorValue = dialog.findChild<QToolButton*>(
            QStringLiteral("ItemPropertiesStrokeColorValue"));
        QDoubleSpinBox* strokeWidthValue = dialog.findChild<QDoubleSpinBox*>(
            QStringLiteral("ItemPropertiesStrokeWidthValue"));
        QDoubleSpinBox* positionXValue = dialog.findChild<QDoubleSpinBox*>(
            QStringLiteral("ItemPropertiesPositionXValue"));
        QDoubleSpinBox* widthValue = dialog.findChild<QDoubleSpinBox*>(
            QStringLiteral("ItemPropertiesWidthValue"));
        check(typeValue && typeValue->text() == propertyShape->displayName(),
              QStringLiteral("属性窗显示图元类型"));
        check(uiText(QStringLiteral("Item name")) == QStringLiteral("图元名称"),
              QStringLiteral("图元名称属性有中文翻译"),
              uiText(QStringLiteral("Item name")));
        check(nameValue && nameValue->isEnabled()
                  && nameValue->text() == propertyShape->name(),
              QStringLiteral("属性窗显示图元名称且可编辑"));
        check(layerValue && layerValue->isEnabled()
                  && layerValue->currentText()
                      == ItemPropertiesDialog::layerDescription(&viewScene,
                                                               propertyShape->layerId()),
              QStringLiteral("属性窗显示并可编辑图元所在图层"));
        check(heightValue && heightValue->isEnabled()
                  && heightValue->value() == propertyShape->height(),
              QStringLiteral("属性窗显示并可编辑层内高度"));
        check(fillStyleValue && fillStyleValue->isEnabled()
                  && fillStyleValue->currentText() == fillStyleName(propertyShape->fillStyle()),
              QStringLiteral("属性窗显示并可编辑填充样式"),
              fillStyleValue ? fillStyleValue->currentText() : QString());
        check(fillColorValue && fillColorValue->isEnabled(),
              QStringLiteral("属性窗填充颜色可编辑"));
        check(strokeColorValue && strokeColorValue->isEnabled(),
              QStringLiteral("属性窗描边颜色可编辑"));
        check(strokeWidthValue && strokeWidthValue->isEnabled()
                  && qFuzzyCompare(strokeWidthValue->value(), propertyShape->strokeWidth()),
              QStringLiteral("属性窗描边宽度可编辑"));
        check(positionXValue && positionXValue->isEnabled()
                  && widthValue && widthValue->isEnabled(),
              QStringLiteral("属性窗位置和尺寸可编辑"));
        dialog.close();
        QApplication::processEvents();
    }

    // 需求 24：除图元类型外，属性弹窗的修改要真正落到图元，并且一次提交只占一条撤销记录。
    {
        CanvasScene editScene;
        editScene.applyCanvasSize(QSizeF(800.0, 600.0));
        editScene.setMode(ToolMode::Edit);
        QUndoStack editUndo;
        editScene.setUndoStack(&editUndo);

        auto* firstShape = new ShapeItem(ShapeType::Rectangle);
        firstShape->setStyle(QColor(QStringLiteral("#1E293B")),
                             QColor(QStringLiteral("#93C5FD")), 3.0);
        firstShape->setFill(QColor(QStringLiteral("#93C5FD")), FillStyle::Solid);
        firstShape->setContentSize(QSizeF(120.0, 80.0));
        firstShape->setPos(QPointF(40.0, 50.0));
        editScene.addItemsWithUndo({ firstShape }, QStringLiteral("Add property test shape"));
        const QString firstAutoName = firstShape->rawName();
        check(firstAutoName == QStringLiteral("矩形 1"),
              QStringLiteral("新图元自动生成名称"), firstAutoName);
        const int firstLayerId = firstShape->layerId();
        const int firstHeight = firstShape->height();
        const ShapeItemState firstState = firstShape->state();

        check(editScene.addLayerWithUndo(QStringLiteral("属性目标层")),
              QStringLiteral("创建属性编辑目标图层"));
        const int targetLayerId = editScene.activeLayerId();
        auto* secondShape = new ShapeItem(ShapeType::Ellipse);
        secondShape->setStyle(QColor(QStringLiteral("#334155")),
                              QColor(QStringLiteral("#FDE68A")), 2.0);
        secondShape->setContentSize(QSizeF(100.0, 100.0));
        secondShape->setPos(QPointF(240.0, 80.0));
        editScene.addItemsWithUndo({ secondShape }, QStringLiteral("Add target layer shape"));
        check(secondShape->rawName() == QStringLiteral("椭圆 1"),
              QStringLiteral("不同图元类型各自从 1 编号"), secondShape->rawName());

        auto* thirdShape = new ShapeItem(ShapeType::Rectangle);
        thirdShape->setContentSize(QSizeF(90.0, 60.0));
        thirdShape->setPos(QPointF(360.0, 200.0));
        editScene.addItemsWithUndo({ thirdShape }, QStringLiteral("Add second rectangle"));
        check(thirdShape->rawName() == QStringLiteral("矩形 2"),
              QStringLiteral("同类图元名称后缀自动递增"), thirdShape->rawName());

        const int undoCountBeforeEdit = editUndo.count();
        {
            ItemPropertiesDialog dialog(&editScene, firstShape);
            dialog.show();
            QApplication::processEvents();
            QComboBox* layerValue = dialog.findChild<QComboBox*>(
                QStringLiteral("ItemPropertiesLayerValue"));
            QLineEdit* nameValue = dialog.findChild<QLineEdit*>(
                QStringLiteral("ItemPropertiesNameValue"));
            QSpinBox* heightValue = dialog.findChild<QSpinBox*>(
                QStringLiteral("ItemPropertiesHeightValue"));
            QComboBox* fillStyleValue = dialog.findChild<QComboBox*>(
                QStringLiteral("ItemPropertiesFillStyleValue"));
            QToolButton* fillColorValue = dialog.findChild<QToolButton*>(
                QStringLiteral("ItemPropertiesFillColorValue"));
            QToolButton* strokeColorValue = dialog.findChild<QToolButton*>(
                QStringLiteral("ItemPropertiesStrokeColorValue"));
            QDoubleSpinBox* strokeWidthValue = dialog.findChild<QDoubleSpinBox*>(
                QStringLiteral("ItemPropertiesStrokeWidthValue"));
            QDoubleSpinBox* positionXValue = dialog.findChild<QDoubleSpinBox*>(
                QStringLiteral("ItemPropertiesPositionXValue"));
            QDoubleSpinBox* positionYValue = dialog.findChild<QDoubleSpinBox*>(
                QStringLiteral("ItemPropertiesPositionYValue"));
            QDoubleSpinBox* widthValue = dialog.findChild<QDoubleSpinBox*>(
                QStringLiteral("ItemPropertiesWidthValue"));
            QDoubleSpinBox* heightSizeValue = dialog.findChild<QDoubleSpinBox*>(
                QStringLiteral("ItemPropertiesHeightSizeValue"));
            QDoubleSpinBox* rotationValue = dialog.findChild<QDoubleSpinBox*>(
                QStringLiteral("ItemPropertiesRotationValue"));
            check(layerValue && heightValue && nameValue && fillStyleValue && fillColorValue
                      && strokeColorValue && strokeWidthValue && positionXValue
                      && positionYValue && widthValue && heightSizeValue && rotationValue,
                  QStringLiteral("属性窗编辑控件齐全"));
            check(rotationValue && qFuzzyCompare(rotationValue->value(),
                                                 firstShape->rotationAngle()),
                  QStringLiteral("属性窗显示图元旋转角度"));

            if (nameValue)
                nameValue->setText(QStringLiteral("Hero panel"));
            if (layerValue) {
                const int targetIndex = layerValue->findData(targetLayerId);
                check(targetIndex >= 0, QStringLiteral("属性窗图层下拉包含目标图层"));
                if (targetIndex >= 0)
                    layerValue->setCurrentIndex(targetIndex);
            }
            QApplication::processEvents();
            if (heightValue)
                heightValue->setValue(2);
            if (fillStyleValue)
                fillStyleValue->setCurrentIndex(
                    fillStyleValue->findData(static_cast<int>(FillStyle::Honeycomb)));
            if (fillColorValue)
                fillColorValue->setProperty("itemColor", QColor(QStringLiteral("#22C55E")));
            if (strokeColorValue)
                strokeColorValue->setProperty("itemColor", QColor(QStringLiteral("#DC2626")));
            if (strokeWidthValue)
                strokeWidthValue->setValue(9.0);
            if (positionXValue)
                positionXValue->setValue(150.0);
            if (positionYValue)
                positionYValue->setValue(210.0);
            if (widthValue)
                widthValue->setValue(240.0);
            if (heightSizeValue)
                heightSizeValue->setValue(160.0);
            if (rotationValue)
                rotationValue->setValue(30.0);
            dialog.accept();
            QApplication::processEvents();
        }

        check(editUndo.count() == undoCountBeforeEdit + 1,
              QStringLiteral("属性窗修改只生成一条撤销记录"));
        check(firstShape->layerId() == targetLayerId && firstShape->height() == 2,
              QStringLiteral("属性窗修改可落到图层和层内高度"));
        check(firstShape->fillStyle() == FillStyle::Honeycomb
                  && firstShape->fillColor() == QColor(QStringLiteral("#22C55E")),
              QStringLiteral("属性窗修改可落到填充样式和填充颜色"));
        check(firstShape->strokeColor() == QColor(QStringLiteral("#DC2626"))
                  && qFuzzyCompare(firstShape->strokeWidth(), 9.0),
              QStringLiteral("属性窗修改可落到描边颜色和描边宽度"));
        check(firstShape->pos() == QPointF(150.0, 210.0)
                  && firstShape->contentSize() == QSizeF(240.0, 160.0),
              QStringLiteral("属性窗修改可落到位置和尺寸"));
        check(firstShape->rawName() == QStringLiteral("Hero panel"),
              QStringLiteral("属性窗修改可落到图元名称"));
        check(qFuzzyCompare(firstShape->rotationAngle(), 30.0),
              QStringLiteral("属性窗修改可落到图元旋转角度"));

        editUndo.undo();
        QApplication::processEvents();
        check(firstShape->layerId() == firstLayerId && firstShape->height() == firstHeight,
              QStringLiteral("属性窗修改可整体撤销图层和高度"));
        check(qFuzzyCompare(firstShape->rotationAngle(), 0.0),
              QStringLiteral("属性窗修改可撤销旋转角度"));
        check(firstShape->state() == firstState,
              QStringLiteral("属性窗修改可整体撤销样式和几何"));
        check(firstShape->rawName() == firstAutoName,
              QStringLiteral("属性窗修改可撤销图元名称"));

        editUndo.redo();
        QApplication::processEvents();
        check(firstShape->layerId() == targetLayerId && firstShape->height() == 2,
              QStringLiteral("属性窗修改可整体重做"));
        check(firstShape->rawName() == QStringLiteral("Hero panel"),
              QStringLiteral("属性窗修改可重做图元名称"));
        check(qFuzzyCompare(firstShape->rotationAngle(), 30.0),
              QStringLiteral("属性窗修改可重做旋转角度"));

        check(qFuzzyCompare(CanvasItem::snapRotation(12.4), 10.0)
                  && qFuzzyCompare(CanvasItem::snapRotation(-12.4), -10.0),
              QStringLiteral("旋转手柄角度按 5° 步长吸附"));
        const QRectF rotationFrame = firstShape->contentRect();
        const QPointF rotationHandlePoint(rotationFrame.center().x(),
                                          rotationFrame.top() - 25.0);
        editScene.clearSelection();
        firstShape->setSelected(true);
        check(firstShape->shape().contains(rotationHandlePoint),
              QStringLiteral("选中单个图元时显示旋转手柄"));
        firstShape->setSelected(false);
        check(!firstShape->shape().contains(rotationHandlePoint),
              QStringLiteral("未选中图元时不显示旋转手柄"));
        firstShape->setSelected(true);
        const int rotateUndoBefore = editUndo.count();
        const int rotatedCount = editScene.rotateSelected(45.0);
        check(rotatedCount == 1 && qFuzzyCompare(firstShape->rotationAngle(), 75.0)
                  && editUndo.count() == rotateUndoBefore + 1,
              QStringLiteral("旋转快捷键对选中图元增加 45°"));
        editUndo.undo();
        check(qFuzzyCompare(firstShape->rotationAngle(), 30.0),
              QStringLiteral("旋转快捷键可整体撤销"));
        editUndo.redo();
        check(qFuzzyCompare(firstShape->rotationAngle(), 75.0),
              QStringLiteral("旋转快捷键可重做"));
        check(editScene.rotateSelected(-45.0) == 1
                  && qFuzzyCompare(firstShape->rotationAngle(), 30.0),
              QStringLiteral("逆时针旋转快捷键减少 45°"));

        // 需求 35：低缩放下旋转手柄必须仍在 boundingRect 与场景命中范围内。
        {
            CanvasView lowZoomView(&editScene);
            lowZoomView.resize(900, 700);
            lowZoomView.show();
            QApplication::processEvents();
            for (int i = 0; i < 5; ++i)
                lowZoomView.zoomOut();
            const qreal lowZoom = lowZoomView.zoom();
            check(lowZoom < 0.6, QStringLiteral("低缩放测试视图确实缩小"));
            firstShape->setSelected(true);
            const QRectF rotationFrame = firstShape->contentRect();
            const qreal lowZoomHandleSize = qBound(6.0, 9.0 / lowZoom, 40.0);
            const QPointF lowZoomHandle(
                rotationFrame.center().x(),
                rotationFrame.top() - (lowZoomHandleSize + 16.0 / lowZoom));
            check(firstShape->boundingRect().contains(lowZoomHandle),
                  QStringLiteral("低缩放下旋转手柄位于 boundingRect 内"));
            check(firstShape->shape().contains(lowZoomHandle),
                  QStringLiteral("低缩放下旋转手柄仍在 shape 命中范围内"));
            check(editScene.items(firstShape->mapToScene(lowZoomHandle)).contains(firstShape),
                  QStringLiteral("低缩放下旋转手柄可被场景命中"));
            lowZoomView.hide();
        }
        editScene.clearSelection();

        QJsonObject namedJson = firstShape->toJson();
        ShapeItem loadedNamedShape;
        loadedNamedShape.fromJson(namedJson);
        check(loadedNamedShape.rawName() == QStringLiteral("Hero panel"),
              QStringLiteral("图元名称持久化到画布"));
        check(qFuzzyCompare(loadedNamedShape.rotationAngle(), 30.0),
              QStringLiteral("图元旋转角度持久化到画布"));

        const int noopUndoBefore = editUndo.count();
        {
            ItemPropertiesDialog dialog(&editScene, firstShape);
            dialog.show();
            QApplication::processEvents();
            dialog.accept();
            QApplication::processEvents();
        }
        check(editUndo.count() == noopUndoBefore,
              QStringLiteral("属性窗未修改时确定不产生撤销记录"));

        auto* imageItem = new ImageItem();
        imageItem->setImage(QPixmap(24, 24), QStringLiteral("old.png"));
        imageItem->setContentSize(QSizeF(80.0, 80.0));
        imageItem->setPos(QPointF(400.0, 200.0));
        editScene.addItemsWithUndo({ imageItem }, QStringLiteral("Add property image"));
        check(imageItem->rawName() == QStringLiteral("图片 1"),
              QStringLiteral("图片图元自动名称不使用文件名"), imageItem->rawName());
        const int imageUndoBefore = editUndo.count();
        {
            ItemPropertiesDialog dialog(&editScene, imageItem);
            dialog.show();
            QApplication::processEvents();
            QLineEdit* imageName = dialog.findChild<QLineEdit*>(
                QStringLiteral("ItemPropertiesImageValue"));
            QLabel* typeValue = dialog.findChild<QLabel*>(
                QStringLiteral("ItemPropertiesTypeValue"));
            QLabel* heading = dialog.findChild<QLabel*>(
                QStringLiteral("ItemPropertiesHeading"));
            check(imageName && imageName->isEnabled()
                      && imageName->text() == QStringLiteral("old.png"),
                  QStringLiteral("图片属性窗图片名称可编辑"));
            check(typeValue && typeValue->text() == uiText(QStringLiteral("Image"))
                      && heading && heading->text() == imageItem->name(),
                  QStringLiteral("图片属性窗标题和类型使用图元名称与类型名"));
            if (imageName)
                imageName->setText(QStringLiteral("new.png"));
            dialog.accept();
            QApplication::processEvents();
        }
        check(imageItem->imageName() == QStringLiteral("new.png")
                  && editUndo.count() == imageUndoBefore + 1,
              QStringLiteral("图片属性窗修改图片名称并生成一条撤销记录"));
        editUndo.undo();
        QApplication::processEvents();
        check(imageItem->imageName() == QStringLiteral("old.png"),
              QStringLiteral("图片属性窗修改可撤销"));

        const int imageBodyUndoIndex = editUndo.index();
        {
            ItemPropertiesDialog dialog(&editScene, imageItem);
            dialog.show();
            QApplication::processEvents();
            QPushButton* replaceButton = dialog.findChild<QPushButton*>(
                QStringLiteral("ItemPropertiesReplaceImageValue"));
            check(replaceButton && replaceButton->isEnabled() && dialog.isModal(),
                  QStringLiteral("图片属性窗提供替换图片按钮且属性窗为模态"));
            QFile iconFile(QStringLiteral(":/icons/app.png"));
            const bool iconRead = iconFile.open(QIODevice::ReadOnly);
            const QByteArray pngBytes = iconRead ? iconFile.readAll() : QByteArray();
            const QString replacementPath = QDir(workDirectory)
                .filePath(QStringLiteral("chprops-replacement-probe.png"));
            QFile probeFile(replacementPath);
            const bool probeWritten = probeFile.open(QIODevice::WriteOnly)
                && probeFile.write(pngBytes) == pngBytes.size();
            probeFile.close();
            QString probeError;
            const bool probeLoaded = probeWritten && !pngBytes.isEmpty()
                && dialog.loadImageFromFile(replacementPath, &probeError);
            QFile::remove(replacementPath);
            check(!pngBytes.isEmpty(),
                  QStringLiteral("图片替换探针可读取应用图标资源"),
                  QString::number(pngBytes.size()));
            check(probeWritten,
                  QStringLiteral("图片替换探针可写入临时图片"),
                  replacementPath);
            check(probeLoaded,
                  QStringLiteral("图片属性窗可从文件载入替换图片"),
                  probeLoaded ? QStringLiteral("ok") : probeError);
            QPixmap replacement(12, 12);
            replacement.fill(Qt::blue);
            dialog.setReplacementImage(replacement);
            dialog.accept();
            QApplication::processEvents();
        }
        check(imageItem->image().size() == QSize(12, 12),
              QStringLiteral("图片本体替换生效"),
              QStringLiteral("%1x%2").arg(imageItem->image().width()).arg(imageItem->image().height()));
        check(imageItem->contentSize() == QSizeF(12.0, 12.0),
              QStringLiteral("图片本体替换后重置为原始像素尺寸"),
              QStringLiteral("%1x%2").arg(imageItem->contentSize().width())
                  .arg(imageItem->contentSize().height()));
        check(editUndo.index() == imageBodyUndoIndex + 1,
              QStringLiteral("图片本体替换进入属性撤销记录"),
              QStringLiteral("%1 / %2").arg(imageBodyUndoIndex).arg(editUndo.index()));
        editUndo.undo();
        QApplication::processEvents();
        check(imageItem->image().size() == QSize(24, 24),
              QStringLiteral("图片本体替换可撤销"));
        check(imageItem->contentSize() == QSizeF(80.0, 80.0),
              QStringLiteral("图片本体撤销恢复旧尺寸"));

        // 需求 42：非编辑模式打开属性窗时字段只读，关闭不会写回。
        const int readOnlyUndoBefore = editUndo.count();
        const QString readOnlyImageName = imageItem->imageName();
        {
            ItemPropertiesDialog dialog(&editScene, imageItem, nullptr, false);
            dialog.show();
            QApplication::processEvents();
            QLineEdit* readOnlyName = dialog.findChild<QLineEdit*>(
                QStringLiteral("ItemPropertiesImageValue"));
            QPushButton* readOnlyReplace = dialog.findChild<QPushButton*>(
                QStringLiteral("ItemPropertiesReplaceImageValue"));
            QLabel* readOnlyHint = dialog.findChild<QLabel*>(
                QStringLiteral("ItemPropertiesReadOnlyHint"));
            check(dialog.isModal() && !dialog.isEditable(),
                  QStringLiteral("属性窗在只读口径下仍为模态"));
            check(readOnlyName && !readOnlyName->isEnabled()
                      && readOnlyReplace && !readOnlyReplace->isEnabled(),
                  QStringLiteral("非编辑模式属性窗字段与替换按钮不可编辑"));
            check(readOnlyHint != nullptr,
                  QStringLiteral("非编辑模式属性窗显示只读提示"));
            if (readOnlyName)
                readOnlyName->setText(QStringLiteral("read-only-probe.png"));
            dialog.accept();
            QApplication::processEvents();
        }
        check(imageItem->imageName() == readOnlyImageName
                  && editUndo.count() == readOnlyUndoBefore,
              QStringLiteral("只读属性窗关闭后不写回图元"));

        QFont propertyFont(QStringLiteral("Microsoft YaHei UI"));
        propertyFont.setPointSize(16);
        auto* textItem = new TextItem();
        textItem->setTextFormat(QStringLiteral("属性文字"), propertyFont,
                                QColor(QStringLiteral("#0F766E")));
        textItem->setContentSize(QSizeF(200.0, 50.0));
        textItem->setPos(QPointF(420.0, 300.0));
        editScene.addItemsWithUndo({ textItem }, QStringLiteral("Add property text"));
        check(textItem->rawName() == QStringLiteral("文字 1"),
              QStringLiteral("文字图元自动名称不使用文字内容"), textItem->rawName());

        const int textPropertyUndoBefore = editUndo.count();
        const QSizeF textSizeBefore = textItem->contentSize();
        {
            ItemPropertiesDialog dialog(&editScene, textItem);
            dialog.show();
            QApplication::processEvents();
            QPlainTextEdit* textValue = dialog.findChild<QPlainTextEdit*>(
                QStringLiteral("ItemPropertiesTextValue"));
            check(textValue && textValue->isEnabled()
                      && textValue->toPlainText() == QStringLiteral("属性文字"),
                  QStringLiteral("文字属性窗可编辑文字正文"));
            if (textValue)
                textValue->setPlainText(QStringLiteral("属性文字已更新"));
            dialog.accept();
            QApplication::processEvents();
        }
        check(textItem->text() == QStringLiteral("属性文字已更新")
                  && editUndo.count() == textPropertyUndoBefore + 1,
              QStringLiteral("文字正文修改进入属性撤销记录"));
        check(textItem->contentSize() == TextItem::measure(textItem->text(), textItem->textFont()),
              QStringLiteral("文字正文修改后重置为自然尺寸"),
              QStringLiteral("%1x%2").arg(textItem->contentSize().width())
                  .arg(textItem->contentSize().height()));
        editUndo.undo();
        QApplication::processEvents();
        check(textItem->text() == QStringLiteral("属性文字"),
              QStringLiteral("文字正文修改可撤销"));
        check(textItem->contentSize() == textSizeBefore,
              QStringLiteral("文字正文撤销恢复旧尺寸"));

        auto* firstNamingStroke = new StrokeItem();
        firstNamingStroke->beginStroke(QPointF(90.0, 420.0),
                                       QColor(QStringLiteral("#0F766E")), 5.0);
        firstNamingStroke->appendScenePoint(QPointF(150.0, 470.0));
        firstNamingStroke->appendScenePoint(QPointF(210.0, 440.0));
        firstNamingStroke->endStroke();
        editScene.addItemsWithUndo({ firstNamingStroke },
                                   QStringLiteral("Add numbered stroke"));
        auto* secondNamingStroke = new StrokeItem();
        secondNamingStroke->beginStroke(QPointF(260.0, 420.0),
                                        QColor(QStringLiteral("#0F766E")), 5.0);
        secondNamingStroke->appendScenePoint(QPointF(320.0, 470.0));
        secondNamingStroke->appendScenePoint(QPointF(380.0, 440.0));
        secondNamingStroke->endStroke();
        editScene.addItemsWithUndo({ secondNamingStroke },
                                   QStringLiteral("Add second numbered stroke"));
        check(firstNamingStroke->rawName() == QStringLiteral("笔画 1")
                  && secondNamingStroke->rawName() == QStringLiteral("笔画 2"),
              QStringLiteral("笔画图元名称后缀自动递增"),
              firstNamingStroke->rawName() + QStringLiteral(" / ")
                  + secondNamingStroke->rawName());

        int strokeUndoBefore = editUndo.count();
        firstNamingStroke->setName(QString());
        {
            ItemPropertiesDialog dialog(&editScene, firstNamingStroke);
            dialog.show();
            QApplication::processEvents();
            QLineEdit* strokeNameValue = dialog.findChild<QLineEdit*>(
                QStringLiteral("ItemPropertiesNameValue"));
            QLabel* strokeTypeValue = dialog.findChild<QLabel*>(
                QStringLiteral("ItemPropertiesTypeValue"));
            check(!firstNamingStroke->rawName().isEmpty()
                      && firstNamingStroke->rawName().startsWith(QStringLiteral("笔画 ")),
                  QStringLiteral("打开属性窗时为无名图元补上自动编号"),
                  firstNamingStroke->rawName());
            check(strokeNameValue && strokeNameValue->isEnabled()
                      && strokeNameValue->text() == firstNamingStroke->name(),
                  QStringLiteral("笔画属性窗显示可编辑的图元名称"),
                  strokeNameValue ? strokeNameValue->text() : QStringLiteral("<missing>"));
            check(strokeTypeValue
                      && strokeTypeValue->text() == firstNamingStroke->typeDisplayName(),
                  QStringLiteral("笔画属性窗图元类型不可编辑且与名称行并列显示"));
            dialog.reject();
            QApplication::processEvents();
        }
        check(editUndo.count() == strokeUndoBefore,
              QStringLiteral("只打开并取消笔画属性窗不产生撤销记录"));
        int unnamedItemCount = 0;
        for (CanvasItem* item : editScene.canvasItems()) {
            if (!item || item->rawName().isEmpty())
                ++unnamedItemCount;
        }
        check(unnamedItemCount == 0,
              QStringLiteral("场景内所有图元都有名称"),
              QString::number(unnamedItemCount));
        const QColor textColorBefore = textItem->textColor();
        const int textUndoBefore = editUndo.count();
        {
            ItemPropertiesDialog dialog(&editScene, textItem);
            dialog.show();
            QApplication::processEvents();
            QToolButton* textColorValue = dialog.findChild<QToolButton*>(
                QStringLiteral("ItemPropertiesTextColorValue"));
            QLabel* typeValue = dialog.findChild<QLabel*>(
                QStringLiteral("ItemPropertiesTypeValue"));
            QLabel* heading = dialog.findChild<QLabel*>(
                QStringLiteral("ItemPropertiesHeading"));
            check(typeValue && typeValue->text() == uiText(QStringLiteral("Text"))
                      && heading && heading->text() == textItem->name(),
                  QStringLiteral("文字属性窗标题和类型使用图元名称与类型名"));
            if (textColorValue)
                textColorValue->setProperty("itemColor", QColor(QStringLiteral("#DB2777")));
            dialog.accept();
            QApplication::processEvents();
        }
        check(textItem->textColor() == QColor(QStringLiteral("#DB2777"))
                  && editUndo.count() == textUndoBefore + 1,
              QStringLiteral("文字属性窗修改文字颜色并生成一条撤销记录"));
        editUndo.undo();
        QApplication::processEvents();
        check(textItem->textColor() == textColorBefore,
              QStringLiteral("文字属性窗修改可撤销"));

        {
            ItemPropertiesDialog dialog(&editScene, textItem);
            dialog.show();
            QApplication::processEvents();
            check(!dialog.findChild<QCheckBox*>(
                      QStringLiteral("ItemPropertiesBoldValue"))
                      && !dialog.findChild<QCheckBox*>(
                          QStringLiteral("ItemPropertiesItalicValue"))
                      && !dialog.findChild<QCheckBox*>(
                          QStringLiteral("ItemPropertiesUnderlineValue"))
                      && !dialog.findChild<QCheckBox*>(
                          QStringLiteral("ItemPropertiesStrikeOutValue")),
                  QStringLiteral("文字属性窗不再显示文字样式选项"));
            dialog.close();
            QApplication::processEvents();
        }

        CanvasScene reloadedNames;
        check(reloadedNames.fromJson(editScene.toJson()),
              QStringLiteral("自动名称随画布重新载入"));
        QStringList reloadedNameList;
        for (CanvasItem* item : reloadedNames.canvasItems())
            reloadedNameList.append(item->rawName());
        check(reloadedNameList.contains(QStringLiteral("Hero panel"))
                  && reloadedNameList.contains(QStringLiteral("矩形 2"))
                  && reloadedNameList.contains(QStringLiteral("椭圆 1"))
                  && reloadedNameList.contains(QStringLiteral("图片 1"))
                  && reloadedNameList.contains(QStringLiteral("文字 1")),
              QStringLiteral("自动名称画布往返不重复编号"),
              reloadedNameList.join(QStringLiteral(" / ")));
    }

    // 图元拷贝：副本偏移、名称自动接续，并整体进入一条撤销记录。
    {
        CanvasScene duplicateScene;
        duplicateScene.applyCanvasSize(QSizeF(800.0, 600.0));
        duplicateScene.setMode(ToolMode::Edit);
        QUndoStack duplicateUndo;
        duplicateScene.setUndoStack(&duplicateUndo);

        auto* source = new ShapeItem(ShapeType::Rectangle);
        source->setContentSize(QSizeF(120.0, 80.0));
        source->setPos(QPointF(40.0, 50.0));
        duplicateScene.addItemsWithUndo({ source }, QStringLiteral("Add duplicate source"));
        check(source->rawName() == QStringLiteral("矩形 1"),
              QStringLiteral("拷贝源图元使用自动名称"), source->rawName());
        source->setSelected(true);

        const int countBefore = duplicateScene.canvasItemCount();
        const int undoBefore = duplicateUndo.count();
        const int duplicated = duplicateScene.duplicateSelectedItems();
        check(duplicated == 1 && duplicateScene.canvasItemCount() == countBefore + 1,
              QStringLiteral("拷贝选中的图元"), QString::number(duplicated));

        CanvasItem* copy = nullptr;
        for (CanvasItem* item : duplicateScene.canvasItems()) {
            if (item != source && item->rawName() == QStringLiteral("矩形 2"))
                copy = item;
        }
        check(copy != nullptr, QStringLiteral("副本名称自动接续为同类下一个编号"));
        check(copy && copy->pos() == source->pos() + QPointF(20.0, 20.0),
              QStringLiteral("副本相对源图元偏移"),
              copy ? QStringLiteral("%1,%2").arg(copy->pos().x()).arg(copy->pos().y())
                   : QStringLiteral("<missing>"));
        check(!duplicateScene.isItemNameAvailable(QStringLiteral("矩形 1"), copy)
                  && duplicateScene.isItemNameAvailable(QStringLiteral("自定义名称"), copy),
              QStringLiteral("画布内名称唯一性可查询"));
        check(duplicateUndo.count() == undoBefore + 1,
              QStringLiteral("拷贝图元只产生一条撤销记录"));

        duplicateUndo.undo();
        QApplication::processEvents();
        check(duplicateScene.canvasItemCount() == countBefore,
              QStringLiteral("撤销拷贝删除副本"));
        duplicateUndo.redo();
        QApplication::processEvents();
        check(duplicateScene.canvasItemCount() == countBefore + 1 && copy
                  && copy->scene() == &duplicateScene && copy->rawName() == QStringLiteral("矩形 2"),
              QStringLiteral("重做拷贝恢复副本和名称"));
    }

    // 自定义名称拷贝：Hero panel / Hero panel 2 之后取 Hero panel 3，不产生重名。
    {
        CanvasScene customNameScene;
        customNameScene.applyCanvasSize(QSizeF(800.0, 600.0));
        customNameScene.setMode(ToolMode::Edit);
        QUndoStack customNameUndo;
        customNameScene.setUndoStack(&customNameUndo);

        auto* first = new ShapeItem(ShapeType::Rectangle);
        first->setContentSize(QSizeF(100.0, 60.0));
        customNameScene.addItemsWithUndo({ first }, QStringLiteral("Add first custom item"));
        first->setName(QStringLiteral("Hero panel"));

        auto* second = new ShapeItem(ShapeType::Ellipse);
        second->setContentSize(QSizeF(90.0, 90.0));
        customNameScene.addItemsWithUndo({ second }, QStringLiteral("Add second custom item"));
        second->setName(QStringLiteral("Hero panel 2"));

        first->setSelected(true);
        const int duplicated = customNameScene.duplicateSelectedItems();
        CanvasItem* copy = nullptr;
        for (CanvasItem* item : customNameScene.canvasItems()) {
            if (item != first && item != second
                && item->rawName() == QStringLiteral("Hero panel 3"))
                copy = item;
        }
        check(duplicated == 1 && copy != nullptr,
              QStringLiteral("自定义名称拷贝时取下一个唯一名称"));
    }

    // 载入或旧文件里已有的重名，在补名时一并修复。
    {
        CanvasScene repairScene;
        auto* first = new ShapeItem(ShapeType::Rectangle);
        first->setContentSize(QSizeF(80.0, 60.0));
        repairScene.addItemsWithUndo({ first }, QStringLiteral("Add repair item"));
        auto* second = new ShapeItem(ShapeType::Rectangle);
        second->setContentSize(QSizeF(70.0, 50.0));
        repairScene.addItemsWithUndo({ second }, QStringLiteral("Add repair item"));
        first->setName(QStringLiteral("Same name"));
        second->setName(QStringLiteral("Same name"));
        repairScene.assignAutoItemNames();
        const QString firstName = first->rawName();
        const QString secondName = second->rawName();
        check(firstName != secondName
                  && (firstName == QStringLiteral("Same name")
                      || firstName == QStringLiteral("Same name 2"))
                  && (secondName == QStringLiteral("Same name")
                      || secondName == QStringLiteral("Same name 2")),
              QStringLiteral("补名时修复画布内已有重名"),
              firstName + QStringLiteral(" / ") + secondName);
    }

    // 图元分组：同层多选可组合；单个组员仍能单独选中并编辑属性；
    // 拖动任意组员时整组移动，分组与取消分组都可撤销。
    {
        CanvasScene groupScene;
        QUndoStack groupUndo;
        groupScene.setUndoStack(&groupUndo);
        groupScene.applyCanvasSize(QSizeF(800.0, 600.0));
        groupScene.setMode(ToolMode::Edit);

        auto* first = new ShapeItem(ShapeType::Rectangle);
        first->setContentSize(QSizeF(90.0, 70.0));
        first->setPos(QPointF(80.0, 90.0));
        auto* second = new ShapeItem(ShapeType::Ellipse);
        second->setContentSize(QSizeF(90.0, 70.0));
        second->setPos(QPointF(320.0, 260.0));
        groupScene.addItemsWithUndo({ first, second }, QStringLiteral("Add grouped sources"));

        first->setSelected(true);
        second->setSelected(true);
        const int grouped = groupScene.groupSelectedItems();
        const int groupId = first->groupId();
        check(grouped == 2 && groupId > 0 && second->groupId() == groupId,
              QStringLiteral("同层多选图元可组合"),
              QStringLiteral("group=%1 first=%2 second=%3")
                  .arg(grouped).arg(first->groupId()).arg(second->groupId()));
        check(groupScene.canUngroupSelectedItems(),
              QStringLiteral("组合后可取消组合"));

        groupUndo.undo();
        check(first->groupId() == 0 && second->groupId() == 0,
              QStringLiteral("撤销组合恢复未分组"));
        groupUndo.redo();
        check(first->groupId() == groupId && second->groupId() == groupId,
              QStringLiteral("重做组合恢复组标记"));

        // 单个组员仍可成为唯一选中项，属性窗保持可编辑。
        groupScene.clearSelection();
        first->setSelected(true);
        check(groupScene.selectedCanvasItems().size() == 1 && first->isEditable(),
              QStringLiteral("组合后单个图元仍可单独选中"));
        ItemPropertiesDialog propertiesDialog(&groupScene, first, nullptr);
        QLineEdit* nameEdit = propertiesDialog.findChild<QLineEdit*>(
            QStringLiteral("ItemPropertiesNameValue"));
        check(nameEdit != nullptr && !nameEdit->isReadOnly(),
              QStringLiteral("组合后单个图元属性窗仍可编辑"));

        CanvasView groupView(&groupScene);
        groupView.resize(900, 700);
        groupView.show();
        groupView.setMode(ToolMode::Edit);
        groupView.fitAll();
        QApplication::processEvents();

        const QPointF firstBefore = first->pos();
        const QPointF secondBefore = second->pos();
        const QPointF firstCenter = first->mapToScene(first->contentRect().center());
        const QPoint dragStart = groupView.mapFromScene(firstCenter);

        // 右键点中已选图元时，Qt 默认选择逻辑不能把多选收成单选，
        // 否则右键菜单里的“组合选中图元”会一直处于禁用态。
        first->setSelected(true);
        second->setSelected(true);
        sendMouseEvent(groupView.viewport(), QEvent::MouseButtonPress, dragStart,
                       Qt::RightButton, Qt::RightButton);
        sendMouseEvent(groupView.viewport(), QEvent::MouseButtonRelease, dragStart,
                       Qt::RightButton, Qt::NoButton);
        QApplication::processEvents();
        check(groupScene.selectedCanvasItems().size() == 2
                  && first->isSelected() && second->isSelected(),
              QStringLiteral("右键已选组员不破坏多选"));

        groupScene.clearSelection();
        first->setSelected(true);
        sendMouseEvent(groupView.viewport(), QEvent::MouseButtonPress, dragStart,
                       Qt::LeftButton, Qt::LeftButton);
        sendMouseEvent(groupView.viewport(), QEvent::MouseMove, dragStart + QPoint(50, 35),
                       Qt::NoButton, Qt::LeftButton);
        sendMouseEvent(groupView.viewport(), QEvent::MouseButtonRelease, dragStart + QPoint(50, 35),
                       Qt::LeftButton, Qt::NoButton);
        QApplication::processEvents();

        const QPointF firstDelta = first->pos() - firstBefore;
        const QPointF secondDelta = second->pos() - secondBefore;
        check(QLineF(firstDelta, QPointF()).length() > 1.0
                  && QLineF(firstDelta, secondDelta).length() < 0.01,
              QStringLiteral("拖动单个组员时整组一起移动"),
              QStringLiteral("first=%1,%2 second=%3,%4")
                  .arg(firstDelta.x()).arg(firstDelta.y())
                  .arg(secondDelta.x()).arg(secondDelta.y()));

        groupUndo.undo();
        check(first->pos() == firstBefore && second->pos() == secondBefore,
              QStringLiteral("撤销整组移动恢复所有组员"));
        groupUndo.redo();
        check(first->pos() != firstBefore && second->pos() != secondBefore,
              QStringLiteral("重做整组移动"));

        CanvasScene reloadedGroups;
        check(reloadedGroups.fromJson(groupScene.toJson()),
              QStringLiteral("分组画布可重新载入"));
        CanvasItem* reloadedFirst = nullptr;
        CanvasItem* reloadedSecond = nullptr;
        for (CanvasItem* item : reloadedGroups.canvasItems()) {
            if (item->rawName() == first->rawName())
                reloadedFirst = item;
            if (item->rawName() == second->rawName())
                reloadedSecond = item;
        }
        check(reloadedFirst && reloadedSecond
                  && reloadedFirst->groupId() > 0
                  && reloadedFirst->groupId() == reloadedSecond->groupId(),
              QStringLiteral("分组关系随画布保存和载入"));

        // 只复制组内一个图元时，副本不能误加入原组。
        groupScene.clearSelection();
        first->setSelected(true);
        const int duplicatedGroupMember = groupScene.duplicateSelectedItems();
        CanvasItem* groupCopy = nullptr;
        for (CanvasItem* item : groupScene.canvasItems()) {
            if (item != first && item != second && item->rawName() == QStringLiteral("矩形 2"))
                groupCopy = item;
        }
        check(duplicatedGroupMember == 1 && groupCopy && groupCopy->groupId() == 0,
              QStringLiteral("只复制单个组员时副本保持未分组"));

        // 只选中一个组员也能取消整个组。
        groupScene.clearSelection();
        first->setSelected(true);
        const int ungrouped = groupScene.ungroupSelectedItems();
        check(ungrouped == 2 && first->groupId() == 0 && second->groupId() == 0,
              QStringLiteral("取消组合会清除整个组"),
              QStringLiteral("ungrouped=%1").arg(ungrouped));
        groupUndo.undo();
        check(first->groupId() == groupId && second->groupId() == groupId,
              QStringLiteral("撤销取消组合恢复整组"));

        groupView.close();
        QApplication::processEvents();
    }

    // 框选按图元中心判断：只碰到边缘不选中，框住中心才选中。
    {
        CanvasScene rubberScene;
        rubberScene.applyCanvasSize(QSizeF(800.0, 600.0));
        rubberScene.setMode(ToolMode::Edit);
        auto* target = new ShapeItem(ShapeType::Rectangle);
        target->setContentSize(QSizeF(120.0, 80.0));
        target->setPos(QPointF(300.0, 250.0));
        rubberScene.addItemsWithUndo({ target }, QStringLiteral("Add rubber-band target"));

        CanvasView rubberView(&rubberScene);
        rubberView.resize(900, 700);
        rubberView.show();
        rubberView.setMode(ToolMode::Edit);
        rubberView.fitAll();
        QApplication::processEvents();

        const QPointF center = target->mapToScene(target->contentRect().center());
        const QPoint start = rubberView.mapFromScene(center + QPointF(-140.0, -100.0));
        const QPoint edgeEnd = rubberView.mapFromScene(center + QPointF(-15.0, -15.0));
        sendMouseEvent(rubberView.viewport(), QEvent::MouseButtonPress, start,
                       Qt::LeftButton, Qt::LeftButton);
        sendMouseEvent(rubberView.viewport(), QEvent::MouseMove, edgeEnd,
                       Qt::NoButton, Qt::LeftButton);
        sendMouseEvent(rubberView.viewport(), QEvent::MouseButtonRelease, edgeEnd,
                       Qt::LeftButton, Qt::NoButton);
        QApplication::processEvents();
        check(!target->isSelected(),
              QStringLiteral("框选只碰到图元边缘时不选中"));

        rubberScene.clearSelection();
        const QPoint centerEnd = rubberView.mapFromScene(center + QPointF(15.0, 15.0));
        sendMouseEvent(rubberView.viewport(), QEvent::MouseButtonPress, start,
                       Qt::LeftButton, Qt::LeftButton);
        sendMouseEvent(rubberView.viewport(), QEvent::MouseMove, centerEnd,
                       Qt::NoButton, Qt::LeftButton);
        sendMouseEvent(rubberView.viewport(), QEvent::MouseButtonRelease, centerEnd,
                       Qt::LeftButton, Qt::NoButton);
        QApplication::processEvents();
        check(!target->isSelected(),
              QStringLiteral("框住中心但图元仍有部分在框外时不选中"));

        rubberScene.clearSelection();
        const QPoint fullEnd = rubberView.mapFromScene(center + QPointF(140.0, 100.0));
        sendMouseEvent(rubberView.viewport(), QEvent::MouseButtonPress, start,
                       Qt::LeftButton, Qt::LeftButton);
        sendMouseEvent(rubberView.viewport(), QEvent::MouseMove, fullEnd,
                       Qt::NoButton, Qt::LeftButton);
        sendMouseEvent(rubberView.viewport(), QEvent::MouseButtonRelease, fullEnd,
                       Qt::LeftButton, Qt::NoButton);
        QApplication::processEvents();
        check(target->isSelected(),
              QStringLiteral("图元完整落在框内时选中"));

        rubberView.close();
        QApplication::processEvents();
    }

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
    check(viewScene.setLayerLocked(lockedLayerId, true),
          QStringLiteral("锁定文字所在图层"));
    viewScene.setActiveLayer(baseLayerId);
    view.fitAll();
    QApplication::processEvents();

    check(lockedLayerId != baseLayerId
              && !lockedText->isEditable()
              && (lockedText->flags() & QGraphicsItem::ItemIsSelectable) == 0,
          QStringLiteral("锁定图层上的文字不可编辑和选择"));
    viewScene.selectAllItems();
    check(!lockedText->isSelected(), QStringLiteral("全选不包含锁定图层图元"));
    viewScene.clearSelection();

    const int requestsBeforeLockedClick = itemPropertyRequests;
    const QPoint lockedPoint = view.mapFromScene(lockedText->sceneContentRect().center());
    QMouseEvent lockedDoubleClick(
        QEvent::MouseButtonDblClick, QPointF(lockedPoint),
        QPointF(view.viewport()->mapToGlobal(lockedPoint)),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &lockedDoubleClick);
    QApplication::processEvents();
    check(itemPropertyRequests == requestsBeforeLockedClick,
          QStringLiteral("双击锁定图层图元不会弹出属性窗"));

    sendMouseEvent(view.viewport(), QEvent::MouseButtonPress, lockedPoint,
                   Qt::LeftButton, Qt::LeftButton);
    sendMouseEvent(view.viewport(), QEvent::MouseButtonRelease, lockedPoint,
                   Qt::LeftButton, Qt::NoButton);
    QApplication::processEvents();
    check(!lockedText->isSelected(), QStringLiteral("锁定图层图元不能被鼠标选中"));

    int lockedLayerMessages = 0;
    QObject::connect(&view, &CanvasView::statusMessage, &view,
                     [&lockedLayerMessages](const QString& message) {
                         if (message.contains(QStringLiteral("锁定")))
                             ++lockedLayerMessages;
                     });
    const QPoint lockedGlobal = view.viewport()->mapToGlobal(lockedPoint);
    QContextMenuEvent lockedContext(QContextMenuEvent::Mouse, lockedPoint, lockedGlobal);
    QApplication::sendEvent(view.viewport(), &lockedContext);
    QApplication::processEvents();
    check(lockedLayerMessages > 0,
          QStringLiteral("右键锁定图层图元提示先解锁图层"));
    check(viewScene.setLayerLocked(lockedLayerId, false),
          QStringLiteral("解锁文字图层"));
    check(lockedText->isEditable(), QStringLiteral("解锁后文字恢复可编辑"));

    view.close();
    QApplication::processEvents();

    // End-to-end check of the actual MainWindow text flow. The modal text
    // dialog is filled and accepted by a timer while insertTextAt() is running.
    {
        MainWindow window;
        window.resize(1200, 800);
        window.show();
        QApplication::processEvents();

        auto* modeBar = window.findChild<QToolBar*>(QStringLiteral("ModeToolBar"));
        check(modeBar != nullptr, QStringLiteral("主窗口存在左侧模式工具栏"));
        if (modeBar) {
            QStringList modeOrder;
            for (QAction* action : modeBar->actions()) {
                if (action && !action->isSeparator())
                    modeOrder.append(action->text());
            }
            check(modeOrder == QStringList({ QStringLiteral("查看模式"), QStringLiteral("编辑模式"),
                                             QStringLiteral("画笔模式"), QStringLiteral("文字模式") }),
                  QStringLiteral("模式工具栏顺序为查看 → 编辑 → 画笔 → 文字"),
                  modeOrder.join(QStringLiteral(" / ")));

            // 需求：支持中英文界面，默认系统语言，可在设置里切换。
            check(uiText(QStringLiteral("New canvas")) == QStringLiteral("新建画布"),
                  QStringLiteral("中文界面下英文源串显示为中文"));
            uiOverrideLanguage(UiLanguage::English);
            check(uiText(QStringLiteral("New canvas")) == QStringLiteral("New canvas")
                      && uiText(QStringLiteral("Hexagon")) == QStringLiteral("Hexagon")
                      && uiText(QStringLiteral("View mode")) == QStringLiteral("View mode"),
                  QStringLiteral("英文界面下显示英文源串"),
                  uiText(QStringLiteral("New canvas")));
            uiRetranslateWidgetTree(&window);
            QStringList englishOrder;
            for (QAction* action : modeBar->actions()) {
                if (action && !action->isSeparator())
                    englishOrder.append(action->text());
            }
            check(englishOrder == QStringList({ QStringLiteral("View mode"), QStringLiteral("Edit mode"),
                                               QStringLiteral("Brush mode"), QStringLiteral("Text mode") }),
                  QStringLiteral("已存在的界面可以原地切换到英文"),
                  englishOrder.join(QStringLiteral(" / ")));
            uiOverrideLanguage(UiLanguage::Chinese);
            uiRetranslateWidgetTree(&window);
            check(modeBar->actions().size() > 1
                      && modeBar->actions().at(0)->text() == QStringLiteral("查看模式"),
                  QStringLiteral("界面可以切回中文"));

            // 切换语言后，走真实刷新路径（写设置 + applyUiLanguage）立即生效。
            uiOverrideLanguage(UiLanguage::System); // 取消测试用的强制语言，才能验证设置生效
            const UiLanguage originalSetting = uiLanguageSetting();
            setUiLanguageSetting(UiLanguage::English);
            check(uiIsEnglish(), QStringLiteral("设置中选中英文后立即生效（不依赖写入是否落盘）"),
                  QStringLiteral("setting=%1")
                      .arg(static_cast<int>(uiLanguageSetting())));
            window.applyUiLanguage();
            const QString firstModeText = modeBar->actions().isEmpty()
                ? QString()
                : modeBar->actions().at(0)->text();
            QMenu* shapeMenu = window.findChild<QMenu*>(QStringLiteral("ShapeMenu"));
            const QString firstShapeText = shapeMenu && !shapeMenu->actions().isEmpty()
                ? shapeMenu->actions().at(0)->text()
                : QString();
            check(firstModeText == QStringLiteral("View mode")
                      && firstShapeText == QStringLiteral("Rectangle"),
                  QStringLiteral("切换语言后主界面立即变为英文"),
                  QStringLiteral("%1 / %2").arg(firstModeText, firstShapeText));
            setUiLanguageSetting(originalSetting);
            uiOverrideLanguage(UiLanguage::Chinese);
            window.applyUiLanguage();
            check(modeBar->actions().at(0)->text() == QStringLiteral("查看模式"),
                  QStringLiteral("切回中文后界面恢复中文"));

            // 设置弹窗的语言下拉：选 English 后 language() 返回 English。
            {
                SettingsDialog dialog{AppSettings()};
                QComboBox* combo = dialog.findChild<QComboBox*>();
                if (combo) {
                    const int englishIndex = combo->findData(static_cast<int>(UiLanguage::English));
                    combo->setCurrentIndex(englishIndex);
                    check(dialog.language() == UiLanguage::English,
                          QStringLiteral("设置弹窗可以选中 English 并返回该语言"),
                          QStringLiteral("index=%1 language=%2")
                              .arg(englishIndex)
                              .arg(static_cast<int>(dialog.language())));
                }
            }

            // 设置必须真的落到纯文本配置里：写盘 → 重新读取 → 每一项都对得上。
            {
                const QString configPath = AppSettings::settingsFilePath();
                check(configPath.endsWith(QStringLiteral("StoryBoard.ini")),
                      QStringLiteral("配置文件是纯文本 StoryBoard.ini"),
                      configPath);

                AppSettings written;
                written.brushColor = QColor(255, 128, 0, 255);
                written.brushWidth = 37;
                written.strokeSmoothing = 11;
                written.canvasSize = QSizeF(2400.0, 1500.0);
                written.showGrid = false;
                written.canvasColor = QColor(18, 22, 30, 255);
                written.lineArtColorLevels = 9;
                written.lineArtPaletteMerge = 70;
                written.lineArtAdjacentMerge = 30;
                written.lineArtMinRegionArea = 40;
                check(written.save() && QFileInfo::exists(configPath),
                      QStringLiteral("设置参数写入 StoryBoard.ini"), configPath);

                AppSettings reloaded;
                reloaded.load();
                const bool same = reloaded.brushColor == written.brushColor
                    && reloaded.brushWidth == written.brushWidth
                    && reloaded.strokeSmoothing == written.strokeSmoothing
                    && reloaded.canvasSize == written.canvasSize
                    && reloaded.showGrid == written.showGrid
                    && reloaded.canvasColor == written.canvasColor
                    && reloaded.lineArtColorLevels == written.lineArtColorLevels
                    && reloaded.lineArtPaletteMerge == written.lineArtPaletteMerge
                    && reloaded.lineArtAdjacentMerge == written.lineArtAdjacentMerge
                    && reloaded.lineArtMinRegionArea == written.lineArtMinRegionArea;
                check(same,
                      QStringLiteral("重新读取后各设置参数与写入时一致"),
                      QStringLiteral("width=%1 smoothing=%2 grid=%3 levels=%4 minArea=%5")
                          .arg(reloaded.brushWidth)
                          .arg(reloaded.strokeSmoothing)
                          .arg(reloaded.showGrid)
                          .arg(reloaded.lineArtColorLevels)
                          .arg(reloaded.lineArtMinRegionArea));

                // 语言与其它设置共用一份文件，写语言不能把已保存的参数冲掉。
                check(AppSettings::setLanguageSetting(QStringLiteral("en"))
                          && AppSettings::languageSetting() == QStringLiteral("en"),
                      QStringLiteral("界面语言写入同一份配置文件"));
                AppSettings afterLanguage;
                afterLanguage.load();
                check(afterLanguage.brushWidth == 37
                          && afterLanguage.lineArtColorLevels == 9
                          && afterLanguage.lineArtMinRegionArea == 40,
                      QStringLiteral("写语言后其它设置仍然保留"),
                      QStringLiteral("width=%1 levels=%2")
                          .arg(afterLanguage.brushWidth)
                          .arg(afterLanguage.lineArtColorLevels));
                AppSettings::setLanguageSetting(QStringLiteral("system"));

                // 设置弹窗里改的线稿化参数必须经主窗口回写并落盘（曾经只改了弹窗内存副本）。
                {
                    MainWindow settingsWindow;
                    QAction* settingsAction = nullptr;
                    for (QAction* action : settingsWindow.findChildren<QAction*>()) {
                        if (action->text() == uiText(QStringLiteral("Settings…"))) {
                            settingsAction = action;
                            break;
                        }
                    }
                    check(settingsAction != nullptr, QStringLiteral("主窗口提供设置入口"));
                    bool dialogSeen = false;
                    bool dialogUpdated = false;
                    if (settingsAction) {
                        QTimer::singleShot(0, &settingsWindow, [&]() {
                            auto* dialog = settingsWindow.findChild<SettingsDialog*>();
                            if (!dialog)
                                return;
                            dialogSeen = true;
                            QSpinBox* levels = dialog->findChild<QSpinBox*>(
                                QStringLiteral("LineArtLevelsSpin"));
                            QSpinBox* minArea = dialog->findChild<QSpinBox*>(
                                QStringLiteral("LineArtMinAreaSpin"));
                            if (levels && minArea) {
                                levels->setValue(8);
                                minArea->setValue(56);
                            }
                            if (auto* box = dialog->findChild<QDialogButtonBox*>()) {
                                dialogUpdated = box->button(QDialogButtonBox::Ok) != nullptr;
                                if (dialogUpdated)
                                    box->button(QDialogButtonBox::Ok)->click();
                            }
                        });
                        settingsAction->trigger();
                    }
                    AppSettings fromDialog;
                    fromDialog.load();
                    check(dialogSeen && dialogUpdated
                              && fromDialog.lineArtColorLevels == 8
                              && fromDialog.lineArtMinRegionArea == 56,
                          QStringLiteral("设置弹窗里的线稿化参数回写并持久化"),
                          QStringLiteral("seen=%1 ok=%2 levels=%3 minArea=%4")
                              .arg(dialogSeen)
                              .arg(dialogUpdated)
                              .arg(fromDialog.lineArtColorLevels)
                              .arg(fromDialog.lineArtMinRegionArea));
                }

                // 关窗时保存窗口几何：重开程序应当恢复成关闭前的尺寸，而不是默认 1440x900。
                {
                    bool closed = false;
                    {
                        MainWindow closeWindow;
                        closeWindow.show();
                        QApplication::processEvents();
                        closeWindow.resize(1280, 820);
                        QApplication::processEvents();
                        closed = closeWindow.close();
                    }
                    AppSettings afterClose;
                    afterClose.load();
                    check(closed && !afterClose.windowGeometry.isEmpty(),
                          QStringLiteral("关闭窗口时把窗口几何写进配置文件"),
                          QStringLiteral("closed=%1 bytes=%2")
                              .arg(closed)
                              .arg(afterClose.windowGeometry.size()));
                }
            }
        }

        // 需求：原来的“导入图片”改名“插入图片”，并在“打开画布”后面加入“导入图片线稿化”。
        QMenu* fileMenu = nullptr;
        QMenu* canvasMenu = nullptr;
        for (QAction* action : window.menuBar()->actions()) {
            QMenu* menu = action ? action->menu() : nullptr;
            if (!menu)
                continue;
            if (menu->title() == QStringLiteral("文件(&F)"))
                fileMenu = menu;
            else if (menu->title() == QStringLiteral("画布(&C)"))
                canvasMenu = menu;
        }
        check(fileMenu != nullptr && canvasMenu != nullptr, QStringLiteral("主窗口存在文件与画布菜单"));
        if (fileMenu) {
            QStringList entries;
            for (QAction* action : fileMenu->actions()) {
                if (action && !action->isSeparator())
                    entries.append(action->text());
            }
            const int openIndex = entries.indexOf(QStringLiteral("打开画布…"));
            const int lineArtIndex = entries.indexOf(QStringLiteral("导入图片线稿化"));
            check(openIndex >= 0 && lineArtIndex == openIndex + 1,
                  QStringLiteral("导入图片线稿化紧跟在打开画布后面"),
                  entries.join(QStringLiteral(" / ")));
            check(entries.contains(QStringLiteral("插入图片…"))
                      && !entries.contains(QStringLiteral("导入图片…")),
                  QStringLiteral("原来的导入图片改名为插入图片"),
                  entries.join(QStringLiteral(" / ")));
        }
        if (canvasMenu) {
            QStringList entries;
            for (QAction* action : canvasMenu->actions()) {
                if (action && !action->isSeparator())
                    entries.append(action->text());
            }
            check(entries.contains(QStringLiteral("插入图片…"))
                      && entries.contains(QStringLiteral("导入图片线稿化")),
                  QStringLiteral("画布菜单同时提供插入图片与导入图片线稿化"),
                  entries.join(QStringLiteral(" / ")));
        }

        // 需求：画板菜单里也有“打开画布”，其后同样要有“导入图片线稿化”。
        QMenu* boardMenu = nullptr;
        for (QAction* action : window.menuBar()->actions()) {
            QMenu* menu = action ? action->menu() : nullptr;
            if (menu && menu->title() == QStringLiteral("画板(&B)"))
                boardMenu = menu;
        }
        check(boardMenu != nullptr, QStringLiteral("主窗口存在画板菜单"));
        if (boardMenu) {
            QStringList entries;
            for (QAction* action : boardMenu->actions()) {
                if (action && !action->isSeparator())
                    entries.append(action->text());
            }
            // 需求：画板菜单不再重复文件菜单里已有的画布操作。
            check(!entries.contains(QStringLiteral("打开画布…"))
                      && !entries.contains(QStringLiteral("保存画布"))
                      && !entries.contains(QStringLiteral("画布另存为…"))
                      && !entries.contains(QStringLiteral("导入图片线稿化")),
                  QStringLiteral("画板菜单不再重复文件菜单的保存与打开项"),
                  entries.join(QStringLiteral(" / ")));
            check(entries.contains(QStringLiteral("设置…"))
                      && entries.contains(QStringLiteral("打开画布文件夹")),
                  QStringLiteral("画板菜单保留设置与画布文件夹入口"),
                  entries.join(QStringLiteral(" / ")));
        }

        window.view()->setMode(ToolMode::Text);
        QTimer::singleShot(120, &window, [&]() {
            auto* dialog = qobject_cast<ItemPropertiesDialog*>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            QPlainTextEdit* textValue = dialog->findChild<QPlainTextEdit*>(
                QStringLiteral("ItemPropertiesTextValue"));
            if (!textValue)
                return;
            textValue->setPlainText(QStringLiteral("端到端文字图元"));
            dialog->accept();
        });
        const QPoint clickPosition = window.view()->mapFromScene(
            window.scene()->canvasRect().center());
        sendMouseEvent(window.view()->viewport(), QEvent::MouseButtonPress, clickPosition,
                       Qt::LeftButton, Qt::LeftButton);
        sendMouseEvent(window.view()->viewport(), QEvent::MouseButtonRelease, clickPosition,
                       Qt::LeftButton, Qt::NoButton);
        QApplication::processEvents();

        check(window.scene()->canvasItemCount() == 1,
              QStringLiteral("主窗口文字模式端到端创建图元"));
        TextItem* createdText = nullptr;
        if (window.scene()->canvasItemCount() == 1)
            createdText = dynamic_cast<TextItem*>(window.scene()->canvasItems().constFirst());
        check(createdText && createdText->text() == QStringLiteral("端到端文字图元"),
              QStringLiteral("主窗口创建的文字内容正确"));
        check(createdText && createdText->textColor() == window.view()->brushColor()
                  && createdText->layerId() == window.scene()->activeLayerId()
                  && window.scene()->layerHeightsUnique(createdText->layerId()),
              QStringLiteral("主窗口文字使用所选颜色且高度唯一"));
        check(window.view()->mode() == ToolMode::Edit,
              QStringLiteral("主窗口添加文字后切回编辑模式"));

        QAction* heightUpAction = nullptr;
        QAction* heightDownAction = nullptr;
        const QList<QAction*> windowActions = window.findChildren<QAction*>();
        for (QAction* action : windowActions) {
            if (action->text().startsWith(QStringLiteral("高度 +1")))
                heightUpAction = action;
            else if (action->text().startsWith(QStringLiteral("高度 -1")))
                heightDownAction = action;
        }
        check(heightUpAction
                  && heightUpAction->shortcut() == QKeySequence(Qt::Key_BracketRight)
                  && heightDownAction
                  && heightDownAction->shortcut() == QKeySequence(Qt::Key_BracketLeft),
              QStringLiteral("编辑菜单提供 ] / [ 高度快捷键"));

        QAction* propertiesAction = nullptr;
        for (QAction* action : windowActions) {
            if (action->text().startsWith(QStringLiteral("图元属性"))
                && action->shortcut() == QKeySequence(QStringLiteral("Q"))) {
                propertiesAction = action;
                break;
            }
        }
        if (createdText)
            createdText->setSelected(true);
        QApplication::processEvents();
        check(propertiesAction && propertiesAction->isEnabled(),
              QStringLiteral("编辑菜单提供 Q 图元属性快捷键且选中单个图元时可用"));
        if (createdText && heightUpAction && heightDownAction) {
            const int heightBeforeShortcut = createdText->height();
            heightUpAction->trigger();
            check(createdText->height() == heightBeforeShortcut + 1,
                  QStringLiteral("] 快捷键提升选中图元高度"));
            heightDownAction->trigger();
            check(createdText->height() == heightBeforeShortcut,
                  QStringLiteral("[ 快捷键降低选中图元高度"));
        }

        // 需求 42 端到端：查看模式双击图元，MainWindow 以只读模态窗打开。
        if (createdText) {
            window.view()->setMode(ToolMode::View);
            QApplication::processEvents();
            bool readOnlyDialogSeen = false;
            QTimer::singleShot(80, &window, [&]() {
                auto* dialog = qobject_cast<ItemPropertiesDialog*>(
                    QApplication::activeModalWidget());
                if (!dialog)
                    return;
                readOnlyDialogSeen = true;
                check(dialog->isModal() && !dialog->isEditable(),
                      QStringLiteral("查看模式端到端打开只读模态属性窗"));
                QLineEdit* nameEdit = dialog->findChild<QLineEdit*>(
                    QStringLiteral("ItemPropertiesNameValue"));
                check(nameEdit && !nameEdit->isEnabled(),
                      QStringLiteral("查看模式端到端属性字段不可编辑"));
                dialog->accept();
            });
            const QPoint textPoint = window.view()->mapFromScene(
                createdText->sceneContentRect().center());
            QMouseEvent textDoubleClick(
                QEvent::MouseButtonDblClick, QPointF(textPoint),
                QPointF(window.view()->viewport()->mapToGlobal(textPoint)),
                Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(window.view()->viewport(), &textDoubleClick);
            QApplication::processEvents();
            check(readOnlyDialogSeen,
                  QStringLiteral("查看模式双击图元确实打开属性窗"));
            window.view()->setMode(ToolMode::Edit);
        }
    }

    const QString shapeCanvasPath = QDir(workDirectory)
                                          .filePath(QStringLiteral("shapes.sbcanvas"));
    {
        QFile shapeCanvasFile(shapeCanvasPath);
        check(shapeCanvasFile.open(QIODevice::WriteOnly),
              QStringLiteral("创建图形画布测试文件"));
        shapeCanvasFile.write(QJsonDocument(shapeRoot).toJson(QJsonDocument::Indented));
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

        QToolButton* fillButton = shapeWindow.findChild<QToolButton*>(QStringLiteral("FillButton"));
        QStringList fillActionTexts;
        if (fillButton && fillButton->menu()) {
            for (QAction* action : fillButton->menu()->actions()) {
                if (!action->isSeparator())
                    fillActionTexts.append(action->text());
            }
        }
        check(fillActionTexts.size() == 7
                  && fillActionTexts.contains(QStringLiteral("反斜线填充"))
                  && fillActionTexts.contains(QStringLiteral("正交叉线填充"))
                  && fillActionTexts.contains(QStringLiteral("六边形填充")),
              QStringLiteral("填充菜单只保留填充样式"),
              fillActionTexts.join(QStringLiteral("|")));

        QToolBar* mainToolBar = shapeWindow.findChild<QToolBar*>(
            QStringLiteral("MainToolBar"));
        auto toolbarWidgetNames = [](QToolBar* bar) {
            QStringList names;
            if (!bar)
                return names;
            for (QAction* action : bar->actions()) {
                if (action->isSeparator())
                    continue;
                if (QWidget* widget = bar->widgetForAction(action))
                    names.append(widget->objectName());
            }
            return names;
        };
        const QStringList mainNames = toolbarWidgetNames(mainToolBar);
        // 画笔样式与填充样式已并入主工具栏，靠标准分隔符分组，不再各占一个工具栏。
        const int shapeIndex = mainNames.indexOf(QStringLiteral("ShapeButton"));
        const int fillIndex = mainNames.indexOf(QStringLiteral("FillButton"));
        const int fillColorIndex = mainNames.indexOf(QStringLiteral("FillColorButton"));
        const int strokeColorIndex = mainNames.indexOf(QStringLiteral("StrokeColorButton"));
        const int brushColorIndex = mainNames.indexOf(QStringLiteral("ColorButton"));
        const int brushWidthIndex = mainNames.indexOf(QStringLiteral("BrushWidthCombo"));
        const int smoothIndex = mainNames.indexOf(QStringLiteral("SmoothStrokeButton"));
        const int smoothingIndex = mainNames.indexOf(QStringLiteral("BrushSmoothingSpin"));
        check(shapeIndex >= 0 && shapeIndex < fillIndex && fillIndex < fillColorIndex
                  && fillColorIndex < strokeColorIndex && strokeColorIndex < brushColorIndex
                  && brushColorIndex < brushWidthIndex && brushWidthIndex < smoothIndex
                  && smoothIndex < smoothingIndex,
              QStringLiteral("主工具栏按图形、填充、画笔三组顺序排列"),
              mainNames.join(QStringLiteral("|")));

        // 需求：工具栏在“保存画布”按钮前放线稿化入口。
        QStringList mainActionTexts;
        if (mainToolBar) {
            for (QAction* action : mainToolBar->actions()) {
                if (action && !action->isSeparator())
                    mainActionTexts.append(action->text());
            }
        }
        const int toolbarSaveIndex = mainActionTexts.indexOf(QStringLiteral("保存画布"));
        const int toolbarLineArtIndex = mainActionTexts.indexOf(QStringLiteral("导入图片线稿化"));
        check(toolbarSaveIndex >= 0 && toolbarLineArtIndex == toolbarSaveIndex - 1,
              QStringLiteral("工具栏导入图片线稿化紧挨在保存画布之前"),
              mainActionTexts.join(QStringLiteral(" / ")));
        QSpinBox* toolbarSmoothing = shapeWindow.findChild<QSpinBox*>(
            QStringLiteral("BrushSmoothingSpin"));
        CanvasView* canvasView = shapeWindow.findChild<CanvasView*>();
        check(toolbarSmoothing && canvasView
                  && toolbarSmoothing->minimum() == 0 && toolbarSmoothing->maximum() == 30
                  && toolbarSmoothing->value() == canvasView->strokeSmoothing(),
              QStringLiteral("画笔工具栏提供可调容差"),
              QStringLiteral("value=%1 view=%2")
                  .arg(toolbarSmoothing ? toolbarSmoothing->value() : -1)
                  .arg(canvasView ? canvasView->strokeSmoothing() : -1));
        if (toolbarSmoothing) {
            toolbarSmoothing->setValue(0);
            check(canvasView && canvasView->strokeSmoothing() == 0,
                  QStringLiteral("工具栏容差即时生效"),
                  QStringLiteral("view=%1").arg(canvasView ? canvasView->strokeSmoothing() : -1));
            toolbarSmoothing->setValue(4);
        }
        QToolButton* smoothButton = shapeWindow.findChild<QToolButton*>(
            QStringLiteral("SmoothStrokeButton"));
        check(smoothButton && smoothButton->defaultAction()
                  && smoothButton->defaultAction()->text() == QStringLiteral("平滑笔画"),
              QStringLiteral("画笔工具栏提供手动平滑按钮"));
        QAction* smoothAction = nullptr;
        for (QAction* action : shapeWindow.findChildren<QAction*>()) {
            if (action->text() == QStringLiteral("平滑笔画"))
                smoothAction = action;
        }
        check(smoothAction
                  && smoothAction->shortcut() == QKeySequence(QStringLiteral("Ctrl+Shift+M")),
              QStringLiteral("编辑菜单提供手动平滑入口与快捷键"));
        int mainSeparatorCount = 0;
        if (mainToolBar) {
            for (QAction* action : mainToolBar->actions()) {
                if (action->isSeparator())
                    ++mainSeparatorCount;
            }
        }
        check(mainSeparatorCount == 5,
              QStringLiteral("主工具栏用 5 个分隔符切成 6 组"),
              QStringLiteral("separatorCount=%1").arg(mainSeparatorCount));
        QString separatorAfterFillGroup;
        if (mainToolBar) {
            const QList<QAction*> actions = mainToolBar->actions();
            for (int index = 1; index + 1 < actions.size(); ++index) {
                if (!actions.at(index)->isSeparator())
                    continue;
                const QWidget* before = mainToolBar->widgetForAction(actions.at(index - 1));
                if (before && before->objectName() == QStringLiteral("StrokeColorButton"))
                    separatorAfterFillGroup = QStringLiteral("ok");
            }
        }
        check(separatorAfterFillGroup == QStringLiteral("ok")
                  && !shapeWindow.findChild<QWidget*>(QStringLiteral("ToolBarDivider")),
              QStringLiteral("填充组与画笔组之间使用标准分隔符"),
              separatorAfterFillGroup);

        // 工具栏尺寸：所有按钮共用一套尺寸，和下拉框/数字框对齐同一基线。
        check(mainToolBar
                  && mainToolBar->iconSize() == QSize(MainWindow::kToolBarIconSize, MainWindow::kToolBarIconSize),
              QStringLiteral("主工具栏统一使用 %1px 图标").arg(MainWindow::kToolBarIconSize),
              QStringLiteral("iconSize=%1x%2")
                  .arg(mainToolBar ? mainToolBar->iconSize().width() : -1)
                  .arg(mainToolBar ? mainToolBar->iconSize().height() : -1));
        const QStringList iconButtonNames = {
            QStringLiteral("ShapeButton"),
            QStringLiteral("FillButton"),
            QStringLiteral("FillColorButton"),
            QStringLiteral("StrokeColorButton"),
            QStringLiteral("ColorButton"),
            QStringLiteral("SmoothStrokeButton")};
        QString iconButtonReport;
        bool iconButtonsUniform = true;
        for (const QString& name : iconButtonNames) {
            QToolButton* button = shapeWindow.findChild<QToolButton*>(name);
            if (!button) {
                iconButtonsUniform = false;
                iconButtonReport += name + QStringLiteral("=missing;");
                continue;
            }
            const int expectedWidth = name == QStringLiteral("ColorButton") ? MainWindow::kToolBarSplitButtonWidth
                                                                           : MainWindow::kToolBarButtonSize;
            const QSize expected(expectedWidth, MainWindow::kToolBarButtonSize);
            const QSize expectedIcon(MainWindow::kToolBarIconSize, MainWindow::kToolBarIconSize);
            iconButtonReport += QStringLiteral("%1=%2x%3/%4x%5/%6;")
                                    .arg(name)
                                    .arg(button->width())
                                    .arg(button->height())
                                    .arg(button->iconSize().width())
                                    .arg(button->iconSize().height())
                                    .arg(button->toolButtonStyle() == Qt::ToolButtonIconOnly
                                             ? QStringLiteral("icon")
                                             : QStringLiteral("withtext"));
            // 平滑按钮也只要图标：统一尺寸之外再校验它没有退化成图标+文字。
            if (button->size() != expected || button->iconSize() != expectedIcon
                || button->toolButtonStyle() != Qt::ToolButtonIconOnly)
                iconButtonsUniform = false;
        }
        check(iconButtonsUniform,
              QStringLiteral("工具栏图标按钮统一为 %1×%1（图标 %2px，分裂按钮宽 %3）")
                  .arg(MainWindow::kToolBarButtonSize)
                  .arg(MainWindow::kToolBarIconSize)
                  .arg(MainWindow::kToolBarSplitButtonWidth),
              iconButtonReport);
        QComboBox* toolbarWidthCombo = shapeWindow.findChild<QComboBox*>(
            QStringLiteral("BrushWidthCombo"));
        check(toolbarWidthCombo && toolbarWidthCombo->height() == MainWindow::kToolBarControlHeight
                  && toolbarSmoothing && toolbarSmoothing->height() == MainWindow::kToolBarControlHeight
                  && smoothButton && smoothButton->height() == MainWindow::kToolBarButtonSize,
              QStringLiteral("工具栏下拉框、数字框与按钮同高"),
              QStringLiteral("combo=%1 spin=%2 smooth=%3")
                  .arg(toolbarWidthCombo ? toolbarWidthCombo->height() : -1)
                  .arg(toolbarSmoothing ? toolbarSmoothing->height() : -1)
                  .arg(smoothButton ? smoothButton->height() : -1));
        QToolButton* iconFillButton = shapeWindow.findChild<QToolButton*>(
            QStringLiteral("FillButton"));
        QToolButton* iconFillColorButton = shapeWindow.findChild<QToolButton*>(
            QStringLiteral("FillColorButton"));
        QToolButton* iconStrokeColorButton = shapeWindow.findChild<QToolButton*>(
            QStringLiteral("StrokeColorButton"));
        const QImage fillStyleImage = iconFillButton ? iconFillButton->icon().pixmap(20, 20).toImage()
                                                     : QImage();
        const QImage fillColorImage = iconFillColorButton ? iconFillColorButton->icon().pixmap(20, 20).toImage()
                                                          : QImage();
        const QImage strokeColorImage = iconStrokeColorButton ? iconStrokeColorButton->icon().pixmap(20, 20).toImage()
                                                               : QImage();
        check(!fillStyleImage.isNull() && !fillColorImage.isNull() && !strokeColorImage.isNull()
                  && fillStyleImage != fillColorImage && fillColorImage != strokeColorImage,
              QStringLiteral("填充样式、填充颜色与描边颜色使用彼此不同的图标"));

        QMenu* brushWidthMenu = shapeWindow.findChild<QMenu*>(
            QStringLiteral("BrushWidthMenu"));
        QComboBox* widthCombo = shapeWindow.findChild<QComboBox*>(
            QStringLiteral("BrushWidthCombo"));
        check(brushWidthMenu && brushWidthMenu->actions().size() == 15,
              QStringLiteral("画笔样式菜单提供粗细选项"),
              QStringLiteral("actions=%1").arg(brushWidthMenu ? brushWidthMenu->actions().size() : 0));
        if (brushWidthMenu && widthCombo && !brushWidthMenu->actions().isEmpty()) {
            const int oldWidth = widthCombo->currentData().toInt();
            QAction* twoPixel = nullptr;
            for (QAction* action : brushWidthMenu->actions()) {
                if (action->data().toInt() == 2) {
                    twoPixel = action;
                    break;
                }
            }
            check(twoPixel && twoPixel->isCheckable(),
                  QStringLiteral("画笔粗细菜单使用互斥可勾选选项"),
                  QStringLiteral("current=%1").arg(oldWidth));
            if (twoPixel) {
                twoPixel->trigger();
                QApplication::processEvents();
                check(widthCombo->currentData().toInt() == 2 && twoPixel->isChecked(),
                      QStringLiteral("画笔粗细菜单选项同步工具栏下拉"));
                QAction* restore = nullptr;
                for (QAction* action : brushWidthMenu->actions()) {
                    if (action->data().toInt() == oldWidth) {
                        restore = action;
                        break;
                    }
                }
                if (restore)
                    restore->trigger();
                QApplication::processEvents();
            }
        }

        QMenu* styleMenu = shapeWindow.findChild<QMenu*>(QStringLiteral("StyleMenu"));
        QMenu* brushStyleMenu = shapeWindow.findChild<QMenu*>(
            QStringLiteral("BrushStyleMenu"));
        QMenu* fillStyleMenu = shapeWindow.findChild<QMenu*>(
            QStringLiteral("FillStyleMenu"));
        check(styleMenu && styleMenu->actions().size() == 4,
              QStringLiteral("样式菜单包含画笔样式、填充样式和两个独立颜色项"),
              QStringLiteral("actions=%1").arg(styleMenu ? styleMenu->actions().size() : 0));
        QList<QAction*> styleActions;
        if (styleMenu)
            styleActions = styleMenu->actions();
        QList<QAction*> brushActions;
        if (brushStyleMenu)
            brushActions = brushStyleMenu->actions();
        QList<QAction*> fillActions;
        if (fillStyleMenu)
            fillActions = fillStyleMenu->actions();
        bool brushHasColor = false;
        bool brushHasOutline = false;
        bool brushHasWidth = brushStyleMenu
            && brushStyleMenu->findChild<QMenu*>(QStringLiteral("BrushWidthMenu"));
        for (QAction* action : brushActions) {
            if (action->objectName() == QStringLiteral("BrushColorAction"))
                brushHasColor = true;
            else if (action->objectName() == QStringLiteral("OutlineColorAction"))
                brushHasOutline = true;
        }
        bool fillHasColor = false;
        int fillMenuItemCount = 0;
        for (QAction* action : fillActions) {
            if (action->isSeparator())
                continue;
            ++fillMenuItemCount;
            if (action->objectName() == QStringLiteral("FillColorAction"))
                fillHasColor = true;
        }
        check(brushStyleMenu && brushHasColor && brushHasWidth && !brushHasOutline,
              QStringLiteral("画笔样式子菜单包含画笔颜色和粗细选项"));
        check(fillStyleMenu && fillMenuItemCount == 7 && !fillHasColor,
              QStringLiteral("填充样式子菜单只保留填充样式"),
              QStringLiteral("actions=%1").arg(fillMenuItemCount));

        int fillStyleMenuIndex = styleActions.indexOf(fillStyleMenu ? fillStyleMenu->menuAction() : nullptr);
        int fillColorActionIndex = -1;
        int outlineActionIndex = -1;
        for (int i = 0; i < styleActions.size(); ++i) {
            if (styleActions.at(i)->objectName() == QStringLiteral("FillColorAction"))
                fillColorActionIndex = i;
            else if (styleActions.at(i)->objectName() == QStringLiteral("OutlineColorAction"))
                outlineActionIndex = i;
        }
        check(fillStyleMenuIndex >= 0 && fillColorActionIndex > fillStyleMenuIndex
                  && outlineActionIndex > fillColorActionIndex,
              QStringLiteral("填充颜色和描边颜色分离到填充样式之后"),
              QStringLiteral("fillStyle=%1 fillColor=%2 outline=%3")
                  .arg(fillStyleMenuIndex).arg(fillColorActionIndex).arg(outlineActionIndex));

        QAction* outlineAction = shapeWindow.findChild<QAction*>(
            QStringLiteral("OutlineColorAction"));
        check(outlineAction && outlineAction->text() == QStringLiteral("描边颜色…"),
              QStringLiteral("主窗口提供描边颜色动作"));

        QAction* ellipseAction = shapeMenu
            ? shapeMenu->findChild<QAction*>(QStringLiteral("ShapeAction_ellipse"))
            : nullptr;
        check(ellipseAction != nullptr, QStringLiteral("图形菜单包含圆形插入动作"));
        if (ellipseAction) {
            ellipseAction->trigger();
            QApplication::processEvents();

            check(shapeWindow.scene()->canvasItemCount() == 1,
                  QStringLiteral("快速插入图形到当前画板"));
            ShapeItem* inserted = shapeWindow.scene()->canvasItemCount() == 1
                ? dynamic_cast<ShapeItem*>(shapeWindow.scene()->canvasItems().constFirst())
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
                      && shapeWindow.scene()->canvasRect().contains(inserted->sceneContentRect()),
                  QStringLiteral("快速插入图形位于画板范围内"),
                  inserted ? geometryText(inserted->pos(), inserted->contentSize()) : QString());
            check(inserted && inserted->fillColor().alpha() > 0
                      && inserted->strokeWidth() > 0.0,
                  QStringLiteral("快速插入图形带有默认描边与填充"));
        }

        QAction* arrowInsertAction = shapeMenu
            ? shapeMenu->findChild<QAction*>(QStringLiteral("ShapeAction_arrow"))
            : nullptr;
        check(arrowInsertAction != nullptr, QStringLiteral("图形菜单包含箭头插入动作"));
        if (arrowInsertAction) {
            arrowInsertAction->trigger();
            QApplication::processEvents();
            ShapeItem* insertedArrow = nullptr;
            for (CanvasItem* item : shapeWindow.scene()->canvasItems()) {
                if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
                    if (shape->shapeType() == ShapeType::Arrow)
                        insertedArrow = shape;
                }
            }
            check(insertedArrow != nullptr,
                  QStringLiteral("快速插入的图元类型为箭头"));
            check(insertedArrow && insertedArrow->fillStyle() == FillStyle::None,
                  QStringLiteral("新插入箭头默认空心"));
        }
    }

    {
        MainWindow shapeCanvasWindow;
        check(shapeCanvasWindow.openCanvasFile(shapeCanvasPath),
              QStringLiteral("主窗口打开含图形的画布"));
        check(shapeCanvasWindow.scene()->canvasItemCount() == shapeTypes.size(),
              QStringLiteral("主窗口载入含图形画布完整内容"));
        int loadedShapeCount = 0;
        for (CanvasItem* item : shapeCanvasWindow.scene()->canvasItems()) {
            if (dynamic_cast<ShapeItem*>(item))
                ++loadedShapeCount;
        }
        check(loadedShapeCount == shapeTypes.size(),
              QStringLiteral("主窗口识别画布中的图形图元"));
    }

    if (QFileInfo::exists(demoPath)) {
        MainWindow canvasWindow;
        check(canvasWindow.openCanvasFile(demoPath),
              QStringLiteral("主窗口打开含文字的示例画布"));
        check(canvasWindow.scene()->canvasItemCount() == 27
                  && canvasWindow.scene()->userLayerCount() == 3,
              QStringLiteral("主窗口载入示例画布完整内容"));
        bool hasTextItem = false;
        for (CanvasItem* item : canvasWindow.scene()->canvasItems()) {
            if (dynamic_cast<TextItem*>(item)) {
                hasTextItem = true;
                break;
            }
        }
        check(hasTextItem, QStringLiteral("主窗口载入画布中的文字图元"));
    }

    // --- 回归：控制点命中、粗描边命中、背景去重、设置颜色持久化 ---
    {
        CanvasScene handleScene;
        handleScene.setMode(ToolMode::Edit);
        handleScene.setShowGrid(false);
        QUndoStack handleUndo;
        handleScene.setUndoStack(&handleUndo);

        auto* handleEllipse = new ShapeItem(ShapeType::Ellipse);
        handleEllipse->setStyle(QColor(QStringLiteral("#0F172A")), QColor(Qt::transparent), 4.0);
        handleEllipse->setContentSize(QSizeF(200, 120));
        handleEllipse->setPos(QPointF(60, 40));
        handleScene.attachItem(handleEllipse);
        handleEllipse->setSelected(true);

        check(handleEllipse->shape().contains(QPointF(-3.0, -3.0)),
              QStringLiteral("选中椭圆后左上角控制点可命中"));
        check(handleEllipse->shape().contains(QPointF(203.0, 123.0)),
              QStringLiteral("选中椭圆后右下角控制点可命中"));
        check(!handleEllipse->shape().contains(QPointF(-40.0, -40.0)),
              QStringLiteral("控制点命中区域不会无限外扩"));

        handleEllipse->setSelected(false);
        check(!handleEllipse->shape().contains(QPointF(-3.0, -3.0)),
              QStringLiteral("未选中时控制点不参与命中"));

        auto* thickRect = new ShapeItem(ShapeType::Rectangle);
        thickRect->setStyle(QColor(QStringLiteral("#1D4ED8")), QColor(29, 78, 216, 40), 40.0);
        thickRect->setContentSize(QSizeF(200, 120));
        thickRect->setPos(QPointF(400, 40));
        handleScene.attachItem(thickRect);

        check(thickRect->shape().contains(QPointF(-10.0, 60.0)),
              QStringLiteral("粗描边图形按描边宽度纳入命中"));
        check(!thickRect->shape().contains(QPointF(-40.0, 60.0)),
              QStringLiteral("粗描边命中范围不超过描边宽度"));
    }

    {
        QPixmap firstBackground(40, 30);
        firstBackground.fill(QColor(QStringLiteral("#0EA5E9")));
        QPixmap secondBackground(40, 30);
        secondBackground.fill(QColor(QStringLiteral("#0EA5E9")));

        BackgroundState firstState;
        firstState.type = BackgroundType::Image;
        firstState.image = firstBackground;
        BackgroundState secondState = firstState;
        secondState.image = secondBackground;

        check(firstState == secondState, QStringLiteral("内容相同的背景图片判定为相等"));

        CanvasScene dedupeScene;
        QUndoStack dedupeUndo;
        dedupeScene.setUndoStack(&dedupeUndo);
        dedupeScene.replaceBackground(firstState);
        const int stepsAfterFirst = dedupeUndo.count();
        dedupeScene.replaceBackground(secondState);
        check(dedupeUndo.count() == stepsAfterFirst,
              QStringLiteral("重复应用同一背景图片不增加撤销步骤"),
              QString::number(dedupeUndo.count()));
    }

    {
        const AppSettings defaults;
        SettingsDialog dialog(defaults);
        dialog.setBrushColor(QColor(QStringLiteral("#FF8800")));
        dialog.setCanvasColor(QColor(QStringLiteral("#102A43")));
        auto* smoothingSpin = dialog.findChild<QSpinBox*>(QStringLiteral("StrokeSmoothingSpin"));
        check(smoothingSpin && smoothingSpin->value() == defaults.strokeSmoothing,
              QStringLiteral("设置对话框默认启用画笔自动平滑"),
              QString::number(smoothingSpin ? smoothingSpin->value() : -1));
        if (smoothingSpin)
            smoothingSpin->setValue(0);
        // 走真实的“确定”按钮，验证 accepted 处理器把平滑参数写回设置。
        if (auto* buttons = dialog.findChild<QDialogButtonBox*>()) {
            if (QPushButton* ok = buttons->button(QDialogButtonBox::Ok))
                ok->click();
            else
                dialog.accept();
        } else {
            dialog.accept();
        }
        check(dialog.settings().brushColor == QColor(QStringLiteral("#FF8800")),
              QStringLiteral("设置对话框保留画笔颜色"));
        check(dialog.settings().canvasColor == QColor(QStringLiteral("#102A43")),
              QStringLiteral("设置对话框保留画布外围颜色"));
        check(dialog.settings().strokeSmoothing == 0,
              QStringLiteral("设置对话框可关闭自动平滑"),
              QString::number(dialog.settings().strokeSmoothing));
    }

    {
        // 需求：把“相邻且颜色接近的色块并成一块”做成可调参数，并放在设置里。
        const AppSettings defaults;
        check(defaults.lineArtAdjacentMerge
                  == qRound(LineArtConverter::Options().adjacentMergeDistance)
                  && defaults.lineArtPaletteMerge
                      == qRound(LineArtConverter::Options().paletteMergeDistance),
              QStringLiteral("线稿化设置的默认值与转换器默认参数一致"),
              QStringLiteral("adjacent=%1 palette=%2")
                  .arg(defaults.lineArtAdjacentMerge)
                  .arg(defaults.lineArtPaletteMerge));

        SettingsDialog dialog(defaults);
        auto* adjacentSpin = dialog.findChild<QSpinBox*>(
            QStringLiteral("LineArtAdjacentMergeSpin"));
        auto* paletteSpin = dialog.findChild<QSpinBox*>(
            QStringLiteral("LineArtPaletteMergeSpin"));
        auto* areaSpin = dialog.findChild<QSpinBox*>(
            QStringLiteral("LineArtMinAreaSpin"));
        check(adjacentSpin && adjacentSpin->value() == defaults.lineArtAdjacentMerge,
              QStringLiteral("设置对话框显示相邻色块合并距离"),
              QString::number(adjacentSpin ? adjacentSpin->value() : -1));
        check(paletteSpin && paletteSpin->value() == defaults.lineArtPaletteMerge,
              QStringLiteral("设置对话框显示颜色聚类距离"),
              QString::number(paletteSpin ? paletteSpin->value() : -1));
        if (adjacentSpin)
            adjacentSpin->setValue(96);
        if (paletteSpin)
            paletteSpin->setValue(24);
        if (areaSpin)
            areaSpin->setValue(8);
        if (auto* buttons = dialog.findChild<QDialogButtonBox*>()) {
            if (QPushButton* ok = buttons->button(QDialogButtonBox::Ok))
                ok->click();
            else
                dialog.accept();
        } else {
            dialog.accept();
        }
        check(dialog.settings().lineArtAdjacentMerge == 96,
              QStringLiteral("设置对话框可调整相邻色块合并距离"),
              QString::number(dialog.settings().lineArtAdjacentMerge));
        check(dialog.settings().lineArtPaletteMerge == 24
                  && dialog.settings().lineArtMinRegionArea == 8,
              QStringLiteral("设置对话框可调整颜色聚类距离与最小色块"),
              QStringLiteral("palette=%1 area=%2")
                  .arg(dialog.settings().lineArtPaletteMerge)
                  .arg(dialog.settings().lineArtMinRegionArea));
    }

    {
        // 需求：选完图片先弹线稿化参数，点确定后才生成笔画。
        const AppSettings defaults;
        LineArtDialog dialog(defaults);
        auto* presetCombo = dialog.findChild<QComboBox*>(QStringLiteral("LineArtPresetCombo"));
        auto* levelsSpin = dialog.findChild<QSpinBox*>(QStringLiteral("LineArtLevelsSpin"));
        auto* paletteSpin = dialog.findChild<QSpinBox*>(QStringLiteral("LineArtPaletteMergeSpin"));
        auto* adjacentSpin = dialog.findChild<QSpinBox*>(QStringLiteral("LineArtAdjacentMergeSpin"));
        auto* areaSpin = dialog.findChild<QSpinBox*>(QStringLiteral("LineArtMinAreaSpin"));
        check(presetCombo && levelsSpin && paletteSpin && adjacentSpin && areaSpin,
              QStringLiteral("线稿化参数对话框提供档位与四项参数"));

        const LineArtConverter::Options initial = dialog.options();
        check(initial.colorLevels == defaults.lineArtColorLevels
                  && qRound(initial.paletteMergeDistance) == defaults.lineArtPaletteMerge
                  && qRound(initial.adjacentMergeDistance) == defaults.lineArtAdjacentMerge
                  && initial.minRegionArea == defaults.lineArtMinRegionArea
                  && qFuzzyCompare(initial.strokeWidth, 1.0),
              QStringLiteral("参数对话框初始值来自设置且线宽固定为 1"),
              QStringLiteral("levels=%1 palette=%2 adjacent=%3 area=%4 width=%5")
                  .arg(initial.colorLevels)
                  .arg(initial.paletteMergeDistance)
                  .arg(initial.adjacentMergeDistance)
                  .arg(initial.minRegionArea)
                  .arg(initial.strokeWidth));

        if (presetCombo && levelsSpin && paletteSpin && adjacentSpin && areaSpin) {
            presetCombo->setCurrentIndex(presetCombo->findData(2)); // 线条干净
            const LineArtConverter::Options clean = dialog.options();
            check(clean.paletteMergeDistance == 64.0 && clean.adjacentMergeDistance == 56.0
                      && clean.minRegionArea == 20,
                  QStringLiteral("参数对话框切档位后输出参数跟着变"),
                  QStringLiteral("palette=%1 adjacent=%2 area=%3")
                      .arg(clean.paletteMergeDistance)
                      .arg(clean.adjacentMergeDistance)
                      .arg(clean.minRegionArea));

            areaSpin->setValue(77);
            check(presetCombo->currentText() == QStringLiteral("自定义"),
                  QStringLiteral("参数对话框手改数值后档位回到自定义"),
                  presetCombo->currentText());
            check(dialog.options().minRegionArea == 77
                      && dialog.options().paletteMergeDistance == 64.0,
                  QStringLiteral("参数对话框手改数值即时反映到输出参数"),
                  QStringLiteral("area=%1 palette=%2")
                      .arg(dialog.options().minRegionArea)
                      .arg(dialog.options().paletteMergeDistance));

            AppSettings written = defaults;
            dialog.applyToSettings(written);
            check(written.lineArtMinRegionArea == 77 && written.lineArtPaletteMerge == 64,
                  QStringLiteral("参数对话框确定后把参数写回设置"),
                  QStringLiteral("palette=%1 area=%2")
                      .arg(written.lineArtPaletteMerge)
                      .arg(written.lineArtMinRegionArea));
        }
    }
    {
        // 需求：色块过多时自适应降采样，默认勾选、可取消。
        const AppSettings defaults;
        LineArtDialog dialog(defaults);
        auto* adaptiveCheck = dialog.findChild<QCheckBox*>(
            QStringLiteral("LineArtAdaptiveCheck"));
        auto* limitSpin = dialog.findChild<QSpinBox*>(
            QStringLiteral("LineArtAdaptiveLimitSpin"));
        check(adaptiveCheck && adaptiveCheck->isChecked() && limitSpin
                  && limitSpin->value() == defaults.lineArtAdaptiveLimit && limitSpin->isEnabled(),
              QStringLiteral("参数对话框默认勾选自适应降采样"),
              QStringLiteral("checked=%1 limit=%2 enabled=%3")
                  .arg(adaptiveCheck && adaptiveCheck->isChecked() ? 1 : 0)
                  .arg(limitSpin ? limitSpin->value() : -1)
                  .arg(limitSpin && limitSpin->isEnabled() ? 1 : 0));

        const LineArtConverter::Options initial = dialog.options();
        check(initial.adaptiveDownscale
                  && initial.adaptiveRegionLimit == defaults.lineArtAdaptiveLimit,
              QStringLiteral("自适应降采样设置会交给转换器"),
              QStringLiteral("adaptive=%1 limit=%2")
                  .arg(initial.adaptiveDownscale ? 1 : 0)
                  .arg(initial.adaptiveRegionLimit));

        if (adaptiveCheck && limitSpin) {
            adaptiveCheck->setChecked(false);
            check(!dialog.options().adaptiveDownscale && !limitSpin->isEnabled(),
                  QStringLiteral("取消勾选后自适应降采样关闭且阈值不可编辑"),
                  QStringLiteral("adaptive=%1 enabled=%2")
                      .arg(dialog.options().adaptiveDownscale ? 1 : 0)
                      .arg(limitSpin->isEnabled() ? 1 : 0));

            AppSettings written = defaults;
            dialog.applyToSettings(written);
            check(!written.lineArtAdaptiveDownscale,
                  QStringLiteral("取消勾选会写回设置"),
                  QString::number(written.lineArtAdaptiveDownscale ? 1 : 0));
        }
    }
    {
        // 需求：简化容差与最细条带也暴露到界面，能在参数对话框和设置里调。
        const AppSettings defaults;
        LineArtDialog dialog(defaults);
        auto* toleranceSpin = dialog.findChild<QDoubleSpinBox*>(
            QStringLiteral("LineArtToleranceSpin"));
        auto* thicknessSpin = dialog.findChild<QDoubleSpinBox*>(
            QStringLiteral("LineArtThicknessSpin"));
        check(toleranceSpin && thicknessSpin
                  && qAbs(toleranceSpin->value() - defaults.lineArtSimplifyTolerance) < 1e-6
                  && qAbs(thicknessSpin->value() - defaults.lineArtMinThickness) < 1e-6,
              QStringLiteral("参数对话框显示简化容差与最细条带"),
              QStringLiteral("tolerance=%1 thickness=%2")
                  .arg(toleranceSpin ? toleranceSpin->value() : -1.0)
                  .arg(thicknessSpin ? thicknessSpin->value() : -1.0));

        const LineArtConverter::Options initial = dialog.options();
        const LineArtConverter::Options converterDefaults;
        check(qAbs(initial.simplifyTolerance - converterDefaults.simplifyTolerance) < 1e-6
                  && qAbs(initial.minRegionThickness - converterDefaults.minRegionThickness) < 1e-6,
              QStringLiteral("默认简化容差与最细条带跟转换器默认一致"),
              QStringLiteral("tolerance=%1 thickness=%2")
                  .arg(initial.simplifyTolerance)
                  .arg(initial.minRegionThickness));

        if (toleranceSpin && thicknessSpin) {
            toleranceSpin->setValue(1.5);
            thicknessSpin->setValue(2.0);
            const LineArtConverter::Options changed = dialog.options();
            check(qAbs(changed.simplifyTolerance - 1.5) < 1e-6
                      && qAbs(changed.minRegionThickness - 2.0) < 1e-6,
                  QStringLiteral("手改简化容差与最细条带即时反映到转换参数"),
                  QStringLiteral("tolerance=%1 thickness=%2")
                      .arg(changed.simplifyTolerance)
                      .arg(changed.minRegionThickness));

            AppSettings written = defaults;
            dialog.applyToSettings(written);
            check(qAbs(written.lineArtSimplifyTolerance - 1.5) < 1e-6
                      && qAbs(written.lineArtMinThickness - 2.0) < 1e-6,
                  QStringLiteral("简化容差与最细条带会写回设置"),
                  QStringLiteral("tolerance=%1 thickness=%2")
                      .arg(written.lineArtSimplifyTolerance)
                      .arg(written.lineArtMinThickness));
        }

        SettingsDialog settingsDialog(defaults);
        check(settingsDialog.findChild<QDoubleSpinBox*>(QStringLiteral("LineArtToleranceSpin"))
                  && settingsDialog.findChild<QDoubleSpinBox*>(
                      QStringLiteral("LineArtThicknessSpin")),
              QStringLiteral("设置对话框也提供简化容差与最细条带"));
    }
    {
        // 需求：色块超过阈值时自动降采样；取消勾选就按原处理尺寸硬算。
        QImage manyBlocks(640, 480, QImage::Format_RGB32);
        manyBlocks.fill(Qt::white);
        {
            QPainter painter(&manyBlocks);
            painter.setPen(Qt::NoPen);
            const QColor palette[] = { QColor(220, 60, 60), QColor(60, 90, 200),
                                       QColor(40, 160, 90), QColor(210, 180, 50) };
            for (int row = 0; row < 30; ++row) {
                for (int col = 0; col < 40; ++col) {
                    painter.fillRect(QRect(col * 16 + 1, row * 16 + 1, 13, 13),
                                     palette[(row + col) % 4]);
                }
            }
        }

        LineArtConverter::Options adaptive;
        adaptive.adaptiveDownscale = true;
        adaptive.adaptiveRegionLimit = 300;
        const LineArtConverter::Result downscaled =
            LineArtConverter::extract(manyBlocks, adaptive);
        check(downscaled.ok && downscaled.imageSize.height() < 480,
              QStringLiteral("色块超过阈值时自动降低处理尺寸"),
              QStringLiteral("size=%1x%2 candidate=%3 outlines=%4")
                  .arg(downscaled.imageSize.width())
                  .arg(downscaled.imageSize.height())
                  .arg(downscaled.candidateCount)
                  .arg(downscaled.outlines.size()));

        LineArtConverter::Options keepSize = adaptive;
        keepSize.adaptiveDownscale = false;
        const LineArtConverter::Result fullSize =
            LineArtConverter::extract(manyBlocks, keepSize);
        check(fullSize.ok && fullSize.imageSize == QSize(640, 480) && !fullSize.truncated,
              QStringLiteral("取消自适应降采样后按原处理尺寸描边"),
              QStringLiteral("size=%1x%2 outlines=%3")
                  .arg(fullSize.imageSize.width())
                  .arg(fullSize.imageSize.height())
                  .arg(fullSize.outlines.size()));
    }
    {
        // 需求：线稿化要给几档预设，一键换掉全部参数；手改任一项就回到“自定义”。
        const AppSettings defaults;
        SettingsDialog dialog(defaults);
        auto* presetCombo = dialog.findChild<QComboBox*>(
            QStringLiteral("LineArtPresetCombo"));
        auto* levelsSpin = dialog.findChild<QSpinBox*>(
            QStringLiteral("LineArtLevelsSpin"));
        auto* paletteSpin = dialog.findChild<QSpinBox*>(
            QStringLiteral("LineArtPaletteMergeSpin"));
        auto* adjacentSpin = dialog.findChild<QSpinBox*>(
            QStringLiteral("LineArtAdjacentMergeSpin"));
        auto* areaSpin = dialog.findChild<QSpinBox*>(
            QStringLiteral("LineArtMinAreaSpin"));
        check(presetCombo && levelsSpin && paletteSpin && adjacentSpin && areaSpin,
              QStringLiteral("设置对话框提供线稿化档位下拉"));

        // 默认参数正好是“均衡”档，打开时就应选中它。
        check(presetCombo && presetCombo->currentText() == QStringLiteral("均衡"),
              QStringLiteral("线稿化档位默认选中均衡"),
              presetCombo ? presetCombo->currentText() : QString());

        if (presetCombo && areaSpin && paletteSpin && adjacentSpin && levelsSpin) {
            const int cleanIndex = presetCombo->findData(2); // 线条干净
            presetCombo->setCurrentIndex(cleanIndex);
            check(paletteSpin->value() == 64 && adjacentSpin->value() == 56
                      && areaSpin->value() == 20,
                  QStringLiteral("切换“线条干净”档位一次性改掉全部参数"),
                  QStringLiteral("palette=%1 adjacent=%2 area=%3")
                      .arg(paletteSpin->value()).arg(adjacentSpin->value())
                      .arg(areaSpin->value()));

            const int detailIndex = presetCombo->findData(0); // 保留细节
            presetCombo->setCurrentIndex(detailIndex);
            check(levelsSpin->value() == 6 && paletteSpin->value() == 40
                      && adjacentSpin->value() == 44 && areaSpin->value() == 8,
                  QStringLiteral("切换“保留细节”档位一次性改掉全部参数"),
                  QStringLiteral("levels=%1 palette=%2 adjacent=%3 area=%4")
                      .arg(levelsSpin->value()).arg(paletteSpin->value())
                      .arg(adjacentSpin->value()).arg(areaSpin->value()));

            // 用户手改任一参数，档位要立刻回到“自定义”。
            areaSpin->setValue(33);
            check(presetCombo->currentText() == QStringLiteral("自定义"),
                  QStringLiteral("手动改动参数后档位回到自定义"),
                  presetCombo->currentText());
            paletteSpin->setValue(51);
            check(presetCombo->currentText() == QStringLiteral("自定义"),
                  QStringLiteral("手改颜色近似度同样回到自定义"),
                  presetCombo->currentText());

            // 再选回“均衡”，写回的值应当与默认一致。
            presetCombo->setCurrentIndex(presetCombo->findData(1));
            if (auto* buttons = dialog.findChild<QDialogButtonBox*>()) {
                if (QPushButton* ok = buttons->button(QDialogButtonBox::Ok))
                    ok->click();
                else
                    dialog.accept();
            } else {
                dialog.accept();
            }
            check(dialog.settings().lineArtPaletteMerge == defaults.lineArtPaletteMerge
                      && dialog.settings().lineArtAdjacentMerge == defaults.lineArtAdjacentMerge
                      && dialog.settings().lineArtMinRegionArea == defaults.lineArtMinRegionArea,
                  QStringLiteral("“均衡”档位写回后与默认参数一致"),
                  QStringLiteral("palette=%1 adjacent=%2 area=%3")
                      .arg(dialog.settings().lineArtPaletteMerge)
                      .arg(dialog.settings().lineArtAdjacentMerge)
                      .arg(dialog.settings().lineArtMinRegionArea));
        }
    }
    // ---------------------------------------------------------------
    // 需求 10/11/12/13/14 回归：锁定、椭圆、填充样式、画板尺寸。
    // ---------------------------------------------------------------
    {
        CanvasScene reqScene;
        reqScene.setMode(ToolMode::Edit);
        reqScene.setShowGrid(false);
        QUndoStack reqUndo;
        reqScene.setUndoStack(&reqUndo);

        // 需求 11：只保留椭圆，插入即标准圆形，可拉伸。
        check(shapeTypeName(ShapeType::Ellipse) == QStringLiteral("椭圆"),
              QStringLiteral("图形名称统一为椭圆"));
        const QSizeF ellipseDefault = defaultShapeSize(ShapeType::Ellipse);
        check(qFuzzyCompare(ellipseDefault.width(), ellipseDefault.height()),
              QStringLiteral("椭圆默认插入为标准圆形"));
        auto* circleItem = new ShapeItem(ShapeType::Ellipse);
        circleItem->setContentSize(QSizeF(200, 200));
        circleItem->setContentSize(QSizeF(260, 140));
        check(circleItem->contentSize() == QSizeF(260, 140),
              QStringLiteral("圆形可在编辑模式拉伸成椭圆"));
        delete circleItem;

        // 需求 14：填充样式（无/纯色/斜线/交叉线）。
        check(fillStyleName(FillStyle::None) == QStringLiteral("无填充")
                  && fillStyleName(FillStyle::Hachure) == QStringLiteral("斜线"),
              QStringLiteral("填充样式名称齐全"));
        check(fillStyleFromString(QStringLiteral("cross_hatch")) == FillStyle::CrossHatch,
              QStringLiteral("交叉线填充样式可反序列化"));

        auto* filledShape = new ShapeItem(ShapeType::Rectangle);
        filledShape->setContentSize(QSizeF(120, 80));
        filledShape->setPos(QPointF(30, 30));
        filledShape->setFill(QColor(59, 130, 246, 160), FillStyle::Hachure);
        check(filledShape->fillStyle() == FillStyle::Hachure
                  && filledShape->fillColor().alpha() == 160,
              QStringLiteral("图形可设置填充颜色与斜线填充"));
        reqScene.attachItem(filledShape);

        // 需求 23：箭头内部空心，可参与填充；只有纯色填充才整体遮挡下层。
        check(shapeTypeIsFilled(ShapeType::Arrow)
                  && shapeTypeIsFilled(ShapeType::DoubleArrow)
                  && !shapeTypeIsFilled(ShapeType::Line),
              QStringLiteral("箭头支持填充，直线仍只支持描边"));

        CanvasScene arrowScene;
        arrowScene.applyCanvasSize(QSizeF(400, 200));
        arrowScene.setShowGrid(false);
        auto* hollowArrow = new ShapeItem(ShapeType::Arrow);
        hollowArrow->setPos(QPointF(40, 50));
        hollowArrow->setContentSize(QSizeF(240, 100));
        hollowArrow->setStyle(QColor(QStringLiteral("#DC2626")), QColor(Qt::transparent), 6.0);
        hollowArrow->setFill(QColor(Qt::transparent), FillStyle::None);
        arrowScene.attachItem(hollowArrow);

        check(!hollowArrow->paintedShape().contains(QPointF(70, 50)),
              QStringLiteral("无填充箭头内部为空心，不遮挡下层"));

        QImage hollowArrowImage(400, 200, QImage::Format_ARGB32_Premultiplied);
        hollowArrowImage.fill(Qt::white);
        QPainter hollowArrowPainter(&hollowArrowImage);
        arrowScene.render(&hollowArrowPainter, QRectF(0, 0, 400, 200),
                          QRectF(0, 0, 400, 200));
        hollowArrowPainter.end();
        const QColor hollowPixel = hollowArrowImage.pixelColor(110, 100);
        check(hollowPixel.red() > 245 && hollowPixel.green() > 245 && hollowPixel.blue() > 245,
              QStringLiteral("无填充箭头内部渲染为空心"),
              hollowPixel.name());

        hollowArrow->setFill(QColor(QStringLiteral("#2563EB")), FillStyle::Solid);
        check(hollowArrow->paintedShape().contains(QPointF(70, 50)),
              QStringLiteral("纯色填充箭头内部整体遮挡下层"));
        QImage filledArrowImage(400, 200, QImage::Format_ARGB32_Premultiplied);
        filledArrowImage.fill(Qt::white);
        QPainter filledArrowPainter(&filledArrowImage);
        arrowScene.render(&filledArrowPainter, QRectF(0, 0, 400, 200),
                          QRectF(0, 0, 400, 200));
        filledArrowPainter.end();
        const QColor filledPixel = filledArrowImage.pixelColor(110, 100);
        check(filledPixel.blue() > 200 && filledPixel.red() < 80,
              QStringLiteral("纯色填充箭头内部渲染为填充色"),
              filledPixel.name());

        const QJsonObject filledRoot = reqScene.toJson();
        CanvasScene filledLoaded;
        QString filledError;
        check(filledLoaded.fromJson(filledRoot, &filledError),
              QStringLiteral("填充样式画布反序列化"), filledError);
        ShapeItem* loadedShape = nullptr;
        for (CanvasItem* item : filledLoaded.canvasItems()) {
            if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
                loadedShape = shape;
                break;
            }
        }
        check(loadedShape && loadedShape->fillStyle() == FillStyle::Hachure
                  && loadedShape->fillColor().alpha() == 160,
              QStringLiteral("填充样式随画布保存与恢复"));

        QUndoStack styleUndo;
        CanvasScene styleScene;
        styleScene.setUndoStack(&styleUndo);
        auto* styleShape = new ShapeItem(ShapeType::Rectangle);
        styleShape->setContentSize(QSizeF(100, 60));
        styleScene.attachItem(styleShape);
        const ShapeItemState styleBefore = styleShape->state();
        styleShape->setFill(QColor(239, 68, 68, 120), FillStyle::CrossHatch);
        const ShapeItemState styleAfter = styleShape->state();
        styleUndo.push(new ShapeStyleCommand(&styleScene, { styleShape },
                                            { styleBefore }, { styleAfter },
                                            QStringLiteral("调整填充样式")));
        check(styleShape->fillStyle() == FillStyle::CrossHatch,
              QStringLiteral("填充样式命令立即生效"));
        styleUndo.undo();
        check(styleShape->fillStyle() == styleBefore.fillStyle
                  && styleShape->fillColor() == styleBefore.fillColor,
              QStringLiteral("填充样式可撤销"));
        styleUndo.redo();
        check(styleShape->fillStyle() == FillStyle::CrossHatch,
              QStringLiteral("填充样式可重做"));

        // 交叉线填充必须铺满整个图形：两个方向的斜线在左下角与右上角密度相当。
        CanvasScene hatchScene;
        hatchScene.applyCanvasSize(QSizeF(400, 300));
        auto* crossShape = new ShapeItem(ShapeType::Rectangle);
        crossShape->setPos(QPointF(50, 45));
        crossShape->setContentSize(QSizeF(300, 200));
        crossShape->setStyle(QColor(QStringLiteral("#94A3B8")),
                             QColor(QStringLiteral("#94A3B8")), 2.0);
        crossShape->setFill(QColor(239, 68, 68, 255), FillStyle::CrossHatch);
        hatchScene.attachItem(crossShape);

        QImage hatchImage(400, 300, QImage::Format_ARGB32_Premultiplied);
        hatchImage.fill(Qt::white);
        QPainter hatchPainter(&hatchImage);
        hatchScene.render(&hatchPainter, QRectF(0, 0, 400, 300), QRectF(0, 0, 400, 300));
        hatchPainter.end();

        auto hatchPixels = [&hatchImage](const QRect& area) {
            int count = 0;
            for (int y = area.top(); y <= area.bottom(); ++y) {
                for (int x = area.left(); x <= area.right(); ++x) {
                    const QColor pixel = hatchImage.pixelColor(x, y);
                    if (pixel.red() - pixel.green() > 25 && pixel.red() - pixel.blue() > 25)
                        ++count;
                }
            }
            return count;
        };
        const int hatchTopRight = hatchPixels(QRect(270, 60, 60, 60));
        const int hatchBottomLeft = hatchPixels(QRect(60, 180, 60, 60));
        check(hatchTopRight > 0 && hatchBottomLeft * 4 >= hatchTopRight * 3,
              QStringLiteral("交叉线填充均匀铺满整个图形，不只出现在右上角"),
              QStringLiteral("topRight=%1 bottomLeft=%2")
                  .arg(hatchTopRight).arg(hatchBottomLeft));

        // 填充样式扩展：反斜线、正交叉线、六边形。
        check(fillStyleName(FillStyle::ReverseHachure) == QStringLiteral("反斜线")
                  && fillStyleName(FillStyle::GridHatch) == QStringLiteral("正交叉线")
                  && fillStyleName(FillStyle::Honeycomb) == QStringLiteral("六边形"),
              QStringLiteral("新增填充样式名称齐全"));
        check(allFillStyles().size() == 7
                  && allFillStyles().at(0) == FillStyle::None
                  && allFillStyles().at(1) == FillStyle::Solid
                  && allFillStyles().at(2) == FillStyle::Hachure
                  && allFillStyles().at(3) == FillStyle::ReverseHachure
                  && allFillStyles().at(4) == FillStyle::CrossHatch
                  && allFillStyles().at(5) == FillStyle::GridHatch
                  && allFillStyles().at(6) == FillStyle::Honeycomb,
              QStringLiteral("填充样式列表包含全部 7 种且顺序稳定"));
        check(fillStyleFromString(fillStyleToString(FillStyle::ReverseHachure))
                      == FillStyle::ReverseHachure
                  && fillStyleFromString(fillStyleToString(FillStyle::GridHatch))
                      == FillStyle::GridHatch
                  && fillStyleFromString(fillStyleToString(FillStyle::Honeycomb))
                      == FillStyle::Honeycomb,
              QStringLiteral("新增填充样式可序列化往返"));

        auto renderPatternFill = [](FillStyle style, QImage& image) {
            CanvasScene scene;
            scene.applyCanvasSize(QSizeF(400, 300));
            auto* shape = new ShapeItem(ShapeType::Rectangle);
            shape->setPos(QPointF(50, 45));
            shape->setContentSize(QSizeF(300, 200));
            shape->setStyle(QColor(QStringLiteral("#94A3B8")),
                            QColor(QStringLiteral("#94A3B8")), 2.0);
            shape->setFill(QColor(239, 68, 68, 255), style);
            scene.attachItem(shape);
            image = QImage(400, 300, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::white);
            QPainter painter(&image);
            scene.render(&painter, QRectF(0, 0, 400, 300), QRectF(0, 0, 400, 300));
            painter.end();
        };
        auto patternLinePixel = [](const QImage& image, int x, int y) {
            const QColor pixel = image.pixelColor(x, y);
            return pixel.red() - pixel.green() > 25 && pixel.red() - pixel.blue() > 25;
        };

        // “\” 与 “/” 的方向判别：沿右下方向的相邻像素多，说明是反斜线。
        QImage reverseImage;
        renderPatternFill(FillStyle::ReverseHachure, reverseImage);
        QImage forwardImage;
        renderPatternFill(FillStyle::Hachure, forwardImage);
        int reverseDownRight = 0;
        int reverseUpRight = 0;
        int forwardDownRight = 0;
        int forwardUpRight = 0;
        for (int y = 60; y < 240; ++y) {
            for (int x = 60; x < 340; ++x) {
                if (patternLinePixel(reverseImage, x, y)) {
                    if (patternLinePixel(reverseImage, x + 2, y + 2))
                        ++reverseDownRight;
                    if (patternLinePixel(reverseImage, x + 2, y - 2))
                        ++reverseUpRight;
                }
                if (patternLinePixel(forwardImage, x, y)) {
                    if (patternLinePixel(forwardImage, x + 2, y + 2))
                        ++forwardDownRight;
                    if (patternLinePixel(forwardImage, x + 2, y - 2))
                        ++forwardUpRight;
                }
            }
        }
        check(reverseDownRight > reverseUpRight * 2 && forwardUpRight > forwardDownRight * 2,
              QStringLiteral("反斜线填充方向与斜线相反"),
              QStringLiteral("反斜线 down=%1 up=%2，斜线 down=%3 up=%4")
                  .arg(reverseDownRight).arg(reverseUpRight)
                  .arg(forwardDownRight).arg(forwardUpRight));

        // 正交叉线：水平线与竖直线同时存在（行/列方向都能扫到大量线条像素）。
        QImage gridImage;
        renderPatternFill(FillStyle::GridHatch, gridImage);
        int gridRowRun = 0;
        for (int x = 60; x < 340; ++x) {
            if (patternLinePixel(gridImage, x, 145))
                ++gridRowRun;
        }
        int gridColumnRun = 0;
        for (int y = 55; y < 240; ++y) {
            if (patternLinePixel(gridImage, 200, y))
                ++gridColumnRun;
        }
        check(gridRowRun >= 15 && gridColumnRun >= 12,
              QStringLiteral("正交叉线填充同时画出水平线与竖直线"),
              QStringLiteral("rowRun=%1 columnRun=%2").arg(gridRowRun).arg(gridColumnRun));

        // 六边形：单元格中心留空、六条边有线条。
        // 间距 = 高度 300 的 3%（上限 12）= 9，半径 = 1.15×间距 = 10.35，
        // 单元宽 √3r ≈ 17.93；第 3 行单元中心 (17.93, 20.7)，右侧竖边在 x≈26.89（局部坐标）。
        QImage honeycombImage;
        renderPatternFill(FillStyle::Honeycomb, honeycombImage);
        const bool honeycombCellEmpty = !patternLinePixel(honeycombImage, 50 + 18, 45 + 21);
        const bool honeycombEdgeDrawn = patternLinePixel(honeycombImage, 50 + 27, 45 + 21);
        int honeycombPixels = 0;
        for (int y = 55; y < 240; ++y) {
            for (int x = 55; x < 345; ++x) {
                if (patternLinePixel(honeycombImage, x, y))
                    ++honeycombPixels;
            }
        }
        const double honeycombRatio = static_cast<double>(honeycombPixels) / (290.0 * 185.0);
        check(honeycombCellEmpty && honeycombEdgeDrawn && honeycombRatio > 0.1
                  && honeycombRatio < 0.55,
              QStringLiteral("六边形填充画出蜂窝网格，格子内部留空"),
              QStringLiteral("cellEmpty=%1 edge=%2 ratio=%3")
                  .arg(honeycombCellEmpty ? 1 : 0)
                  .arg(honeycombEdgeDrawn ? 1 : 0)
                  .arg(honeycombRatio, 0, 'f', 3));

        // 遮挡必须完全不透：上层是半透明颜色时，重叠处只能看到“上层颜色 + 背景”，
        // 不能把下层颜色混进来。
        CanvasScene blendScene;
        blendScene.applyCanvasSize(QSizeF(300, 200));
        blendScene.setShowGrid(false);
        auto* lowerBand = new ShapeItem(ShapeType::Rectangle);
        lowerBand->setPos(QPointF(20, 20));
        lowerBand->setContentSize(QSizeF(160, 120));
        lowerBand->setStyle(QColor(QStringLiteral("#94A3B8")),
                            QColor(QStringLiteral("#94A3B8")), 1.0);
        lowerBand->setFill(QColor(255, 0, 0, 255), FillStyle::Solid);
        blendScene.attachItem(lowerBand);

        auto* upperBand = new ShapeItem(ShapeType::Rectangle);
        upperBand->setPos(QPointF(100, 50));
        upperBand->setContentSize(QSizeF(160, 120));
        upperBand->setStyle(QColor(QStringLiteral("#94A3B8")),
                            QColor(QStringLiteral("#94A3B8")), 1.0);
        upperBand->setFill(QColor(0, 0, 255, 128), FillStyle::Solid);
        blendScene.attachItem(upperBand);

        QImage blendImage(300, 200, QImage::Format_ARGB32_Premultiplied);
        blendImage.fill(Qt::white);
        QPainter blendPainter(&blendImage);
        blendScene.render(&blendPainter, QRectF(0, 0, 300, 200), QRectF(0, 0, 300, 200));
        blendPainter.end();

        const QColor overlapPixel = blendImage.pixelColor(140, 90);
        const QColor lowerOnlyPixel = blendImage.pixelColor(60, 60);
        check(overlapPixel.green() > 60 && overlapPixel.blue() > 150,
              QStringLiteral("遮挡处完全不透，只显示上层颜色与背景"),
              QStringLiteral("overlap=%1,%2,%3")
                  .arg(overlapPixel.red()).arg(overlapPixel.green()).arg(overlapPixel.blue()));
        check(lowerOnlyPixel.red() > 200 && lowerOnlyPixel.green() < 80,
              QStringLiteral("未被遮挡的部分仍显示下层颜色"),
              QStringLiteral("lower=%1,%2,%3")
                  .arg(lowerOnlyPixel.red()).arg(lowerOnlyPixel.green())
                  .arg(lowerOnlyPixel.blue()));

        // 真实视图路径（QGraphicsView::paintEvent）也必须完全不透：默认视图走
        // QGraphicsScenePrivate::drawItems，绕过 CanvasScene::drawItems；只有打开
        // IndirectPainting 才会调用虚函数，这里用截图验证它确实生效。
        {
            CanvasView blendView(&blendScene);
            blendView.resize(460, 340);
            blendView.setMode(ToolMode::View);
            blendView.show();
            QApplication::processEvents();

            const QImage viewImage = blendView.viewport()->grab().toImage();
            const QPoint viewPoint = blendView.mapFromScene(QPointF(140, 90));
            const QPoint viewLowerPoint = blendView.mapFromScene(QPointF(60, 60));
            QColor viewOverlapColor(0, 0, 0);
            QColor viewLowerColor(0, 0, 0);
            if (viewImage.rect().contains(viewPoint))
                viewOverlapColor = viewImage.pixelColor(viewPoint);
            if (viewImage.rect().contains(viewLowerPoint))
                viewLowerColor = viewImage.pixelColor(viewLowerPoint);
            check(viewOverlapColor.green() > 60 && viewOverlapColor.blue() > 150,
                  QStringLiteral("真实视图中的遮挡处同样完全不透"),
                  QStringLiteral("view=%1,%2,%3 point=%4,%5")
                      .arg(viewOverlapColor.red()).arg(viewOverlapColor.green())
                      .arg(viewOverlapColor.blue()).arg(viewPoint.x()).arg(viewPoint.y()));
            check(viewLowerColor.red() > 200 && viewLowerColor.green() < 80,
                  QStringLiteral("真实视图中未被遮挡的部分仍显示下层颜色"),
                  QStringLiteral("viewLower=%1,%2,%3")
                      .arg(viewLowerColor.red()).arg(viewLowerColor.green())
                      .arg(viewLowerColor.blue()));
        }

        // 无填充、纹理填充的图形也按“图元占据的区域”完整遮挡下层：
        // 图形内部（含纹理空隙）透出的是画板底色，而不是下层图元的颜色。
        {
            CanvasScene hollowScene;
            hollowScene.applyCanvasSize(QSizeF(300, 200));
            hollowScene.setShowGrid(false);
            auto* lower = new ShapeItem(ShapeType::Rectangle);
            lower->setPos(QPointF(40, 40));
            lower->setContentSize(QSizeF(220, 120));
            lower->setStyle(QColor(QStringLiteral("#94A3B8")),
                            QColor(QStringLiteral("#94A3B8")), 1.0);
            lower->setFill(QColor(239, 68, 68), FillStyle::Solid);
            hollowScene.attachItem(lower);

            auto* hollow = new ShapeItem(ShapeType::Rectangle);
            hollow->setPos(QPointF(80, 60));
            hollow->setContentSize(QSizeF(140, 100));
            hollow->setStyle(QColor(37, 99, 235), QColor(Qt::transparent), 2.0);
            hollow->setFill(QColor(Qt::transparent), FillStyle::None);
            hollowScene.attachItem(hollow);

            QImage hollowImage(300, 200, QImage::Format_ARGB32_Premultiplied);
            hollowImage.fill(Qt::white);
            QPainter hollowPainter(&hollowImage);
            hollowScene.render(&hollowPainter, QRectF(0, 0, 300, 200), QRectF(0, 0, 300, 200));
            hollowPainter.end();
            const QColor hollowPixel = hollowImage.pixelColor(150, 110);
            check(hollowPixel.red() > 200 && hollowPixel.green() < 120,
                  QStringLiteral("无填充图形的内部不遮挡下层，下层颜色照常可见"),
                  QStringLiteral("pixel=%1,%2,%3")
                      .arg(hollowPixel.red()).arg(hollowPixel.green())
                      .arg(hollowPixel.blue()));

            auto* pattern = new ShapeItem(ShapeType::Rectangle);
            pattern->setPos(QPointF(80, 60));
            pattern->setContentSize(QSizeF(140, 100));
            pattern->setStyle(QColor(37, 99, 235), QColor(37, 99, 235, 200), 2.0);
            pattern->setFill(QColor(37, 99, 235, 200), FillStyle::Honeycomb);

            CanvasScene patternScene;
            patternScene.applyCanvasSize(QSizeF(300, 200));
            patternScene.setShowGrid(false);
            auto* patternLower = new ShapeItem(ShapeType::Rectangle);
            patternLower->setPos(QPointF(40, 40));
            patternLower->setContentSize(QSizeF(220, 120));
            patternLower->setStyle(QColor(QStringLiteral("#94A3B8")),
                                   QColor(QStringLiteral("#94A3B8")), 1.0);
            patternLower->setFill(QColor(239, 68, 68), FillStyle::Solid);
            patternScene.attachItem(patternLower);
            patternScene.attachItem(pattern);

            QImage patternImage(300, 200, QImage::Format_ARGB32_Premultiplied);
            patternImage.fill(Qt::white);
            QPainter patternPainter(&patternImage);
            patternScene.render(&patternPainter, QRectF(0, 0, 300, 200), QRectF(0, 0, 300, 200));
            patternPainter.end();
            int lowerVisible = 0;
            int patternLines = 0;
            for (int y = 72; y < 150; y += 4) {
                for (int x = 92; x < 210; x += 4) {
                    const QColor pixel = patternImage.pixelColor(x, y);
                    if (pixel.red() > 200 && pixel.green() < 120)
                        ++lowerVisible;
                    if (pixel.blue() > 150 && pixel.red() < 150)
                        ++patternLines;
                }
            }
            check(lowerVisible > 0 && patternLines > 0,
                  QStringLiteral("纹理填充不遮挡下层：空隙里透出下层颜色，线条仍然画在上层"),
                  QStringLiteral("lowerVisible=%1 lines=%2").arg(lowerVisible).arg(patternLines));

            // 反过来：纯色填充（含半透明）压住纹理填充的下层时，下层纹理必须完全消失。
            CanvasScene solidOverTexture;
            solidOverTexture.applyCanvasSize(QSizeF(300, 200));
            solidOverTexture.setShowGrid(false);
            auto* texturedLower = new ShapeItem(ShapeType::Rectangle);
            texturedLower->setPos(QPointF(40, 40));
            texturedLower->setContentSize(QSizeF(220, 120));
            texturedLower->setStyle(QColor(239, 68, 68), QColor(239, 68, 68, 200), 1.0);
            texturedLower->setFill(QColor(239, 68, 68, 200), FillStyle::Honeycomb);
            solidOverTexture.attachItem(texturedLower);

            auto* solidCover = new ShapeItem(ShapeType::Rectangle);
            solidCover->setPos(QPointF(80, 60));
            solidCover->setContentSize(QSizeF(140, 100));
            solidCover->setStyle(QColor(37, 99, 235), QColor(37, 99, 235, 128), 2.0);
            solidCover->setFill(QColor(37, 99, 235, 128), FillStyle::Solid);
            solidOverTexture.attachItem(solidCover);

            QImage solidOverImage(300, 200, QImage::Format_ARGB32_Premultiplied);
            solidOverImage.fill(Qt::white);
            QPainter solidOverPainter(&solidOverImage);
            solidOverTexture.render(&solidOverPainter, QRectF(0, 0, 300, 200), QRectF(0, 0, 300, 200));
            solidOverPainter.end();
            int textureLeaked = 0;
            int solidPixels = 0;
            for (int y = 72; y < 150; y += 4) {
                for (int x = 92; x < 210; x += 4) {
                    const QColor pixel = solidOverImage.pixelColor(x, y);
                    if (pixel.green() < 120)
                        ++textureLeaked;
                    if (pixel.blue() > 200 && pixel.green() > 120)
                        ++solidPixels;
                }
            }
            check(textureLeaked == 0 && solidPixels > 0,
                  QStringLiteral("纯色填充也遮挡下层纹理，重叠处只剩纯色"),
                  QStringLiteral("leaked=%1 solid=%2").arg(textureLeaked).arg(solidPixels));
        }

        // 需求 13：画板通过拉伸边缘扩大或裁剪，并随画布保存原点。
        QUndoStack canvasUndo;
        CanvasScene canvasScene;
        canvasScene.setUndoStack(&canvasUndo);
        const QRectF initialCanvas = canvasScene.canvasRect();
        const QRectF grownCanvas = initialCanvas.adjusted(-240, -160, 320, 260);
        canvasScene.setCanvasRectWithUndo(grownCanvas);
        check(canvasScene.canvasRect() == grownCanvas
                  && canvasScene.canvasOrigin() == grownCanvas.topLeft(),
              QStringLiteral("画板可向四边拉伸扩大"));
        const QRectF shrunkCanvas(grownCanvas.left() + 120, grownCanvas.top() + 90,
                                 grownCanvas.width() - 240, grownCanvas.height() - 180);
        canvasScene.setCanvasRectWithUndo(shrunkCanvas);
        check(canvasScene.canvasRect() == shrunkCanvas,
              QStringLiteral("画板可裁剪缩小"));
        canvasUndo.undo();
        check(canvasScene.canvasRect() == grownCanvas,
              QStringLiteral("画板尺寸调整可撤销"));

        CanvasView canvasView(&canvasScene);
        canvasView.resize(1200, 800);
        canvasView.setMode(ToolMode::Edit);
        canvasView.fitAll();
        QApplication::processEvents();
        const QRectF canvasBeforeDrag = canvasScene.canvasRect();
        const int commandsBeforeDrag = canvasUndo.index();
        const QPoint rightEdge = canvasView.mapFromScene(
            QPointF(canvasBeforeDrag.right(), canvasBeforeDrag.center().y()));
        sendMouseEvent(canvasView.viewport(), QEvent::MouseButtonPress, rightEdge,
                       Qt::LeftButton, Qt::LeftButton);
        sendMouseEvent(canvasView.viewport(), QEvent::MouseMove, rightEdge + QPoint(12, 0),
                       Qt::NoButton, Qt::LeftButton);
        sendMouseEvent(canvasView.viewport(), QEvent::MouseButtonRelease, rightEdge + QPoint(12, 0),
                       Qt::LeftButton, Qt::NoButton);
        QApplication::processEvents();
        check(canvasScene.canvasRect().width() > canvasBeforeDrag.width() + 5.0
                  && qFuzzyCompare(canvasScene.canvasRect().left(), canvasBeforeDrag.left()),
              QStringLiteral("拖动右边缘手柄可扩大画板"),
              QStringLiteral("before=%1x%2 after=%3x%4")
                  .arg(canvasBeforeDrag.width()).arg(canvasBeforeDrag.height())
                  .arg(canvasScene.canvasRect().width()).arg(canvasScene.canvasRect().height()));
        check(canvasUndo.index() == commandsBeforeDrag + 1
                  && canvasUndo.count() == commandsBeforeDrag + 1,
              QStringLiteral("画板边缘拖动只生成一条撤销记录"),
              QStringLiteral("index before=%1 after=%2")
                  .arg(commandsBeforeDrag).arg(canvasUndo.index()));

        const QJsonObject canvasRoot = canvasScene.toJson();
        CanvasScene canvasLoaded;
        QString canvasError;
        check(canvasLoaded.fromJson(canvasRoot, &canvasError)
                  && canvasLoaded.canvasRect() == canvasScene.canvasRect(),
              QStringLiteral("画板原点随画布保存"), canvasError);

        // 需求 10：锁定状态随画布保存。
        CanvasScene lockScene;
        lockScene.setMode(ToolMode::Edit);
        const int lockLayerId = lockScene.activeLayerId();
        check(lockScene.setLayerLocked(lockLayerId, true),
              QStringLiteral("锁定图层用于序列化测试"));
        const QJsonObject lockRoot = lockScene.toJson();
        CanvasScene lockLoaded;
        QString lockError;
        check(lockLoaded.fromJson(lockRoot, &lockError)
                  && lockLoaded.isLayerLocked(lockLoaded.activeLayerId()),
              QStringLiteral("图层锁定状态随画布保存"), lockError);
    }

    {
        // 需求 15：图层面板选中行有高亮，且重叠图元优先命中已选图层。
        CanvasScene styleScene;
        LayerPanel stylePanel(&styleScene);
        auto* layerList = stylePanel.findChild<QListWidget*>();
        check(layerList && layerList->count() >= 2,
              QStringLiteral("图层面板包含用户图层与背景行"));
        QListWidgetItem* selectedEntry = nullptr;
        for (int row = 0; row < layerList->count(); ++row) {
            QListWidgetItem* entry = layerList->item(row);
            if (entry->data(Qt::UserRole).toInt() == styleScene.activeLayerId())
                selectedEntry = entry;
        }
        check(selectedEntry && layerList->itemWidget(selectedEntry)
                  && layerList->itemWidget(selectedEntry)->styleSheet().contains(
                         QStringLiteral("232, 238, 251")),
              QStringLiteral("选中的图层行带背景高亮"));
        check(styleScene.preferredLayerIds().contains(styleScene.activeLayerId()),
              QStringLiteral("图层面板选中图层同步为首选命中图层"));

        CanvasScene overlapScene;
        overlapScene.setMode(ToolMode::Edit);
        const int upperLayerId = overlapScene.createLayer(QStringLiteral("上层"));
        auto* lowerItem = new ShapeItem(ShapeType::Rectangle);
        lowerItem->setLayerId(CanvasScene::BackgroundLayerId + 1);
        lowerItem->setStyle(QColor(QStringLiteral("#2563EB")), QColor(Qt::transparent), 4.0);
        lowerItem->setPos(QPointF(200, 170));
        lowerItem->setContentSize(QSizeF(200, 140));
        auto* upperItem = new ShapeItem(ShapeType::Rectangle);
        upperItem->setLayerId(upperLayerId);
        upperItem->setStyle(QColor(QStringLiteral("#DC2626")), QColor(Qt::transparent), 4.0);
        upperItem->setPos(QPointF(200, 170));
        upperItem->setContentSize(QSizeF(200, 140));
        overlapScene.addItemsWithUndo({ lowerItem, upperItem }, QStringLiteral("重叠图元"));

        CanvasView overlapView(&overlapScene);
        overlapView.resize(640, 460);
        overlapView.setMode(ToolMode::Edit);
        overlapView.centerOn(QPointF(300, 240));
        QApplication::processEvents();
        const QPoint overlapPoint = overlapView.mapFromScene(QPointF(300, 240));

        overlapScene.clearSelection();
        overlapScene.setPreferredLayerIds({ CanvasScene::BackgroundLayerId + 1 });
        sendMouseEvent(overlapView.viewport(), QEvent::MouseButtonPress, overlapPoint,
                       Qt::LeftButton, Qt::LeftButton);
        sendMouseEvent(overlapView.viewport(), QEvent::MouseButtonRelease, overlapPoint,
                       Qt::LeftButton, Qt::NoButton);
        const QList<CanvasItem*> lowerSelection = overlapScene.selectedCanvasItems();
        check(lowerSelection.size() == 1 && lowerSelection.first() == lowerItem,
              QStringLiteral("重叠区域优先选中已选图层中的图元"),
              QStringLiteral("selected=%1").arg(lowerSelection.size()));

        overlapScene.clearSelection();
        overlapScene.setPreferredLayerIds({ upperLayerId });
        sendMouseEvent(overlapView.viewport(), QEvent::MouseButtonPress, overlapPoint,
                       Qt::LeftButton, Qt::LeftButton);
        sendMouseEvent(overlapView.viewport(), QEvent::MouseButtonRelease, overlapPoint,
                       Qt::LeftButton, Qt::NoButton);
        const QList<CanvasItem*> upperSelection = overlapScene.selectedCanvasItems();
        check(upperSelection.size() == 1 && upperSelection.first() == upperItem,
              QStringLiteral("切换已选图层后优先选中该层图元"));

        // 需求 16：批量高度调整只占一条撤销记录。
        overlapScene.clearSelection();
        lowerItem->setSelected(true);
        upperItem->setSelected(true);
        QUndoStack heightUndo;
        overlapScene.setUndoStack(&heightUndo);
        const int lowerHeightBefore = lowerItem->height();
        const int upperHeightBefore = upperItem->height();
        const int heightIndexBefore = heightUndo.index();
        const int heightChanged = overlapScene.adjustSelectedHeights(1);
        check(heightChanged == 2
                  && lowerItem->height() == lowerHeightBefore + 1
                  && upperItem->height() == upperHeightBefore + 1,
              QStringLiteral("快捷键批量提升选中图元高度"));
        check(heightUndo.index() == heightIndexBefore + 1,
              QStringLiteral("批量高度调整只生成一条撤销记录"));
        heightUndo.undo();
        check(lowerItem->height() == lowerHeightBefore
                  && upperItem->height() == upperHeightBefore,
              QStringLiteral("批量高度调整可整体撤销"));
        overlapScene.setUndoStack(nullptr);
    }

    {
        // 需求 27：合并可见图层导出 / 每个可见图层单独导出。
        CanvasScene layerExportScene;
        BackgroundState exportBackground;
        exportBackground.type = BackgroundType::Color;
        exportBackground.color = QColor(Qt::white);
        layerExportScene.applyBackground(exportBackground);

        const int redLayer = layerExportScene.createLayer(QStringLiteral("红色层"));
        auto* red = new ShapeItem(ShapeType::Rectangle);
        red->setLayerId(redLayer);
        red->setStyle(QColor(QStringLiteral("#DC2626")), QColor(QStringLiteral("#DC2626")), 1.0);
        red->setPos(QPointF(10, 10));
        red->setContentSize(QSizeF(40, 40));
        layerExportScene.attachItem(red);

        const int blueLayer = layerExportScene.createLayer(QStringLiteral("蓝色层"));
        auto* blue = new ShapeItem(ShapeType::Rectangle);
        blue->setLayerId(blueLayer);
        blue->setStyle(QColor(QStringLiteral("#2563EB")), QColor(QStringLiteral("#2563EB")), 1.0);
        blue->setPos(QPointF(100, 100));
        blue->setContentSize(QSizeF(40, 40));
        layerExportScene.attachItem(blue);

        const QImage merged = layerExportScene.renderImage(layerExportScene.canvasRect(), 1.0);
        check(merged.pixelColor(20, 20) == QColor(QStringLiteral("#DC2626"))
                  && merged.pixelColor(110, 110) == QColor(QStringLiteral("#2563EB")),
              QStringLiteral("合并导出包含所有可见图层"));
        const QImage redOnly = layerExportScene.renderLayerImage(layerExportScene.canvasRect(), 1.0, redLayer);
        const QImage blueOnly = layerExportScene.renderLayerImage(layerExportScene.canvasRect(), 1.0, blueLayer);
        check(redOnly.pixelColor(20, 20) == QColor(QStringLiteral("#DC2626"))
                  && redOnly.pixelColor(110, 110) == QColor(Qt::white),
              QStringLiteral("逐层导出只渲染指定图层（红层）"));
        check(blueOnly.pixelColor(110, 110) == QColor(QStringLiteral("#2563EB"))
                  && blueOnly.pixelColor(20, 20) == QColor(Qt::white),
              QStringLiteral("逐层导出只渲染指定图层（蓝层）"));

        ExportDialog exportDialog(&layerExportScene, QDir::tempPath());
        auto* modeCombo = exportDialog.findChild<QComboBox*>(
            QStringLiteral("ExportLayerModeCombo"));
        check(modeCombo && modeCombo->count() == 2 && modeCombo->currentIndex() == 0,
              QStringLiteral("导出对话框默认合并可见图层"));
        const ExportDialog::Options mergedOptions = exportDialog.options();
        check(!mergedOptions.separateLayers && mergedOptions.filePaths.size() == 1,
              QStringLiteral("合并模式只输出一个文件"),
              QStringLiteral("files=%1").arg(mergedOptions.filePaths.size()));
        if (modeCombo)
            modeCombo->setCurrentIndex(1);
        const ExportDialog::Options layerOptions = exportDialog.options();
        int visibleLayers = 0;
        for (const CanvasLayer& layer : layerExportScene.layers()) {
            if (layer.visible)
                ++visibleLayers;
        }
        bool namesLookRight = layerOptions.separateLayers
            && layerOptions.filePaths.size() == visibleLayers
            && layerOptions.layerIds.size() == visibleLayers;
        for (const QString& path : layerOptions.filePaths) {
            if (!QFileInfo(path).fileName().contains(QStringLiteral("_L")))
                namesLookRight = false;
        }
        check(namesLookRight,
              QStringLiteral("逐层模式为每个可见图层生成独立文件"),
              QStringLiteral("files=%1 visible=%2")
                  .arg(layerOptions.filePaths.size()).arg(visibleLayers));
    }

    {
        // 需求 28：图层名可编辑，进入撤销栈并随画布持久化。
        CanvasScene renameScene;
        QUndoStack renameUndo;
        renameUndo.setUndoLimit(30);
        renameScene.setUndoStack(&renameUndo);
        const int renameLayerId = renameScene.createLayer(QStringLiteral("初版分镜"));
        const QString beforeName = renameScene.layer(renameLayerId).name;
        check(renameScene.renameLayerWithUndo(renameLayerId, QStringLiteral("终版分镜")),
              QStringLiteral("图层重命名成功"));
        check(renameScene.layer(renameLayerId).name == QStringLiteral("终版分镜"),
              QStringLiteral("图层重命名立即生效"));
        check(renameUndo.count() == 1, QStringLiteral("图层重命名生成一条撤销记录"));
        renameUndo.undo();
        check(renameScene.layer(renameLayerId).name == beforeName,
              QStringLiteral("图层重命名可撤销"));
        renameUndo.redo();
        check(renameScene.layer(renameLayerId).name == QStringLiteral("终版分镜"),
              QStringLiteral("图层重命名可重做"));
        check(!renameScene.renameLayerWithUndo(CanvasScene::BackgroundLayerId,
                                               QStringLiteral("不能改名")),
              QStringLiteral("背景图层名不可编辑"));
        check(!renameScene.renameLayerWithUndo(renameLayerId, QStringLiteral("  ")),
              QStringLiteral("空图层名被拒绝"));

        CanvasScene reloadedScene;
        check(reloadedScene.fromJson(renameScene.toJson()),
              QStringLiteral("重命名后的画布可重新载入"));
        check(reloadedScene.layer(renameLayerId).name == QStringLiteral("终版分镜"),
              QStringLiteral("图层名持久化到画布"));

        LayerPanel renamePanel(&renameScene);
        auto* renameList = renamePanel.findChild<QListWidget*>(
            QStringLiteral("LayerList"));
        QLabel* renameLabel = nullptr;
        if (renameList) {
            for (int row = 0; row < renameList->count(); ++row) {
                if (renameList->item(row)->data(Qt::UserRole).toInt() != renameLayerId)
                    continue;
                QWidget* rowWidget = renameList->itemWidget(renameList->item(row));
                renameLabel = rowWidget ? rowWidget->findChild<QLabel*>(
                    QStringLiteral("LayerRowLabel")) : nullptr;
                break;
            }
        }
        check(renameLabel && renameLabel->property("layerId").toInt() == renameLayerId
                  && renameLabel->toolTip().contains(QStringLiteral("重命名")),
              QStringLiteral("图层面板行名提供双击重命名入口"));
        renameScene.setUndoStack(nullptr);
    }

    {
        // 需求：导入图片线稿化 —— 图片里的各色块轮廓要描成笔画图元，线宽固定为 1。
        QImage artwork(240, 160, QImage::Format_RGB32);
        artwork.fill(Qt::white);
        {
            QPainter painter(&artwork);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.fillRect(QRectF(24.0, 24.0, 80.0, 56.0), QColor(QStringLiteral("#EF4444")));
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(QStringLiteral("#2563EB")));
            painter.drawEllipse(QPointF(168.0, 78.0), 28.0, 28.0);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(QStringLiteral("#0F766E")), 5.0));
            QPainterPath wave;
            wave.moveTo(30.0, 128.0);
            wave.cubicTo(70.0, 100.0, 110.0, 150.0, 210.0, 118.0);
            painter.drawPath(wave);
        }

        const LineArtConverter::Result traced = LineArtConverter::extract(artwork);
        check(traced.ok, QStringLiteral("图片线稿化提取成功"), traced.error);
        check(traced.outlines.size() >= 3 && traced.outlines.size() <= 8,
              QStringLiteral("线稿化把每一块颜色描成一条轮廓"),
              QStringLiteral("outlines=%1 regions=%2")
                  .arg(traced.outlines.size()).arg(traced.regionCount));

        bool closedLoops = !traced.outlines.isEmpty();
        bool insideImage = closedLoops;
        bool redBlockFound = false;
        for (const LineArtConverter::Outline& outline : traced.outlines) {
            if (outline.points.size() < 4
                || outline.points.constFirst() != outline.points.constLast()) {
                closedLoops = false;
            }
            for (const QPointF& point : outline.points) {
                if (point.x() < -1.0 || point.y() < -1.0
                    || point.x() > artwork.width() + 1.0 || point.y() > artwork.height() + 1.0) {
                    insideImage = false;
                }
            }
            if (outline.color.red() > 200 && outline.color.green() < 130
                && outline.color.blue() < 130) {
                redBlockFound = true;
            }
        }
        check(closedLoops, QStringLiteral("线稿轮廓都是闭合回路"));
        check(insideImage, QStringLiteral("线稿轮廓落在图片范围内"));
        check(redBlockFound, QStringLiteral("线稿笔画颜色取自对应色块"));

        CanvasScene lineArtScene;
        QUndoStack lineArtUndo;
        lineArtUndo.setUndoLimit(30);
        lineArtScene.setUndoStack(&lineArtUndo);
        lineArtScene.applyCanvasSize(QSizeF(240.0, 160.0));
        QList<CanvasItem*> lineArtItems;
        for (const LineArtConverter::Outline& outline : traced.outlines) {
            auto* stroke = new StrokeItem();
            stroke->setStrokePoints(outline.points, outline.color,
                                    LineArtConverter::Options().strokeWidth, 0.0, true);
            lineArtItems.append(stroke);
        }
        check(!lineArtItems.isEmpty(), QStringLiteral("线稿轮廓生成笔画图元"));
        if (!lineArtItems.isEmpty()) {
            lineArtScene.addItemsWithUndo(lineArtItems, QStringLiteral("导入图片线稿化"));
            check(lineArtScene.canvasItemCount() == lineArtItems.size(),
                  QStringLiteral("线稿笔画加入画布"));
            check(lineArtUndo.count() == 1, QStringLiteral("线稿化整体只占一条撤销记录"));

            int thinStrokes = 0;
            for (CanvasItem* item : lineArtItems) {
                if (auto* stroke = dynamic_cast<StrokeItem*>(item);
                    stroke && qFuzzyCompare(stroke->strokeWidth(), 1.0)) {
                    ++thinStrokes;
                }
            }
            check(thinStrokes == lineArtItems.size(), QStringLiteral("线稿笔画线宽都是 1"),
                  QStringLiteral("thin=%1 total=%2").arg(thinStrokes).arg(lineArtItems.size()));

            int linearStrokes = 0;
            for (CanvasItem* item : lineArtItems) {
                if (auto* stroke = dynamic_cast<StrokeItem*>(item);
                    stroke && stroke->isLinearPath()) {
                    ++linearStrokes;
                }
            }
            check(linearStrokes == lineArtItems.size(),
                  QStringLiteral("线稿笔画按折线绘制，方角不会被磨圆"));

            bool linearPersisted = false;
            if (auto* firstStroke = dynamic_cast<StrokeItem*>(lineArtItems.constFirst())) {
                StrokeItem reloadedStroke;
                reloadedStroke.fromJson(firstStroke->toJson());
                linearPersisted = reloadedStroke.isLinearPath()
                    && qFuzzyCompare(reloadedStroke.strokeWidth(), 1.0);
            }
            check(linearPersisted, QStringLiteral("折线线稿笔画可以存取还原"));

            const QImage rendered = lineArtScene.renderImage(QRectF(0.0, 0.0, 240.0, 160.0), 2.0);
            int inkPixels = 0;
            for (int y = 0; y < rendered.height(); ++y) {
                for (int x = 0; x < rendered.width(); ++x) {
                    if (rendered.pixelColor(x, y) != QColor(Qt::white))
                        ++inkPixels;
                }
            }
            check(inkPixels > 1000, QStringLiteral("线稿笔画在画布上可见"),
                  QStringLiteral("ink=%1").arg(inkPixels));

            const QString lineArtPath = QDir(workDirectory).filePath(
                QStringLiteral("lineart.png"));
            check(rendered.save(lineArtPath), QStringLiteral("线稿渲染结果可保存为 PNG"),
                  lineArtPath);
        }
        lineArtScene.setUndoStack(nullptr);

        // 纯色图片没有可描的色块，应当明确报告而不是生成空图元。
        QImage flat(64, 64, QImage::Format_RGB32);
        flat.fill(Qt::white);
        const LineArtConverter::Result flatResult = LineArtConverter::extract(flat);
        check(!flatResult.ok && !flatResult.error.isEmpty(),
              QStringLiteral("纯色图片不会生成空线稿"), flatResult.error);

        // 相邻且颜色接近的两块，应该在合并参数打开时并成一条轮廓；参数为 0 时各自一条。
        QImage twin(320, 200, QImage::Format_RGB32);
        twin.fill(Qt::white);
        {
            QPainter painter(&twin);
            painter.setPen(Qt::NoPen);
            painter.fillRect(QRect(40, 40, 120, 120), QColor(200, 120, 60));
            painter.fillRect(QRect(160, 40, 120, 120), QColor(212, 132, 72));
        }
        LineArtConverter::Options splitOptions;
        splitOptions.adjacentMergeDistance = 0.0;
        // 关掉颜色聚类，让两块近似色先各自成为独立色块，才能单独验证相邻合并这一步。
        splitOptions.paletteMergeDistance = 0.0;
        const LineArtConverter::Result splitResult =
            LineArtConverter::extract(twin, splitOptions);
        LineArtConverter::Options mergeOptions;
        mergeOptions.adjacentMergeDistance = 48.0;
        const LineArtConverter::Result mergeResult =
            LineArtConverter::extract(twin, mergeOptions);
        check(splitResult.ok && splitResult.outlines.size() == 2,
              QStringLiteral("关闭相邻合并时两块近似色各自成轮廓"),
              QStringLiteral("outlines=%1").arg(splitResult.outlines.size()));
        check(mergeResult.ok && mergeResult.outlines.size() == 1,
              QStringLiteral("相邻近似色块合并成一条轮廓"),
              QStringLiteral("outlines=%1 regions=%2")
                  .arg(mergeResult.outlines.size())
                  .arg(mergeResult.regionCount));
        if (mergeResult.outlines.size() == 1 && splitResult.outlines.size() == 2) {
            qreal splitSpan = 0.0;
            for (const LineArtConverter::Outline& outline : splitResult.outlines) {
                qreal maxX = 0.0;
                for (const QPointF& point : outline.points)
                    maxX = qMax(maxX, point.x());
                splitSpan += maxX;
            }
            qreal mergedMaxX = 0.0;
            for (const QPointF& point : mergeResult.outlines.constFirst().points)
                mergedMaxX = qMax(mergedMaxX, point.x());
            check(mergedMaxX > splitSpan / 2.0,
                  QStringLiteral("合并后的轮廓覆盖原来两个色块的位置"),
                  QStringLiteral("mergedMaxX=%1").arg(mergedMaxX));
        }

        // 颜色差得远的相邻色块不能被合并，否则真实边界会消失。
        QImage farTwin(320, 200, QImage::Format_RGB32);
        farTwin.fill(Qt::white);
        {
            QPainter painter(&farTwin);
            painter.setPen(Qt::NoPen);
            painter.fillRect(QRect(40, 40, 120, 120), QColor(200, 120, 60));
            painter.fillRect(QRect(160, 40, 120, 120), QColor(60, 90, 200));
        }
        const LineArtConverter::Result farResult =
            LineArtConverter::extract(farTwin, mergeOptions);
        check(farResult.ok && farResult.outlines.size() == 2,
              QStringLiteral("颜色差得远的相邻色块不会被合并"),
              QStringLiteral("outlines=%1").arg(farResult.outlines.size()));

        // 渐变底色的图：色块很多，结果必须有界，不能把画布塞满。
        QImage gradient(400, 300, QImage::Format_RGB32);
        {
            QPainter painter(&gradient);
            QLinearGradient back(0.0, 0.0, 0.0, 300.0);
            back.setColorAt(0.0, QColor(QStringLiteral("#1E293B")));
            back.setColorAt(1.0, QColor(QStringLiteral("#64748B")));
            painter.fillRect(gradient.rect(), back);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(Qt::white));
            painter.drawEllipse(QPointF(140.0, 140.0), 60.0, 60.0);
            painter.setBrush(QColor(QStringLiteral("#F59E0B")));
            painter.drawRoundedRect(QRectF(230.0, 90.0, 120.0, 90.0), 12.0, 12.0);
        }
        // 色块数量上限已放开：旧上限是 1200 块，超过的也要全部描出来。
        QImage manyBlocks(640, 480, QImage::Format_RGB32);
        manyBlocks.fill(Qt::white);
        {
            QPainter painter(&manyBlocks);
            painter.setPen(Qt::NoPen);
            const QColor palette[] = { QColor(220, 60, 60), QColor(60, 90, 200),
                                       QColor(40, 160, 90), QColor(210, 180, 50) };
            for (int row = 0; row < 30; ++row) {
                for (int col = 0; col < 40; ++col) {
                    painter.fillRect(QRect(col * 16 + 1, row * 16 + 1, 13, 13),
                                     palette[(row + col) % 4]);
                }
            }
        }
        const LineArtConverter::Result manyResult = LineArtConverter::extract(manyBlocks);
        check(manyResult.ok && !manyResult.truncated && manyResult.outlines.size() >= 1200,
              QStringLiteral("色块超过 1200 块时不再截断"),
              QStringLiteral("outlines=%1 regions=%2 truncated=%3")
                  .arg(manyResult.outlines.size())
                  .arg(manyResult.regionCount)
                  .arg(manyResult.truncated ? 1 : 0));

        const LineArtConverter::Result gradientResult = LineArtConverter::extract(gradient);
        check(gradientResult.ok && !gradientResult.truncated && !gradientResult.outlines.isEmpty(),
              QStringLiteral("渐变底色的图片线稿不再按数量截断"),
              QStringLiteral("outlines=%1 regions=%2 truncated=%3")
                  .arg(gradientResult.outlines.size())
                  .arg(gradientResult.regionCount)
                  .arg(gradientResult.truncated ? 1 : 0));
        {
            CanvasScene gradientScene;
            gradientScene.applyCanvasSize(QSizeF(400.0, 300.0));
            QList<CanvasItem*> gradientItems;
            for (const LineArtConverter::Outline& outline : gradientResult.outlines) {
                auto* stroke = new StrokeItem();
                stroke->setStrokePoints(outline.points, outline.color,
                                        LineArtConverter::Options().strokeWidth, 0.0, true);
                gradientItems.append(stroke);
            }
            gradientScene.addItemsWithUndo(gradientItems, QStringLiteral("导入图片线稿化"));
            const QImage rendered = gradientScene.renderImage(QRectF(0.0, 0.0, 400.0, 300.0), 1.5);
            rendered.save(QDir(workDirectory).filePath(QStringLiteral("lineart-gradient.png")));
        }
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

bool renderDemoCanvas(const QString& outputPath)
{
    CanvasScene scene;
    populateDemoScene(scene);

    const QImage image = scene.renderImage(scene.canvasRect(), 1.0);
    if (image.isNull())
        return false;
    return image.save(outputPath);
}

bool writeDemoCanvas(const QString& outputPath)
{
    CanvasScene scene;
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

void populateDemoScene(CanvasScene& scene)
{
    scene.applyCanvasSize(QSizeF(1600, 1000));

    BackgroundState background;
    background.type = BackgroundType::Color;
    background.color = QColor(QStringLiteral("#F7F3EA"));
    scene.applyBackground(background);

    const int frameLayer = scene.createLayer(QStringLiteral("分镜框"));
    const int shotLayer = scene.createLayer(QStringLiteral("镜头画面"));
    const int inkLayer = scene.createLayer(QStringLiteral("注释与标题"));
    scene.removeLayerWithUndo(CanvasScene::BackgroundLayerId + 1);
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

