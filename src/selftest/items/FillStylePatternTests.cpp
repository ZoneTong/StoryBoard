#include "items/CanvasTypes.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "items/shapes/ShapeFactory.h"
#include "scene/CanvasScene.h"
#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSizeF>


void selftest::RunFillStylePatternTests(Context& ctx)
{
    // 交叉线填充必须铺满整个图形：两个方向的斜线在左下角与右上角密度相当。
    CanvasScene hatchScene;
    hatchScene.ApplyCanvasSize(QSizeF(400, 300));
    auto* crossShape = ShapeFactory::Create(ShapeType::RECTANGLE);
    crossShape->setPos(QPointF(50, 45));
    crossShape->SetContentSize(QSizeF(300, 200));
    crossShape->SetStyle(QColor(QStringLiteral("#94A3B8")), QColor(QStringLiteral("#94A3B8")), 2.0);
    crossShape->SetFill(QColor(239, 68, 68, 255), FillStyle::CROSS_HATCH);
    hatchScene.AttachItem(crossShape);

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
    ctx.Check(hatchTopRight > 0 && hatchBottomLeft * 4 >= hatchTopRight * 3,
              QStringLiteral("交叉线填充均匀铺满整个图形，不只出现在右上角"),
              QStringLiteral("topRight=%1 bottomLeft=%2").arg(hatchTopRight).arg(hatchBottomLeft));

    // 填充样式扩展：反斜线、十字线、六边形、砖纹、点阵（横线 / 竖线两式已按需求删除）。
    ctx.Check(FillStyles::Name(FillStyle::REVERSE_HACHURE) == QStringLiteral("反斜线") &&
                  FillStyles::Name(FillStyle::GRID_HATCH) == QStringLiteral("十字线") &&
                  FillStyles::Name(FillStyle::HONEYCOMB) == QStringLiteral("六边形") &&
                  FillStyles::Name(FillStyle::BRICK) == QStringLiteral("砖纹") &&
                  FillStyles::Name(FillStyle::DOTS) == QStringLiteral("点阵"),
              QStringLiteral("填充样式名称齐全"));
    ctx.Check(AllFillStyles().size() == 9 && AllFillStyles().at(0) == FillStyle::NONE &&
                  AllFillStyles().at(1) == FillStyle::SOLID && AllFillStyles().at(2) == FillStyle::HACHURE &&
                  AllFillStyles().at(3) == FillStyle::REVERSE_HACHURE &&
                  AllFillStyles().at(4) == FillStyle::CROSS_HATCH && AllFillStyles().at(5) == FillStyle::GRID_HATCH &&
                  AllFillStyles().at(6) == FillStyle::HONEYCOMB && AllFillStyles().at(7) == FillStyle::BRICK &&
                  AllFillStyles().at(8) == FillStyle::DOTS,
              QStringLiteral("填充样式列表只剩 9 种且顺序稳定"),
              QStringLiteral("size=%1").arg(AllFillStyles().size()));
    ctx.Check(FillStyleFromString(FillStyleToString(FillStyle::REVERSE_HACHURE)) == FillStyle::REVERSE_HACHURE &&
                  FillStyleFromString(FillStyleToString(FillStyle::GRID_HATCH)) == FillStyle::GRID_HATCH &&
                  FillStyleFromString(FillStyleToString(FillStyle::HONEYCOMB)) == FillStyle::HONEYCOMB &&
                  FillStyleFromString(FillStyleToString(FillStyle::BRICK)) == FillStyle::BRICK &&
                  FillStyleFromString(FillStyleToString(FillStyle::DOTS)) == FillStyle::DOTS &&
                  FillStyleFromString(QStringLiteral("brickwall")) == FillStyle::BRICK &&
                  FillStyleFromString(QStringLiteral("dot")) == FillStyle::DOTS,
              QStringLiteral("填充样式可序列化往返"));
    // 旧画布里的横线 / 竖线串仍要能读，退化成斜线填充而不是掉回纯色。
    ctx.Check(FillStyleFromString(QStringLiteral("horizontal_lines")) == FillStyle::HACHURE &&
                  FillStyleFromString(QStringLiteral("vertical")) == FillStyle::HACHURE,
              QStringLiteral("已删除的横竖线样式按斜线填充读回"));

    auto renderPatternFill = [](FillStyle style, QImage& image) {
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(400, 300));
        auto* shape = ShapeFactory::Create(ShapeType::RECTANGLE);
        shape->setPos(QPointF(50, 45));
        shape->SetContentSize(QSizeF(300, 200));
        shape->SetStyle(QColor(QStringLiteral("#94A3B8")), QColor(QStringLiteral("#94A3B8")), 2.0);
        shape->SetFill(QColor(239, 68, 68, 255), style);
        scene.AttachItem(shape);
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
    renderPatternFill(FillStyle::REVERSE_HACHURE, reverseImage);
    QImage forwardImage;
    renderPatternFill(FillStyle::HACHURE, forwardImage);
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
    ctx.Check(reverseDownRight > reverseUpRight * 2 && forwardUpRight > forwardDownRight * 2,
              QStringLiteral("反斜线填充方向与斜线相反"),
              QStringLiteral("反斜线 down=%1 up=%2，斜线 down=%3 up=%4")
                  .arg(reverseDownRight)
                  .arg(reverseUpRight)
                  .arg(forwardDownRight)
                  .arg(forwardUpRight));

    // 正交叉线：水平线与竖直线同时存在（行/列方向都能扫到大量线条像素）。
    QImage gridImage;
    renderPatternFill(FillStyle::GRID_HATCH, gridImage);
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
    ctx.Check(gridRowRun >= 15 && gridColumnRun >= 12, QStringLiteral("正交叉线填充同时画出水平线与竖直线"),
              QStringLiteral("rowRun=%1 columnRun=%2").arg(gridRowRun).arg(gridColumnRun));

    // 六边形：单元格中心留空、六条边有线条。
    // 间距 = 高度 300 的 3%（上限 12）= 9，半径 = 1.15×间距 = 10.35，
    // 单元宽 √3r ≈ 17.93；第 3 行单元中心 (17.93, 20.7)，右侧竖边在 x≈26.89（局部坐标）。
    QImage honeycombImage;
    renderPatternFill(FillStyle::HONEYCOMB, honeycombImage);
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
    ctx.Check(honeycombCellEmpty && honeycombEdgeDrawn && honeycombRatio > 0.1 && honeycombRatio < 0.55,
              QStringLiteral("六边形填充画出蜂窝网格，格子内部留空"),
              QStringLiteral("cellEmpty=%1 edge=%2 ratio=%3")
                  .arg(honeycombCellEmpty ? 1 : 0)
                  .arg(honeycombEdgeDrawn ? 1 : 0)
                  .arg(honeycombRatio, 0, 'f', 3));

    // 砖纹：有横线也有竖缝，竖缝在行间出现（比纯水平线多出竖直方向像素）。
    QImage brickImage;
    renderPatternFill(FillStyle::BRICK, brickImage);
    int brickPixels = 0;
    for (int y = 55; y < 240; ++y) {
        for (int x = 55; x < 345; ++x) {
            if (patternLinePixel(brickImage, x, y)) {
                ++brickPixels;
            }
        }
    }
    ctx.Check(brickPixels > 0, QStringLiteral("砖纹填充画出砖块网格"));

    // 点阵：50% 圆点，点像素占比小且中心离散（取样窗口里既有像素也有空白）。
    QImage dotsImage;
    renderPatternFill(FillStyle::DOTS, dotsImage);
    int dotPixels = 0;
    for (int y = 55; y < 240; ++y) {
        for (int x = 55; x < 345; ++x) {
            if (patternLinePixel(dotsImage, x, y)) {
                ++dotPixels;
            }
        }
    }
    const double dotsRatio = static_cast<double>(dotPixels) / (290.0 * 185.0);
    ctx.Check(dotPixels > 100 && dotsRatio > 0.1 && dotsRatio < 0.6, QStringLiteral("点阵填充画出离散圆点"),
              QStringLiteral("pixels=%1 ratio=%2").arg(dotPixels).arg(dotsRatio, 0, 'f', 3));
}
