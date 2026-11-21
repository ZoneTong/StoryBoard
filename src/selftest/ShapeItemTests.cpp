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
    // 菱形不再是独立类型，只剩平行四边形预置。
    ctx.Check(shapeTypes.size() == 16, QStringLiteral("提供 16 种有面积图形（直线/弧线/正弦已并入曲线族）"),
              QStringLiteral("count=%1").arg(shapeTypes.size()));
    // 需求 9：平行四边形默认 60° 内角、内容尺寸 110.85×64，角度钳在 15–165°。
    {
        ParallelogramItem para;
        const QSizeF paraSize = DefaultShapeSize(ShapeType::PARALLELOGRAM);
        ctx.Check(qAbs(paraSize.width() - 110.85) < 0.1 && qAbs(paraSize.height() - 64.0) < 0.1,
                  QStringLiteral("平行四边形默认尺寸 110.85×64"),
                  QStringLiteral("%1×%2").arg(paraSize.width()).arg(paraSize.height()));
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
        ctx.Check(qAbs(para.ParallelogramAngle() - 165.0) < 0.01, QStringLiteral("平行四边形内角钳在 15–165°"),
                  QStringLiteral("angle=%1").arg(para.ParallelogramAngle()));
        const ShapeItemState paraState = para.State();
        ctx.Check(qAbs(paraState.parallelogramAngle - 165.0) < 0.01, QStringLiteral("平行四边形内角进入状态快照"),
                  QStringLiteral("angle=%1").arg(paraState.parallelogramAngle));
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
