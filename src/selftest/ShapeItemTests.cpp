#include "Alignment.h"
#include "ArrowStyle.h"
#include "BoolOps.h"
#include "CanvasItemInteraction.h"
#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CanvasView.h"
#include "Commands.h"
#include "CompositeShapeItem.h"
#include "CurveItem.h"
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


void selftest::runShapeItemTests(Context& ctx)
{
    // Shape items: common diagrams, quick insertion geometry and rendering.
    // ---------------------------------------------------------------
    CanvasScene shapeScene;
    QUndoStack shapeUndo;
    shapeUndo.setUndoLimit(100);
    shapeScene.setUndoStack(&shapeUndo);
    shapeScene.applyCanvasSize(QSizeF(1000, 1000));
    shapeScene.setShowGrid(false);

    const QList<ShapeType> shapeTypes = allShapeTypes();
    ctx.check(shapeTypes.size() == 13, QStringLiteral("提供 13 种常见图形"),
              QStringLiteral("count=%1").arg(shapeTypes.size()));
    ctx.check(shapeTypeFromString(QStringLiteral("circle")) == ShapeType::Ellipse &&
                  shapeTypeFromString(QStringLiteral("roundrect")) == ShapeType::Rectangle &&
                  shapeTypeFromString(QStringLiteral("double_arrow")) == ShapeType::DoubleArrow &&
                  shapeTypeFromString(QStringLiteral("leftbrace")) == ShapeType::BraceLeft &&
                  shapeTypeFromString(QStringLiteral("speech")) == ShapeType::Callout,
              QStringLiteral("图形类型字符串兼容常见别名（圆角矩形读成矩形）"));
    ctx.check(shapeTypeFromString(QStringLiteral("does-not-exist")) == ShapeType::Rectangle,
              QStringLiteral("未知图形类型回退为矩形"));

    // 需求 5：圆角矩形并入矩形后，旧 rounded_rectangle 文件读成「矩形 + 自动圆角」。
    {
        ShapeItem legacyRounded;
        QJsonObject legacyJson = legacyRounded.toJson();
        legacyJson.insert(QStringLiteral("shapeType"), QStringLiteral("rounded_rectangle"));
        legacyJson.remove(QStringLiteral("cornerRadius"));
        legacyJson.remove(QStringLiteral("cornerAuto"));
        ShapeItem loadedRounded;
        loadedRounded.fromJson(legacyJson);
        ctx.check(loadedRounded.shapeType() == ShapeType::Rectangle && loadedRounded.autoCorner(),
                  QStringLiteral("旧 rounded_rectangle 读成矩形自动圆角"));
    }

    bool shapePathsOk = true;
    bool shapeIconsOk = true;
    for (ShapeType type : shapeTypes) {
        if (ShapeItem::pathFor(type, QRectF(0, 0, 140, 90)).isEmpty()) {
            shapePathsOk = false;
        }
        if (makeShapeIcon(type, QColor(58, 64, 80), 18).isNull()) {
            shapeIconsOk = false;
        }
        if (shapeTypeName(type).isEmpty()) {
            shapePathsOk = false;
        }
    }
    ctx.check(shapePathsOk, QStringLiteral("每种图形都能生成有效路径与名称"));
    ctx.check(shapeIconsOk, QStringLiteral("每种图形都能生成菜单图标"));

    ctx.check(defaultShapeSize(ShapeType::Rectangle).width() == defaultShapeSize(ShapeType::Rectangle).height(),
              QStringLiteral("矩形默认插入为正方形"));
    ctx.check(shapeTypeFromString(QStringLiteral("sector")) == ShapeType::Sector &&
                  shapeTypeFromString(QStringLiteral("arc")) == ShapeType::Arc &&
                  shapeTypeIsFilled(ShapeType::Sector) && shapeTypeIsStrokeOnly(ShapeType::Arc),
              QStringLiteral("扇形与弧线可识别，扇形填充、弧线只描边"));
    {
        ShapeItem sector(ShapeType::Sector);
        sector.setContentSize(QSizeF(200.0, 200.0));
        sector.setArcAngles(0.0, 90.0);
        sector.setShapeRadius(60.0);
        const QPainterPath sectorPath = sector.currentPath(sector.contentRect());
        ShapeItem arc(ShapeType::Arc);
        arc.setContentSize(QSizeF(200.0, 200.0));
        arc.setArcAngles(15.0, 140.0);
        arc.setShapeRadius(55.0);
        const QPainterPath arcPath = arc.currentPath(arc.contentRect());
        ctx.check(!sectorPath.isEmpty() && !arcPath.isEmpty() && sectorPath.boundingRect().width() <= 122.0 &&
                      arcPath.boundingRect().width() <= 112.0,
                  QStringLiteral("扇形与弧线按角度和半径生成路径"));
        ShapeItem ovalArc(ShapeType::Arc);
        ovalArc.setContentSize(QSizeF(240.0, 120.0));
        ovalArc.setArcAngles(0.0, 90.0);
        const QRectF ovalBounds = ovalArc.currentPath(ovalArc.contentRect()).boundingRect();
        ctx.check(qAbs(ovalBounds.width() - 120.0) < 2.0 && qAbs(ovalBounds.height() - 60.0) < 2.0,
                  QStringLiteral("扇形与弧线由椭圆派生并跟随图元拉伸"));
        ShapeItem copy;
        copy.fromJson(sector.toJson());
        ctx.check(copy.shapeType() == ShapeType::Sector && qFuzzyCompare(copy.sweepAngle() + 2.0, 92.0) &&
                      qFuzzyCompare(copy.shapeRadius() + 2.0, 62.0),
                  QStringLiteral("扇形角度与半径随画布保存和恢复"));
    }
    {
        // 扇形 / 弧线的选中框贴着实际轮廓，而不是整个内容矩形。
        ShapeItem sector(ShapeType::Sector);
        sector.setContentSize(QSizeF(200.0, 200.0));
        sector.setArcAngles(0.0, 90.0);
        const QRectF sectorFrame = sector.frameRect();
        const QRectF sectorBounds = sector.currentPath(sector.contentRect()).boundingRect();
        ShapeItem arc(ShapeType::Arc);
        arc.setContentSize(QSizeF(200.0, 200.0));
        arc.setArcAngles(15.0, 140.0);
        const QRectF arcFrame = arc.frameRect();
        ctx.check(sectorFrame.width() < 200.0 && arcFrame.width() < 200.0 &&
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
        ShapeItem triangle(ShapeType::Triangle);
        triangle.setContentSize(QSizeF(120.0, 90.0));
        const qreal derived = 120.0 / (2.0 / qTan(qDegreesToRadians(60.0)));
        const QSizeF first = triangle.contentSize();
        triangle.setContentSize(QSizeF(160.0, first.height()));
        const QSizeF second = triangle.contentSize();
        ctx.check(qAbs(first.height() - derived) < 0.6 && qAbs(second.width() - 160.0) < 0.01 &&
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
        ShapeItem triangle(ShapeType::Triangle);
        triangle.setPos(QPointF(60.0, 40.0));
        triangle.setContentSize(QSizeF(120.0, 90.0));
        const qreal cot1 = 1.0 / qTan(qDegreesToRadians(triangle.bottomAngle1()));
        const qreal angle1Before = triangle.bottomAngle1();
        const qreal angle2Before = triangle.bottomAngle2();
        const auto apexScene = [&triangle, cot1]() {
            return triangle.pos() + QPointF(triangle.contentSize().height() * cot1, 0.0);
        };
        const auto baseMidScene = [&triangle]() {
            const QSizeF size = triangle.contentSize();
            return triangle.pos() + QPointF(size.width() / 2.0, size.height());
        };
        const QPointF apexBefore = apexScene();
        const QPointF baseMidBefore = baseMidScene();
        const qreal startHeight = triangle.contentSize().height();
        const qreal startWidth = triangle.contentSize().width();

        // 拖底边：对面点 = 上边中点（等腰三角时就是顶点）。
        CanvasItemInteraction bottomDrag(&triangle);
        bottomDrag.beginResize(CanvasItem::Bottom, triangle.pos() + QPointF(60.0, startHeight));
        bottomDrag.applyResize(triangle.pos() + QPointF(60.0, startHeight + 30.0), Qt::NoModifier);
        const QPointF apexAfter = apexScene();
        ctx.check(qAbs(apexAfter.x() - apexBefore.x()) < 0.5 && qAbs(apexAfter.y() - apexBefore.y()) < 0.5 &&
                      qAbs(triangle.bottomAngle1() - angle1Before) < 0.05 &&
                      qAbs(triangle.bottomAngle2() - angle2Before) < 0.05 &&
                      qAbs(triangle.contentSize().width() / startWidth -
                           triangle.contentSize().height() / startHeight) < 0.01 &&
                      triangle.contentSize().height() > startHeight + 1.0,
                  QStringLiteral("拖三角形底边：顶点钉住、内角不变、宽高等比放大"),
                  QStringLiteral("apex=%1,%2->%3,%4 angles=%5/%6->%7/%8")
                      .arg(apexBefore.x())
                      .arg(apexBefore.y())
                      .arg(apexAfter.x())
                      .arg(apexAfter.y())
                      .arg(angle1Before)
                      .arg(angle2Before)
                      .arg(triangle.bottomAngle1())
                      .arg(triangle.bottomAngle2()));

        // 拖上边：对面点 = 底边中点，钉住；底边两端随等比缩放外扩。
        ShapeItem topTriangle(ShapeType::Triangle);
        topTriangle.setPos(QPointF(60.0, 40.0));
        topTriangle.setContentSize(QSizeF(120.0, 90.0));
        const qreal topA1 = topTriangle.bottomAngle1();
        const qreal topA2 = topTriangle.bottomAngle2();
        const auto topBaseMid = [&topTriangle]() {
            const QSizeF size = topTriangle.contentSize();
            return topTriangle.pos() + QPointF(size.width() / 2.0, size.height());
        };
        const QPointF topBaseMidBefore = topBaseMid();
        CanvasItemInteraction topDrag(&topTriangle);
        topDrag.beginResize(CanvasItem::Top, topTriangle.pos() + QPointF(60.0, 0.0));
        topDrag.applyResize(topTriangle.pos() + QPointF(60.0, -30.0), Qt::NoModifier);
        const QPointF topBaseMidAfter = topBaseMid();
        ctx.check(qAbs(topBaseMidAfter.x() - topBaseMidBefore.x()) < 0.5 &&
                      qAbs(topBaseMidAfter.y() - topBaseMidBefore.y()) < 0.5 &&
                      qAbs(topTriangle.bottomAngle1() - topA1) < 0.05 &&
                      qAbs(topTriangle.bottomAngle2() - topA2) < 0.05 && topTriangle.contentSize().width() > 120.0,
                  QStringLiteral("拖三角形上边：底边中点钉住、内角不变、宽度等比放大"),
                  QStringLiteral("baseMid=%1,%2->%3,%4 width=%5")
                      .arg(topBaseMidBefore.x())
                      .arg(topBaseMidBefore.y())
                      .arg(topBaseMidAfter.x())
                      .arg(topBaseMidAfter.y())
                      .arg(topTriangle.contentSize().width()));

        // 拖左上角：对面点 = 右下角，钉住；宽高比与内角都不变。
        ShapeItem cornerTriangle(ShapeType::Triangle);
        cornerTriangle.setPos(QPointF(60.0, 40.0));
        cornerTriangle.setContentSize(QSizeF(120.0, 90.0));
        const qreal cornerA1 = cornerTriangle.bottomAngle1();
        const qreal cornerA2 = cornerTriangle.bottomAngle2();
        const auto bottomRight = [&cornerTriangle]() {
            const QSizeF size = cornerTriangle.contentSize();
            return cornerTriangle.pos() + QPointF(size.width(), size.height());
        };
        const QPointF cornerBefore = bottomRight();
        const qreal cornerRatioBefore = cornerTriangle.contentSize().width() / cornerTriangle.contentSize().height();
        CanvasItemInteraction cornerDrag(&cornerTriangle);
        cornerDrag.beginResize(CanvasItem::TopLeft, cornerTriangle.pos());
        cornerDrag.applyResize(cornerTriangle.pos() + QPointF(-20.0, -20.0), Qt::NoModifier);
        const QPointF cornerAfter = bottomRight();
        const qreal cornerRatioAfter = cornerTriangle.contentSize().width() / cornerTriangle.contentSize().height();
        ctx.check(qAbs(cornerAfter.x() - cornerBefore.x()) < 0.5 && qAbs(cornerAfter.y() - cornerBefore.y()) < 0.5 &&
                      qAbs(cornerRatioAfter - cornerRatioBefore) < 0.01 &&
                      qAbs(cornerTriangle.bottomAngle1() - cornerA1) < 0.05 &&
                      qAbs(cornerTriangle.bottomAngle2() - cornerA2) < 0.05,
                  QStringLiteral("拖三角形左上角：右下角钉住、宽高比与内角不变"),
                  QStringLiteral("corner=%1,%2->%3,%4 ratio=%5->%6")
                      .arg(cornerBefore.x())
                      .arg(cornerBefore.y())
                      .arg(cornerAfter.x())
                      .arg(cornerAfter.y())
                      .arg(cornerRatioBefore)
                      .arg(cornerRatioAfter));
    }

    QList<ShapeItem*> shapeItems;
    const QColor shapeStroke(QStringLiteral("#1D4ED8"));
    const QColor shapeFill(29, 78, 216, 40);
    for (int i = 0; i < shapeTypes.size(); ++i) {
        const ShapeType type = shapeTypes.at(i);
        auto* item = new ShapeItem(type);

        QSizeF size(150, 90);
        if (type == ShapeType::BraceLeft || type == ShapeType::BraceRight) {
            size = QSizeF(60, 160);
        } else if (type == ShapeType::Line) {
            size = QSizeF(160, 50);
        } else if (type == ShapeType::Arrow || type == ShapeType::DoubleArrow) {
            size = QSizeF(180, 80);
        } else if (type == ShapeType::Star || type == ShapeType::Triangle || type == ShapeType::Diamond) {
            size = QSizeF(120, 110);
        }

        const int column = i % 4;
        const int row = i / 4;
        const QPointF cell(40.0 + column * 240.0, 40.0 + row * 240.0);
        item->setPos(cell + QPointF((220.0 - size.width()) / 2.0, (220.0 - size.height()) / 2.0));
        item->setContentSize(size);
        item->setStyle(shapeStroke, shapeTypeIsFilled(type) ? shapeFill : QColor(Qt::transparent), 4.0);
        shapeScene.addItemsWithUndo({item}, QStringLiteral("插入%1").arg(shapeTypeName(type)));
        shapeItems.append(item);

        ctx.check(item->kind() == QStringLiteral("shape") && item->shapeType() == type &&
                      item->layerId() == shapeScene.activeLayerId() && item->height() == i + 1,
                  QStringLiteral("图形图元 %1 进入当前图层且高度唯一").arg(shapeTypeName(type)),
                  QStringLiteral("layer=%1 height=%2").arg(item->layerId()).arg(item->height()));
    }
    ctx.check(shapeScene.canvasItemCount() == shapeTypes.size(), QStringLiteral("所有常见图形加入场景"));
    ctx.check(shapeScene.layerHeightsUnique(shapeScene.activeLayerId()), QStringLiteral("图形同层 height 唯一"));

    const QImage shapesRendered = shapeScene.renderImage(shapeScene.canvasRect(), 1.0);
    bool everyShapeVisible = !shapesRendered.isNull();
    QString invisibleShape;
    for (ShapeItem* item : shapeItems) {
        const QRect area =
            item->sceneContentRect().toAlignedRect().adjusted(-3, -3, 3, 3).intersected(shapesRendered.rect());
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
            invisibleShape = shapeTypeName(item->shapeType());
            break;
        }
    }
    ctx.check(everyShapeVisible, QStringLiteral("每种图形都实际渲染到画板"), invisibleShape);

    ShapeItem* lastShape = shapeItems.constLast();
    shapeUndo.undo();
    ctx.check(shapeScene.canvasItemCount() == shapeTypes.size() - 1, QStringLiteral("撤销插入图形"));
    shapeUndo.redo();
    ctx.check(shapeScene.canvasItemCount() == shapeTypes.size() && lastShape->scene() == &shapeScene,
              QStringLiteral("重做插入图形"));

    ShapeItem* firstShape = shapeItems.constFirst();
    const QPointF shapePosBefore = firstShape->pos();
    const QSizeF shapeSizeBefore = firstShape->contentSize();
    const ItemGeometry shapeBefore{firstShape, shapePosBefore, shapeSizeBefore};
    firstShape->setPos(shapePosBefore + QPointF(28, 20));
    firstShape->setContentSize(shapeSizeBefore + QSizeF(24, 16));
    const ItemGeometry shapeAfter{firstShape, firstShape->pos(), firstShape->contentSize()};
    shapeScene.pushGeometry({shapeBefore}, {shapeAfter}, QStringLiteral("缩放图形"));
    shapeUndo.undo();
    ctx.check(firstShape->pos() == shapePosBefore && firstShape->contentSize() == shapeSizeBefore,
              QStringLiteral("撤销图形移动与缩放"));
    shapeUndo.redo();
    ctx.check(firstShape->pos() != shapePosBefore && firstShape->contentSize() != shapeSizeBefore,
              QStringLiteral("重做图形移动与缩放"));

    ctx.check(shapeScene.adjustItemHeight(firstShape, 1), QStringLiteral("图形支持层内高度调整"));
    ctx.check(firstShape->height() == 2 && shapeScene.layerHeightsUnique(firstShape->layerId()),
              QStringLiteral("图形高度交换后仍然唯一"));
    shapeUndo.undo();
    ctx.check(firstShape->height() == 1, QStringLiteral("撤销图形高度调整"));

    const QJsonObject shapeRoot = shapeScene.toJson();
    const QByteArray shapeBytes = QJsonDocument(shapeRoot).toJson(QJsonDocument::Compact);
    ctx.check(shapeBytes.contains("\"type\":\"shape\"") && shapeBytes.contains("\"shapeType\":\"rectangle\"") &&
                  shapeBytes.contains("\"strokeColor\"") && shapeBytes.contains("\"fillColor\""),
              QStringLiteral("画布保存图形类型、描边与填充"));

    CanvasScene shapeLoaded;
    QUndoStack shapeLoadedUndo;
    shapeLoaded.setUndoStack(&shapeLoadedUndo);
    QString shapeLoadError;
    ctx.check(shapeLoaded.fromJson(shapeRoot, &shapeLoadError), QStringLiteral("画布反序列化图形图元"), shapeLoadError);
    ctx.check(shapeLoaded.canvasItemCount() == shapeItems.size(), QStringLiteral("画布恢复全部图形"));
    ctx.check(shapeLoaded.layerHeightsUnique(shapeLoaded.activeLayerId()),
              QStringLiteral("画布恢复图形后 height 唯一"));
    QSet<QString> loadedShapeTypes;
    ShapeItem* loadedRectangle = nullptr;
    for (CanvasItem* item : shapeLoaded.canvasItems()) {
        if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
            loadedShapeTypes.insert(shapeTypeToString(shape->shapeType()));
            if (shape->shapeType() == ShapeType::Rectangle && !loadedRectangle) {
                loadedRectangle = shape;
            }
        }
    }
    ctx.check(loadedShapeTypes.size() == shapeTypes.size(), QStringLiteral("画布恢复全部 14 种图形类型"));
    ctx.check(loadedRectangle && loadedRectangle->strokeColor() == shapeStroke &&
                  loadedRectangle->fillColor().alpha() == shapeFill.alpha() &&
                  qAbs(loadedRectangle->strokeWidth() - 4.0) < 0.01,
              QStringLiteral("画布恢复图形描边、填充和线宽"));

    ctx.shapeRoot = shapeRoot;

    // 需求 6：箭头样式枚举、直线与曲线的两端箭头，以及 JSON 兼容。
    {
        bool stringRoundTrip = true;
        for (ArrowStyle style : allArrowStyles()) {
            if (arrowStyleFromString(arrowStyleToString(style)) != style) {
                stringRoundTrip = false;
                break;
            }
        }
        ctx.check(stringRoundTrip && !arrowStyleIsHead(ArrowStyle::None) && arrowStyleIsHead(ArrowStyle::OpenArrow) &&
                      allArrowStyles().size() == 6,
                  QStringLiteral("箭头样式字符串往返且共 6 种"));

        ShapeItem arrowLine;
        arrowLine.setShapeType(ShapeType::Line);
        ctx.check(arrowLine.startArrow() == ArrowStyle::None && arrowLine.endArrow() == ArrowStyle::None,
                  QStringLiteral("直线默认两端无箭头"));
        arrowLine.setArrowStyles(ArrowStyle::Bar, ArrowStyle::OpenArrow);
        const ShapeItemState arrowState = arrowLine.state();
        ctx.check(arrowState.startArrow == ArrowStyle::Bar && arrowState.endArrow == ArrowStyle::OpenArrow,
                  QStringLiteral("直线两端箭头写入 state"));
        ctx.check(arrowLine.displayName() != shapeTypeName(ShapeType::Line),
                  QStringLiteral("带箭头直线显示名与直线类型名区分"));

        arrowLine.setContentSize(QSizeF(200.0, 40.0));
        arrowLine.setStyle(QColor(20, 20, 20), QColor(20, 20, 20), 3.0);
        const QJsonObject arrowJson = arrowLine.toJson();
        ShapeItem reloadedLine;
        reloadedLine.fromJson(arrowJson);
        ctx.check(reloadedLine.startArrow() == ArrowStyle::Bar && reloadedLine.endArrow() == ArrowStyle::OpenArrow,
                  QStringLiteral("直线箭头 JSON 往返"));
        QJsonObject legacyLineJson = arrowJson;
        legacyLineJson.remove(QStringLiteral("startArrow"));
        legacyLineJson.remove(QStringLiteral("endArrow"));
        ShapeItem legacyLine;
        legacyLine.fromJson(legacyLineJson);
        ctx.check(legacyLine.startArrow() == ArrowStyle::None && legacyLine.endArrow() == ArrowStyle::None,
                  QStringLiteral("旧直线文件读入后默认无箭头"));

        CurveItem arrowCurve;
        arrowCurve.setStrokePoints({QPointF(0.0, 100.0), QPointF(80.0, 20.0), QPointF(160.0, 100.0)},
                                   QColor(20, 20, 20), 3.0);
        arrowCurve.setArrowStyles(ArrowStyle::None, ArrowStyle::TriangleFilled);
        const QJsonObject curveJson = arrowCurve.toJson();
        CurveItem reloadedCurve;
        reloadedCurve.fromJson(curveJson);
        ctx.check(reloadedCurve.endArrow() == ArrowStyle::TriangleFilled, QStringLiteral("曲线箭头 JSON 往返"));

        const int headLength = qRound(qMax(4.0, 3.0 * 4.0));
        const QPainterPath filledHead =
            arrowHeadPath(QPointF(0.0, 0.0), QPointF(-1.0, 0.0), 3.0, ArrowStyle::TriangleFilled);
        ctx.check(!filledHead.isEmpty() && qAbs(filledHead.boundingRect().width() - headLength) < 1.0,
                  QStringLiteral("箭头头长等于 4 倍线宽"),
                  QStringLiteral("width=%1 expected=%2").arg(filledHead.boundingRect().width()).arg(headLength));
        ctx.check(arrowHeadPath(QPointF(0.0, 0.0), QPointF(-1.0, 0.0), 3.0, ArrowStyle::None).isEmpty(),
                  QStringLiteral("无样式不生成箭头几何"));
    }
}
