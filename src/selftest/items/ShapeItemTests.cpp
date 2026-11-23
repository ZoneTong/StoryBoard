#include "app/I18n.h"
#include "app/Settings.h"
#include "canvas/CanvasView.h"
#include "canvas/controllers/CanvasItemHandles.h"
#include "canvas/controllers/CanvasItemInteraction.h"
#include "io/CustomShapeTemplates.h"
#include "items/CanvasTypes.h"
#include "items/geometry/EllipseShape.h"
#include "items/geometry/ShapeMeta.h"
#include "items/geometry/ShapePathBuilder.h"
#include "items/lines/ArcItem.h"
#include "items/lines/BraceItem.h"
#include "items/lines/CurveItem.h"
#include "items/lines/LineAbstractItem.h"
#include "items/lines/SineItem.h"
#include "items/lines/StraightLineItem.h"
#include "items/polygon/PolygonAccess.h"
#include "items/polygon/PolygonItem.h"
#include "items/properties/ArrowStyle.h"
#include "items/shapes/AllShapes.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "items/shapes/ShapeRegistry.h"
#include "items/text/TextItem.h"
#include "lineart/LineArtConverter.h"
#include "lineart/LineArtDialog.h"
#include "lineart/LineArtPresets.h"
#include "ops/Alignment.h"
#include "ops/BoolOps.h"
#include "scene/CanvasScene.h"
#include "scene/Commands.h"
#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"
#include "selftest/core/SelftestHelpers.h"
#include "ui/dialogs/ExportDialog.h"
#include "ui/icons/IconFactory.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/panels/itemdialog/ItemPropertiesDialog.h"
#include "ui/panels/layers/LayerPanel.h"
#include "ui/panels/properties/PropertiesPanel.h"
#include "ui/settings/SettingsDialog.h"

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
#include <QRect>
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


