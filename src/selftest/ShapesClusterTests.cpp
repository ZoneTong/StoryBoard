#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CompositeShapeItem.h"
#include "CurveItem.h"
#include "MainWindow.h"
#include "ShapeItem.h"
#include "ShapeMeta.h"
#include "ShapePathBuilder.h"
#include "SineItem.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"

#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QtMath>
#include <limits>

namespace {

// 波线中点必须落在中心线上：一个完整周期时 sin(π)=0；该断言能击穿「周期数没改干净」的回归。
// 轴线用采样点自身的峰谷中点定义（内容框含笔宽余量，不能当轴线基准）。
void checkSineWaveOnePeriod(selftest::Context& ctx)
{
    SineItem sine;
    sine.setWaveGeometry(QPointF(0.0, 0.0), QSizeF(128.0, 40.0), QColor(Qt::black), 2.0);
    const QVector<QPointF> points = sine.strokePoints();
    ctx.check(points.size() > 8, QStringLiteral("正弦波由采样点构成"),
              QStringLiteral("points=%1").arg(points.size()));
    qreal minY = std::numeric_limits<qreal>::max();
    qreal maxY = std::numeric_limits<qreal>::lowest();
    for (const QPointF& point : points) {
        minY = qMin(minY, point.y());
        maxY = qMax(maxY, point.y());
    }
    const qreal cy = (minY + maxY) / 2.0;
    const qreal amplitude = (maxY - minY) / 2.0;
    ctx.check(qAbs(amplitude - sine.params().amplitude) < 1.5,
              QStringLiteral("波峰到中心线的距离等于设定的像素振幅"),
              QStringLiteral("amp=%1 param=%2").arg(amplitude).arg(sine.params().amplitude));
    const QPointF mid = points.value(points.size() / 2, QPointF(0.0, cy));
    ctx.check(qAbs(mid.y() - cy) < amplitude * 0.15, QStringLiteral("正弦波为一个完整周期（中点回到中心线）"),
              QStringLiteral("mid=(%1,%2) cy=%3").arg(mid.x()).arg(mid.y()).arg(cy));
    const QPointF start = points.value(0, QPointF(0.0, cy));
    const QPointF end = points.value(points.size() - 1, QPointF(0.0, cy));
    ctx.check(qAbs(start.y() - cy) < amplitude * 0.15 && qAbs(end.y() - cy) < amplitude * 0.15,
              QStringLiteral("正弦波首尾都在中心线上"),
              QStringLiteral("start=(%1,%2) end=(%3,%4)").arg(start.x()).arg(start.y()).arg(end.x()).arg(end.y()));
    ctx.check(qAbs(end.x() - start.x() - 128.0) < 2.0, QStringLiteral("正弦波横向铺满波形框"),
              QStringLiteral("width=%1").arg(end.x() - start.x()));
}

// 需求 17：波形旗帜占满内容宽度（两棋盘格）且纵向重心居中。
void checkFlagWaveNoPole(selftest::Context& ctx)
{
    const QRectF rect(0.0, 0.0, 128.0, 86.0);
    const QPainterPath path = ShapePathBuilder::pathFor(ShapeType::FlagWave, rect, -1.0);
    const QRectF bounds = path.boundingRect();
    ctx.check(qAbs(bounds.width() - rect.width()) < 1.0, QStringLiteral("波形旗帜宽度占满内容（两棋盘格）"),
              QStringLiteral("width=%1").arg(bounds.width()));
    ctx.check(qAbs(bounds.center().y() - rect.center().y()) < rect.height() * 0.05,
              QStringLiteral("波形旗帜重心纵向居中"),
              QStringLiteral("center=%1 want=%2").arg(bounds.center().y()).arg(rect.center().y()));
    // 需求 22：选择框为最小包围盒——路径上下缘触及内容矩形上下缘。
    ctx.check(qAbs(bounds.top() - rect.top()) < 1.0 && qAbs(bounds.bottom() - rect.bottom()) < 1.0,
              QStringLiteral("波形旗帜包围盒为最小包围盒"),
              QStringLiteral("top=%1 bottom=%2").arg(bounds.top()).arg(bounds.bottom()));
}

void checkScrollShape(selftest::Context& ctx)
{
    ctx.check(ShapeMeta::typeToString(ShapeType::Scroll) == QStringLiteral("scroll"),
              QStringLiteral("卷轴序列化字符串为 scroll"));
    ctx.check(ShapeMeta::typeFromString(QStringLiteral("flag_curl")) == ShapeType::Scroll &&
                  ShapeMeta::typeFromString(QStringLiteral("banner")) == ShapeType::Scroll,
              QStringLiteral("旧文件的 flag_curl / banner 兼容读成卷轴"));
    const QRectF rect(0.0, 0.0, 128.0, 96.0);
    const QPainterPath path = ShapePathBuilder::pathFor(ShapeType::Scroll, rect, -1.0);
    ctx.check(path.intersects(QRectF(rect.center().x() - 2.0, rect.center().y() - 2.0, 4.0, 4.0)),
              QStringLiteral("卷轴中段有可填充纸面"));
    // 需求 22：卷筒纵向撑满内容高度，路径包围盒与内容矩形一致。
    const QRectF scrollBounds = path.boundingRect();
    ctx.check(qAbs(scrollBounds.top() - rect.top()) < 1.0 && qAbs(scrollBounds.bottom() - rect.bottom()) < 1.0 &&
                  qAbs(scrollBounds.left() - rect.left()) < 1.0 && qAbs(scrollBounds.right() - rect.right()) < 1.0,
              QStringLiteral("卷轴包围盒为最小包围盒"),
              QStringLiteral("bounds=(%1,%2,%3,%4)")
                  .arg(scrollBounds.top())
                  .arg(scrollBounds.bottom())
                  .arg(scrollBounds.left())
                  .arg(scrollBounds.right()));
}

} // namespace

