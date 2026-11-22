#include "items/CanvasTypes.h"
#include "items/geometry/ShapeMeta.h"
#include "items/geometry/ShapePathBuilder.h"
#include "items/lines/ArcItem.h"
#include "items/lines/CurveItem.h"
#include "items/lines/SineItem.h"
#include "items/polygon/PolygonItem.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "scene/CanvasScene.h"
#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"
#include "ui/mainwindow/MainWindow.h"

#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QtMath>
#include <limits>

namespace {

// 波线中点必须落在中心线上：一个完整周期时 sin(π)=0；该断言能击穿「周期数没改干净」的回归。
// 轴线用采样点自身的峰谷中点定义（内容框含笔宽余量，不能当轴线基准）。
void CheckSineWaveOnePeriod(selftest::Context& ctx)
{
    SineItem sine;
    sine.SetWaveGeometry(QPointF(0.0, 0.0), QSizeF(128.0, 40.0), QColor(Qt::black), 2.0);
    const QVector<QPointF> points = sine.StrokePoints();
    ctx.Check(points.size() > 8, QStringLiteral("正弦波由采样点构成"), QStringLiteral("points=%1").arg(points.size()));
    qreal minY = std::numeric_limits<qreal>::max();
    qreal maxY = std::numeric_limits<qreal>::lowest();
    for (const QPointF& point : points) {
        minY = qMin(minY, point.y());
        maxY = qMax(maxY, point.y());
    }
    const qreal cy = (minY + maxY) / 2.0;
    const qreal amplitude = (maxY - minY) / 2.0;
    ctx.Check(qAbs(amplitude - sine.params().amplitude) < 1.5, QStringLiteral("波峰到中心线的距离等于设定的像素振幅"),
              QStringLiteral("amp=%1 param=%2").arg(amplitude).arg(sine.params().amplitude));
    const QPointF mid = points.value(points.size() / 2, QPointF(0.0, cy));
    ctx.Check(qAbs(mid.y() - cy) < amplitude * 0.15, QStringLiteral("正弦波为一个完整周期（中点回到中心线）"),
              QStringLiteral("mid=(%1,%2) cy=%3").arg(mid.x()).arg(mid.y()).arg(cy));
    const QPointF start = points.value(0, QPointF(0.0, cy));
    const QPointF end = points.value(points.size() - 1, QPointF(0.0, cy));
    ctx.Check(qAbs(start.y() - cy) < amplitude * 0.15 && qAbs(end.y() - cy) < amplitude * 0.15,
              QStringLiteral("正弦波首尾都在中心线上"),
              QStringLiteral("start=(%1,%2) end=(%3,%4)").arg(start.x()).arg(start.y()).arg(end.x()).arg(end.y()));
    ctx.Check(qAbs(end.x() - start.x() - 128.0) < 2.0, QStringLiteral("正弦波横向铺满波形框"),
              QStringLiteral("width=%1").arg(end.x() - start.x()));
}

// 需求 17：波形旗帜占满内容宽度（两棋盘格）且纵向重心居中。
void CheckFlagWaveNoPole(selftest::Context& ctx)
{
    const QRectF rect(0.0, 0.0, 128.0, 86.0);
    const QPainterPath path = ShapePathBuilder::PathFor(ShapeType::FLAG_WAVE, rect, -1.0);
    const QRectF bounds = path.boundingRect();
    ctx.Check(qAbs(bounds.width() - rect.width()) < 1.0, QStringLiteral("波形旗帜宽度占满内容（两棋盘格）"),
              QStringLiteral("width=%1").arg(bounds.width()));
    ctx.Check(qAbs(bounds.center().y() - rect.center().y()) < rect.height() * 0.05,
              QStringLiteral("波形旗帜重心纵向居中"),
              QStringLiteral("center=%1 want=%2").arg(bounds.center().y()).arg(rect.center().y()));
    // 需求 22：选择框为最小包围盒——路径上下缘触及内容矩形上下缘。
    ctx.Check(qAbs(bounds.top() - rect.top()) < 1.0 && qAbs(bounds.bottom() - rect.bottom()) < 1.0,
              QStringLiteral("波形旗帜包围盒为最小包围盒"),
              QStringLiteral("top=%1 bottom=%2").arg(bounds.top()).arg(bounds.bottom()));
}

void CheckScrollShape(selftest::Context& ctx)
{
    ctx.Check(ShapeMeta::TypeToString(ShapeType::SCROLL) == QStringLiteral("scroll"),
              QStringLiteral("卷轴序列化字符串为 scroll"));
    ctx.Check(ShapeMeta::TypeFromString(QStringLiteral("flag_curl")) == ShapeType::SCROLL &&
                  ShapeMeta::TypeFromString(QStringLiteral("banner")) == ShapeType::SCROLL,
              QStringLiteral("旧文件的 flag_curl / banner 兼容读成卷轴"));
    const QRectF rect(0.0, 0.0, 128.0, 96.0);
    const QPainterPath path = ShapePathBuilder::PathFor(ShapeType::SCROLL, rect, -1.0);
    ctx.Check(path.intersects(QRectF(rect.center().x() - 2.0, rect.center().y() - 2.0, 4.0, 4.0)),
              QStringLiteral("卷轴中段有可填充纸面"));
    // 需求 22：卷筒纵向撑满内容高度，路径包围盒与内容矩形一致。
    const QRectF scrollBounds = path.boundingRect();
    ctx.Check(qAbs(scrollBounds.top() - rect.top()) < 1.0 && qAbs(scrollBounds.bottom() - rect.bottom()) < 1.0 &&
                  qAbs(scrollBounds.left() - rect.left()) < 1.0 && qAbs(scrollBounds.right() - rect.right()) < 1.0,
              QStringLiteral("卷轴包围盒为最小包围盒"),
              QStringLiteral("bounds=(%1,%2,%3,%4)")
                  .arg(scrollBounds.top())
                  .arg(scrollBounds.bottom())
                  .arg(scrollBounds.left())
                  .arg(scrollBounds.right()));
}

} // namespace

