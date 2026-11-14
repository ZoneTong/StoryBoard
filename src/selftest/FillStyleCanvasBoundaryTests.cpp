#include "Alignment.h"
#include "BoolOps.h"
#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CanvasView.h"
#include "Commands.h"
#include "CompositeShapeItem.h"
#include "CustomShapeTemplates.h"
#include "ExportDialog.h"
#include "I18n.h"
#include "ImageItem.h"
#include "ItemDockPanel.h"
#include "ItemPropertiesDialog.h"
#include "LayerPanel.h"
#include "LineArtConverter.h"
#include "LineArtDialog.h"
#include "LineArtPresets.h"
#include "MainWindow.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "ShapeItem.h"
#include "StrokeItem.h"
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


void selftest::runFillStyleCanvasBoundaryTests(Context& ctx)
{
    // 需求 10/11/12/13/14 回归：锁定、椭圆、填充样式、画板尺寸。
    // ---------------------------------------------------------------
    {
        CanvasScene reqScene;
        reqScene.setMode(ToolMode::Edit);
        reqScene.setShowGrid(false);
        QUndoStack reqUndo;
        reqScene.setUndoStack(&reqUndo);

        // 需求 11：只保留椭圆，插入即标准圆形，可拉伸。
        ctx.check(shapeTypeName(ShapeType::Ellipse) == QStringLiteral("椭圆"), QStringLiteral("图形名称统一为椭圆"));
        const QSizeF ellipseDefault = defaultShapeSize(ShapeType::Ellipse);
        ctx.check(qFuzzyCompare(ellipseDefault.width(), ellipseDefault.height()),
                  QStringLiteral("椭圆默认插入为标准圆形"));
        auto* circleItem = new ShapeItem(ShapeType::Ellipse);
        circleItem->setContentSize(QSizeF(200, 200));
        circleItem->setContentSize(QSizeF(260, 140));
        ctx.check(circleItem->contentSize() == QSizeF(260, 140), QStringLiteral("圆形可在编辑模式拉伸成椭圆"));
        delete circleItem;

        // 需求 14：填充样式（无/纯色/斜线/交叉线）。
        ctx.check(fillStyleName(FillStyle::None) == QStringLiteral("无填充") &&
                      fillStyleName(FillStyle::Hachure) == QStringLiteral("斜线"),
                  QStringLiteral("填充样式名称齐全"));
        ctx.check(fillStyleFromString(QStringLiteral("cross_hatch")) == FillStyle::CrossHatch,
                  QStringLiteral("交叉线填充样式可反序列化"));

        auto* filledShape = new ShapeItem(ShapeType::Rectangle);
        filledShape->setContentSize(QSizeF(120, 80));
        filledShape->setPos(QPointF(30, 30));
        filledShape->setFill(QColor(59, 130, 246, 160), FillStyle::Hachure);
        ctx.check(filledShape->fillStyle() == FillStyle::Hachure && filledShape->fillColor().alpha() == 160,
                  QStringLiteral("图形可设置填充颜色与斜线填充"));
        reqScene.attachItem(filledShape);

        // 需求 23：箭头内部空心，可参与填充；只有纯色填充才整体遮挡下层。
        ctx.check(shapeTypeIsFilled(ShapeType::Arrow) && shapeTypeIsFilled(ShapeType::DoubleArrow) &&
                      !shapeTypeIsFilled(ShapeType::Line),
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

        ctx.check(!hollowArrow->paintedShape().contains(QPointF(70, 50)),
                  QStringLiteral("无填充箭头内部为空心，不遮挡下层"));

        QImage hollowArrowImage(400, 200, QImage::Format_ARGB32_Premultiplied);
        hollowArrowImage.fill(Qt::white);
        QPainter hollowArrowPainter(&hollowArrowImage);
        arrowScene.render(&hollowArrowPainter, QRectF(0, 0, 400, 200), QRectF(0, 0, 400, 200));
        hollowArrowPainter.end();
        const QColor hollowPixel = hollowArrowImage.pixelColor(110, 100);
        ctx.check(hollowPixel.red() > 245 && hollowPixel.green() > 245 && hollowPixel.blue() > 245,
                  QStringLiteral("无填充箭头内部渲染为空心"), hollowPixel.name());

        hollowArrow->setFill(QColor(QStringLiteral("#2563EB")), FillStyle::Solid);
        ctx.check(hollowArrow->paintedShape().contains(QPointF(70, 50)),
                  QStringLiteral("纯色填充箭头内部整体遮挡下层"));
        QImage filledArrowImage(400, 200, QImage::Format_ARGB32_Premultiplied);
        filledArrowImage.fill(Qt::white);
        QPainter filledArrowPainter(&filledArrowImage);
        arrowScene.render(&filledArrowPainter, QRectF(0, 0, 400, 200), QRectF(0, 0, 400, 200));
        filledArrowPainter.end();
        const QColor filledPixel = filledArrowImage.pixelColor(110, 100);
        ctx.check(filledPixel.blue() > 200 && filledPixel.red() < 80, QStringLiteral("纯色填充箭头内部渲染为填充色"),
                  filledPixel.name());

        const QJsonObject filledRoot = reqScene.toJson();
        CanvasScene filledLoaded;
        QString filledError;
        ctx.check(filledLoaded.fromJson(filledRoot, &filledError), QStringLiteral("填充样式画布反序列化"), filledError);
        ShapeItem* loadedShape = nullptr;
        for (CanvasItem* item : filledLoaded.canvasItems()) {
            if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
                loadedShape = shape;
                break;
            }
        }
        ctx.check(loadedShape && loadedShape->fillStyle() == FillStyle::Hachure &&
                      loadedShape->fillColor().alpha() == 160,
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
        styleUndo.push(new ShapeStyleCommand(&styleScene, {styleShape}, {styleBefore}, {styleAfter},
                                             QStringLiteral("调整填充样式")));
        ctx.check(styleShape->fillStyle() == FillStyle::CrossHatch, QStringLiteral("填充样式命令立即生效"));
        styleUndo.undo();
        ctx.check(styleShape->fillStyle() == styleBefore.fillStyle && styleShape->fillColor() == styleBefore.fillColor,
                  QStringLiteral("填充样式可撤销"));
        styleUndo.redo();
        ctx.check(styleShape->fillStyle() == FillStyle::CrossHatch, QStringLiteral("填充样式可重做"));

        // 交叉线填充必须铺满整个图形：两个方向的斜线在左下角与右上角密度相当。
        CanvasScene hatchScene;
        hatchScene.applyCanvasSize(QSizeF(400, 300));
        auto* crossShape = new ShapeItem(ShapeType::Rectangle);
        crossShape->setPos(QPointF(50, 45));
        crossShape->setContentSize(QSizeF(300, 200));
        crossShape->setStyle(QColor(QStringLiteral("#94A3B8")), QColor(QStringLiteral("#94A3B8")), 2.0);
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
                    if (pixel.red() - pixel.green() > 25 && pixel.red() - pixel.blue() > 25) {
                        ++count;
                    }
                }
            }
            return count;
        };
        const int hatchTopRight = hatchPixels(QRect(270, 60, 60, 60));
        const int hatchBottomLeft = hatchPixels(QRect(60, 180, 60, 60));
        ctx.check(hatchTopRight > 0 && hatchBottomLeft * 4 >= hatchTopRight * 3,
                  QStringLiteral("交叉线填充均匀铺满整个图形，不只出现在右上角"),
                  QStringLiteral("topRight=%1 bottomLeft=%2").arg(hatchTopRight).arg(hatchBottomLeft));

        // 填充样式扩展：反斜线、正交叉线、六边形。
        ctx.check(fillStyleName(FillStyle::ReverseHachure) == QStringLiteral("反斜线") &&
                      fillStyleName(FillStyle::GridHatch) == QStringLiteral("正交叉线") &&
                      fillStyleName(FillStyle::Honeycomb) == QStringLiteral("六边形"),
                  QStringLiteral("新增填充样式名称齐全"));
        ctx.check(allFillStyles().size() == 7 && allFillStyles().at(0) == FillStyle::None &&
                      allFillStyles().at(1) == FillStyle::Solid && allFillStyles().at(2) == FillStyle::Hachure &&
                      allFillStyles().at(3) == FillStyle::ReverseHachure &&
                      allFillStyles().at(4) == FillStyle::CrossHatch && allFillStyles().at(5) == FillStyle::GridHatch &&
                      allFillStyles().at(6) == FillStyle::Honeycomb,
                  QStringLiteral("填充样式列表包含全部 7 种且顺序稳定"));
        ctx.check(fillStyleFromString(fillStyleToString(FillStyle::ReverseHachure)) == FillStyle::ReverseHachure &&
                      fillStyleFromString(fillStyleToString(FillStyle::GridHatch)) == FillStyle::GridHatch &&
                      fillStyleFromString(fillStyleToString(FillStyle::Honeycomb)) == FillStyle::Honeycomb,
                  QStringLiteral("新增填充样式可序列化往返"));

        auto renderPatternFill = [](FillStyle style, QImage& image) {
            CanvasScene scene;
            scene.applyCanvasSize(QSizeF(400, 300));
            auto* shape = new ShapeItem(ShapeType::Rectangle);
            shape->setPos(QPointF(50, 45));
            shape->setContentSize(QSizeF(300, 200));
            shape->setStyle(QColor(QStringLiteral("#94A3B8")), QColor(QStringLiteral("#94A3B8")), 2.0);
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
                    if (patternLinePixel(reverseImage, x + 2, y + 2)) {
                        ++reverseDownRight;
                    }
                    if (patternLinePixel(reverseImage, x + 2, y - 2)) {
                        ++reverseUpRight;
                    }
                }
                if (patternLinePixel(forwardImage, x, y)) {
                    if (patternLinePixel(forwardImage, x + 2, y + 2)) {
                        ++forwardDownRight;
                    }
                    if (patternLinePixel(forwardImage, x + 2, y - 2)) {
                        ++forwardUpRight;
                    }
                }
            }
        }
        ctx.check(reverseDownRight > reverseUpRight * 2 && forwardUpRight > forwardDownRight * 2,
                  QStringLiteral("反斜线填充方向与斜线相反"),
                  QStringLiteral("反斜线 down=%1 up=%2，斜线 down=%3 up=%4")
                      .arg(reverseDownRight)
                      .arg(reverseUpRight)
                      .arg(forwardDownRight)
                      .arg(forwardUpRight));

        // 正交叉线：水平线与竖直线同时存在（行/列方向都能扫到大量线条像素）。
        QImage gridImage;
        renderPatternFill(FillStyle::GridHatch, gridImage);
        int gridRowRun = 0;
        for (int x = 60; x < 340; ++x) {
            if (patternLinePixel(gridImage, x, 145)) {
                ++gridRowRun;
            }
        }
        int gridColumnRun = 0;
        for (int y = 55; y < 240; ++y) {
            if (patternLinePixel(gridImage, 200, y)) {
                ++gridColumnRun;
            }
        }
        ctx.check(gridRowRun >= 15 && gridColumnRun >= 12, QStringLiteral("正交叉线填充同时画出水平线与竖直线"),
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
                if (patternLinePixel(honeycombImage, x, y)) {
                    ++honeycombPixels;
                }
            }
        }
        const double honeycombRatio = static_cast<double>(honeycombPixels) / (290.0 * 185.0);
        ctx.check(honeycombCellEmpty && honeycombEdgeDrawn && honeycombRatio > 0.1 && honeycombRatio < 0.55,
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
        ctx.check(overlapPixel.green() > 60 && overlapPixel.blue() > 150,
                  QStringLiteral("遮挡处完全不透，只显示上层颜色与背景"),
                  QStringLiteral("overlap=%1,%2,%3")
                      .arg(overlapPixel.red())
                      .arg(overlapPixel.green())
                      .arg(overlapPixel.blue()));
        ctx.check(lowerOnlyPixel.red() > 200 && lowerOnlyPixel.green() < 80,
                  QStringLiteral("未被遮挡的部分仍显示下层颜色"),
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
            ctx.check(hollowPixel.red() > 200 && hollowPixel.green() < 120,
                      QStringLiteral("无填充图形的内部不遮挡下层，下层颜色照常可见"),
                      QStringLiteral("pixel=%1,%2,%3")
                          .arg(hollowPixel.red())
                          .arg(hollowPixel.green())
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
        const QPoint rightEdge =
            canvasView.mapFromScene(QPointF(canvasBeforeDrag.right(), canvasBeforeDrag.center().y()));
        sendMouseEvent(canvasView.viewport(), QEvent::MouseButtonPress, rightEdge, Qt::LeftButton, Qt::LeftButton);
        sendMouseEvent(canvasView.viewport(), QEvent::MouseMove, rightEdge + QPoint(12, 0), Qt::NoButton,
                       Qt::LeftButton);
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
        ctx.check(canvasLoaded.fromJson(canvasRoot, &canvasError) &&
                      canvasLoaded.canvasRect() == canvasScene.canvasRect(),
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
}
