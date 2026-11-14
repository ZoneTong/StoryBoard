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
    ctx.check(shapeTypes.size() == 14, QStringLiteral("提供 14 种常见图形"),
              QStringLiteral("count=%1").arg(shapeTypes.size()));
    ctx.check(shapeTypeFromString(QStringLiteral("circle")) == ShapeType::Ellipse &&
                  shapeTypeFromString(QStringLiteral("roundrect")) == ShapeType::RoundedRectangle &&
                  shapeTypeFromString(QStringLiteral("double_arrow")) == ShapeType::DoubleArrow &&
                  shapeTypeFromString(QStringLiteral("leftbrace")) == ShapeType::BraceLeft &&
                  shapeTypeFromString(QStringLiteral("speech")) == ShapeType::Callout,
              QStringLiteral("图形类型字符串兼容常见别名"));
    ctx.check(shapeTypeFromString(QStringLiteral("does-not-exist")) == ShapeType::Rectangle,
              QStringLiteral("未知图形类型回退为矩形"));

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

    ctx.check(defaultShapeSize(ShapeType::Rectangle).width() == defaultShapeSize(ShapeType::Rectangle).height() &&
                  defaultShapeSize(ShapeType::RoundedRectangle).width() ==
                      defaultShapeSize(ShapeType::RoundedRectangle).height(),
              QStringLiteral("矩形与圆角矩形默认插入为正方形"));
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
}