void selftest::RunShapesClusterTests(Context& ctx)
{
    CheckSineWaveOnePeriod(ctx);
    CheckFlagWaveNoPole(ctx);
    CheckScrollShape(ctx);

    // 需求 4/5 + 需求 30：立方体 = 3 个菱形图元组成的组；圆柱体 = 顶椭圆 +
    // 有面积的多边形柱身，椭圆叠在多边形之上。
    {
        MainWindow window;
        window.InsertShape(ShapeType::CUBE);
        CanvasScene* scene = window.findChild<CanvasScene*>();
        QList<CanvasItem*> items = scene->CanvasItems();
        bool threeFaces = items.size() == 3;
        int groupId = 0;
        bool sameGroup = threeFaces;
        int diamondFaces = 0;
        int rotatedFaces = 0;
        for (CanvasItem* item : items) {
            if (groupId == 0) {
                groupId = item->GroupId();
            } else if (item->GroupId() != groupId) {
                sameGroup = false;
            }
            auto* face = qgraphicsitem_cast<ShapeAbstractItem*>(item);
            if (face && face->shapeType() == ShapeType::PARALLELOGRAM) {
                ++diamondFaces;
                if (!qFuzzyIsNull(face->RotationAngle())) {
                    ++rotatedFaces;
                }
            }
        }
        ctx.Check(sameGroup && groupId > 0, QStringLiteral("立方体插入为 3 面图元组成的分组"),
                  QStringLiteral("items=%1 group=%2").arg(items.size()).arg(groupId));
        ctx.Check(diamondFaces == 3, QStringLiteral("立方体的三个面都是平行四边形类型图元"),
                  QStringLiteral("parallelograms=%1").arg(diamondFaces));
        ctx.Check(rotatedFaces == 3, QStringLiteral("立方体三个面都带旋转角度"),
                  QStringLiteral("rotated=%1").arg(rotatedFaces));

        // §6 验收：单一边长缝合 —— 三面两两至少共享两个顶点（误差 ≤0.05px），
        // 且组的外接框保持六边形比例（宽 : 高 = 1.732 : 2），顶点不溢出内容框。
        {
            // 取图形本体轮廓（不是内容框）：斜切平行四边形的轮廓顶点才是共享顶点。
            QList<QPolygonF> faces;
            for (CanvasItem* item : items) {
                QPolygonF quad;
                const QPainterPath painted = item->mapToScene(item->PaintedShape());
                for (const QPolygonF& polygon : painted.toSubpathPolygons()) {
                    for (const QPointF& point : polygon) {
                        if (!quad.contains(point)) {
                            quad.append(point);
                        }
                    }
                }
                faces.append(quad);
            }
            int sharedPairs = 0;
            for (int i = 0; i < faces.size(); ++i) {
                for (int j = i + 1; j < faces.size(); ++j) {
                    int shared = 0;
                    for (const QPointF& first : faces.at(i)) {
                        for (const QPointF& second : faces.at(j)) {
                            if (QLineF(first, second).length() <= 0.05) {
                                ++shared;
                            }
                        }
                    }
                    if (shared >= 2) {
                        ++sharedPairs;
                    }
                }
            }
            QRectF unionRect;
            for (const QPolygonF& quad : faces) {
                unionRect = unionRect.isNull() ? quad.boundingRect() : unionRect.united(quad.boundingRect());
            }
            const qreal ratio = unionRect.height() > 0.0 ? unionRect.width() / unionRect.height() : 0.0;
            ctx.Check(faces.size() == 3 && sharedPairs == 3,
                      QStringLiteral("立方体三面两两共享至少两个顶点（误差 ≤0.05px）"),
                      QStringLiteral("pairs=%1 verts=%2").arg(sharedPairs).arg([&faces]() {
                          QString text;
                          for (const QPolygonF& quad : faces) {
                              for (const QPointF& point : quad) {
                                  text += QStringLiteral("(%1,%2)").arg(point.x(), 0, 'f', 1).arg(point.y(), 0, 'f', 1);
                              }
                              text += QLatin1Char('|');
                          }
                          return text;
                      }()));
            ctx.Check(qAbs(ratio - 0.866) <= 0.02, QStringLiteral("立方体组外接框保持六边形比例 1.732:2"),
                      QStringLiteral("ratio=%1").arg(ratio));
        }

        window.InsertShape(ShapeType::CYLINDER);
        items = scene->CanvasItems();
        bool twoMore = items.size() == 5;
        int cylGroup = 0;
        bool cylinderGrouped = twoMore;
        int ellipseCount = 0;
        for (int i = 3; i < items.size(); ++i) {
            if (cylGroup == 0) {
                cylGroup = items.at(i)->GroupId();
            } else if (items.at(i)->GroupId() != cylGroup) {
                cylinderGrouped = false;
            }
            auto* shape = qgraphicsitem_cast<ShapeAbstractItem*>(items.at(i));
            if (shape && shape->shapeType() == ShapeType::ELLIPSE) {
                ++ellipseCount;
            }
        }
        ctx.Check(cylinderGrouped && cylGroup > 0 && ellipseCount == 1,
                  QStringLiteral("圆柱体插入为顶椭圆加柱身组成的分组"),
                  QStringLiteral("items=%1 group=%2 ellipse=%3").arg(items.size()).arg(cylGroup).arg(ellipseCount));
        // 需求 30：柱身是带面积层的多边形图元（与合并 / 分割产物同型），不是矩形。
        auto* body = twoMore ? dynamic_cast<PolygonItem*>(items.at(3)) : nullptr;
        ctx.Check(body != nullptr && body->LayerCount() == 1, QStringLiteral("圆柱体柱身是有面积的多边形图元"),
                  QStringLiteral("layers=%1").arg(body ? body->LayerCount() : -1));
        // 需求 18/30：顶椭圆叠在柱身上层（分组内后加入的图元高度更高）。
        auto* lastFace = qgraphicsitem_cast<ShapeAbstractItem*>(items.last());
        ctx.Check(lastFace && lastFace->shapeType() == ShapeType::ELLIPSE && lastFace->GroupId() == cylGroup,
                  QStringLiteral("圆柱体顶椭圆叠在柱身上方"));
        ctx.Check(body && lastFace && lastFace->ZOrder() > body->ZOrder(),
                  QStringLiteral("顶椭圆的层内高度高于柱身多边形"),
                  QStringLiteral("ellipse=%1 body=%2")
                      .arg(lastFace ? lastFace->ZOrder() : -1)
                      .arg(body ? body->ZOrder() : -1));
    }

    // 需求 26：正弦波形插入即生成曲线图元；今日新增的其余填充图形插入后
    // 归类为多边形图元（与合并、分割产物同型）。
    {
        MainWindow window;
        window.InsertCurve(LineGlyph::SINE);
        CanvasScene* scene = window.findChild<CanvasScene*>();
        const QList<CanvasItem*> items = scene->CanvasItems();
        bool sineIsSineItem = items.size() == 1 && dynamic_cast<SineItem*>(items.constFirst()) != nullptr;
        ctx.Check(sineIsSineItem, QStringLiteral("正弦波形插入为独立正弦图元"),
                  QStringLiteral("items=%1").arg(items.size()));

        const QList<ShapeType> insertedShapes = {ShapeType::STAR4, ShapeType::HEXAGRAM, ShapeType::HEART,
                                                 ShapeType::FLAG_WAVE, ShapeType::SCROLL};
        for (ShapeType type : insertedShapes) {
            window.InsertShape(type);
        }
        const QList<CanvasItem*> after = scene->CanvasItems();
        int shapeCount = 0;
        int mismatched = 0;
        for (int i = 1; i < after.size(); ++i) {
            auto* shape = dynamic_cast<ShapeAbstractItem*>(after.at(i));
            if (shape && shape->shapeType() == insertedShapes.at(i - 1)) {
                ++shapeCount;
            } else {
                ++mismatched;
            }
        }
        ctx.Check(shapeCount == insertedShapes.size() && mismatched == 0,
                  QStringLiteral("新增填充图形插入后归类为各自的具体图形图元"),
                  QStringLiteral("shape=%1 other=%2").arg(shapeCount).arg(mismatched));
    }

    // 需求：弧线默认插入的长短半轴与椭圆 / 扇形一致（外接框 128×128，半轴 64/64）。
    {
        MainWindow window;
        window.InsertCurve(LineGlyph::ARC);
        CanvasScene* scene = window.findChild<CanvasScene*>();
        const auto* arc =
            scene->CanvasItems().isEmpty() ? nullptr : dynamic_cast<const ArcItem*>(scene->CanvasItems().constFirst());
        const QSizeF arcFrame = arc ? arc->EllipseSize() : QSizeF();
        const QSizeF ellipseDefault = DefaultShapeSize(ShapeType::ELLIPSE);
        const QSizeF sectorDefault = DefaultShapeSize(ShapeType::SECTOR);
        ctx.Check(arcFrame == ellipseDefault && arcFrame == sectorDefault,
                  QStringLiteral("弧线默认长短半轴与椭圆 / 扇形一致"),
                  QStringLiteral("arc=%1x%2 ellipse=%3x%4 sector=%5x%6")
                      .arg(arcFrame.width())
                      .arg(arcFrame.height())
                      .arg(ellipseDefault.width())
                      .arg(ellipseDefault.height())
                      .arg(sectorDefault.width())
                      .arg(sectorDefault.height()));
    }

    // 需求 28：几个星星按角的个数顺序排在一起（四角/十字星 → 五角星 → 六芒星）。
    {
        const QList<ShapeType> order = ShapeMeta::AllTypes();
        const int star4 = order.indexOf(ShapeType::STAR4);
        const int star = order.indexOf(ShapeType::STAR);
        const int hexagram = order.indexOf(ShapeType::HEXAGRAM);
        ctx.Check(star4 >= 0 && star == star4 + 1 && hexagram == star + 1,
                  QStringLiteral("星星按角的个数排序：十字星、五角星、六芒星"),
                  QStringLiteral("star4=%1 star=%2 hexagram=%3").arg(star4).arg(star).arg(hexagram));
        // 更名走 storyboard_zh.ts 手工翻译：源串不变，译文按需求更新。
        ctx.Check(ShapeAbstractItem::tr("Four-point star") == QStringLiteral("十字星") &&
                      ShapeAbstractItem::tr("Sine wave") == QStringLiteral("正弦波") &&
                      ShapeAbstractItem::tr("Wavy flag") == QStringLiteral("旗帜"),
                  QStringLiteral("四角星/正弦波形/波形旗帜已更名为十字星/正弦波/旗帜"),
                  QStringLiteral("%1/%2/%3")
                      .arg(ShapeAbstractItem::tr("Four-point star"), ShapeAbstractItem::tr("Sine wave"),
                           ShapeAbstractItem::tr("Wavy flag")));
    }
}
