#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "MainWindow.h"
#include "ShapeItem.h"
#include "ShapeMeta.h"
#include "ShapePathBuilder.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"

#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QtMath>

namespace {

// 波线中点必须落在轴线上：一个完整周期时 sin(π)=0；1.5 个周期时 sin(1.5π)=-1，
// 该断言能击穿「周期数没改干净」的回归。
void checkSineWaveOnePeriod(selftest::Context& ctx)
{
    const QRectF rect(0.0, 0.0, 128.0, 40.0);
    const QPainterPath path = ShapePathBuilder::pathFor(ShapeType::SineWave, rect, -1.0);
    const qreal cy = rect.center().y();
    const qreal amplitude = rect.height() * 0.20;
    const QPointF mid = path.pointAtPercent(0.5);
    ctx.check(qAbs(mid.y() - cy) < amplitude * 0.15, QStringLiteral("正弦波形为一个完整周期（中点回到轴线）"),
              QStringLiteral("mid=(%1,%2) cy=%3").arg(mid.x()).arg(mid.y()).arg(cy));
    const QPointF start = path.pointAtPercent(0.0);
    const QPointF end = path.pointAtPercent(1.0);
    ctx.check(qAbs(start.y() - cy) < amplitude * 0.15 && qAbs(end.y() - cy) < amplitude * 0.15,
              QStringLiteral("正弦波形首尾都在轴线上"),
              QStringLiteral("start=(%1,%2) end=(%3,%4)").arg(start.x()).arg(start.y()).arg(end.x()).arg(end.y()));
}

// 波形旗帜去掉左边旗杆后，轮廓最左缘应该是旗面起点（16% 宽度处），
// 若旗杆回归，boundingRect 左缘会退回 10% 处。
void checkFlagWaveNoPole(selftest::Context& ctx)
{
    const QRectF rect(0.0, 0.0, 128.0, 86.0);
    const QPainterPath path = ShapePathBuilder::pathFor(ShapeType::FlagWave, rect, -1.0);
    const qreal left = path.boundingRect().left();
    ctx.check(left > rect.left() + rect.width() * 0.12, QStringLiteral("波形旗帜没有左边旗杆竖线"),
              QStringLiteral("left=%1").arg(left));
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
}

} // namespace

void selftest::runShapesClusterTests(Context& ctx)
{
    checkSineWaveOnePeriod(ctx);
    checkFlagWaveNoPole(ctx);
    checkScrollShape(ctx);

    // 需求 4/5：立方体与圆柱体插入即得到分组面图元。
    {
        MainWindow window;
        window.insertShape(ShapeType::Cube);
        CanvasScene* scene = window.findChild<CanvasScene*>();
        QList<CanvasItem*> items = scene->canvasItems();
        bool threeFaces = items.size() == 3;
        int groupId = 0;
        bool sameGroup = threeFaces;
        for (CanvasItem* item : items) {
            if (groupId == 0) {
                groupId = item->groupId();
            } else if (item->groupId() != groupId) {
                sameGroup = false;
            }
        }
        ctx.check(sameGroup && groupId > 0, QStringLiteral("立方体插入为 3 面图元组成的分组"),
                  QStringLiteral("items=%1 group=%2").arg(items.size()).arg(groupId));

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
    }
}
