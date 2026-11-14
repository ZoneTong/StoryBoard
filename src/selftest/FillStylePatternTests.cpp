#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "ShapeItem.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSizeF>


void selftest::runFillStylePatternTests(Context& ctx)
{
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
}