void selftest::runShapesClusterTests(Context& ctx)
{
    checkSineWaveOnePeriod(ctx);
    checkFlagWaveNoPole(ctx);
    checkScrollShape(ctx);

    // 需求 4/5 + 需求 30：立方体 = 3 个菱形图元组成的组；圆柱体 = 顶椭圆 +
    // 有面积的多边形柱身，椭圆叠在多边形之上。
    {
        MainWindow window;
        window.insertShape(ShapeType::Cube);
        CanvasScene* scene = window.findChild<CanvasScene*>();
        QList<CanvasItem*> items = scene->canvasItems();
        bool threeFaces = items.size() == 3;
        int groupId = 0;
        bool sameGroup = threeFaces;
        int diamondFaces = 0;
        int rotatedFaces = 0;
        for (CanvasItem* item : items) {
            if (groupId == 0) {
                groupId = item->groupId();
            } else if (item->groupId() != groupId) {
                sameGroup = false;
            }
            auto* face = qgraphicsitem_cast<ShapeItem*>(item);
            if (face && face->shapeType() == ShapeType::Parallelogram) {
                ++diamondFaces;
                if (!qFuzzyIsNull(face->rotationAngle())) {
                    ++rotatedFaces;
                }
            }
        }
        ctx.check(sameGroup && groupId > 0, QStringLiteral("立方体插入为 3 面图元组成的分组"),
                  QStringLiteral("items=%1 group=%2").arg(items.size()).arg(groupId));
        ctx.check(diamondFaces == 3, QStringLiteral("立方体的三个面都是平行四边形类型图元"),
                  QStringLiteral("parallelograms=%1").arg(diamondFaces));
        ctx.check(rotatedFaces == 3, QStringLiteral("立方体三个面都带旋转角度"),
                  QStringLiteral("rotated=%1").arg(rotatedFaces));

        window.insertShape(ShapeType::Cylinder);
        items = scene->canvasItems();
        bool twoMore = items.size() == 5;
        int cylGroup = 0;
        bool cylinderGrouped = twoMore;
        int ellipseCount = 0;
        for (int i = 3; i < items.size(); ++i) {
            if (cylGroup == 0) {
                cylGroup = items.at(i)->groupId();
            } else if (items.at(i)->groupId() != cylGroup) {
                cylinderGrouped = false;
            }
            auto* shape = qgraphicsitem_cast<ShapeItem*>(items.at(i));
            if (shape && shape->shapeType() == ShapeType::Ellipse) {
                ++ellipseCount;
            }
        }
        ctx.check(cylinderGrouped && cylGroup > 0 && ellipseCount == 1,
                  QStringLiteral("圆柱体插入为顶椭圆加柱身组成的分组"),
                  QStringLiteral("items=%1 group=%2 ellipse=%3").arg(items.size()).arg(cylGroup).arg(ellipseCount));
        // 需求 30：柱身是带面积层的多边形图元（与合并 / 分割产物同型），不是矩形。
        auto* body = twoMore ? dynamic_cast<CompositeShapeItem*>(items.at(3)) : nullptr;
        ctx.check(body != nullptr && body->layerCount() == 1, QStringLiteral("圆柱体柱身是有面积的多边形图元"),
                  QStringLiteral("layers=%1").arg(body ? body->layerCount() : -1));
        // 需求 18/30：顶椭圆叠在柱身上层（分组内后加入的图元高度更高）。
        auto* lastFace = qgraphicsitem_cast<ShapeItem*>(items.last());
        ctx.check(lastFace && lastFace->shapeType() == ShapeType::Ellipse && lastFace->groupId() == cylGroup,
                  QStringLiteral("圆柱体顶椭圆叠在柱身上方"));
        ctx.check(body && lastFace && lastFace->height() > body->height(),
                  QStringLiteral("顶椭圆的层内高度高于柱身多边形"),
                  QStringLiteral("ellipse=%1 body=%2")
                      .arg(lastFace ? lastFace->height() : -1)
                      .arg(body ? body->height() : -1));
    }

    // 需求 26：正弦波形插入即生成曲线图元；今日新增的其余填充图形插入后
    // 归类为多边形图元（与合并、分割产物同型）。
    {
        MainWindow window;
        window.insertCurve(CurveKind::Sine);
        CanvasScene* scene = window.findChild<CanvasScene*>();
        const QList<CanvasItem*> items = scene->canvasItems();
        bool sineIsCurve = items.size() == 1 && dynamic_cast<CurveItem*>(items.constFirst()) != nullptr;
        ctx.check(sineIsCurve, QStringLiteral("正弦波形插入为曲线图元"), QStringLiteral("items=%1").arg(items.size()));

        const QList<ShapeType> polygonShapes = {ShapeType::Star4, ShapeType::Hexagram, ShapeType::Heart,
                                                ShapeType::FlagWave, ShapeType::Scroll};
        for (ShapeType type : polygonShapes) {
            window.insertShape(type);
        }
        const QList<CanvasItem*> after = scene->canvasItems();
        int polygonCount = 0;
        int nonPolygon = 0;
        for (int i = 1; i < after.size(); ++i) {
            if (dynamic_cast<CompositeShapeItem*>(after.at(i)) != nullptr) {
                ++polygonCount;
            } else {
                ++nonPolygon;
            }
        }
        ctx.check(polygonCount == polygonShapes.size() && nonPolygon == 0,
                  QStringLiteral("新增填充图形插入后归类为多边形图元"),
                  QStringLiteral("polygon=%1 other=%2").arg(polygonCount).arg(nonPolygon));
    }

    // 需求 28：几个星星按角的个数顺序排在一起（四角/十字星 → 五角星 → 六芒星）。
    {
        const QList<ShapeType> order = ShapeMeta::allTypes();
        const int star4 = order.indexOf(ShapeType::Star4);
        const int star = order.indexOf(ShapeType::Star);
        const int hexagram = order.indexOf(ShapeType::Hexagram);
        ctx.check(star4 >= 0 && star == star4 + 1 && hexagram == star + 1,
                  QStringLiteral("星星按角的个数排序：十字星、五角星、六芒星"),
                  QStringLiteral("star4=%1 star=%2 hexagram=%3").arg(star4).arg(star).arg(hexagram));
        // 更名走 storyboard_zh.ts 手工翻译：源串不变，译文按需求更新。
        ctx.check(ShapeItem::tr("Four-point star") == QStringLiteral("十字星") &&
                      ShapeItem::tr("Sine wave") == QStringLiteral("正弦波") &&
                      ShapeItem::tr("Wavy flag") == QStringLiteral("旗帜"),
                  QStringLiteral("四角星/正弦波形/波形旗帜已更名为十字星/正弦波/旗帜"),
                  QStringLiteral("%1/%2/%3")
                      .arg(ShapeItem::tr("Four-point star"), ShapeItem::tr("Sine wave"), ShapeItem::tr("Wavy flag")));
    }
}
