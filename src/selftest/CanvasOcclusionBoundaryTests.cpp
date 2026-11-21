#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CanvasView.h"
#include "ShapeItem.h"
#include "TextItem.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"
#include "selftest/SelftestHelpers.h"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QImage>
#include <QJsonObject>
#include <QPainter>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QUndoStack>


void selftest::RunCanvasOcclusionBoundaryTests(Context& ctx)
{
    // 遮挡必须完全不透：上层是半透明颜色时，重叠处只能看到“上层颜色 + 背景”，
    // 不能把下层颜色混进来。
    CanvasScene blendScene;
    blendScene.ApplyCanvasSize(QSizeF(300, 200));
    blendScene.SetShowGrid(false);
    // 真实视图截图路径与离屏 render 路径对齐：显式铺白底，避免默认透明衬底参与取色。
    CanvasBackground blendBackground;
    blendBackground.type = BackgroundType::COLOR;
    blendBackground.color = QColor(Qt::white);
    blendScene.SetCanvasBackground(blendBackground);
    auto* lowerBand = CreateShapeItem(ShapeType::RECTANGLE);
    lowerBand->setPos(QPointF(20, 20));
    lowerBand->SetContentSize(QSizeF(160, 120));
    lowerBand->SetStyle(QColor(QStringLiteral("#94A3B8")), QColor(QStringLiteral("#94A3B8")), 1.0);
    lowerBand->SetFill(QColor(255, 0, 0, 255), FillStyle::SOLID);
    blendScene.AttachItem(lowerBand);

    auto* upperBand = CreateShapeItem(ShapeType::RECTANGLE);
    upperBand->setPos(QPointF(100, 50));
    upperBand->SetContentSize(QSizeF(160, 120));
    upperBand->SetStyle(QColor(QStringLiteral("#94A3B8")), QColor(QStringLiteral("#94A3B8")), 1.0);
    upperBand->SetFill(QColor(0, 0, 255, 128), FillStyle::SOLID);
    blendScene.AttachItem(upperBand);

    QImage blendImage(300, 200, QImage::Format_ARGB32_Premultiplied);
    blendImage.fill(Qt::white);
    QPainter blendPainter(&blendImage);
    blendScene.render(&blendPainter, QRectF(0, 0, 300, 200), QRectF(0, 0, 300, 200));
    blendPainter.end();

    const QColor overlapPixel = blendImage.pixelColor(140, 90);
    const QColor lowerOnlyPixel = blendImage.pixelColor(60, 60);
    ctx.Check(
        overlapPixel.green() > 60 && overlapPixel.blue() > 150, QStringLiteral("遮挡处完全不透，只显示上层颜色与背景"),
        QStringLiteral("overlap=%1,%2,%3").arg(overlapPixel.red()).arg(overlapPixel.green()).arg(overlapPixel.blue()));
    ctx.Check(lowerOnlyPixel.red() > 200 && lowerOnlyPixel.green() < 80, QStringLiteral("未被遮挡的部分仍显示下层颜色"),
              QStringLiteral("lower=%1,%2,%3")
                  .arg(lowerOnlyPixel.red())
                  .arg(lowerOnlyPixel.green())
                  .arg(lowerOnlyPixel.blue()));

    // 文字只画字形，不能把下层填充按字形挖空：低缩放或亚像素偏移时，
    // 挖空区域会从黑字边缘露出白色背景，形成错位白色文字重影。
    {
        CanvasScene textScene;
        textScene.ApplyCanvasSize(QSizeF(300, 240));
        textScene.SetShowGrid(false);
        CanvasBackground textBackground;
        textBackground.type = BackgroundType::COLOR;
        textBackground.color = QColor(Qt::white);
        textScene.SetCanvasBackground(textBackground);

        auto* rectangle = CreateShapeItem(ShapeType::RECTANGLE);
        rectangle->setPos(QPointF(30, 30));
        rectangle->SetContentSize(QSizeF(200, 150));
        rectangle->SetStyle(QColor(QStringLiteral("#1E293B")), QColor(QStringLiteral("#1E293B")), 2.0);
        rectangle->SetFill(QColor(QStringLiteral("#FFF59D")), FillStyle::SOLID);
        textScene.AttachItem(rectangle);

        auto* text = new TextItem;
        text->SetTextFormat(QStringLiteral("文字"), QFont(QStringLiteral("Microsoft YaHei"), 24), QColor(Qt::black));
        text->setPos(QPointF(100, 100));
        textScene.AttachItem(text);

        QImage textImage(300, 240, QImage::Format_ARGB32_Premultiplied);
        textImage.fill(Qt::white);
        QPainter textPainter(&textImage);
        textScene.render(&textPainter, QRectF(0, 0, 300, 240), QRectF(0, 0, 300, 240));
        textPainter.end();

        int whiteHoles = 0;
        int yellowFill = 0;
        for (int y = 50; y < 160; ++y) {
            for (int x = 50; x < 210; ++x) {
                const QColor pixel = textImage.pixelColor(x, y);
                if (pixel.red() > 245 && pixel.green() > 245 && pixel.blue() > 245) {
                    ++whiteHoles;
                }
                if (pixel.red() > 230 && pixel.green() > 220 && pixel.blue() < 190) {
                    ++yellowFill;
                }
            }
        }
        ctx.Check(whiteHoles == 0 && yellowFill > 1000,
                  QStringLiteral("文字覆盖在有填充矩形上时不在字形后留下白色孔洞"),
                  QStringLiteral("white=%1 yellow=%2").arg(whiteHoles).arg(yellowFill));
    }

    // 真实视图路径（QGraphicsView::paintEvent）也必须完全不透：默认视图走
    // QGraphicsScenePrivate::drawItems，绕过 CanvasScene::drawItems；只有打开
    // IndirectPainting 才会调用虚函数，这里用截图验证它确实生效。
    {
        CanvasView blendView(&blendScene);
        blendView.resize(460, 340);
        blendView.SetMode(ToolMode::VIEW);
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
        ctx.Check(viewOverlapColor.green() > 60 && viewOverlapColor.blue() > 150,
                  QStringLiteral("真实视图中的遮挡处同样完全不透"),
                  QStringLiteral("view=%1,%2,%3 point=%4,%5")
                      .arg(viewOverlapColor.red())
                      .arg(viewOverlapColor.green())
                      .arg(viewOverlapColor.blue())
                      .arg(viewPoint.x())
                      .arg(viewPoint.y()));
        ctx.Check(viewLowerColor.red() > 200 && viewLowerColor.green() < 80,
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
        hollowScene.ApplyCanvasSize(QSizeF(300, 200));
        hollowScene.SetShowGrid(false);
        auto* lower = CreateShapeItem(ShapeType::RECTANGLE);
        lower->setPos(QPointF(40, 40));
        lower->SetContentSize(QSizeF(220, 120));
        lower->SetStyle(QColor(QStringLiteral("#94A3B8")), QColor(QStringLiteral("#94A3B8")), 1.0);
        lower->SetFill(QColor(239, 68, 68), FillStyle::SOLID);
        hollowScene.AttachItem(lower);

        auto* hollow = CreateShapeItem(ShapeType::RECTANGLE);
        hollow->setPos(QPointF(80, 60));
        hollow->SetContentSize(QSizeF(140, 100));
        hollow->SetStyle(QColor(37, 99, 235), QColor(Qt::transparent), 2.0);
        hollow->SetFill(QColor(Qt::transparent), FillStyle::NONE);
        hollowScene.AttachItem(hollow);

        QImage hollowImage(300, 200, QImage::Format_ARGB32_Premultiplied);
        hollowImage.fill(Qt::white);
        QPainter hollowPainter(&hollowImage);
        hollowScene.render(&hollowPainter, QRectF(0, 0, 300, 200), QRectF(0, 0, 300, 200));
        hollowPainter.end();
        const QColor hollowPixel = hollowImage.pixelColor(150, 110);
        ctx.Check(
            hollowPixel.red() > 200 && hollowPixel.green() < 120,
            QStringLiteral("无填充图形的内部不遮挡下层，下层颜色照常可见"),
            QStringLiteral("pixel=%1,%2,%3").arg(hollowPixel.red()).arg(hollowPixel.green()).arg(hollowPixel.blue()));

        auto* pattern = CreateShapeItem(ShapeType::RECTANGLE);
        pattern->setPos(QPointF(80, 60));
        pattern->SetContentSize(QSizeF(140, 100));
        pattern->SetStyle(QColor(37, 99, 235), QColor(37, 99, 235, 200), 2.0);
        pattern->SetFill(QColor(37, 99, 235, 200), FillStyle::HONEYCOMB);

        CanvasScene patternScene;
        patternScene.ApplyCanvasSize(QSizeF(300, 200));
        patternScene.SetShowGrid(false);
        auto* patternLower = CreateShapeItem(ShapeType::RECTANGLE);
        patternLower->setPos(QPointF(40, 40));
        patternLower->SetContentSize(QSizeF(220, 120));
        patternLower->SetStyle(QColor(QStringLiteral("#94A3B8")), QColor(QStringLiteral("#94A3B8")), 1.0);
        patternLower->SetFill(QColor(239, 68, 68), FillStyle::SOLID);
        patternScene.AttachItem(patternLower);
        patternScene.AttachItem(pattern);

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
        ctx.Check(lowerVisible > 0 && patternLines > 0,
                  QStringLiteral("纹理填充不遮挡下层：空隙里透出下层颜色，线条仍然画在上层"),
                  QStringLiteral("lowerVisible=%1 lines=%2").arg(lowerVisible).arg(patternLines));

        // 反过来：纯色填充（含半透明）压住纹理填充的下层时，下层纹理必须完全消失。
        CanvasScene solidOverTexture;
        solidOverTexture.ApplyCanvasSize(QSizeF(300, 200));
        solidOverTexture.SetShowGrid(false);
        auto* texturedLower = CreateShapeItem(ShapeType::RECTANGLE);
        texturedLower->setPos(QPointF(40, 40));
        texturedLower->SetContentSize(QSizeF(220, 120));
        texturedLower->SetStyle(QColor(239, 68, 68), QColor(239, 68, 68, 200), 1.0);
        texturedLower->SetFill(QColor(239, 68, 68, 200), FillStyle::HONEYCOMB);
        solidOverTexture.AttachItem(texturedLower);

        auto* solidCover = CreateShapeItem(ShapeType::RECTANGLE);
        solidCover->setPos(QPointF(80, 60));
        solidCover->SetContentSize(QSizeF(140, 100));
        solidCover->SetStyle(QColor(37, 99, 235), QColor(37, 99, 235, 128), 2.0);
        solidCover->SetFill(QColor(37, 99, 235, 128), FillStyle::SOLID);
        solidOverTexture.AttachItem(solidCover);

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
        ctx.Check(textureLeaked == 0 && solidPixels > 0, QStringLiteral("纯色填充也遮挡下层纹理，重叠处只剩纯色"),
                  QStringLiteral("leaked=%1 solid=%2").arg(textureLeaked).arg(solidPixels));
    }

    // 需求 13：画板通过拉伸边缘扩大或裁剪，并随画布保存原点。
    QUndoStack canvasUndo;
    CanvasScene canvasScene;
    canvasScene.SetUndoStack(&canvasUndo);
    const QRectF initialCanvas = canvasScene.CanvasRect();
    const QRectF grownCanvas = initialCanvas.adjusted(-240, -160, 320, 260);
    canvasScene.SetCanvasRectWithUndo(grownCanvas);
    ctx.Check(canvasScene.CanvasRect() == grownCanvas && canvasScene.CanvasOrigin() == grownCanvas.topLeft(),
              QStringLiteral("画板可向四边拉伸扩大"));
    const QRectF shrunkCanvas(grownCanvas.left() + 120, grownCanvas.top() + 90, grownCanvas.width() - 240,
                              grownCanvas.height() - 180);
    canvasScene.SetCanvasRectWithUndo(shrunkCanvas);
    ctx.Check(canvasScene.CanvasRect() == shrunkCanvas, QStringLiteral("画板可裁剪缩小"));
    canvasUndo.undo();
    ctx.Check(canvasScene.CanvasRect() == grownCanvas, QStringLiteral("画板尺寸调整可撤销"));

    CanvasView canvasView(&canvasScene);
    canvasView.resize(1200, 800);
    canvasView.SetMode(ToolMode::EDIT);
    canvasView.FitAll();
    QApplication::processEvents();
    const QRectF canvasBeforeDrag = canvasScene.CanvasRect();
    const int commandsBeforeDrag = canvasUndo.index();
    const QPoint rightEdge = canvasView.mapFromScene(QPointF(canvasBeforeDrag.right(), canvasBeforeDrag.center().y()));
    SendMouseEvent(canvasView.viewport(), QEvent::MouseButtonPress, rightEdge, Qt::LeftButton, Qt::LeftButton);
    SendMouseEvent(canvasView.viewport(), QEvent::MouseMove, rightEdge + QPoint(12, 0), Qt::NoButton, Qt::LeftButton);
    SendMouseEvent(canvasView.viewport(), QEvent::MouseButtonRelease, rightEdge + QPoint(12, 0), Qt::LeftButton,
                   Qt::NoButton);
    QApplication::processEvents();
    ctx.Check(canvasScene.CanvasRect().width() > canvasBeforeDrag.width() + 5.0 &&
                  qFuzzyCompare(canvasScene.CanvasRect().left(), canvasBeforeDrag.left()),
              QStringLiteral("拖动右边缘手柄可扩大画板"),
              QStringLiteral("before=%1x%2 after=%3x%4")
                  .arg(canvasBeforeDrag.width())
                  .arg(canvasBeforeDrag.height())
                  .arg(canvasScene.CanvasRect().width())
                  .arg(canvasScene.CanvasRect().height()));
    ctx.Check(canvasUndo.index() == commandsBeforeDrag + 1 && canvasUndo.count() == commandsBeforeDrag + 1,
              QStringLiteral("画板边缘拖动只生成一条撤销记录"),
              QStringLiteral("index before=%1 after=%2").arg(commandsBeforeDrag).arg(canvasUndo.index()));

    const QJsonObject canvasRoot = canvasScene.ToJson();
    CanvasScene canvasLoaded;
    QString canvasError;
    ctx.Check(canvasLoaded.FromJson(canvasRoot, &canvasError) && canvasLoaded.CanvasRect() == canvasScene.CanvasRect(),
              QStringLiteral("画板原点随画布保存"), canvasError);

    // 需求 10：锁定状态随画布保存。
    CanvasScene lockScene;
    lockScene.SetMode(ToolMode::EDIT);
    const int lockLayerId = lockScene.ActiveLayerId();
    ctx.Check(lockScene.SetLayerLocked(lockLayerId, true), QStringLiteral("锁定图层用于序列化测试"));
    const QJsonObject lockRoot = lockScene.ToJson();
    CanvasScene lockLoaded;
    QString lockError;
    ctx.Check(lockLoaded.FromJson(lockRoot, &lockError) && lockLoaded.IsLayerLocked(lockLoaded.ActiveLayerId()),
              QStringLiteral("图层锁定状态随画布保存"), lockError);
}
