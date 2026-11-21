#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "Commands.h"
#include "MainWindow.h"
#include "ShapeItem.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"

#include <QColor>
#include <QImage>
#include <QJsonObject>
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QUndoStack>


void selftest::runFillStyleBasicTests(Context& ctx)
{
    // 需求 10/11/12/13/14 回归：锁定、椭圆、填充样式、画板尺寸。
    {
        CanvasScene reqScene;
        reqScene.setMode(ToolMode::EDIT);
        reqScene.setShowGrid(false);
        QUndoStack reqUndo;
        reqScene.setUndoStack(&reqUndo);

        // 需求 11：只保留椭圆，插入即标准圆形，可拉伸。
        ctx.check(shapeTypeName(ShapeType::ELLIPSE) == QStringLiteral("椭圆"), QStringLiteral("图形名称统一为椭圆"));
        const QSizeF ellipseDefault = defaultShapeSize(ShapeType::ELLIPSE);
        ctx.check(qFuzzyCompare(ellipseDefault.width(), ellipseDefault.height()),
                  QStringLiteral("椭圆默认插入为标准圆形"));
        auto* circleItem = createShapeItem(ShapeType::ELLIPSE);
        circleItem->SetContentSize(QSizeF(200, 200));
        circleItem->SetContentSize(QSizeF(260, 140));
        ctx.check(circleItem->contentSize() == QSizeF(260, 140), QStringLiteral("圆形可在编辑模式拉伸成椭圆"));
        delete circleItem;

        // 需求 14：填充样式（无/纯色/斜线/交叉线）。
        ctx.check(fillStyleName(FillStyle::NONE) == QStringLiteral("无填充") &&
                      fillStyleName(FillStyle::HACHURE) == QStringLiteral("斜线"),
                  QStringLiteral("填充样式名称齐全"));
        ctx.check(fillStyleFromString(QStringLiteral("cross_hatch")) == FillStyle::CROSS_HATCH,
                  QStringLiteral("交叉线填充样式可反序列化"));

        auto* filledShape = createShapeItem(ShapeType::RECTANGLE);
        filledShape->SetContentSize(QSizeF(120, 80));
        filledShape->setPos(QPointF(30, 30));
        filledShape->SetFill(QColor(59, 130, 246, 160), FillStyle::HACHURE);
        ctx.check(filledShape->fillStyle() == FillStyle::HACHURE && filledShape->FillColor().alpha() == 160,
                  QStringLiteral("图形可设置填充颜色与斜线填充"));
        reqScene.attachItem(filledShape);

        // 需求 23：箭头内部空心，可参与填充；只有纯色填充才整体遮挡下层。
        // 直线已成为曲线族（零面积描线），不再出现在形状类型的填充判定里。
        ctx.check(shapeTypeIsFilled(ShapeType::ARROW) && shapeTypeIsFilled(ShapeType::DOUBLE_ARROW),
                  QStringLiteral("箭头支持填充，直线仍是零面积描线图元"));

        CanvasScene arrowScene;
        arrowScene.applyCanvasSize(QSizeF(400, 200));
        arrowScene.setShowGrid(false);
        auto* hollowArrow = createShapeItem(ShapeType::ARROW);
        hollowArrow->setPos(QPointF(40, 50));
        hollowArrow->SetContentSize(QSizeF(240, 100));
        hollowArrow->setStyle(QColor(QStringLiteral("#DC2626")), QColor(Qt::transparent), 6.0);
        hollowArrow->SetFill(QColor(Qt::transparent), FillStyle::NONE);
        arrowScene.attachItem(hollowArrow);

        ctx.check(!hollowArrow->PaintedShape().contains(QPointF(70, 50)),
                  QStringLiteral("无填充箭头内部为空心，不遮挡下层"));

        QImage hollowArrowImage(400, 200, QImage::Format_ARGB32_Premultiplied);
        hollowArrowImage.fill(Qt::white);
        QPainter hollowArrowPainter(&hollowArrowImage);
        arrowScene.render(&hollowArrowPainter, QRectF(0, 0, 400, 200), QRectF(0, 0, 400, 200));
        hollowArrowPainter.end();
        const QColor hollowPixel = hollowArrowImage.pixelColor(110, 100);
        ctx.check(hollowPixel.red() > 245 && hollowPixel.green() > 245 && hollowPixel.blue() > 245,
                  QStringLiteral("无填充箭头内部渲染为空心"), hollowPixel.name());

        hollowArrow->SetFill(QColor(QStringLiteral("#2563EB")), FillStyle::SOLID);
        ctx.check(hollowArrow->PaintedShape().contains(QPointF(70, 50)),
                  QStringLiteral("纯色填充箭头内部整体遮挡下层"));
        QImage filledArrowImage(400, 200, QImage::Format_ARGB32_Premultiplied);
        filledArrowImage.fill(Qt::white);
        QPainter filledArrowPainter(&filledArrowImage);
        arrowScene.render(&filledArrowPainter, QRectF(0, 0, 400, 200), QRectF(0, 0, 400, 200));
        filledArrowPainter.end();
        const QColor filledPixel = filledArrowImage.pixelColor(110, 100);
        ctx.check(filledPixel.blue() > 200 && filledPixel.red() < 80, QStringLiteral("纯色填充箭头内部渲染为填充色"),
                  filledPixel.name());

        const QJsonObject filledRoot = reqScene.ToJson();
        CanvasScene filledLoaded;
        QString filledError;
        ctx.check(filledLoaded.FromJson(filledRoot, &filledError), QStringLiteral("填充样式画布反序列化"), filledError);
        ShapeItem* loadedShape = nullptr;
        for (CanvasItem* item : filledLoaded.canvasItems()) {
            if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
                loadedShape = shape;
                break;
            }
        }
        ctx.check(loadedShape && loadedShape->fillStyle() == FillStyle::HACHURE &&
                      loadedShape->FillColor().alpha() == 160,
                  QStringLiteral("填充样式随画布保存与恢复"));

        QUndoStack styleUndo;
        CanvasScene styleScene;
        styleScene.setUndoStack(&styleUndo);
        auto* styleShape = createShapeItem(ShapeType::RECTANGLE);
        styleShape->SetContentSize(QSizeF(100, 60));
        styleScene.attachItem(styleShape);
        const FillStyle styleBefore = styleShape->fillStyle();
        const QColor styleBeforeColor = styleShape->FillColor();
        styleShape->SetFill(QColor(239, 68, 68, 120), FillStyle::CROSS_HATCH);
        QMap<ItemProperty, QVariant> beforeValues;
        beforeValues.insert(ItemProperty::FILL_COLOR, styleBeforeColor);
        beforeValues.insert(ItemProperty::FILL_STYLE, static_cast<int>(styleBefore));
        QMap<ItemProperty, QVariant> afterValues;
        afterValues.insert(ItemProperty::FILL_COLOR, QColor(239, 68, 68, 120));
        afterValues.insert(ItemProperty::FILL_STYLE, static_cast<int>(FillStyle::CROSS_HATCH));
        QList<CanvasItem*> styleItems{styleShape};
        styleUndo.push(new ItemPropertyCommand(&styleScene, styleItems, {beforeValues}, {afterValues},
                                               QStringLiteral("调整填充样式")));
        ctx.check(styleShape->fillStyle() == FillStyle::CROSS_HATCH, QStringLiteral("填充样式命令立即生效"));
        styleUndo.undo();
        ctx.check(styleShape->fillStyle() == styleBefore && styleShape->FillColor() == styleBeforeColor,
                  QStringLiteral("填充样式可撤销"));
        styleUndo.redo();
        ctx.check(styleShape->fillStyle() == FillStyle::CROSS_HATCH, QStringLiteral("填充样式可重做"));
    }

    // 需求 16：常见图形按工具栏已选择的填充样式 / 描线颜色 / 描线宽度生成；
    // 填充色默认是工具栏灰 #9CA3AF（首启初值），不再回退到画笔色。
    {
        qreal insertedWidth = -1.0;
        QString fillColorText;
        QString strokeColorText;
        FillStyle insertedFillStyle = FillStyle::NONE;
        {
            MainWindow window;
            window.SetFillStyle(FillStyle::HACHURE);
            window.setBrushColor(QColor(37, 99, 235), false);
            window.setBrushWidthUi(1);
            window.insertShape(ShapeType::RECTANGLE);
            CanvasScene* scene = window.findChild<CanvasScene*>();
            if (auto* shape = qgraphicsitem_cast<ShapeItem*>(scene->canvasItems().value(0))) {
                insertedWidth = shape->StrokeWidth();
                fillColorText = shape->FillColor().name();
                strokeColorText = shape->StrokeColor().name();
                insertedFillStyle = shape->fillStyle();
            }
        }
        ctx.check(
            insertedFillStyle == FillStyle::HACHURE && fillColorText == QStringLiteral("#9ca3af") &&
                strokeColorText == QStringLiteral("#2563eb") && qAbs(insertedWidth - 1.0) < 0.01,
            QStringLiteral("常见图形按工具栏样式生成"),
            QStringLiteral("fill=%1 stroke=%2 width=%3").arg(fillColorText).arg(strokeColorText).arg(insertedWidth));
        // 宽度偏大时同样不再被钳到 10。
        {
            MainWindow wideWindow;
            wideWindow.SetFillStyle(FillStyle::NONE);
            wideWindow.setBrushColor(QColor(37, 99, 235), false);
            wideWindow.setBrushWidthUi(18);
            wideWindow.insertShape(ShapeType::ELLIPSE);
            CanvasScene* wideScene = wideWindow.findChild<CanvasScene*>();
            if (auto* wideShape = qgraphicsitem_cast<ShapeItem*>(wideScene->canvasItems().value(0))) {
                insertedWidth = wideShape->StrokeWidth();
                insertedFillStyle = wideShape->fillStyle();
            }
        }
        ctx.check(qAbs(insertedWidth - 18.0) < 0.01 && insertedFillStyle == FillStyle::NONE,
                  QStringLiteral("插入图形描线宽度按工具栏取值不钳制"), QString::number(insertedWidth));
    }
}
