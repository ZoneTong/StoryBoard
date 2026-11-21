#include "Alignment.h"
#include "ArcItem.h"
#include "ArrowStyle.h"
#include "BoolOps.h"
#include "CanvasItemInteraction.h"
#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CanvasView.h"
#include "Commands.h"
#include "CurveItem.h"
#include "CustomShapeTemplates.h"
#include "EllipseShape.h"
#include "ExportDialog.h"
#include "I18n.h"
#include "IconFactory.h"
#include "ItemPropertiesDialog.h"
#include "LayerPanel.h"
#include "LineArtConverter.h"
#include "LineArtDialog.h"
#include "LineArtPresets.h"
#include "LineItemAbstract.h"
#include "MainWindow.h"
#include "PolygonItem.h"
#include "PropertiesPanel.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "ShapeItem.h"
#include "ShapePathBuilder.h"
#include "SineItem.h"
#include "StraightLineItem.h"
#include "TextItem.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"
#include "selftest/SelftestHelpers.h"
#include "shapes/AllShapes.h"

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
    // 菱形已是独立类型（DiamondItem，派生自平行四边形）。
    ctx.Check(shapeTypes.size() == 17, QStringLiteral("提供 17 种有面积图形（直线/弧线/正弦已并入曲线族）"),
              QStringLiteral("count=%1").arg(shapeTypes.size()));

    // 透明度统一为“内容透明度”：不压 QGraphicsItem，只压 PaintContent() 的像素。
    {
        CanvasScene opacityScene;
        opacityScene.ApplyCanvasSize(QSizeF(400, 300));
        opacityScene.SetShowGrid(false);
        ShapeItem* underRect = CreateShapeItem(ShapeType::RECTANGLE);
        underRect->SetFillColor(QColor(30, 80, 200));
        underRect->SetStrokeWidth(0.0);
        underRect->SetContentSize(QSizeF(80, 80));
        underRect->setPos(QPointF(20, 20));
        opacityScene.AddItemsWithUndo({underRect}, QStringLiteral("添加底层矩形"));

        ShapeItem* opacityRect = CreateShapeItem(ShapeType::RECTANGLE);
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
        ShapeItem* rect = CreateShapeItem(ShapeType::RECTANGLE);
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
    // 需求 9：平行四边形默认 60° 内角、底边 2 格 = 64px，角度钳在 5–175°。
    {
        ParallelogramItem para;
        const QSizeF paraSize = DefaultShapeSize(ShapeType::PARALLELOGRAM);
        const QPainterPath defaultPath = para.CurrentPath(QRectF(0.0, 0.0, paraSize.width(), paraSize.height()));
        const qreal defaultBase = QLineF(defaultPath.elementAt(0), defaultPath.elementAt(1)).length();
        ctx.Check(qAbs(defaultBase - 64.0) < 0.1 && qAbs(paraSize.height() - 64.0) < 0.1,
                  QStringLiteral("平行四边形默认底边 64px、高 64px"),
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
        ctx.Check(qAbs(paraState.parallelogramAngle - 170.0) < 0.01, QStringLiteral("平行四边形内角进入状态快照"),
                  QStringLiteral("angle=%1").arg(paraState.parallelogramAngle));
    }
    // 需求：内角旋钮绕左下顶点拖动，底边长度全程不变；锐角在右、钝角在左，可连续跨过 90°。
    // 旋钮下移到框内 22px，90° 时不再与任何角手柄重合。
    {
        ParallelogramItem para;
        bool ok = false;
        auto knobInScene = [&para, &ok]() {
            const QPointF local = para.CustomHandleCenter(CanvasItem::SKEW, &ok);
            return para.mapToScene(local);
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
            ok && acuteKnob.x() > rightKnob.x() && rightKnob.x() > obtuseKnob.x() &&
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
        ctx.Check(!rect.SkewHandleEnabled() && !rect.ApexAngleHandle(),
                  QStringLiteral("矩形不显示内角倾斜旋钮与顶点手柄"));
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
        ShapeItem* diamondBase = &diamond;
        ctx.Check(dynamic_cast<ParallelogramItem*>(diamondBase) != nullptr && diamond.shapeType() == ShapeType::DIAMOND,
                  QStringLiteral("菱形派生自平行四边形且自带类型"),
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
        ctx.Check(!diamond.Properties().Contains(ItemProperty::PARALLELOGRAM_ANGLE) &&
                      qAbs(diamond.ContentSize().width() - diamondSize.width()) < 0.01,
                  QStringLiteral("菱形不开放内角属性"), QStringLiteral("angle=%1").arg(diamond.ParallelogramAngle()));
    }
    // 需求 4：弧线 / 正弦波这些曲线族子型同样能设两端箭头，箭头并入命中轮廓。
    {
        ArcItem arc;
        SineItem wave;
        arc.SetArcGeometry(QPointF(0.0, 0.0), QSizeF(120.0, 80.0), 15.0, 140.0, QColor(Qt::black), 2.0);
        wave.SetWaveGeometry(QPointF(0.0, 0.0), QSizeF(120.0, 80.0), QColor(Qt::black), 2.0);
        for (LineItemAbstract* line : {static_cast<LineItemAbstract*>(&arc), static_cast<LineItemAbstract*>(&wave)}) {
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
        if (ShapeItem::PathFor(type, QRectF(0, 0, 140, 90)).isEmpty()) {
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
        // 三角形一次性折算尺寸：拖宽即派生高度，不再二次改尺寸（原先的来回抖动来源）。
        TriangleItem triangle;
        triangle.SetContentSize(QSizeF(120.0, 90.0));
        const qreal derived = 120.0 / (2.0 / qTan(qDegreesToRadians(60.0)));
        const QSizeF first = triangle.ContentSize();
        triangle.SetContentSize(QSizeF(160.0, first.height()));
        const QSizeF second = triangle.ContentSize();
        ctx.Check(qAbs(first.height() - derived) < 0.6 && qAbs(second.width() - 160.0) < 0.01 &&
                      second.height() > first.height(),
                  QStringLiteral("三角形宽度变化时一次折算派生高度并等比增长"),
                  QStringLiteral("first=%1x%2 second=%3x%4")
                      .arg(first.width())
                      .arg(first.height())
                      .arg(second.width())
                      .arg(second.height()));
    }
    {
        // 三角形拖包围框：只能等比缩放，三个内角不变，手柄对面的点钉住。
        TriangleItem triangle;
        triangle.setPos(QPointF(60.0, 40.0));
        triangle.SetContentSize(QSizeF(120.0, 90.0));
        const qreal cot1 = 1.0 / qTan(qDegreesToRadians(triangle.BottomAngle1()));
        const qreal angle1Before = triangle.BottomAngle1();
        const qreal angle2Before = triangle.BottomAngle2();
        const auto apexScene = [&triangle, cot1]() {
            return triangle.pos() + QPointF(triangle.ContentSize().height() * cot1, 0.0);
        };
        const auto baseMidScene = [&triangle]() {
            const QSizeF size = triangle.ContentSize();
            return triangle.pos() + QPointF(size.width() / 2.0, size.height());
        };
        const QPointF apexBefore = apexScene();
        const QPointF baseMidBefore = baseMidScene();
        const qreal startHeight = triangle.ContentSize().height();
        const qreal startWidth = triangle.ContentSize().width();

        // 拖底边：对面点 = 上边中点（等腰三角时就是顶点）。
        CanvasItemInteraction bottomDrag(&triangle);
        bottomDrag.BeginResize(CanvasItem::BOTTOM, triangle.pos() + QPointF(60.0, startHeight));
        bottomDrag.ApplyResize(triangle.pos() + QPointF(60.0, startHeight + 30.0), Qt::NoModifier);
        const QPointF apexAfter = apexScene();
        const QPointF baseMidAfter = baseMidScene();
        ctx.Check(qAbs(apexAfter.x() - apexBefore.x()) < 0.5 && qAbs(apexAfter.y() - apexBefore.y()) < 0.5 &&
                      qAbs(triangle.BottomAngle1() - angle1Before) < 0.05 &&
                      qAbs(triangle.BottomAngle2() - angle2Before) < 0.05 &&
                      qAbs(triangle.ContentSize().width() / startWidth -
                           triangle.ContentSize().height() / startHeight) < 0.01 &&
                      triangle.ContentSize().height() > startHeight + 1.0 &&
                      qAbs(baseMidAfter.x() - baseMidBefore.x()) < 0.5 && baseMidAfter.y() - baseMidBefore.y() > 1.0,
                  QStringLiteral("拖三角形底边：顶点钉住、内角不变、宽高等比放大"),
                  QStringLiteral("apex=%1,%2->%3,%4 angles=%5/%6->%7/%8 baseMid=%9,%10->%11,%12")
                      .arg(apexBefore.x())
                      .arg(apexBefore.y())
                      .arg(apexAfter.x())
                      .arg(apexAfter.y())
                      .arg(angle1Before)
                      .arg(angle2Before)
                      .arg(triangle.BottomAngle1())
                      .arg(triangle.BottomAngle2())
                      .arg(baseMidBefore.x())
                      .arg(baseMidBefore.y())
                      .arg(baseMidAfter.x())
                      .arg(baseMidAfter.y()));

        // 拖上边：对面点 = 底边中点，钉住；底边两端随等比缩放外扩。
        TriangleItem topTriangle;
        topTriangle.setPos(QPointF(60.0, 40.0));
        topTriangle.SetContentSize(QSizeF(120.0, 90.0));
        const qreal topA1 = topTriangle.BottomAngle1();
        const qreal topA2 = topTriangle.BottomAngle2();
        const auto topBaseMid = [&topTriangle]() {
            const QSizeF size = topTriangle.ContentSize();
            return topTriangle.pos() + QPointF(size.width() / 2.0, size.height());
        };
        const QPointF topBaseMidBefore = topBaseMid();
        CanvasItemInteraction topDrag(&topTriangle);
        topDrag.BeginResize(CanvasItem::TOP, topTriangle.pos() + QPointF(60.0, 0.0));
        topDrag.ApplyResize(topTriangle.pos() + QPointF(60.0, -30.0), Qt::NoModifier);
        const QPointF topBaseMidAfter = topBaseMid();
        ctx.Check(qAbs(topBaseMidAfter.x() - topBaseMidBefore.x()) < 0.5 &&
                      qAbs(topBaseMidAfter.y() - topBaseMidBefore.y()) < 0.5 &&
                      qAbs(topTriangle.BottomAngle1() - topA1) < 0.05 &&
                      qAbs(topTriangle.BottomAngle2() - topA2) < 0.05 && topTriangle.ContentSize().width() > 120.0,
                  QStringLiteral("拖三角形上边：底边中点钉住、内角不变、宽度等比放大"),
                  QStringLiteral("baseMid=%1,%2->%3,%4 width=%5")
                      .arg(topBaseMidBefore.x())
                      .arg(topBaseMidBefore.y())
                      .arg(topBaseMidAfter.x())
                      .arg(topBaseMidAfter.y())
                      .arg(topTriangle.ContentSize().width()));

        // 拖左上角：对面点 = 右下角，钉住；宽高比与内角都不变。
        TriangleItem cornerTriangle;
        cornerTriangle.setPos(QPointF(60.0, 40.0));
        cornerTriangle.SetContentSize(QSizeF(120.0, 90.0));
        const qreal cornerA1 = cornerTriangle.BottomAngle1();
        const qreal cornerA2 = cornerTriangle.BottomAngle2();
        const auto bottomRight = [&cornerTriangle]() {
            const QSizeF size = cornerTriangle.ContentSize();
            return cornerTriangle.pos() + QPointF(size.width(), size.height());
        };
        const QPointF cornerBefore = bottomRight();
        const qreal cornerRatioBefore = cornerTriangle.ContentSize().width() / cornerTriangle.ContentSize().height();
        CanvasItemInteraction cornerDrag(&cornerTriangle);
        cornerDrag.BeginResize(CanvasItem::TOP_LEFT, cornerTriangle.pos());
        cornerDrag.ApplyResize(cornerTriangle.pos() + QPointF(-20.0, -20.0), Qt::NoModifier);
        const QPointF cornerAfter = bottomRight();
        const qreal cornerRatioAfter = cornerTriangle.ContentSize().width() / cornerTriangle.ContentSize().height();
        ctx.Check(qAbs(cornerAfter.x() - cornerBefore.x()) < 0.5 && qAbs(cornerAfter.y() - cornerBefore.y()) < 0.5 &&
                      qAbs(cornerRatioAfter - cornerRatioBefore) < 0.01 &&
                      qAbs(cornerTriangle.BottomAngle1() - cornerA1) < 0.05 &&
                      qAbs(cornerTriangle.BottomAngle2() - cornerA2) < 0.05,
                  QStringLiteral("拖三角形左上角：右下角钉住、宽高比与内角不变"),
                  QStringLiteral("corner=%1,%2->%3,%4 ratio=%5->%6")
                      .arg(cornerBefore.x())
                      .arg(cornerBefore.y())
                      .arg(cornerAfter.x())
                      .arg(cornerAfter.y())
                      .arg(cornerRatioBefore)
                      .arg(cornerRatioAfter));
    }
    {
        // 钝角三角形：拖顶点越过 A/B 的垂线后，底角可大于 90°，底边两端在场景中不动。
        TriangleItem triangle;
        triangle.setPos(QPointF(50.0, 50.0));
        triangle.SetContentSize(QSizeF(128.0, 111.0));
        const QPointF aBefore = triangle.mapToScene(triangle.triangleVertex(ShapeItem::TriangleVertex::A));
        const QPointF bBefore = triangle.mapToScene(triangle.triangleVertex(ShapeItem::TriangleVertex::B));
        const qreal height =
            qAbs(aBefore.y() - triangle.mapToScene(triangle.triangleVertex(ShapeItem::TriangleVertex::C)).y());
        CanvasItemInteraction apexDrag(&triangle);
        apexDrag.BeginResize(CanvasItem::APEX,
                             triangle.mapToScene(triangle.triangleVertex(ShapeItem::TriangleVertex::C)));
        apexDrag.ApplyResize(aBefore + QPointF(-30.0, -height), Qt::NoModifier);
        const QPointF aAfter = triangle.mapToScene(triangle.triangleVertex(ShapeItem::TriangleVertex::A));
        const QPointF bAfter = triangle.mapToScene(triangle.triangleVertex(ShapeItem::TriangleVertex::B));
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
        bool apexOk = false;
        const QPointF apexCenter = rightTriangle.CustomHandleCenter(CanvasItem::APEX, &apexOk);
        const QPointF vertex = rightTriangle.triangleVertex(ShapeItem::TriangleVertex::C);
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
        // 平行四边形内角范围放开到 5–175°。
        auto* parallelogram = CreateShapeItem(ShapeType::PARALLELOGRAM);
        parallelogram->SetParallelogramAngle(5.0);
        const qreal low = parallelogram->ParallelogramAngle();
        parallelogram->SetParallelogramAngle(175.0);
        const qreal high = parallelogram->ParallelogramAngle();
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
        QSpinBox* startSpin = dialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesArcStartAngleValue"));
        QSpinBox* sweepSpin = dialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesArcSweepAngleValue"));
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

    QList<ShapeItem*> shapeItems;
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
    for (ShapeItem* item : shapeItems) {
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

    ShapeItem* lastShape = shapeItems.constLast();
    shapeUndo.undo();
    ctx.Check(shapeScene.CanvasItemCount() == shapeTypes.size() - 1, QStringLiteral("撤销插入图形"));
    shapeUndo.redo();
    ctx.Check(shapeScene.CanvasItemCount() == shapeTypes.size() && lastShape->scene() == &shapeScene,
              QStringLiteral("重做插入图形"));

    ShapeItem* firstShape = shapeItems.constFirst();
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
    ShapeItem* loadedRectangle = nullptr;
    int loadedSineCurves = 0;
    for (CanvasItem* item : shapeLoaded.CanvasItems()) {
        if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
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
