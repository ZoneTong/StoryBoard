#include "Alignment.h"
#include "BoolOps.h"
#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CanvasView.h"
#include "Commands.h"
#include "CurveItem.h"
#include "CustomShapeTemplates.h"
#include "ExportDialog.h"
#include "I18n.h"
#include "ItemDockPanel.h"
#include "ItemPropertiesDialog.h"
#include "LayerPanel.h"
#include "LineArtConverter.h"
#include "LineArtDialog.h"
#include "LineArtPresets.h"
#include "MainWindow.h"
#include "PolygonItem.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "ShapeItem.h"
#include "TextItem.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"
#include "selftest/SelftestHelpers.h"

#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>
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
#include <QListWidgetItem>
#include <QMarginsF>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPolygonF>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSize>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>
#include <QWheelEvent>
#include <QWidget>
#include <QWidgetAction>
#include <QtMath>
#include <cstdlib>


void selftest::runLineArtConverterTests(Context& ctx)
{
    // 需求：导入图片线稿化 —— 图片里的各色块轮廓要描成曲线图元，线宽固定为 1。
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
    ctx.check(traced.ok, QStringLiteral("图片线稿化提取成功"), traced.error);
    ctx.check(traced.outlines.size() >= 3 && traced.outlines.size() <= 8,
              QStringLiteral("线稿化把每一块颜色描成一条轮廓"),
              QStringLiteral("outlines=%1 regions=%2").arg(traced.outlines.size()).arg(traced.regionCount));

    bool closedLoops = !traced.outlines.isEmpty();
    bool insideImage = closedLoops;
    bool redBlockFound = false;
    for (const LineArtConverter::Outline& outline : traced.outlines) {
        if (outline.points.size() < 4 || outline.points.constFirst() != outline.points.constLast()) {
            closedLoops = false;
        }
        for (const QPointF& point : outline.points) {
            if (point.x() < -1.0 || point.y() < -1.0 || point.x() > artwork.width() + 1.0 ||
                point.y() > artwork.height() + 1.0) {
                insideImage = false;
            }
        }
        if (outline.color.red() > 200 && outline.color.green() < 130 && outline.color.blue() < 130) {
            redBlockFound = true;
        }
    }
    ctx.check(closedLoops, QStringLiteral("线稿轮廓都是闭合回路"));
    ctx.check(insideImage, QStringLiteral("线稿轮廓落在图片范围内"));
    ctx.check(redBlockFound, QStringLiteral("线稿曲线颜色取自对应色块"));

    CanvasScene lineArtScene;
    QUndoStack lineArtUndo;
    lineArtUndo.setUndoLimit(30);
    lineArtScene.setUndoStack(&lineArtUndo);
    lineArtScene.applyCanvasSize(QSizeF(240.0, 160.0));
    QList<CanvasItem*> lineArtItems;
    for (const LineArtConverter::Outline& outline : traced.outlines) {
        auto* stroke = new CurveItem();
        stroke->setStrokePoints(outline.points, outline.color, LineArtConverter::Options().strokeWidth, 0.0, true);
        lineArtItems.append(stroke);
    }
    ctx.check(!lineArtItems.isEmpty(), QStringLiteral("线稿轮廓生成曲线图元"));
    if (!lineArtItems.isEmpty()) {
        lineArtScene.addItemsWithUndo(lineArtItems, QStringLiteral("导入图片线稿化"));
        ctx.check(lineArtScene.canvasItemCount() == lineArtItems.size(), QStringLiteral("线稿曲线加入画布"));
        ctx.check(lineArtUndo.count() == 1, QStringLiteral("线稿化整体只占一条撤销记录"));

        int thinStrokes = 0;
        for (CanvasItem* item : lineArtItems) {
            if (auto* stroke = dynamic_cast<CurveItem*>(item); stroke && qFuzzyCompare(stroke->strokeWidth(), 1.0)) {
                ++thinStrokes;
            }
        }
        ctx.check(thinStrokes == lineArtItems.size(), QStringLiteral("线稿曲线线宽都是 1"),
                  QStringLiteral("thin=%1 total=%2").arg(thinStrokes).arg(lineArtItems.size()));

        int linearStrokes = 0;
        for (CanvasItem* item : lineArtItems) {
            if (auto* stroke = dynamic_cast<CurveItem*>(item); stroke && stroke->isLinearPath()) {
                ++linearStrokes;
            }
        }
        ctx.check(linearStrokes == lineArtItems.size(), QStringLiteral("线稿曲线按折线绘制，方角不会被磨圆"));

        bool linearPersisted = false;
        if (auto* firstStroke = dynamic_cast<CurveItem*>(lineArtItems.constFirst())) {
            CurveItem reloadedStroke;
            reloadedStroke.fromJson(firstStroke->toJson());
            linearPersisted = reloadedStroke.isLinearPath() && qFuzzyCompare(reloadedStroke.strokeWidth(), 1.0);
        }
        ctx.check(linearPersisted, QStringLiteral("折线线稿曲线可以存取还原"));

        const QImage rendered = lineArtScene.renderImage(QRectF(0.0, 0.0, 240.0, 160.0), 2.0);
        int inkPixels = 0;
        for (int y = 0; y < rendered.height(); ++y) {
            for (int x = 0; x < rendered.width(); ++x) {
                if (rendered.pixelColor(x, y).alpha() > 0) {
                    ++inkPixels;
                }
            }
        }
        ctx.check(inkPixels > 1000, QStringLiteral("线稿曲线在画布上可见"), QStringLiteral("ink=%1").arg(inkPixels));

        const QString lineArtPath = QDir(ctx.workDirectory).filePath(QStringLiteral("lineart.png"));
        ctx.check(rendered.save(lineArtPath), QStringLiteral("线稿渲染结果可保存为 PNG"), lineArtPath);
    }
    lineArtScene.setUndoStack(nullptr);

    // 纯色图片没有可描的色块，应当明确报告而不是生成空图元。
    QImage flat(64, 64, QImage::Format_RGB32);
    flat.fill(Qt::white);
    const LineArtConverter::Result flatResult = LineArtConverter::extract(flat);
    ctx.check(!flatResult.ok && !flatResult.error.isEmpty(), QStringLiteral("纯色图片不会生成空线稿"),
              flatResult.error);

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
    const LineArtConverter::Result splitResult = LineArtConverter::extract(twin, splitOptions);
    LineArtConverter::Options mergeOptions;
    mergeOptions.adjacentMergeDistance = 48.0;
    const LineArtConverter::Result mergeResult = LineArtConverter::extract(twin, mergeOptions);
    ctx.check(splitResult.ok && splitResult.outlines.size() == 2, QStringLiteral("关闭相邻合并时两块近似色各自成轮廓"),
              QStringLiteral("outlines=%1").arg(splitResult.outlines.size()));
    ctx.check(mergeResult.ok && mergeResult.outlines.size() == 1, QStringLiteral("相邻近似色块合并成一条轮廓"),
              QStringLiteral("outlines=%1 regions=%2").arg(mergeResult.outlines.size()).arg(mergeResult.regionCount));
    if (mergeResult.outlines.size() == 1 && splitResult.outlines.size() == 2) {
        qreal splitSpan = 0.0;
        for (const LineArtConverter::Outline& outline : splitResult.outlines) {
            qreal maxX = 0.0;
            for (const QPointF& point : outline.points) {
                maxX = qMax(maxX, point.x());
            }
            splitSpan += maxX;
        }
        qreal mergedMaxX = 0.0;
        for (const QPointF& point : mergeResult.outlines.constFirst().points) {
            mergedMaxX = qMax(mergedMaxX, point.x());
        }
        ctx.check(mergedMaxX > splitSpan / 2.0, QStringLiteral("合并后的轮廓覆盖原来两个色块的位置"),
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
    const LineArtConverter::Result farResult = LineArtConverter::extract(farTwin, mergeOptions);
    ctx.check(farResult.ok && farResult.outlines.size() == 2, QStringLiteral("颜色差得远的相邻色块不会被合并"),
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
        const QColor palette[] = {QColor(220, 60, 60), QColor(60, 90, 200), QColor(40, 160, 90), QColor(210, 180, 50)};
        for (int row = 0; row < 30; ++row) {
            for (int col = 0; col < 40; ++col) {
                painter.fillRect(QRect(col * 16 + 1, row * 16 + 1, 13, 13), palette[(row + col) % 4]);
            }
        }
    }
    const LineArtConverter::Result manyResult = LineArtConverter::extract(manyBlocks);
    ctx.check(manyResult.ok && !manyResult.truncated && manyResult.outlines.size() >= 1200,
              QStringLiteral("色块超过 1200 块时不再截断"),
              QStringLiteral("outlines=%1 regions=%2 truncated=%3")
                  .arg(manyResult.outlines.size())
                  .arg(manyResult.regionCount)
                  .arg(manyResult.truncated ? 1 : 0));

    const LineArtConverter::Result gradientResult = LineArtConverter::extract(gradient);
    ctx.check(gradientResult.ok && !gradientResult.truncated && !gradientResult.outlines.isEmpty(),
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
            auto* stroke = new CurveItem();
            stroke->setStrokePoints(outline.points, outline.color, LineArtConverter::Options().strokeWidth, 0.0, true);
            gradientItems.append(stroke);
        }
        gradientScene.addItemsWithUndo(gradientItems, QStringLiteral("导入图片线稿化"));
        const QImage rendered = gradientScene.renderImage(QRectF(0.0, 0.0, 400.0, 300.0), 1.5);
        rendered.save(QDir(ctx.workDirectory).filePath(QStringLiteral("lineart-gradient.png")));
    }
}