void selftest::RunShapeItemTests(Context& ctx)
{
    // Shape items: common diagrams, quick insertion geometry and rendering.
    // ---------------------------------------------------------------
    CanvasScene shapeScene;
    QUndoStack shapeUndo;
    shapeUndo.setUndoLimit(100);
    shapeScene.SetUndoStack(&shapeUndo);
    shapeScene.ApplyCanvasSize(QSizeF(1000, 1000));
    shapeScene.SetShowGrid(false);

    const QList<ShapeType> shapeTypes = AllShapeTypes();
    // 菱形已是独立类型（DiamondItem，派生自平行四边形）；正六边形由 HexagonItem 提供。
    ctx.Check(shapeTypes.size() == 18, QStringLiteral("提供 18 种有面积图形（直线/弧线/正弦已并入曲线族）"),
              QStringLiteral("count=%1").arg(shapeTypes.size()));

    // §3.5：正六边形按语义插在三角形之后、显示顺序放在菱形之后扇形之前。
    {
        ctx.Check(ShapeTypeToString(ShapeType::HEXAGON) == QStringLiteral("hexagon") &&
                      ShapeTypeFromString(QStringLiteral("hexagon")) == ShapeType::HEXAGON,
                  QStringLiteral("正六边形的枚举串为 hexagon"));
        ctx.Check(ShapeTypeName(ShapeType::HEXAGON) != QStringLiteral("Honeycomb"),
                  QStringLiteral("正六边形源码串避开填充纹理 Honeycomb"), ShapeTypeName(ShapeType::HEXAGON));
        const QList<ShapeType> order = ShapeMeta::AllTypes();
        const int diamond = order.indexOf(ShapeType::DIAMOND);
        const int hexagon = order.indexOf(ShapeType::HEXAGON);
        const int sector = order.indexOf(ShapeType::SECTOR);
        ctx.Check(diamond >= 0 && hexagon == diamond + 1 && sector == hexagon + 1,
                  QStringLiteral("正六边形显示在菱形之后、扇形之前"),
                  QStringLiteral("diamond=%1 hexagon=%2 sector=%3").arg(diamond).arg(hexagon).arg(sector));
        const QSizeF hexSize = DefaultShapeSize(ShapeType::HEXAGON);
        ctx.Check(qAbs(hexSize.width() - 128.0) < 0.01 && qAbs(hexSize.height() - 111.0) < 0.01,
                  QStringLiteral("正六边形默认约 128×111"),
                  QStringLiteral("%1×%2").arg(hexSize.width()).arg(hexSize.height()));
        ctx.Check(ShapeTypeIsFilled(ShapeType::HEXAGON) && ShapeMeta::SupportsCenter(ShapeType::HEXAGON),
                  QStringLiteral("正六边形可填充且支持中心点"));
        const QPainterPath hexPath = ShapePathBuilder::PathFor(ShapeType::HEXAGON, QRectF(0, 0, 128, 111), 0.0);
        // 平顶：上下两条水平边 + 左右两个尖角；moveTo 起点 + 5 次 lineTo 后闭合，共 7 个元素（6 个顶点）。
        ctx.Check(!hexPath.isEmpty() && hexPath.elementCount() == 7,
                  QStringLiteral("正六边形轮廓为 6 个顶点的平顶六边形"),
                  QStringLiteral("elements=%1").arg(hexPath.elementCount()));
    }

    // 透明度统一为“内容透明度”：不压 QGraphicsItem，只压 PaintContent() 的像素。
    {
        CanvasScene opacityScene;
        opacityScene.ApplyCanvasSize(QSizeF(400, 300));
        opacityScene.SetShowGrid(false);
        ShapeAbstractItem* underRect = CreateShapeItem(ShapeType::RECTANGLE);
        underRect->SetFillColor(QColor(30, 80, 200));
        underRect->SetStrokeWidth(0.0);
        underRect->SetContentSize(QSizeF(80, 80));
        underRect->setPos(QPointF(20, 20));
        opacityScene.AddItemsWithUndo({underRect}, QStringLiteral("添加底层矩形"));

        ShapeAbstractItem* opacityRect = CreateShapeItem(ShapeType::RECTANGLE);
        opacityRect->SetFillColor(QColor(220, 38, 38));
        opacityRect->SetStrokeWidth(0.0);
        opacityRect->SetContentSize(QSizeF(80, 80));
        opacityRect->setPos(QPointF(20, 20));
        opacityRect->SetItemOpacity(0.4);
        opacityScene.AddItemsWithUndo({opacityRect}, QStringLiteral("添加半透明矩形"));

        const QImage rendered = opacityScene.RenderImage(opacityRect->SceneContentRect(), 1.0);
        const QColor center = rendered.pixelColor(rendered.width() / 2, rendered.height() / 2);
        ctx.Check(center.alpha() >= 245 && center.red() >= 80 && center.blue() >= 100 &&
                      qFuzzyCompare(opacityRect->opacity(), 1.0),
                  QStringLiteral("图形内容透明度与下层正常混色"),
                  QStringLiteral("rgba=%1,%2,%3,%4")
                      .arg(center.red())
                      .arg(center.green())
                      .arg(center.blue())
                      .arg(center.alpha()));
    }

    // 需求 6：填充 / 描线属性各自归入基类，有面积图形同时多继承两者，
    // 零面积描线只继承描线基类；属性面板按基类虚函数批量改选中图元。
    {
        ShapeAbstractItem* rect = CreateShapeItem(ShapeType::RECTANGLE);
        StraightLineItem line;
        CanvasItem* lineItem = &line;
        ctx.Check(dynamic_cast<FillAttributes*>(rect) != nullptr && dynamic_cast<StrokeAttributes*>(rect) != nullptr,
                  QStringLiteral("有面积图形同时继承填充与描线属性基类"));
        ctx.Check(dynamic_cast<StrokeAttributes*>(lineItem) != nullptr &&
                      dynamic_cast<FillAttributes*>(lineItem) == nullptr,
                  QStringLiteral("直线只继承描线属性基类，不继承填充基类"));

        const QColor fill(239, 68, 68, 120);
        auto* fillAttrs = dynamic_cast<FillAttributes*>(rect);
        auto* strokeAttrs = dynamic_cast<StrokeAttributes*>(rect);
        fillAttrs->SetFillColor(fill);
        strokeAttrs->SetStrokeWidth(9.0);
        ctx.Check(rect->FillColor() == fill && qAbs(rect->StrokeWidth() - 9.0) < 0.01,
                  QStringLiteral("通过属性基类写回填充颜色与描线宽度"),
                  QStringLiteral("fill=%1 width=%2").arg(rect->FillColor().name()).arg(rect->StrokeWidth()));

        // 多边形从内部图层取色：经 FillAttributes 虚函数写入时也要落到图层上。
        PolygonItem::Layer layer;
        layer.path.addRect(QRectF(0.0, 0.0, 40.0, 30.0));
        layer.fill = QColor(QStringLiteral("#3B82F6"));
        layer.stroke = QColor(QStringLiteral("#1E293B"));
        layer.filled = true;
        PolygonItem polygon({layer});
        auto* polygonFill = dynamic_cast<FillAttributes*>(&polygon);
        polygonFill->SetFillColor(fill);
        ctx.Check(polygonFill && polygon.FillColor() == fill, QStringLiteral("填充基类虚函数对多边形落到图层取色"),
                  polygon.FillColor().name());
        delete rect;
    }

    // 需求 5：三角形不再暴露底边长度属性，只保留两个底角。
    {
        TriangleItem triangle;
        const ItemPropertySet properties = triangle.Properties();
        ctx.Check(properties.Size() == 16 && properties.Contains(ItemProperty::BOTTOM_ANGLE1) &&
                      properties.Contains(ItemProperty::BOTTOM_ANGLE2),
                  QStringLiteral("三角形属性只剩两个底角（底边长度已移除）"),
                  QStringLiteral("size=%1").arg(properties.Size()));
    }
    // 需求 9：平行四边形默认 60° 内角、底边 2 格（需求 16 起 1 格 64px，2 格 = 128px）。
    {
        GeneralParallelogramItem para;
        const QSizeF paraSize = DefaultShapeSize(ShapeType::PARALLELOGRAM);
        const QPainterPath defaultPath = para.CurrentPath(QRectF(0.0, 0.0, paraSize.width(), paraSize.height()));
        const qreal defaultBase = QLineF(defaultPath.elementAt(0), defaultPath.elementAt(1)).length();
        ctx.Check(qAbs(defaultBase - 128.0) < 0.1 && qAbs(paraSize.height() - 128.0) < 0.1,
                  QStringLiteral("平行四边形默认底边 128px、高 128px"),
                  QStringLiteral("%1×%2 base=%3").arg(paraSize.width()).arg(paraSize.height()).arg(defaultBase));
        ctx.Check(qAbs(para.ParallelogramAngle() - 60.0) < 0.01, QStringLiteral("平行四边形默认内角 60°"),
                  QStringLiteral("angle=%1").arg(para.ParallelogramAngle()));
        para.SetParallelogramAngle(20.0);
        const QPainterPath acute = para.CurrentPath(QRectF(0.0, 0.0, paraSize.width(), paraSize.height()));
        const QPainterPath blunt =
            ShapePathBuilder::ParallelogramPath(QRectF(0.0, 0.0, paraSize.width(), paraSize.height()), 120.0);
        ctx.Check(acute.elementCount() >= 4 && blunt.elementCount() >= 4 &&
                      qAbs(acute.boundingRect().width() - paraSize.width()) < 1.0,
                  QStringLiteral("平行四边形锐角/钝角都能画出闭合轮廓"),
                  QStringLiteral("acute=%1 blunt=%2").arg(acute.elementCount()).arg(blunt.elementCount()));
        para.SetParallelogramAngle(170.0);
        ctx.Check(qAbs(para.ParallelogramAngle() - 170.0) < 0.01, QStringLiteral("平行四边形内角保持 170°"),
                  QStringLiteral("angle=%1").arg(para.ParallelogramAngle()));
        const ShapeItemState paraState = para.State();
        const qreal paraStateAngle = paraState.propertyExtras.value(ItemProperty::PARALLELOGRAM_ANGLE, -1.0).toDouble();
        ctx.Check(qAbs(paraStateAngle - 170.0) < 0.01, QStringLiteral("平行四边形内角进入状态快照"),
                  QStringLiteral("angle=%1").arg(paraStateAngle));
    }
    // 需求：内角旋钮绕左下顶点拖动，底边长度全程不变；锐角在右、钝角在左，可连续跨过 90°。
    // 旋钮下移到框内 22px，90° 时不再与任何角手柄重合。
    {
        GeneralParallelogramItem para;
        auto knobInScene = [&para]() {
            const HandleSpecList specs = para.CustomHandles();
            return specs.isEmpty() ? QPointF() : para.mapToScene(specs.constFirst().position);
        };
        auto hasSkewKnob = [&para]() {
            const HandleSpecList specs = para.CustomHandles();
            return !specs.isEmpty() && specs.constFirst().id == CanvasItem::CUSTOM_1 &&
                   specs.constFirst().behavior == HandleBehavior::SKEW;
        };
        auto bottomLength = [&para]() {
            const QPainterPath path = para.CurrentPath(para.ContentRect());
            return QLineF(path.elementAt(0), path.elementAt(1)).length();
        };
        para.SetParallelogramAngle(60.0);
        const qreal baseLength = bottomLength();
        const QPointF acuteKnob = knobInScene();
        para.SetParallelogramAngle(90.0);
        const qreal rightBase = bottomLength();
        const qreal rightCenterX = para.ContentSize().width() / 2.0;
        const QPointF rightKnob = knobInScene();
        para.SetParallelogramAngle(120.0);
        const qreal obtuseBase = bottomLength();
        const QPointF obtuseKnob = knobInScene();
        ctx.Check(
            hasSkewKnob() && acuteKnob.x() > rightKnob.x() && rightKnob.x() > obtuseKnob.x() &&
                qAbs(rightKnob.x() - rightCenterX) < 0.01,
            QStringLiteral("平行四边形内角旋钮连续跨越锐角与钝角"),
            QStringLiteral("acute=%1 right=%2 obtuse=%3").arg(acuteKnob.x()).arg(rightKnob.x()).arg(obtuseKnob.x()));
        ctx.Check(qAbs(baseLength - rightBase) < 0.01 && qAbs(baseLength - obtuseBase) < 0.01,
                  QStringLiteral("改变左下内角时底边长度保持不变"),
                  QStringLiteral("60=%1 90=%2 120=%3").arg(baseLength).arg(rightBase).arg(obtuseBase));
        ctx.Check(acuteKnob.y() > para.ContentRect().top() + 12.0 && rightKnob.y() > para.ContentRect().top() + 12.0,
                  QStringLiteral("内角旋钮下移，不在顶边/角手柄处"),
                  QStringLiteral("y=%1/%2").arg(acuteKnob.y()).arg(rightKnob.y()));
        // 角度是真相：89°/91° 都落在同一内容框宽度内，说明 90° 前后没有断档或跳变。
        para.SetParallelogramAngle(89.0);
        const QPainterPath steepPath = para.CurrentPath(para.ContentRect());
        para.SetParallelogramAngle(91.0);
        const QPainterPath bluntPath = para.CurrentPath(para.ContentRect());
        ctx.Check(
            qAbs(steepPath.boundingRect().width() - para.ContentSize().width()) < 1.0 &&
                qAbs(bluntPath.boundingRect().width() - para.ContentSize().width()) < 1.0,
            QStringLiteral("90° 前后的平行四边形轮廓都不越出内容框"),
            QStringLiteral("89=%1 91=%2").arg(steepPath.boundingRect().width()).arg(bluntPath.boundingRect().width()));
    }
    // 矩形锁 90°，不能继承平行四边形的内角旋钮：那个旋钮在矩形里落在中心，
    // 拖动只会调用锁 90° 的写入，视觉上是一个没有用途的图标。
    {
        RectangleItem rect;
        ctx.Check(rect.CustomHandles().isEmpty(), QStringLiteral("矩形不显示内角倾斜旋钮与顶点手柄"));
    }
    {
        // §2.2 自定义句柄只推一条属性撤销记录：三角形顶点拖拽的撤销 / 重做要能回放角度。
        TriangleItem* triangle = new TriangleItem;
        triangle->setPos(QPointF(20.0, 20.0));
        triangle->SetContentSize(QSizeF(120.0, 90.0));
        CanvasScene scene;
        QUndoStack undo;
        scene.SetUndoStack(&undo);
        scene.AddItemsWithUndo({triangle}, QStringLiteral("Add triangle"));
        const qreal angle1Before = triangle->BottomAngle1();
        const qreal angle2Before = triangle->BottomAngle2();
        CanvasItemInteraction apexDrag(triangle);
        const QPointF aBefore = triangle->mapToScene(triangle->triangleVertex(ShapeAbstractItem::TriangleVertex::A));
        const QPointF cBefore = triangle->mapToScene(triangle->triangleVertex(ShapeAbstractItem::TriangleVertex::C));
        apexDrag.BeginResize(CanvasItem::CUSTOM_0, cBefore);
        apexDrag.ApplyResize(QPointF(aBefore.x() - 30.0, cBefore.y() - 45.0), Qt::NoModifier);
        apexDrag.FinishInteraction();
        const qreal angle1Dragged = triangle->BottomAngle1();
        ctx.Check(undo.count() == 2, QStringLiteral("顶点手柄拖拽只落一条撤销记录"),
                  QStringLiteral("undo=%1 angle1=%2").arg(undo.count()).arg(angle1Dragged));
        undo.undo();
        ctx.Check(qAbs(triangle->BottomAngle1() - angle1Before) < 0.5 &&
                      qAbs(triangle->BottomAngle2() - angle2Before) < 0.5,
                  QStringLiteral("撤销顶点拖拽恢复两个底角"),
                  QStringLiteral("angles=%1/%2 before=%3/%4")
                      .arg(triangle->BottomAngle1())
                      .arg(triangle->BottomAngle2())
                      .arg(angle1Before)
                      .arg(angle2Before));
        undo.redo();
        ctx.Check(qAbs(triangle->BottomAngle1() - angle1Dragged) < 0.5, QStringLiteral("重做顶点拖拽恢复拖拽后角度"),
                  QStringLiteral("angle1=%1 dragged=%2").arg(triangle->BottomAngle1()).arg(angle1Dragged));
    }
    // 需求 4：菱形独立成类，派生自平行四边形，四边恒等且创建时一条对角线水平。
    {
        DiamondItem diamond;
        const QSizeF diamondSize = DefaultShapeSize(ShapeType::DIAMOND);
        const QPainterPath path = diamond.CurrentPath(QRectF(0.0, 0.0, diamondSize.width(), diamondSize.height()));
        const QPointF top(path.elementAt(0).x, path.elementAt(0).y);
        const QPointF right(path.elementAt(1).x, path.elementAt(1).y);
        const QPointF bottom(path.elementAt(2).x, path.elementAt(2).y);
        const QPointF left(path.elementAt(3).x, path.elementAt(3).y);
        const qreal sideTop = QLineF(top, right).length();
        const qreal sideRight = QLineF(right, bottom).length();
        const qreal sideBottom = QLineF(bottom, left).length();
        const qreal sideLeft = QLineF(left, top).length();
        ShapeAbstractItem* diamondBase = &diamond;
        ctx.Check(dynamic_cast<ParallelogramAbstractItem*>(diamondBase) != nullptr &&
                      diamond.shapeType() == ShapeType::DIAMOND,
                  QStringLiteral("菱形派生自平行四边形族且自带类型"),
                  QStringLiteral("type=%1").arg(static_cast<int>(diamond.shapeType())));
        ctx.Check(qAbs(sideTop - sideRight) < 0.01 && qAbs(sideRight - sideBottom) < 0.01 &&
                      qAbs(sideBottom - sideLeft) < 0.01,
                  QStringLiteral("菱形四条边恒等"),
                  QStringLiteral("%1/%2/%3/%4").arg(sideTop).arg(sideRight).arg(sideBottom).arg(sideLeft));
        // 左右顶点等高 ⇒ 一条对角线水平；上下顶点等宽 ⇒ 另一条竖直。
        ctx.Check(qAbs(right.y() - left.y()) < 0.01 && qAbs(top.x() - bottom.x()) < 0.01 &&
                      qAbs(diamondSize.width() - 128.0) < 0.01 && qAbs(diamondSize.height() - 72.0) < 0.01,
                  QStringLiteral("菱形插入时水平对角线为宽、默认 128×72"),
                  QStringLiteral("w=%1 h=%2").arg(diamondSize.width()).arg(diamondSize.height()));
        // §3.2：菱形复用内角 A，A = 2·atan(h/w) 由宽高派生（128×72 ⇒ ≈58.72°）。
        ctx.Check(diamond.Properties().Contains(ItemProperty::PARALLELOGRAM_ANGLE) &&
                      qAbs(diamond.ParallelogramAngle() - 58.72) < 0.05,
                  QStringLiteral("菱形开放派生内角 A"), QStringLiteral("angle=%1").arg(diamond.ParallelogramAngle()));
    }
    // 需求 4：弧线 / 正弦波这些曲线族子型同样能设两端箭头，箭头并入命中轮廓。
    {
        ArcItem arc;
        SineItem wave;
        arc.SetArcGeometry(QPointF(0.0, 0.0), QSizeF(120.0, 80.0), 15.0, 140.0, QColor(Qt::black), 2.0);
        wave.SetWaveGeometry(QPointF(0.0, 0.0), QSizeF(120.0, 80.0), QColor(Qt::black), 2.0);
        for (LineAbstractItem* line : {static_cast<LineAbstractItem*>(&arc), static_cast<LineAbstractItem*>(&wave)}) {
            line->SetArrowStyles(ArrowStyle::TRIANGLE_FILLED, ArrowStyle::OPEN_ARROW);
            const QRectF hit = line->PaintedShape().boundingRect();
            const QRectF plain = line->OutlineShape().boundingRect();
            ctx.Check(line->StartArrow() == ArrowStyle::TRIANGLE_FILLED && line->EndArrow() == ArrowStyle::OPEN_ARROW &&
                          !hit.isEmpty() && hit.width() >= plain.width(),
                      QStringLiteral("线条类图元可设两端箭头并计入命中轮廓"),
                      QStringLiteral("hit=%1,%2 plain=%3,%4")
                          .arg(hit.width())
                          .arg(hit.height())
                          .arg(plain.width())
                          .arg(plain.height()));
        }
    }
    ctx.Check(ShapeTypeFromString(QStringLiteral("circle")) == ShapeType::ELLIPSE &&
                  ShapeTypeFromString(QStringLiteral("roundrect")) == ShapeType::RECTANGLE &&
                  ShapeTypeFromString(QStringLiteral("double_arrow")) == ShapeType::DOUBLE_ARROW &&
                  ShapeTypeFromString(QStringLiteral("four_point_star")) == ShapeType::STAR4 &&
                  ShapeTypeFromString(QStringLiteral("love")) == ShapeType::HEART &&
                  ShapeTypeFromString(QStringLiteral("speech")) == ShapeType::CALLOUT,
              QStringLiteral("图形类型字符串兼容常见别名（圆角矩形读成矩形）"));
    ctx.Check(ShapeTypeFromString(QStringLiteral("does-not-exist")) == ShapeType::RECTANGLE,
              QStringLiteral("未知图形类型回退为矩形"));

    // G4：描述器表是唯一元数据源，落盘字符串、别名、菜单顺序与能力标记都查同一张表。
    {
        const QList<ShapeType> order = ShapeMeta::AllTypes();
        bool ordered = true;
        for (int i = 1; i < order.size(); ++i) {
            const ShapeRegistry::Descriptor* previous = ShapeRegistry::Find(order.at(i - 1));
            const ShapeRegistry::Descriptor* current = ShapeRegistry::Find(order.at(i));
            ordered = ordered && previous && current && previous->menuOrder < current->menuOrder;
        }
        ctx.Check(ordered && order.size() == ShapeRegistry::Entries().size(),
                  QStringLiteral("常见图形顺序表与描述器一一对应"),
                  QStringLiteral("types=%1 entries=%2").arg(order.size()).arg(ShapeRegistry::Entries().size()));
        bool roundTrip = true;
        for (const ShapeRegistry::Entry& entry : ShapeRegistry::Entries()) {
            roundTrip =
                roundTrip && ShapeTypeFromString(ShapeTypeToString(entry.descriptor.type)) == entry.descriptor.type;
        }
        ctx.Check(roundTrip, QStringLiteral("每种图形的落盘字符串都能读回原类型"));
        ctx.Check(!ShapeMeta::SupportsCenter(ShapeType::STAR) && ShapeMeta::SupportsCenter(ShapeType::STAR4),
                  QStringLiteral("中心点能力沿用描述器标记（五角星不支持）"));
        ctx.Check(ShapeMeta::IsFilled(ShapeType::DIAMOND) && !ShapeMeta::IsFilled(ShapeType::POLYGON),
                  QStringLiteral("填充能力沿用描述器标记（POLYGON 占位无面积）"));
    }

    // G5：多边形按层取几何统一走 VisitPolygon，不再由调用方强转叶子类。
    {
        PolygonItem polygon;
        CanvasItem* asItem = &polygon;
        ctx.Check(AsPolygon(asItem) == &polygon && IsPolygonShape(asItem),
                  QStringLiteral("多边形经 VisitPolygon 可被识别"));
        RectangleItem rectangle;
        ctx.Check(AsPolygon(&rectangle) == nullptr && !IsPolygonShape(&rectangle),
                  QStringLiteral("普通图形不会被误判成多边形"));
    }

    // 需求 5：圆角矩形并入矩形后，旧 rounded_rectangle 文件读成「矩形 + 自动圆角」。
    {
        RectangleItem legacyRounded;
        QJsonObject legacyJson = legacyRounded.ToJson();
        legacyJson.insert(QStringLiteral("shapeType"), QStringLiteral("rounded_rectangle"));
        legacyJson.remove(QStringLiteral("cornerRadius"));
        RectangleItem loadedRounded;
        loadedRounded.FromJson(legacyJson);
        ctx.Check(loadedRounded.shapeType() == ShapeType::RECTANGLE && loadedRounded.CornerRadius() > 0.0,
                  QStringLiteral("旧 rounded_rectangle 读成矩形显式圆角"));
    }

    // 矩形 JSON 重载锁 90°：旧文件 / 复制粘贴带的 parallelogramAngle 不能把矩形拖成斜四边形。
    {
        RectangleItem legacyRect;
        QJsonObject legacyJson = legacyRect.ToJson();
        legacyJson.insert(QStringLiteral("parallelogramAngle"), 60.0);
        RectangleItem loadedRect;
        loadedRect.FromJson(legacyJson);
        ctx.Check(loadedRect.shapeType() == ShapeType::RECTANGLE &&
                      qFuzzyCompare(loadedRect.ParallelogramAngle() + 2.0, 92.0),
                  QStringLiteral("矩形 JSON 重载后内角仍锁 90°"),
                  QStringLiteral("angle=%1").arg(loadedRect.ParallelogramAngle()));
    }

    bool shapePathsOk = true;
    bool shapeIconsOk = true;
    for (ShapeType type : shapeTypes) {
        if (ShapeAbstractItem::PathFor(type, QRectF(0, 0, 140, 90)).isEmpty()) {
            shapePathsOk = false;
        }
        if (MakeShapeIcon(type, QColor(58, 64, 80), 18).isNull()) {
            shapeIconsOk = false;
        }
        if (ShapeTypeName(type).isEmpty()) {
            shapePathsOk = false;
        }
    }
    ctx.Check(shapePathsOk, QStringLiteral("每种图形都能生成有效路径与名称"));
    ctx.Check(shapeIconsOk, QStringLiteral("每种图形都能生成菜单图标"));

    ctx.Check(DefaultShapeSize(ShapeType::RECTANGLE).width() == DefaultShapeSize(ShapeType::RECTANGLE).height(),
              QStringLiteral("矩形默认插入为正方形"),
              QStringLiteral("size=%1x%2")
                  .arg(DefaultShapeSize(ShapeType::RECTANGLE).width())
                  .arg(DefaultShapeSize(ShapeType::RECTANGLE).height()));
    ctx.Check(ShapeTypeFromString(QStringLiteral("sector")) == ShapeType::SECTOR &&
                  ShapeTypeIsFilled(ShapeType::SECTOR),
              QStringLiteral("扇形可识别且可填充（弧线已并入曲线族，不再出现在形状类型表里）"));
    {
        SectorItem sector;
        sector.SetContentSize(QSizeF(200.0, 200.0));
        sector.SetArcAngles(0.0, 90.0);
        const QPainterPath sectorPath = sector.CurrentPath(sector.ContentRect());
        // 弧线几何与椭圆共用 EllipseShape，纯几何可直接验证角度采样。
        const QPainterPath arcPath = EllipseShape(QRectF(0.0, 0.0, 200.0, 200.0)).Arc(15.0, 140.0);
        ctx.Check(!sectorPath.isEmpty() && !arcPath.isEmpty() && sectorPath.boundingRect().width() <= 202.0 &&
                      arcPath.boundingRect().width() <= 202.0,
                  QStringLiteral("扇形与弧线按角度生成路径，外接框由内容矩形决定"));
        const QRectF ovalBounds = EllipseShape(QRectF(0.0, 0.0, 240.0, 120.0)).Arc(0.0, 90.0).boundingRect();
        ctx.Check(qAbs(ovalBounds.width() - 120.0) < 2.0 && qAbs(ovalBounds.height() - 60.0) < 2.0,
                  QStringLiteral("扇形与弧线由椭圆派生并跟随图元拉伸"));
        SectorItem copy;
        copy.FromJson(sector.ToJson());
        ctx.Check(copy.shapeType() == ShapeType::SECTOR && qFuzzyCompare(copy.SweepAngle() + 2.0, 92.0),
                  QStringLiteral("扇形角度随画布保存和恢复，半径字段已移除"));

        // 需求 6 扩展：扇形与弧线共用 ArcAngleAttributes，角度语义只有一份。
        ArcAngleAttributes* sectorAngles = dynamic_cast<ArcAngleAttributes*>(&sector);
        sectorAngles->SetStartAngle(45.0);
        ctx.Check(sectorAngles && qFuzzyCompare(sector.StartAngle() + 2.0, 47.0) &&
                      qFuzzyCompare(sector.SweepAngle() + 2.0, 92.0),
                  QStringLiteral("扇形通过角度属性基类读写起始角"),
                  QStringLiteral("start=%1 sweep=%2").arg(sector.StartAngle()).arg(sector.SweepAngle()));
        ArcItem baseArc;
        baseArc.SetArcAngles(45.0, 0.0);
        QJsonObject arcJson = baseArc.ToJson();
        ArcItem loadedArc;
        loadedArc.FromJson(arcJson);
        ctx.Check(qFuzzyCompare(baseArc.StartAngle() + 2.0, 47.0) && qFuzzyCompare(baseArc.SweepAngle() + 2.0, 122.0) &&
                      arcJson.contains(QStringLiteral("startAngle")) &&
                      qFuzzyCompare(loadedArc.StartAngle() + 2.0, 47.0) &&
                      qFuzzyCompare(loadedArc.SweepAngle() + 2.0, 122.0),
                  QStringLiteral("弧线通过角度属性基类兜底扫过角并保持 JSON 兼容"),
                  QStringLiteral("start=%1 sweep=%2").arg(loadedArc.StartAngle()).arg(loadedArc.SweepAngle()));
    }
    {
        // 扇形 / 弧线的选中框贴着实际轮廓，而不是整个内容矩形。
        SectorItem sector;
        sector.SetContentSize(QSizeF(200.0, 200.0));
        sector.SetArcAngles(0.0, 90.0);
        const QRectF sectorFrame = sector.FrameRect();
        const QRectF sectorBounds = sector.CurrentPath(sector.ContentRect()).boundingRect();
        ArcItem arc;
        arc.SetArcGeometry(QPointF(0.0, 0.0), QSizeF(200.0, 200.0), 15.0, 140.0, QColor(Qt::black), 2.0);
        const QRectF arcFrame = arc.FrameRect();
        ctx.Check(sectorFrame.width() < 200.0 && arcFrame.width() < 200.0 &&
                      qAbs(sectorFrame.right() - sectorBounds.right()) < 0.5 &&
                      qAbs(sectorFrame.top() - sectorBounds.top()) < 0.5,
                  QStringLiteral("扇形与弧线的包围框按轮廓最小覆盖矩形"),
                  QStringLiteral("sector=%1x%2 arc=%3x%4")
                      .arg(sectorFrame.width())
                      .arg(sectorFrame.height())
                      .arg(arcFrame.width())
                      .arg(arcFrame.height()));
    }
    {
        // §3.3 弧端点手柄：扇形两端各一个端点手柄；拖起始端固定结束端、拖结束端固定起始端，
        // 只改 start / sweep（中心与半轴不动），一次拖拽只落一条属性撤销记录。
        SectorItem* sector = new SectorItem;
        sector->SetContentSize(QSizeF(200.0, 200.0));
        sector->SetArcAngles(0.0, 90.0);
        CanvasScene scene;
        QUndoStack undo;
        scene.SetUndoStack(&undo);
        scene.AddItemsWithUndo({sector}, QStringLiteral("Add sector"));
        const QRectF frameBefore = sector->Frame();
        const HandleSpecList sectorSpecs = sector->CustomHandles();
        const bool sectorSpecsOk = sectorSpecs.size() == 2 && sectorSpecs.at(0).id == CanvasItem::CUSTOM_0 &&
                                   sectorSpecs.at(1).id == CanvasItem::CUSTOM_1 &&
                                   sectorSpecs.at(0).behavior == HandleBehavior::ARC_ENDPOINT &&
                                   sectorSpecs.at(1).behavior == HandleBehavior::ARC_ENDPOINT;
        const QPointF startHandle = sectorSpecs.isEmpty() ? QPointF() : sectorSpecs.constFirst().position;
        const QPointF expectedStart = EllipseShape::PointOnFrame(frameBefore, 0.0);
        ctx.Check(sectorSpecsOk && QLineF(startHandle, expectedStart).length() < 0.01,
                  QStringLiteral("扇形提供起始端 / 结束端两个弧端点手柄"),
                  QStringLiteral("count=%1 start=%2,%3 expected=%4,%5")
                      .arg(sectorSpecs.size())
                      .arg(startHandle.x())
                      .arg(startHandle.y())
                      .arg(expectedStart.x())
                      .arg(expectedStart.y()));

        // 拖结束端 90°→45°：起始端仍钉在 0°，扫过角收窄到 45°。
        CanvasItemInteraction endDrag(sector);
        const QPointF endTarget = sector->mapToScene(EllipseShape::PointOnFrame(frameBefore, 45.0));
        endDrag.BeginResize(CanvasItem::CUSTOM_1, sector->mapToScene(EllipseShape::PointOnFrame(frameBefore, 90.0)));
        endDrag.ApplyResize(endTarget, Qt::NoModifier);
        endDrag.FinishInteraction();
        const QRectF frameAfterEnd = sector->Frame();
        ctx.Check(qAbs(sector->StartAngle()) < 0.5 && qAbs(sector->SweepAngle() - 45.0) < 0.5 &&
                      QLineF(frameAfterEnd.center(), frameBefore.center()).length() < 0.5 &&
                      qAbs(frameAfterEnd.width() - frameBefore.width()) < 0.5 &&
                      qAbs(frameAfterEnd.height() - frameBefore.height()) < 0.5 && undo.count() == 2,
                  QStringLiteral("拖扇形结束端固定起始端、只改扫过角且不动外接框"),
                  QStringLiteral("start=%1 sweep=%2 frame=%3x%4 undo=%5")
                      .arg(sector->StartAngle())
                      .arg(sector->SweepAngle())
                      .arg(frameAfterEnd.width())
                      .arg(frameAfterEnd.height())
                      .arg(undo.count()));

        // 拖起始端 0°→-45°：结束端仍钉在 45°，扫过角重新张开。
        CanvasItemInteraction startDrag(sector);
        startDrag.BeginResize(CanvasItem::CUSTOM_0, sector->mapToScene(EllipseShape::PointOnFrame(frameAfterEnd, 0.0)));
        startDrag.ApplyResize(sector->mapToScene(EllipseShape::PointOnFrame(frameAfterEnd, -45.0)), Qt::NoModifier);
        ctx.Check(qAbs(sector->StartAngle() + 45.0) < 0.5 &&
                      qAbs(sector->StartAngle() + sector->SweepAngle() - 45.0) < 0.5,
                  QStringLiteral("拖扇形起始端固定结束端"),
                  QStringLiteral("start=%1 sweep=%2 end=%3")
                      .arg(sector->StartAngle())
                      .arg(sector->SweepAngle())
                      .arg(sector->StartAngle() + sector->SweepAngle()));
        startDrag.FinishInteraction();
        undo.undo();
        ctx.Check(qAbs(sector->StartAngle()) < 0.5 && qAbs(sector->SweepAngle() - 45.0) < 0.5,
                  QStringLiteral("撤销弧端点拖拽恢复起始角与扫过角"),
                  QStringLiteral("start=%1 sweep=%2").arg(sector->StartAngle()).arg(sector->SweepAngle()));

        // 端点拖到与另一端重合：至少留 1°，不能出现 0° 的“空扇形”。
        CanvasItemInteraction overlapDrag(sector);
        overlapDrag.BeginResize(CanvasItem::CUSTOM_1,
                                sector->mapToScene(EllipseShape::PointOnFrame(sector->Frame(), 45.0)));
        overlapDrag.ApplyResize(sector->mapToScene(EllipseShape::PointOnFrame(sector->Frame(), 0.0)), Qt::NoModifier);
        ctx.Check(qAbs(sector->SweepAngle() - 1.0) < 0.5, QStringLiteral("弧端点重合时扫过角保底 1°"),
                  QStringLiteral("sweep=%1").arg(sector->SweepAngle()));
        overlapDrag.FinishInteraction();
    }
    {
        // 弧线端点手柄：位置取椭圆外接框上的角度点，拖动只改角度、椭圆尺寸不变。
        ArcItem arc;
        arc.SetArcGeometry(QPointF(10.0, 20.0), QSizeF(200.0, 100.0), 15.0, 140.0, QColor(Qt::black), 2.0);
        const QSizeF ellipseBefore = arc.EllipseSize();
        const QRectF arcFrame = arc.Frame();
        const HandleSpecList arcSpecs = arc.CustomHandles();
        ctx.Check(
            arcSpecs.size() == 2 && arcSpecs.at(0).id == CanvasItem::CUSTOM_0 &&
                arcSpecs.at(1).id == CanvasItem::CUSTOM_1 && arcSpecs.at(0).behavior == HandleBehavior::ARC_ENDPOINT &&
                arcSpecs.at(1).behavior == HandleBehavior::ARC_ENDPOINT,
            QStringLiteral("弧线提供起始端 / 结束端两个弧端点手柄"), QStringLiteral("count=%1").arg(arcSpecs.size()));
        CanvasItemInteraction arcDrag(&arc);
        arcDrag.BeginResize(CanvasItem::CUSTOM_1,
                            arc.mapToScene(arcSpecs.isEmpty() ? QPointF() : arcSpecs.at(1).position));
        // 结束端从 155° 拖到 100°：起始角 15° 钉住，扫过角变 85°。
        const QPointF arcTarget = EllipseShape::PointOnFrame(arcFrame, 100.0);
        arcDrag.ApplyResize(arcTarget, Qt::NoModifier);
        const QSizeF ellipseAfter = arc.EllipseSize();
        ctx.Check(qAbs(arc.StartAngle() - 15.0) < 0.5 && qAbs(arc.SweepAngle() - 85.0) < 0.5 &&
                      qAbs(ellipseAfter.width() - ellipseBefore.width()) < 3.0 &&
                      qAbs(ellipseAfter.height() - ellipseBefore.height()) < 3.0,
                  QStringLiteral("拖弧线结束端只改扫过角、椭圆外接框不跳变"),
                  QStringLiteral("start=%1 sweep=%2 ellipse=%3x%4->%5x%6")
                      .arg(arc.StartAngle())
                      .arg(arc.SweepAngle())
                      .arg(ellipseBefore.width())
                      .arg(ellipseBefore.height())
                      .arg(ellipseAfter.width())
                      .arg(ellipseAfter.height()));
        arcDrag.FinishInteraction();
    }
    {
        // P3：弧角 / 正弦参数只按属性表暴露，普通描线不再带任何参数能力位。
        ArcItem arc;
        SineItem sine;
        StraightLineItem line;
        BraceItem brace;
        const bool arcOk = arc.Properties().Contains(ItemProperty::START_ANGLE) &&
                           arc.Properties().Contains(ItemProperty::SWEEP_ANGLE) &&
                           !arc.Properties().Contains(ItemProperty::SINE_CYCLES);
        const bool sineOk = sine.Properties().Contains(ItemProperty::SINE_CYCLES) &&
                            sine.Properties().Contains(ItemProperty::SINE_AMPLITUDE) &&
                            !sine.Properties().Contains(ItemProperty::START_ANGLE);
        const bool plainOk = !line.Properties().Contains(ItemProperty::START_ANGLE) &&
                             !line.Properties().Contains(ItemProperty::SWEEP_ANGLE) &&
                             !line.Properties().Contains(ItemProperty::SINE_CYCLES) &&
                             !brace.Properties().Contains(ItemProperty::SINE_AMPLITUDE);
        ctx.Check(arcOk && sineOk && plainOk, QStringLiteral("弧角与正弦参数走属性表、普通描线不带参数属性"),
                  QStringLiteral("arc=%1 sine=%2 plain=%3").arg(arcOk).arg(sineOk).arg(plainOk));

        // 曲线简化只属于手绘曲线：直线即便有多个节点也不进入简化流程。
        CanvasScene smoothScene;
        smoothScene.SetMode(ToolMode::EDIT);
        QUndoStack smoothUndo;
        smoothScene.SetUndoStack(&smoothUndo);
        auto* straight = new StraightLineItem;
        straight->SetEndpoints(QPointF(10.0, 10.0), QPointF(90.0, 60.0), QColor(Qt::black), 3.0);
        smoothScene.AttachItem(straight);
        straight->setSelected(true);
        ctx.Check(!smoothScene.CanSmoothSelectedStrokes() && smoothScene.SmoothSelectedStrokes(4.0) == 0 &&
                      smoothUndo.count() == 0,
                  QStringLiteral("直线不参与曲线简化，简化入口只认可简化曲线"),
                  QStringLiteral("can=%1 changed=%2 undo=%3")
                      .arg(smoothScene.CanSmoothSelectedStrokes())
                      .arg(smoothScene.SmoothSelectedStrokes(4.0))
                      .arg(smoothUndo.count()));
    }
    {
        // P4：文字属性只按属性表暴露（TEXT_CONTENT / TEXT_COLOR / TEXT_FONT），
        // 非文字图元不带这些行；写入正文后自然尺寸随之重算。
        TextItem text;
        RectangleItem rect;
        const ItemPropertySet textProperties = text.Properties();
        const bool textOk = textProperties.Contains(ItemProperty::TEXT_CONTENT) &&
                            textProperties.Contains(ItemProperty::TEXT_COLOR) &&
                            textProperties.Contains(ItemProperty::TEXT_FONT) &&
                            text.PropertyValue(ItemProperty::TEXT_COLOR).value<QColor>().isValid();
        const bool plainOk = !rect.Properties().Contains(ItemProperty::TEXT_CONTENT) &&
                             !rect.Properties().Contains(ItemProperty::TEXT_COLOR) &&
                             !rect.Properties().Contains(ItemProperty::TEXT_FONT);
        ctx.Check(textOk && plainOk, QStringLiteral("文字属性走属性表、非文字图元不带文字属性"),
                  QStringLiteral("text=%1 plain=%2").arg(textOk).arg(plainOk));

        text.SetPropertyValue(ItemProperty::TEXT_COLOR, QColor(QStringLiteral("#EF4444")));
        text.SetPropertyValue(ItemProperty::TEXT_CONTENT, QStringLiteral("属性协议文字"));
        ctx.Check(text.PropertyValue(ItemProperty::TEXT_COLOR).value<QColor>() == QColor(QStringLiteral("#EF4444")) &&
                      text.PropertyValue(ItemProperty::TEXT_CONTENT).toString() == QStringLiteral("属性协议文字") &&
                      text.ContentSize() ==
                          TextItem::Measure(QStringLiteral("属性协议文字"),
                                            text.PropertyValue(ItemProperty::TEXT_FONT).value<QFont>()),
                  QStringLiteral("文字正文与颜色可经属性协议写入并重算自然尺寸"),
                  QStringLiteral("size=%1x%2").arg(text.ContentSize().width()).arg(text.ContentSize().height()));
    }
    {
        // §3.4 中心点派生属性：对称图形上报 CENTER，写回只平移位置。
        RectangleItem rectangle;
        rectangle.SetContentSize(QSizeF(200.0, 100.0));
        rectangle.setPos(QPointF(100.0, 80.0));
        const bool rectangleHasCenter = rectangle.Properties().Contains(ItemProperty::CENTER);
        const QPointF rectangleCenter = rectangle.PropertyValue(ItemProperty::CENTER).toPointF();
        EllipseItem ellipse;
        ellipse.SetContentSize(QSizeF(160.0, 80.0));
        ellipse.setPos(QPointF(40.0, 60.0));
        TriangleItem triangle;
        TextItem text;
        ctx.Check(rectangleHasCenter && ellipse.Properties().Contains(ItemProperty::CENTER) &&
                      !triangle.Properties().Contains(ItemProperty::CENTER) &&
                      !text.Properties().Contains(ItemProperty::CENTER),
                  QStringLiteral("中心点只上报给对称图形"));
        ctx.Check(qAbs(rectangleCenter.x() - 200.0) < 0.01 && qAbs(rectangleCenter.y() - 130.0) < 0.01,
                  QStringLiteral("矩形中心点取内容矩形中心"),
                  QStringLiteral("%1,%2").arg(rectangleCenter.x()).arg(rectangleCenter.y()));

        const QSizeF sizeBefore = rectangle.ContentSize();
        rectangle.SetPropertyValue(ItemProperty::CENTER, QPointF(300.0, 200.0));
        const QPointF centerAfter = rectangle.CenterScenePoint();
        ctx.Check(qAbs(centerAfter.x() - 300.0) < 0.01 && qAbs(centerAfter.y() - 200.0) < 0.01 &&
                      rectangle.ContentSize() == sizeBefore,
                  QStringLiteral("写中心点只平移位置、不改尺寸"),
                  QStringLiteral("pos=%1,%2").arg(rectangle.pos().x()).arg(rectangle.pos().y()));
    }
    {
        // §3.1 三角形自由缩放：尺寸按输入直接生效，A 不变，B 按新框宽高反算。
        TriangleItem triangle;
        triangle.SetContentSize(QSizeF(120.0, 90.0));
        const qreal angleA = triangle.BottomAngle1();
        const qreal cotA = 1.0 / qTan(qDegreesToRadians(angleA));
        triangle.SetContentSize(QSizeF(160.0, 90.0));
        const QSizeF size = triangle.ContentSize();
        const qreal expectedB = qRadiansToDegrees(qAtan2(1.0, 160.0 / 90.0 - cotA));
        ctx.Check(qAbs(size.width() - 160.0) < 0.01 && qAbs(size.height() - 90.0) < 0.01 &&
                      qAbs(triangle.BottomAngle1() - angleA) < 0.05 && qAbs(triangle.BottomAngle2() - expectedB) < 0.05,
                  QStringLiteral("三角形宽度变化时高度保持、A 不变、B 反算"),
                  QStringLiteral("size=%1x%2 angles=%3/%4 expectedB=%5")
                      .arg(size.width())
                      .arg(size.height())
                      .arg(triangle.BottomAngle1())
                      .arg(triangle.BottomAngle2())
                      .arg(expectedB));
    }
    {
        // §3.1 三角形拖包围框：四角 / 边走通用自由缩放，A 不变、B 反算，对面边钉住。
        TriangleItem triangle;
        triangle.setPos(QPointF(60.0, 40.0));
        triangle.SetContentSize(QSizeF(120.0, 90.0));
        const qreal angle1Before = triangle.BottomAngle1();
        const qreal cot1 = 1.0 / qTan(qDegreesToRadians(angle1Before));

        // 拖底边：高度是输入，宽度不变，A 不变，B 按新宽高反算。
        CanvasItemInteraction bottomDrag(&triangle);
        bottomDrag.BeginResize(CanvasItem::BOTTOM, triangle.pos() + QPointF(60.0, 90.0));
        bottomDrag.ApplyResize(triangle.pos() + QPointF(60.0, 120.0), Qt::NoModifier);
        const qreal bottomExpectedB = qRadiansToDegrees(qAtan2(1.0, 120.0 / 120.0 - cot1));
        ctx.Check(qAbs(triangle.ContentSize().width() - 120.0) < 0.01 &&
                      qAbs(triangle.ContentSize().height() - 120.0) < 0.01 &&
                      qAbs(triangle.BottomAngle1() - angle1Before) < 0.05 &&
                      qAbs(triangle.BottomAngle2() - bottomExpectedB) < 0.05,
                  QStringLiteral("拖三角形底边：宽度不变、A 不变、B 反算"),
                  QStringLiteral("size=%1x%2 angles=%3/%4 expectedB=%5")
                      .arg(triangle.ContentSize().width())
                      .arg(triangle.ContentSize().height())
                      .arg(triangle.BottomAngle1())
                      .arg(triangle.BottomAngle2())
                      .arg(bottomExpectedB));

        // 拖上边：底边钉住，高度是输入，宽度不变。
        TriangleItem topTriangle;
        topTriangle.setPos(QPointF(60.0, 40.0));
        topTriangle.SetContentSize(QSizeF(120.0, 90.0));
        const qreal topA1 = topTriangle.BottomAngle1();
        const QPointF topBaseMidBefore = topTriangle.pos() + QPointF(60.0, 90.0);
        CanvasItemInteraction topDrag(&topTriangle);
        topDrag.BeginResize(CanvasItem::TOP, topTriangle.pos() + QPointF(60.0, 0.0));
        topDrag.ApplyResize(topTriangle.pos() + QPointF(60.0, -30.0), Qt::NoModifier);
        const QPointF topBaseMidAfter =
            topTriangle.pos() + QPointF(topTriangle.ContentSize().width() / 2.0, topTriangle.ContentSize().height());
        ctx.Check(qAbs(topBaseMidAfter.x() - topBaseMidBefore.x()) < 0.5 &&
                      qAbs(topBaseMidAfter.y() - topBaseMidBefore.y()) < 0.5 &&
                      qAbs(topTriangle.ContentSize().width() - 120.0) < 0.01 &&
                      qAbs(topTriangle.ContentSize().height() - 120.0) < 0.01 &&
                      qAbs(topTriangle.BottomAngle1() - topA1) < 0.05,
                  QStringLiteral("拖三角形上边：底边中点钉住、宽度不变、A 不变"),
                  QStringLiteral("baseMid=%1,%2->%3,%4 size=%5x%6")
                      .arg(topBaseMidBefore.x())
                      .arg(topBaseMidBefore.y())
                      .arg(topBaseMidAfter.x())
                      .arg(topBaseMidAfter.y())
                      .arg(topTriangle.ContentSize().width())
                      .arg(topTriangle.ContentSize().height()));

        // 拖左上角：右下角钉住，宽高自由变化（宽高比不再保持），A 不变、B 反算。
        TriangleItem cornerTriangle;
        cornerTriangle.setPos(QPointF(60.0, 40.0));
        cornerTriangle.SetContentSize(QSizeF(120.0, 90.0));
        const qreal cornerA1 = cornerTriangle.BottomAngle1();
        const QPointF cornerBefore(180.0, 130.0);
        CanvasItemInteraction cornerDrag(&cornerTriangle);
        cornerDrag.BeginResize(CanvasItem::TOP_LEFT, cornerTriangle.pos());
        cornerDrag.ApplyResize(cornerTriangle.pos() + QPointF(-20.0, -20.0), Qt::NoModifier);
        const QPointF cornerAfter =
            cornerTriangle.pos() + QPointF(cornerTriangle.ContentSize().width(), cornerTriangle.ContentSize().height());
        const qreal cornerExpectedB =
            qRadiansToDegrees(qAtan2(1.0, 140.0 / 110.0 - 1.0 / qTan(qDegreesToRadians(cornerA1))));
        ctx.Check(qAbs(cornerAfter.x() - cornerBefore.x()) < 0.5 && qAbs(cornerAfter.y() - cornerBefore.y()) < 0.5 &&
                      qAbs(cornerTriangle.ContentSize().width() - 140.0) < 0.01 &&
                      qAbs(cornerTriangle.ContentSize().height() - 110.0) < 0.01 &&
                      qAbs(cornerTriangle.BottomAngle1() - cornerA1) < 0.05 &&
                      qAbs(cornerTriangle.BottomAngle2() - cornerExpectedB) < 0.05,
                  QStringLiteral("拖三角形左上角：右下角钉住、自由缩放、A 不变 B 反算"),
                  QStringLiteral("corner=%1,%2->%3,%4 size=%5x%6 angles=%7/%8 expectedB=%9")
                      .arg(cornerBefore.x())
                      .arg(cornerBefore.y())
                      .arg(cornerAfter.x())
                      .arg(cornerAfter.y())
                      .arg(cornerTriangle.ContentSize().width())
                      .arg(cornerTriangle.ContentSize().height())
                      .arg(cornerTriangle.BottomAngle1())
                      .arg(cornerTriangle.BottomAngle2())
                      .arg(cornerExpectedB));
    }
    {
        // 钝角三角形：拖顶点越过 A/B 的垂线后，底角可大于 90°，底边两端在场景中不动。
        TriangleItem triangle;
        triangle.setPos(QPointF(50.0, 50.0));
        triangle.SetContentSize(QSizeF(128.0, 111.0));
        const QPointF aBefore = triangle.mapToScene(triangle.triangleVertex(ShapeAbstractItem::TriangleVertex::A));
        const QPointF bBefore = triangle.mapToScene(triangle.triangleVertex(ShapeAbstractItem::TriangleVertex::B));
        const qreal height =
            qAbs(aBefore.y() - triangle.mapToScene(triangle.triangleVertex(ShapeAbstractItem::TriangleVertex::C)).y());
        CanvasItemInteraction apexDrag(&triangle);
        apexDrag.BeginResize(CanvasItem::CUSTOM_0,
                             triangle.mapToScene(triangle.triangleVertex(ShapeAbstractItem::TriangleVertex::C)));
        apexDrag.ApplyResize(aBefore + QPointF(-30.0, -height), Qt::NoModifier);
        const QPointF aAfter = triangle.mapToScene(triangle.triangleVertex(ShapeAbstractItem::TriangleVertex::A));
        const QPointF bAfter = triangle.mapToScene(triangle.triangleVertex(ShapeAbstractItem::TriangleVertex::B));
        ctx.Check(triangle.BottomAngle1() > 90.0 && triangle.BottomAngle1() < 175.0 &&
                      triangle.BottomAngle1() + triangle.BottomAngle2() <= 175.01 &&
                      QLineF(aAfter, aBefore).length() < 0.5 && QLineF(bAfter, bBefore).length() < 0.5 &&
                      triangle.AngleReadoutVisible(),
                  QStringLiteral("拖三角形顶点可越过垂线得到钝角底角且 A/B 固定、角度浮层显示"),
                  QStringLiteral("angles=%1/%2 A=%3,%4->%5,%6 B=%7,%8->%9,%10")
                      .arg(triangle.BottomAngle1())
                      .arg(triangle.BottomAngle2())
                      .arg(aBefore.x())
                      .arg(aBefore.y())
                      .arg(aAfter.x())
                      .arg(aAfter.y())
                      .arg(bBefore.x())
                      .arg(bBefore.y())
                      .arg(bAfter.x())
                      .arg(bAfter.y()));
        apexDrag.FinishInteraction();
        ctx.Check(!triangle.AngleReadoutVisible(), QStringLiteral("松开顶点后隐藏三角形底角度数"));

        TriangleItem clampedTriangle;
        clampedTriangle.SetTriangleAngles(1.0, 179.0);
        ctx.Check(
            qAbs(clampedTriangle.BottomAngle1() - 5.0) < 0.01 && qAbs(clampedTriangle.BottomAngle2() - 170.0) < 0.01,
            QStringLiteral("三角形内角下限为 5°、两底角和不超过 175°"),
            QStringLiteral("angles=%1/%2").arg(clampedTriangle.BottomAngle1()).arg(clampedTriangle.BottomAngle2()));
    }
    {
        // 底角拖到 90° 时顶点 C 与角顶点横向重合：APEX 手柄必须下移避开角落手柄，
        // 否则把手柄顶在顶点上，用户点中的是 TOP_LEFT / TOP_RIGHT 缩放手柄。
        TriangleItem rightTriangle;
        rightTriangle.SetTriangleAngles(90.0, 45.0);
        const HandleSpecList apexSpecs = rightTriangle.CustomHandles();
        const bool apexOk = apexSpecs.size() == 1 && apexSpecs.constFirst().id == CanvasItem::CUSTOM_0 &&
                            apexSpecs.constFirst().behavior == HandleBehavior::ANGLE;
        const QPointF apexCenter = apexOk ? apexSpecs.constFirst().position : QPointF();
        const QPointF vertex = rightTriangle.triangleVertex(ShapeAbstractItem::TriangleVertex::C);
        ctx.Check(apexOk && apexCenter.y() > rightTriangle.ContentRect().top() + 12.0 &&
                      qAbs(apexCenter.x() - vertex.x()) < 0.01 && apexCenter.y() < rightTriangle.ContentRect().bottom(),
                  QStringLiteral("三角形顶点手柄下移避开角落手柄"),
                  QStringLiteral("apex=%1,%2 vertex=%3,%4")
                      .arg(apexCenter.x())
                      .arg(apexCenter.y())
                      .arg(vertex.x())
                      .arg(vertex.y()));
    }
    {
        // B1：角度读数移到基边外，命中区按屏幕尺寸保持稳定，三角形 C 顶点用水平双箭头。
        CanvasScene scene;
        scene.SetMode(ToolMode::EDIT);
        CanvasView view(&scene);
        view.resize(480, 360);
        TriangleItem triangle;
        scene.AttachItem(&triangle);
        triangle.setSelected(true);

        CanvasItemHandles handles(&triangle);
        const HandleSpecList specs = triangle.CustomHandles();
        const bool apexSpecOk = specs.size() == 1 && specs.constFirst().cursor == Qt::SizeHorCursor &&
                                qFuzzyCompare(specs.constFirst().hitPadding + 1.0, 15.0);
        const QPointF apex = apexSpecOk ? specs.constFirst().position : QPointF();
        const int hitAt100 = handles.HitAt(apex + QPointF(0.0, 16.0));
        view.setTransform(QTransform::fromScale(0.25, 0.25));
        const HandleSpecList scaledSpecs = triangle.CustomHandles();
        const QPointF scaledApex = scaledSpecs.isEmpty() ? QPointF() : scaledSpecs.constFirst().position;
        const int hitAt25 = handles.HitAt(scaledApex);
        const bool expandedAt25 = handles.HitRect(CanvasItem::CUSTOM_0).contains(scaledApex + QPointF(0.0, 44.0));
        ctx.Check(apexSpecOk && hitAt100 == CanvasItem::CUSTOM_0 && hitAt25 == CanvasItem::CUSTOM_0 && expandedAt25,
                  QStringLiteral("三角形顶点手柄为水平双箭头且 100% / 25% 缩放下命中区稳定"),
                  QStringLiteral(
                      "cursor=%1 padding=%2 hit100=%3 hit25=%4 scale=%5 apex=%6,%7 editable=%8 selected=%9 rect=%10,"
                      "%11,%12,%13")
                      .arg(static_cast<int>(specs.isEmpty() ? Qt::ArrowCursor : specs.constFirst().cursor))
                      .arg(specs.isEmpty() ? -1.0 : specs.constFirst().hitPadding)
                      .arg(hitAt100)
                      .arg(hitAt25)
                      .arg(view.transform().m11())
                      .arg(scaledApex.x())
                      .arg(scaledApex.y())
                      .arg(triangle.IsEditable())
                      .arg(triangle.isSelected())
                      .arg(handles.HitRect(CanvasItem::CUSTOM_0).left())
                      .arg(handles.HitRect(CanvasItem::CUSTOM_0).top())
                      .arg(handles.HitRect(CanvasItem::CUSTOM_0).width())
                      .arg(handles.HitRect(CanvasItem::CUSTOM_0).height()));

        triangle.SetAngleReadoutVisible(true);
        const QRectF content = triangle.ContentRect();
        const QRectF bounds = triangle.boundingRect();
        ctx.Check(bounds.bottom() > content.bottom() + 20.0 && bounds.contains(triangle.shape().boundingRect()),
                  QStringLiteral("25% 缩放下三角形底角读数落在包围盒内且不被基边裁掉"),
                  QStringLiteral("contentBottom=%1 boundsBottom=%2").arg(content.bottom()).arg(bounds.bottom()));
        triangle.SetAngleReadoutVisible(false);
    }
    {
        // 平行四边形内角范围放开到 5–175°。
        auto* parallelogram = CreateShapeItem(ShapeType::PARALLELOGRAM);
        parallelogram->SetPropertyValue(ItemProperty::PARALLELOGRAM_ANGLE, 5.0);
        const qreal low = parallelogram->PropertyValue(ItemProperty::PARALLELOGRAM_ANGLE).toDouble();
        parallelogram->SetPropertyValue(ItemProperty::PARALLELOGRAM_ANGLE, 175.0);
        const qreal high = parallelogram->PropertyValue(ItemProperty::PARALLELOGRAM_ANGLE).toDouble();
        ctx.Check(qAbs(low - 5.0) < 0.01 && qAbs(high - 175.0) < 0.01, QStringLiteral("平行四边形内角可到 5° / 175°"),
                  QStringLiteral("%1/%2").arg(low).arg(high));
        delete parallelogram;
    }
    {
        // 弧线属性窗按真实椭圆外接框显示长短半轴（不是弧线笔迹的包围盒），且可直接编辑：
        // 写入保持椭圆中心不动，并入属性窗的同一份撤销记录。
        ArcItem* arc = new ArcItem;
        arc->SetArcGeometry(QPointF(10.0, 20.0), QSizeF(200.0, 100.0), 15.0, 140.0, QColor(Qt::black), 2.0);
        const QSizeF ellipseSize = arc->EllipseSize();
        CanvasScene arcScene;
        QUndoStack arcUndo;
        arcScene.SetUndoStack(&arcUndo);
        arcScene.AddItemsWithUndo({arc}, QStringLiteral("Add arc"));
        ItemPropertiesDialog dialog(&arcScene, arc);
        dialog.show();
        QApplication::processEvents();
        QDoubleSpinBox* majorAxisValue =
            dialog.findChild<QDoubleSpinBox*>(QStringLiteral("ItemPropertiesMajorAxisValue"));
        QDoubleSpinBox* minorAxisValue =
            dialog.findChild<QDoubleSpinBox*>(QStringLiteral("ItemPropertiesMinorAxisValue"));
        ctx.Check(qAbs(ellipseSize.width() - 200.0) < 0.01 && qAbs(ellipseSize.height() - 100.0) < 0.01 &&
                      majorAxisValue && minorAxisValue && !majorAxisValue->isReadOnly() &&
                      !minorAxisValue->isReadOnly() && qAbs(majorAxisValue->value() - 100.0) < 0.01 &&
                      qAbs(minorAxisValue->value() - 50.0) < 0.01,
                  QStringLiteral("弧线属性窗显示椭圆长半轴 / 短半轴且可编辑"),
                  QStringLiteral("ellipse=%1x%2 major=%3 minor=%4")
                      .arg(ellipseSize.width())
                      .arg(ellipseSize.height())
                      .arg(majorAxisValue ? majorAxisValue->value() : -999.0)
                      .arg(minorAxisValue ? minorAxisValue->value() : -999.0));
        if (majorAxisValue) {
            majorAxisValue->setValue(120.0);
            dialog.accept();
            QApplication::processEvents();
            const QSizeF newEllipse = arc->EllipseSize();
            ctx.Check(qAbs(newEllipse.width() - 240.0) < 0.01 && qAbs(newEllipse.height() - 100.0) < 0.01 &&
                          arcUndo.count() == 2,
                      QStringLiteral("弧线长半轴编辑并入属性窗撤销记录"),
                      QStringLiteral("ellipse=%1x%2 undo=%3")
                          .arg(newEllipse.width())
                          .arg(newEllipse.height())
                          .arg(arcUndo.count()));
            arcUndo.undo();
            QApplication::processEvents();
            const QSizeF restored = arc->EllipseSize();
            ctx.Check(qAbs(restored.width() - 200.0) < 0.01 && qAbs(restored.height() - 100.0) < 0.01,
                      QStringLiteral("撤销恢复弧线椭圆外接框"),
                      QStringLiteral("ellipse=%1x%2").arg(restored.width()).arg(restored.height()));
        }
    }
    {
        // 属性窗为弧线补起始角 / 扫过角行：确认后走同一条撤销记录，可整体回退。
        ArcItem* arc = new ArcItem;
        arc->SetArcGeometry(QPointF(10.0, 20.0), QSizeF(200.0, 100.0), 15.0, 140.0, QColor(Qt::black), 2.0);
        CanvasScene arcScene;
        QUndoStack arcUndo;
        arcScene.SetUndoStack(&arcUndo);
        arcScene.AddItemsWithUndo({arc}, QStringLiteral("Add arc"));
        ItemPropertiesDialog dialog(&arcScene, arc);
        dialog.show();
        QApplication::processEvents();
        QSpinBox* startSpin = dialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesStartAngleValue"));
        QSpinBox* sweepSpin = dialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesSweepAngleValue"));
        ctx.Check(startSpin && sweepSpin && startSpin->value() == 15 && sweepSpin->value() == 140,
                  QStringLiteral("弧线属性窗显示起始角 / 扫过角"),
                  QStringLiteral("start=%1 sweep=%2")
                      .arg(startSpin ? startSpin->value() : -1)
                      .arg(sweepSpin ? sweepSpin->value() : -1));
        startSpin->setValue(30);
        sweepSpin->setValue(180);
        dialog.accept();
        QApplication::processEvents();
        ctx.Check(qAbs(arc->StartAngle() - 30.0) < 0.01 && qAbs(arc->SweepAngle() - 180.0) < 0.01 &&
                      arcUndo.count() == 2,
                  QStringLiteral("弧线角度改动并入属性窗撤销记录"),
                  QStringLiteral("start=%1 sweep=%2 undo=%3")
                      .arg(arc->StartAngle())
                      .arg(arc->SweepAngle())
                      .arg(arcUndo.count()));
        arcUndo.undo();
        ctx.Check(qAbs(arc->StartAngle() - 15.0) < 0.01 && qAbs(arc->SweepAngle() - 140.0) < 0.01,
                  QStringLiteral("撤销后弧线角度还原"),
                  QStringLiteral("start=%1 sweep=%2").arg(arc->StartAngle()).arg(arc->SweepAngle()));
    }
    {
        // 属性窗为正弦波补周期 / 振幅行：确认后波形重建且可撤销。
        SineItem* wave = new SineItem;
        wave->SetWaveGeometry(QPointF(10.0, 20.0), QSizeF(200.0, 100.0), QColor(Qt::black), 2.0);
        CanvasScene waveScene;
        QUndoStack waveUndo;
        waveScene.SetUndoStack(&waveUndo);
        waveScene.AddItemsWithUndo({wave}, QStringLiteral("Add wave"));
        ItemPropertiesDialog dialog(&waveScene, wave);
        dialog.show();
        QApplication::processEvents();
        QSpinBox* cyclesSpin = dialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesSineCyclesValue"));
        QSpinBox* amplitudeSpin = dialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesSineAmplitudeValue"));
        ctx.Check(cyclesSpin && amplitudeSpin && cyclesSpin->value() == wave->params().cycles &&
                      amplitudeSpin->value() == qRound(wave->params().amplitude),
                  QStringLiteral("正弦属性窗显示周期 / 振幅"),
                  QStringLiteral("cycles=%1 amplitude=%2")
                      .arg(cyclesSpin ? cyclesSpin->value() : -1)
                      .arg(amplitudeSpin ? amplitudeSpin->value() : -1));
        cyclesSpin->setValue(6);
        amplitudeSpin->setValue(120);
        dialog.accept();
        QApplication::processEvents();
        ctx.Check(wave->params().cycles == 6 && qAbs(wave->params().amplitude - 120.0) < 0.01 && waveUndo.count() == 2,
                  QStringLiteral("正弦参数改动并入属性窗撤销记录"),
                  QStringLiteral("cycles=%1 amplitude=%2 undo=%3")
                      .arg(wave->params().cycles)
                      .arg(wave->params().amplitude)
                      .arg(waveUndo.count()));
    }
    {
        // 底角 A / B 的图标：开口方向不同（A 朝右、B 朝左），且都不是空白图标。
        const auto iconImage = [](IconId id) {
            const QIcon icon = MakeIcon(id, QColor(0, 0, 0), 22);
            return icon.pixmap(QSize(44, 44)).toImage();
        };
        const QImage right = iconImage(IconId::ANGLE_RIGHT);
        const QImage left = iconImage(IconId::ANGLE_LEFT);
        const auto hasInk = [](const QImage& image) {
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    if (image.pixelColor(x, y).alpha() > 0) {
                        return true;
                    }
                }
            }
            return false;
        };
        ctx.Check(hasInk(right) && hasInk(left) && right != left,
                  QStringLiteral("底角 A / B 图标分别为开口朝右、朝左且都不是空白"));
        // 正弦周期 / 振幅图标：波峰下左右双向箭头与波峰右上下双向箭头，两者互不相同，
        // 也不再复用通用的旋转图标。
        const QImage sineCycles = iconImage(IconId::SINE_CYCLES);
        const QImage sineAmplitude = iconImage(IconId::SINE_AMPLITUDE);
        const QImage rotate = iconImage(IconId::ROTATE_CCW);
        ctx.Check(hasInk(sineCycles) && hasInk(sineAmplitude) && sineCycles != sineAmplitude && sineCycles != rotate &&
                      sineAmplitude != rotate,
                  QStringLiteral("正弦周期 / 振幅使用独立波形箭头图标"));
        // 双向箭头：周期图标的箭杆两端（左右下角）都有墨迹，振幅图标的箭杆两端
        // （右上与右下）都有墨迹。
        const auto hasInkIn = [](const QImage& image, const QRect& region) {
            for (int y = region.top(); y <= region.bottom(); ++y) {
                for (int x = region.left(); x <= region.right(); ++x) {
                    if (image.valid(x, y) && image.pixelColor(x, y).alpha() > 0) {
                        return true;
                    }
                }
            }
            return false;
        };
        ctx.Check(hasInkIn(sineCycles, QRect(0, 30, 12, 14)) && hasInkIn(sineCycles, QRect(32, 30, 12, 14)) &&
                      hasInkIn(sineAmplitude, QRect(30, 0, 14, 12)) && hasInkIn(sineAmplitude, QRect(30, 32, 14, 12)),
                  QStringLiteral("周期图标为左右双向箭头、振幅图标为上下双向箭头"));
        // 椭圆长 / 短半轴图标：椭圆轮廓 + 左焦点 + 半轴线；两者互不相同，
        // 也不再复用通用的图形集合图标。
        const QImage axisMajor = iconImage(IconId::AXIS_MAJOR);
        const QImage axisMinor = iconImage(IconId::AXIS_MINOR);
        const QImage shapesIcon = iconImage(IconId::SHAPES);
        ctx.Check(hasInk(axisMajor) && hasInk(axisMinor) && axisMajor != axisMinor && axisMajor != shapesIcon &&
                      axisMinor != shapesIcon,
                  QStringLiteral("长 / 短半轴使用独立的焦点半轴图标"));
        // 长半轴图标在左焦点右侧的横线上有墨迹，短半轴图标在右焦点与右顶点
        // 之间的横线上有墨迹（x=40 逻辑 20，既不在焦点圆点也不在椭圆轮廓上）。
        ctx.Check(hasInkIn(axisMajor, QRect(24, 18, 18, 10)) && hasInkIn(axisMinor, QRect(40, 23, 1, 3)),
                  QStringLiteral("长 / 短半轴图标分别为左、右焦点横线到右顶点"));
    }

    QList<ShapeAbstractItem*> shapeItems;
    const QColor shapeStroke(QStringLiteral("#1D4ED8"));
    const QColor shapeFill(29, 78, 216, 40);
    for (int i = 0; i < shapeTypes.size(); ++i) {
        const ShapeType type = shapeTypes.at(i);
        auto* item = CreateShapeItem(type);

        QSizeF size(150, 90);
        if (type == ShapeType::ARROW || type == ShapeType::DOUBLE_ARROW) {
            size = QSizeF(180, 80);
        } else if (type == ShapeType::STAR || type == ShapeType::TRIANGLE) {
            size = QSizeF(120, 110);
        }

        // 图形增多到 19 种后 4 列要排 5 行，行距压到 190 保证最后一行仍落在 1000 高的测试画布内。
        const int column = i % 4;
        const int row = i / 4;
        const QPointF cell(40.0 + column * 240.0, 40.0 + row * 190.0);
        item->setPos(cell + QPointF((220.0 - size.width()) / 2.0, (220.0 - size.height()) / 2.0));
        item->SetContentSize(size);
        item->SetStyle(shapeStroke, ShapeTypeIsFilled(type) ? shapeFill : QColor(Qt::transparent), 4.0);
        shapeScene.AddItemsWithUndo({item}, QStringLiteral("插入%1").arg(ShapeTypeName(type)));
        shapeItems.append(item);

        ctx.Check(item->Kind() == QStringLiteral("shape") && item->shapeType() == type &&
                      item->LayerId() == shapeScene.ActiveLayerId() && item->ZOrder() == i + 1,
                  QStringLiteral("图形图元 %1 进入当前图层且高度唯一").arg(ShapeTypeName(type)),
                  QStringLiteral("layer=%1 height=%2").arg(item->LayerId()).arg(item->ZOrder()));
    }
    ctx.Check(shapeScene.CanvasItemCount() == shapeTypes.size(), QStringLiteral("所有常见图形加入场景"));
    ctx.Check(shapeScene.LayerHeightsUnique(shapeScene.ActiveLayerId()), QStringLiteral("图形同层 height 唯一"));

    const QImage shapesRendered = shapeScene.RenderImage(shapeScene.CanvasRect(), 1.0);
    bool everyShapeVisible = !shapesRendered.isNull();
    QString invisibleShape;
    for (ShapeAbstractItem* item : shapeItems) {
        const QRect area =
            item->SceneContentRect().toAlignedRect().adjusted(-3, -3, 3, 3).intersected(shapesRendered.rect());
        int inkPixels = 0;
        for (int y = area.top(); y <= area.bottom(); ++y) {
            for (int x = area.left(); x <= area.right(); ++x) {
                if (shapesRendered.pixelColor(x, y).alpha() > 0) {
                    ++inkPixels;
                }
            }
        }
        if (inkPixels < 20) {
            everyShapeVisible = false;
            invisibleShape = ShapeTypeName(item->shapeType());
            break;
        }
    }
    ctx.Check(everyShapeVisible, QStringLiteral("每种图形都实际渲染到画板"), invisibleShape);

    ShapeAbstractItem* lastShape = shapeItems.constLast();
    shapeUndo.undo();
    ctx.Check(shapeScene.CanvasItemCount() == shapeTypes.size() - 1, QStringLiteral("撤销插入图形"));
    shapeUndo.redo();
    ctx.Check(shapeScene.CanvasItemCount() == shapeTypes.size() && lastShape->scene() == &shapeScene,
              QStringLiteral("重做插入图形"));

    ShapeAbstractItem* firstShape = shapeItems.constFirst();
    const QPointF shapePosBefore = firstShape->pos();
    const QSizeF shapeSizeBefore = firstShape->ContentSize();
    const ItemGeometry shapeBefore{firstShape, shapePosBefore, shapeSizeBefore};
    firstShape->setPos(shapePosBefore + QPointF(28, 20));
    firstShape->SetContentSize(shapeSizeBefore + QSizeF(24, 16));
    const ItemGeometry shapeAfter{firstShape, firstShape->pos(), firstShape->ContentSize()};
    shapeScene.PushGeometry({shapeBefore}, {shapeAfter}, QStringLiteral("缩放图形"));
    shapeUndo.undo();
    ctx.Check(firstShape->pos() == shapePosBefore && firstShape->ContentSize() == shapeSizeBefore,
              QStringLiteral("撤销图形移动与缩放"));
    shapeUndo.redo();
    ctx.Check(firstShape->pos() != shapePosBefore && firstShape->ContentSize() != shapeSizeBefore,
              QStringLiteral("重做图形移动与缩放"));

    ctx.Check(shapeScene.AdjustItemHeight(firstShape, 1), QStringLiteral("图形支持层内高度调整"));
    ctx.Check(firstShape->ZOrder() == 2 && shapeScene.LayerHeightsUnique(firstShape->LayerId()),
              QStringLiteral("图形高度交换后仍然唯一"));
    shapeUndo.undo();
    ctx.Check(firstShape->ZOrder() == 1, QStringLiteral("撤销图形高度调整"));

    const QJsonObject shapeRoot = shapeScene.ToJson();
    const QByteArray shapeBytes = QJsonDocument(shapeRoot).toJson(QJsonDocument::Compact);
    ctx.Check(shapeBytes.contains("\"kind\":\"shape\"") && shapeBytes.contains("\"shapeType\":\"rectangle\"") &&
                  shapeBytes.contains("\"strokeColor\"") && shapeBytes.contains("\"fillColor\""),
              QStringLiteral("画布保存图形类型、描边与填充"));

    CanvasScene shapeLoaded;
    QUndoStack shapeLoadedUndo;
    shapeLoaded.SetUndoStack(&shapeLoadedUndo);
    QString shapeLoadError;
    ctx.Check(shapeLoaded.FromJson(shapeRoot, &shapeLoadError), QStringLiteral("画布反序列化图形图元"), shapeLoadError);
    ctx.Check(shapeLoaded.CanvasItemCount() == shapeItems.size(), QStringLiteral("画布恢复全部图形"));
    ctx.Check(shapeLoaded.LayerHeightsUnique(shapeLoaded.ActiveLayerId()),
              QStringLiteral("画布恢复图形后 height 唯一"));
    QSet<QString> loadedShapeTypes;
    ShapeAbstractItem* loadedRectangle = nullptr;
    int loadedSineCurves = 0;
    for (CanvasItem* item : shapeLoaded.CanvasItems()) {
        if (auto* shape = dynamic_cast<ShapeAbstractItem*>(item)) {
            loadedShapeTypes.insert(ShapeTypeToString(shape->shapeType()));
            if (shape->shapeType() == ShapeType::RECTANGLE && !loadedRectangle) {
                loadedRectangle = shape;
            }
        } else if (dynamic_cast<CurveItem*>(item)) {
            ++loadedSineCurves;
        }
    }
    // 形状类型只剩有面积图形，直线 / 弧线 / 正弦已并入曲线族，恢复出的形状数量与类型表一致。
    ctx.Check(loadedShapeTypes.size() == shapeTypes.size() && loadedSineCurves == 0,
              QStringLiteral("画布恢复全部有面积图形类型"),
              QStringLiteral("shapes=%1 curves=%2").arg(loadedShapeTypes.size()).arg(loadedSineCurves));
    ctx.Check(loadedRectangle && loadedRectangle->StrokeColor() == shapeStroke &&
                  loadedRectangle->FillColor().alpha() == shapeFill.alpha() &&
                  qAbs(loadedRectangle->StrokeWidth() - 4.0) < 0.01,
              QStringLiteral("画布恢复图形描边、填充和线宽"));

    ctx.shapeRoot = shapeRoot;

    // 需求 6：箭头样式枚举、直线与曲线的两端箭头，以及 JSON 兼容。
    {
        bool stringRoundTrip = true;
        for (ArrowStyle style : AllArrowStyles()) {
            if (ArrowStyleFromString(ArrowStyleToString(style)) != style) {
                stringRoundTrip = false;
                break;
            }
        }
        ctx.Check(stringRoundTrip && !ArrowStyleIsHead(ArrowStyle::NONE) && ArrowStyleIsHead(ArrowStyle::OPEN_ARROW) &&
                      AllArrowStyles().size() == 6,
                  QStringLiteral("箭头样式字符串往返且共 6 种"));

        StraightLineItem arrowLine;
        arrowLine.SetEndpoints(QPointF(0.0, 0.0), QPointF(200.0, 0.0), QColor(20, 20, 20), 3.0);
        ctx.Check(arrowLine.StartArrow() == ArrowStyle::NONE && arrowLine.EndArrow() == ArrowStyle::NONE,
                  QStringLiteral("直线默认两端无箭头"));
        arrowLine.SetArrowStyles(ArrowStyle::BAR, ArrowStyle::OPEN_ARROW);
        ctx.Check(arrowLine.StartArrow() == ArrowStyle::BAR && arrowLine.EndArrow() == ArrowStyle::OPEN_ARROW,
                  QStringLiteral("直线两端箭头写入曲线属性"));
        ctx.Check(arrowLine.DisplayName() != MainWindow::tr("Straight Line"),
                  QStringLiteral("带箭头直线显示名与直线类型名区分"));

        const QJsonObject arrowJson = arrowLine.ToJson();
        StraightLineItem reloadedLine;
        reloadedLine.FromJson(arrowJson);
        ctx.Check(reloadedLine.StartArrow() == ArrowStyle::BAR && reloadedLine.EndArrow() == ArrowStyle::OPEN_ARROW,
                  QStringLiteral("直线箭头 JSON 往返"));
        QJsonObject legacyLineJson = arrowJson;
        legacyLineJson.remove(QStringLiteral("startArrow"));
        legacyLineJson.remove(QStringLiteral("endArrow"));
        StraightLineItem legacyLine;
        legacyLine.FromJson(legacyLineJson);
        ctx.Check(legacyLine.StartArrow() == ArrowStyle::NONE && legacyLine.EndArrow() == ArrowStyle::NONE,
                  QStringLiteral("缺箭头字段的直线 JSON 读入后默认无箭头"));

        CurveItem arrowCurve;
        arrowCurve.SetStrokePoints({QPointF(0.0, 100.0), QPointF(80.0, 20.0), QPointF(160.0, 100.0)},
                                   QColor(20, 20, 20), 3.0);
        arrowCurve.SetArrowStyles(ArrowStyle::NONE, ArrowStyle::TRIANGLE_FILLED);
        const QJsonObject curveJson = arrowCurve.ToJson();
        CurveItem reloadedCurve;
        reloadedCurve.FromJson(curveJson);
        ctx.Check(reloadedCurve.EndArrow() == ArrowStyle::TRIANGLE_FILLED, QStringLiteral("曲线箭头 JSON 往返"));

        const int headLength = qRound(qMax(4.0, 3.0 * 4.0));
        const QPainterPath filledHead =
            ArrowHeadPath(QPointF(0.0, 0.0), QPointF(-1.0, 0.0), 3.0, ArrowStyle::TRIANGLE_FILLED);
        ctx.Check(!filledHead.isEmpty() && qAbs(filledHead.boundingRect().width() - headLength) < 1.0,
                  QStringLiteral("箭头头长等于 4 倍线宽"),
                  QStringLiteral("width=%1 expected=%2").arg(filledHead.boundingRect().width()).arg(headLength));
        ctx.Check(ArrowHeadPath(QPointF(0.0, 0.0), QPointF(-1.0, 0.0), 3.0, ArrowStyle::NONE).isEmpty(),
                  QStringLiteral("无样式不生成箭头几何"));
    }
}
