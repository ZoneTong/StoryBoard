#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CanvasView.h"
#include "ShapeItem.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"
#include "selftest/SelftestHelpers.h"

#include <QApplication>
#include <QColor>
#include <QImage>
#include <QJsonObject>
#include <QPainter>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QUndoStack>


void selftest::runCanvasOcclusionBoundaryTests(Context& ctx)
{
    // 遮挡必须完全不透：上层是半透明颜色时，重叠处只能看到“上层颜色 + 背景”，
    // 不能把下层颜色混进来。
    CanvasScene blendScene;
    blendScene.applyCanvasSize(QSizeF(300, 200));
    blendScene.setShowGrid(false);
    // 真实视图截图路径与离屏 render 路径对齐：显式铺白底，避免默认透明衬底参与取色。
    BackgroundState blendBackground;
    blendBackground.type = BackgroundType::Color;
    blendBackground.color = QColor(Qt::white);
    blendScene.applyBackground(blendBackground);
    auto* lowerBand = new ShapeItem(ShapeType::Rectangle);
    lowerBand->setPos(QPointF(20, 20));
    lowerBand->setContentSize(QSizeF(160, 120));
    lowerBand->setStyle(QColor(QStringLiteral("#94A3B8")), QColor(QStringLiteral("#94A3B8")), 1.0);
    lowerBand->setFill(QColor(255, 0, 0, 255), FillStyle::Solid);
    blendScene.attachItem(lowerBand);

    auto* upperBand = new ShapeItem(ShapeType::Rectangle);
    upperBand->setPos(QPointF(100, 50));
    upperBand->setContentSize(QSizeF(160, 120));
    upperBand->setStyle(QColor(QStringLiteral("#94A3B8")), QColor(QStringLiteral("#94A3B8")), 1.0);
    upperBand->setFill(QColor(0, 0, 255, 128), FillStyle::Solid);
    blendScene.attachItem(upperBand);

    QImage blendImage(300, 200, QImage::Format_ARGB32_Premultiplied);
    blendImage.fill(Qt::white);
    QPainter blendPainter(&blendImage);
    blendScene.render(&blendPainter, QRectF(0, 0, 300, 200), QRectF(0, 0, 300, 200));
    blendPainter.end();

    const QColor overlapPixel = blendImage.pixelColor(140, 90);
    const QColor lowerOnlyPixel = blendImage.pixelColor(60, 60);
    ctx.check(
        overlapPixel.green() > 60 && overlapPixel.blue() > 150, QStringLiteral("遮挡处完全不透，只显示上层颜色与背景"),
        QStringLiteral("overlap=%1,%2,%3").arg(overlapPixel.red()).arg(overlapPixel.green()).arg(overlapPixel.blue()));
    ctx.check(lowerOnlyPixel.red() > 200 && lowerOnlyPixel.green() < 80, QStringLiteral("未被遮挡的部分仍显示下层颜色"),
              QStringLiteral("lower=%1,%2,%3")
                  .arg(lowerOnlyPixel.red())
                  .arg(lowerOnlyPixel.green())
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
        // offscreen 下首帧可能只画了图元还没铺画布底色，先显式重绘一次再截图。
        blendView.viewport()->repaint();
        QApplication::processEvents();

        const QImage viewImage = blendView.viewport()->grab().toImage();
        const QPoint viewPoint = blendView.mapFromScene(QPointF(140, 90));
        const QPoint viewLowerPoint = blendView.mapFromScene(QPointF(60, 60));
        QColor viewOverlapColor(0, 0, 0);
        QColor viewLowerColor(0, 0, 0);
        if (viewImage.rect().contains(viewPoint)) {
            viewOverlapColor = viewImage.pixelColor(viewPoint);
        }
        if (viewImage.rect().contains(viewLowerPoint)) {
            viewLowerColor = viewImage.pixelColor(viewLowerPoint);
        }
        ctx.check(viewOverlapColor.green() > 60 && viewOverlapColor.blue() > 150,
                  QStringLiteral("真实视图中的遮挡处同样完全不透"),
                  QStringLiteral("view=%1,%2,%3 point=%4,%5")
                      .arg(viewOverlapColor.red())
                      .arg(viewOverlapColor.green())
                      .arg(viewOverlapColor.blue())
                      .arg(viewPoint.x())
                      .arg(viewPoint.y()));
        ctx.check(viewLowerColor.red() > 200 && viewLowerColor.green() < 80,
                  QStringLiteral("真实视图中未被遮挡的部分仍显示下层颜色"),
                  QStringLiteral("viewLower=%1,%2,%3")
                      .arg(viewLowerColor.red())
                      .arg(viewLowerColor.green())
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
        lower->setStyle(QColor(QStringLiteral("#94A3B8")), QColor(QStringLiteral("#94A3B8")), 1.0);
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
        ctx.check(
            hollowPixel.red() > 200 && hollowPixel.green() < 120,
            QStringLiteral("无填充图形的内部不遮挡下层，下层颜色照常可见"),
            QStringLiteral("pixel=%1,%2,%3").arg(hollowPixel.red()).arg(hollowPixel.green()).arg(hollowPixel.blue()));

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
        patternLower->setStyle(QColor(QStringLiteral("#94A3B8")), QColor(QStringLiteral("#94A3B8")), 1.0);
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
                if (pixel.red() > 200 && pixel.green() < 120) {
                    ++lowerVisible;
                }
                if (pixel.blue() > 150 && pixel.red() < 150) {
                    ++patternLines;
                }
            }
        }
        ctx.check(lowerVisible > 0 && patternLines > 0,
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
                if (pixel.green() < 120) {
                    ++textureLeaked;
                }
                if (pixel.blue() > 200 && pixel.green() > 120) {
                    ++solidPixels;
                }
            }
        }
        ctx.check(textureLeaked == 0 && solidPixels > 0, QStringLiteral("纯色填充也遮挡下层纹理，重叠处只剩纯色"),
                  QStringLiteral("leaked=%1 solid=%2").arg(textureLeaked).arg(solidPixels));
    }

    // 需求 13：画板通过拉伸边缘扩大或裁剪，并随画布保存原点。
    QUndoStack canvasUndo;
    CanvasScene canvasScene;
    canvasScene.setUndoStack(&canvasUndo);
    const QRectF initialCanvas = canvasScene.canvasRect();
    const QRectF grownCanvas = initialCanvas.adjusted(-240, -160, 320, 260);
    canvasScene.setCanvasRectWithUndo(grownCanvas);
    ctx.check(canvasScene.canvasRect() == grownCanvas && canvasScene.canvasOrigin() == grownCanvas.topLeft(),
              QStringLiteral("画板可向四边拉伸扩大"));
    const QRectF shrunkCanvas(grownCanvas.left() + 120, grownCanvas.top() + 90, grownCanvas.width() - 240,
                              grownCanvas.height() - 180);
    canvasScene.setCanvasRectWithUndo(shrunkCanvas);
    ctx.check(canvasScene.canvasRect() == shrunkCanvas, QStringLiteral("画板可裁剪缩小"));
    canvasUndo.undo();
    ctx.check(canvasScene.canvasRect() == grownCanvas, QStringLiteral("画板尺寸调整可撤销"));

    CanvasView canvasView(&canvasScene);
    canvasView.resize(1200, 800);
    canvasView.setMode(ToolMode::Edit);
    canvasView.fitAll();
    QApplication::processEvents();
    const QRectF canvasBeforeDrag = canvasScene.canvasRect();
    const int commandsBeforeDrag = canvasUndo.index();
    const QPoint rightEdge = canvasView.mapFromScene(QPointF(canvasBeforeDrag.right(), canvasBeforeDrag.center().y()));
    sendMouseEvent(canvasView.viewport(), QEvent::MouseButtonPress, rightEdge, Qt::LeftButton, Qt::LeftButton);
    sendMouseEvent(canvasView.viewport(), QEvent::MouseMove, rightEdge + QPoint(12, 0), Qt::NoButton, Qt::LeftButton);
    sendMouseEvent(canvasView.viewport(), QEvent::MouseButtonRelease, rightEdge + QPoint(12, 0), Qt::LeftButton,
                   Qt::NoButton);
    QApplication::processEvents();
    ctx.check(canvasScene.canvasRect().width() > canvasBeforeDrag.width() + 5.0 &&
                  qFuzzyCompare(canvasScene.canvasRect().left(), canvasBeforeDrag.left()),
              QStringLiteral("拖动右边缘手柄可扩大画板"),
              QStringLiteral("before=%1x%2 after=%3x%4")
                  .arg(canvasBeforeDrag.width())
                  .arg(canvasBeforeDrag.height())
                  .arg(canvasScene.canvasRect().width())
                  .arg(canvasScene.canvasRect().height()));
    ctx.check(canvasUndo.index() == commandsBeforeDrag + 1 && canvasUndo.count() == commandsBeforeDrag + 1,
              QStringLiteral("画板边缘拖动只生成一条撤销记录"),
              QStringLiteral("index before=%1 after=%2").arg(commandsBeforeDrag).arg(canvasUndo.index()));

    const QJsonObject canvasRoot = canvasScene.toJson();
    CanvasScene canvasLoaded;
    QString canvasError;
    ctx.check(canvasLoaded.fromJson(canvasRoot, &canvasError) && canvasLoaded.canvasRect() == canvasScene.canvasRect(),
              QStringLiteral("画板原点随画布保存"), canvasError);

    // 需求 10：锁定状态随画布保存。
    CanvasScene lockScene;
    lockScene.setMode(ToolMode::Edit);
    const int lockLayerId = lockScene.activeLayerId();
    ctx.check(lockScene.setLayerLocked(lockLayerId, true), QStringLiteral("锁定图层用于序列化测试"));
    const QJsonObject lockRoot = lockScene.toJson();
    CanvasScene lockLoaded;
    QString lockError;
    ctx.check(lockLoaded.fromJson(lockRoot, &lockError) && lockLoaded.isLayerLocked(lockLoaded.activeLayerId()),
              QStringLiteral("图层锁定状态随画布保存"), lockError);
}
