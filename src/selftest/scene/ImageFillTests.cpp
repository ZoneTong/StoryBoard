#include "io/ExcalidrawFormat.h"
#include "items/CanvasTypes.h"
#include "items/polygon/PolygonItem.h"
#include "items/properties/ImageFill.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "ops/BoolOps.h"
#include "scene/CanvasScene.h"
#include "scene/Commands.h"
#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"

#include <QColor>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRect>
#include <QRectF>
#include <QSizeF>
#include <QUndoStack>

namespace {

const QColor RED(220, 40, 40);
const QColor GREEN(40, 180, 90);
const QColor BLUE(50, 90, 220);
const QColor YELLOW(240, 200, 60);

// 四象限图片：左上红 / 右上绿 / 左下蓝 / 右下黄。
QPixmap QuadrantImage(int width, int height)
{
    QPixmap pixmap(width, height);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.fillRect(QRect(0, 0, width / 2, height / 2), RED);
    painter.fillRect(QRect(width / 2, 0, width - width / 2, height / 2), GREEN);
    painter.fillRect(QRect(0, height / 2, width / 2, height - height / 2), BLUE);
    painter.fillRect(QRect(width / 2, height / 2, width - width / 2, height - height / 2), YELLOW);
    painter.end();
    return pixmap;
}

// 左半红右半蓝的条纹图：用于确认平铺步长等于原图像素。
QPixmap StripedImage(int width, int height)
{
    QPixmap pixmap(width, height);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.fillRect(QRect(0, 0, width / 2, height), RED);
    painter.fillRect(QRect(width / 2, 0, width - width / 2, height), BLUE);
    painter.end();
    return pixmap;
}

QPixmap SolidImage(const QColor& color, int size)
{
    QPixmap pixmap(size, size);
    pixmap.fill(color);
    return pixmap;
}

bool ColorNear(const QColor& actual, const QColor& expected, int tolerance = 28)
{
    return qAbs(actual.red() - expected.red()) <= tolerance && qAbs(actual.green() - expected.green()) <= tolerance &&
           qAbs(actual.blue() - expected.blue()) <= tolerance;
}

QImage RenderCanvas(CanvasScene& scene, const QSize& size)
{
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    scene.render(&painter, QRectF(0, 0, size.width(), size.height()), QRectF(0, 0, size.width(), size.height()));
    painter.end();
    return image;
}

// 采样用的矩形：无描边、可带纯色填充，只留下图片本体参与像素比较。
ShapeAbstractItem* MakePlainShape(ShapeType type, const QSizeF& size, const QColor& fillColor)
{
    auto* shape = CreateShapeItem(type);
    shape->setPos(QPointF(0, 0));
    shape->SetContentSize(size);
    shape->SetStyle(QColor(0, 0, 0, 0), fillColor, 0.0);
    shape->SetFill(fillColor, FillStyle::SOLID);
    return shape;
}

} // namespace

void selftest::RunImageFillTests(Context& ctx)
{
    // 需求 15：填充图片三档摆放、裁剪到填充区、撤销、序列化与导出降级。
    // 1) Stretch：四角颜色对应图片四角，缩放 2 倍后仍对应。
    {
        CanvasScene stretchScene;
        stretchScene.SetMode(ToolMode::EDIT);
        stretchScene.SetShowGrid(false);
        stretchScene.ApplyCanvasSize(QSizeF(200, 100));
        auto* rect = MakePlainShape(ShapeType::RECTANGLE, QSizeF(200, 100), QColor(255, 255, 255, 0));
        rect->SetFillImage(QuadrantImage(40, 40), ImageFill::Mode::STRETCH);
        stretchScene.AttachItem(rect);

        const QImage stretched = RenderCanvas(stretchScene, QSize(200, 100));
        const bool cornersMatch =
            ColorNear(stretched.pixelColor(20, 15), RED) && ColorNear(stretched.pixelColor(180, 15), GREEN) &&
            ColorNear(stretched.pixelColor(20, 85), BLUE) && ColorNear(stretched.pixelColor(180, 85), YELLOW);
        ctx.Check(cornersMatch, QStringLiteral("Stretch 填充的四角对应图片四角"),
                  QStringLiteral("%1/%2/%3/%4")
                      .arg(stretched.pixelColor(20, 15).name())
                      .arg(stretched.pixelColor(180, 15).name())
                      .arg(stretched.pixelColor(20, 85).name())
                      .arg(stretched.pixelColor(180, 85).name()));

        rect->SetContentSize(QSizeF(400, 200));
        stretchScene.ApplyCanvasSize(QSizeF(400, 200));
        const QImage doubled = RenderCanvas(stretchScene, QSize(400, 200));
        const bool doubledMatch =
            ColorNear(doubled.pixelColor(40, 30), RED) && ColorNear(doubled.pixelColor(360, 30), GREEN) &&
            ColorNear(doubled.pixelColor(40, 170), BLUE) && ColorNear(doubled.pixelColor(360, 170), YELLOW);
        ctx.Check(doubledMatch, QStringLiteral("Stretch 填充缩放到 2 倍后四角仍对应"),
                  QStringLiteral("%1/%2/%3/%4")
                      .arg(doubled.pixelColor(40, 30).name())
                      .arg(doubled.pixelColor(360, 30).name())
                      .arg(doubled.pixelColor(40, 170).name())
                      .arg(doubled.pixelColor(360, 170).name()));
    }

    // 2) 心形：图片裁到填充区，包围框四角（形状外）没有图片墨迹。
    {
        CanvasScene heartScene;
        heartScene.SetMode(ToolMode::EDIT);
        heartScene.SetShowGrid(false);
        heartScene.ApplyCanvasSize(QSizeF(200, 200));
        auto* heart = MakePlainShape(ShapeType::HEART, QSizeF(200, 200), QColor(255, 255, 255, 0));
        heart->SetFillImage(SolidImage(GREEN, 32), ImageFill::Mode::STRETCH);
        heartScene.AttachItem(heart);

        const QImage heartImage = RenderCanvas(heartScene, QSize(200, 200));
        const QColor outside = heartImage.pixelColor(14, 14);
        const QColor inside = heartImage.pixelColor(100, 130);
        ctx.Check(ColorNear(outside, QColor(255, 255, 255)) && ColorNear(inside, GREEN),
                  QStringLiteral("心形填充区外没有图片墨迹、区内有图"),
                  QStringLiteral("outside=%1 inside=%2").arg(outside.name()).arg(inside.name()));
    }

    // 3) 带孔多边形：图片只铺在环形区域，孔洞露出画布背景。
    {
        CanvasScene holeScene;
        holeScene.SetMode(ToolMode::EDIT);
        holeScene.SetShowGrid(false);
        holeScene.ApplyCanvasSize(QSizeF(200, 200));
        QPainterPath outer;
        outer.addRect(QRectF(0, 0, 200, 200));
        QPainterPath inner;
        inner.addRect(QRectF(70, 70, 60, 60));
        PolygonItem::Layer ring;
        ring.path = outer.subtracted(inner);
        ring.fill = QColor(0, 0, 0, 0);
        ring.stroke = QColor(0, 0, 0, 0);
        ring.strokeWidth = 0.0;
        ring.filled = true;
        auto* polygon = new PolygonItem({ring}, FillStyle::SOLID, Qt::SolidLine);
        polygon->SetFillImage(SolidImage(GREEN, 32), ImageFill::Mode::STRETCH);
        holeScene.AttachItem(polygon);

        const QImage ringImage = RenderCanvas(holeScene, QSize(200, 200));
        const QColor ringPixel = ringImage.pixelColor(20, 20);
        const QColor holePixel = ringImage.pixelColor(100, 100);
        ctx.Check(ColorNear(ringPixel, GREEN) && ColorNear(holePixel, QColor(255, 255, 255)),
                  QStringLiteral("带孔多边形的孔洞露出画布背景"),
                  QStringLiteral("ring=%1 hole=%2").arg(ringPixel.name()).arg(holePixel.name()));
    }

    // 4) 需求 10：背景图片在填充样式之下，纯色不透明填充会把它整片盖住，
    // Fit 留边与中段都只能看到填充色。
    {
        CanvasScene fitScene;
        fitScene.SetMode(ToolMode::EDIT);
        fitScene.SetShowGrid(false);
        fitScene.ApplyCanvasSize(QSizeF(100, 200));
        auto* tall = MakePlainShape(ShapeType::RECTANGLE, QSizeF(100, 200), BLUE);
        tall->SetFillImage(SolidImage(GREEN, 40), ImageFill::Mode::FIT);
        fitScene.AttachItem(tall);

        const QImage fitImage = RenderCanvas(fitScene, QSize(100, 200));
        const QColor topBand = fitImage.pixelColor(50, 20);
        const QColor middle = fitImage.pixelColor(50, 100);
        ctx.Check(ColorNear(topBand, BLUE) && ColorNear(middle, BLUE),
                  QStringLiteral("背景图片在填充样式之下，纯色填充将它完全遮挡"),
                  QStringLiteral("top=%1 middle=%2").arg(topBand.name()).arg(middle.name()));
    }

    // 需求 11/12：同一张图片被多个图元共用时，画布文件里只存一份；图元只记 id 引用。
    {
        CanvasScene storeScene;
        storeScene.SetMode(ToolMode::EDIT);
        storeScene.SetShowGrid(false);
        storeScene.ApplyCanvasSize(QSizeF(300, 200));
        const QPixmap shared = SolidImage(GREEN, 40);
        auto* first = MakePlainShape(ShapeType::RECTANGLE, QSizeF(80, 80), BLUE);
        auto* second = MakePlainShape(ShapeType::ELLIPSE, QSizeF(80, 80), BLUE);
        first->SetFillImage(shared, ImageFill::Mode::STRETCH);
        second->SetFillImage(shared, ImageFill::Mode::STRETCH);
        second->setPos(100.0, 0.0);
        storeScene.AttachItem(first);
        storeScene.AttachItem(second);

        const QJsonObject root = storeScene.ToJson();
        const QJsonObject images = root.value(QStringLiteral("images")).toObject();
        const QJsonArray items = root.value(QStringLiteral("items")).toArray();
        const QString firstId = items.at(0).toObject().value(QStringLiteral("fillImageId")).toString();
        const QString secondId = items.at(1).toObject().value(QStringLiteral("fillImageId")).toString();
        const bool noInline = !items.at(0).toObject().contains(QStringLiteral("fillImage")) &&
                              !items.at(1).toObject().contains(QStringLiteral("fillImage"));
        ctx.Check(images.size() == 1 && firstId == secondId && !firstId.isEmpty() && noInline,
                  QStringLiteral("同一张背景图片在画布文件里只存一份、图元只记引用"),
                  QStringLiteral("images=%1 sameId=%2 inline=%3")
                      .arg(images.size())
                      .arg(firstId == secondId ? QStringLiteral("yes") : QStringLiteral("no"))
                      .arg(noInline ? QStringLiteral("yes") : QStringLiteral("no")));
    }

    // 5) Tile：按原图像素 1:1 平铺，出现 ≥2×2 周期且步长等于图片宽度。
    {
        CanvasScene tileScene;
        tileScene.SetMode(ToolMode::EDIT);
        tileScene.SetShowGrid(false);
        tileScene.ApplyCanvasSize(QSizeF(100, 100));
        auto* tiled = MakePlainShape(ShapeType::RECTANGLE, QSizeF(100, 100), QColor(255, 255, 255, 0));
        tiled->SetFillImage(StripedImage(20, 20), ImageFill::Mode::TILE);
        tileScene.AttachItem(tiled);

        const QImage tileImage = RenderCanvas(tileScene, QSize(100, 100));
        const QColor firstPeriodRed = tileImage.pixelColor(4, 6);
        const QColor firstPeriodBlue = tileImage.pixelColor(14, 6);
        const QColor secondPeriodRed = tileImage.pixelColor(24, 6);
        const QColor secondPeriodBlue = tileImage.pixelColor(34, 6);
        ctx.Check(ColorNear(firstPeriodRed, RED) && ColorNear(firstPeriodBlue, BLUE) &&
                      ColorNear(secondPeriodRed, RED) && ColorNear(secondPeriodBlue, BLUE),
                  QStringLiteral("Tile 平铺出现 2×2 周期且步长等于原图像素"),
                  QStringLiteral("%1/%2/%3/%4")
                      .arg(firstPeriodRed.name())
                      .arg(firstPeriodBlue.name())
                      .arg(secondPeriodRed.name())
                      .arg(secondPeriodBlue.name()));
    }

    // 6) 撤销：设置、换档、清除各占一条撤销，撤销后回到上一步。
    {
        CanvasScene undoScene;
        undoScene.SetMode(ToolMode::EDIT);
        QUndoStack undo;
        undoScene.SetUndoStack(&undo);
        auto* shape = CreateShapeItem(ShapeType::RECTANGLE);
        shape->setPos(QPointF(10, 10));
        shape->SetContentSize(QSizeF(120, 80));
        undoScene.AttachItem(shape);

        ItemPropertiesState before;
        before.pos = shape->pos();
        before.size = shape->ContentSize();
        before.rotation = shape->RotationAngle();
        before.hasShapeState = true;
        before.shapeState = shape->State();

        ItemPropertiesState afterSet = before;
        afterSet.shapeState.fill.hasImage = true;
        afterSet.shapeState.fill.image = SolidImage(GREEN, 16);
        afterSet.shapeState.fill.imageMode = ImageFill::Mode::STRETCH;
        undo.push(new ItemPropertiesCommand(&undoScene, shape, before, afterSet, QStringLiteral("设图片填充")));
        ctx.Check(undo.index() == 1 && shape->HasFillImage(), QStringLiteral("设置图片填充占一条撤销记录"),
                  QStringLiteral("index=%1 has=%2").arg(undo.index()).arg(shape->HasFillImage() ? 1 : 0));

        undo.undo();
        ctx.Check(!shape->HasFillImage(), QStringLiteral("撤销后图片填充被移除"));
        undo.redo();
        ctx.Check(shape->HasFillImage(), QStringLiteral("重做后图片填充恢复"));

        ItemPropertiesState afterMode = afterSet;
        afterMode.shapeState.fill.imageMode = ImageFill::Mode::TILE;
        undo.push(new ItemPropertiesCommand(&undoScene, shape, afterSet, afterMode, QStringLiteral("换档")));
        ctx.Check(undo.index() == 2 && shape->FillImageMode() == ImageFill::Mode::TILE,
                  QStringLiteral("换摆放模式占一条撤销记录"));
        undo.undo();
        ctx.Check(shape->FillImageMode() == ImageFill::Mode::STRETCH && shape->HasFillImage(),
                  QStringLiteral("撤销换档回到 Stretch 且图片还在"));
        undo.redo();

        ItemPropertiesState afterClear = afterMode;
        afterClear.shapeState.fill.hasImage = false;
        afterClear.shapeState.fill.image = QPixmap();
        undo.push(new ItemPropertiesCommand(&undoScene, shape, afterMode, afterClear, QStringLiteral("清除")));
        undo.undo();
        ctx.Check(shape->HasFillImage() && shape->FillImageMode() == ImageFill::Mode::TILE,
                  QStringLiteral("撤销清除恢复图片与 Tile 模式"));
        delete shape;
    }

    // 7) 序列化：往返后字节与模式一致；没有 fillImage 键的文件按纯色无图载入。
    {
        auto* saved = CreateShapeItem(ShapeType::ELLIPSE);
        saved->SetContentSize(QSizeF(80, 60));
        saved->SetFillImage(SolidImage(BLUE, 24), ImageFill::Mode::TILE);
        const QJsonObject json = saved->ToJson();
        const QJsonObject fillJson = json.value(QStringLiteral("fillImage")).toObject();
        ctx.Check(!fillJson.isEmpty() && fillJson.value(QStringLiteral("mode")).toString() == QStringLiteral("tile"),
                  QStringLiteral("图片填充按 data + mode 序列化"),
                  QStringLiteral("mode=%1").arg(fillJson.value(QStringLiteral("mode")).toString()));

        auto* loaded = CreateShapeItem(ShapeType::RECTANGLE);
        loaded->FromJson(json);
        const QJsonObject loadedJson = ImageFill::ToJson(loaded->FillImage(), loaded->FillImageMode());
        ctx.Check(loaded->HasFillImage() && loaded->FillImageMode() == ImageFill::Mode::TILE &&
                      loadedJson.value(QStringLiteral("data")) == fillJson.value(QStringLiteral("data")),
                  QStringLiteral("图片填充保存往返后字节与模式一致"));

        auto* plain = CreateShapeItem(ShapeType::RECTANGLE);
        const QJsonObject plainJson = plain->ToJson();
        auto* plainLoaded = CreateShapeItem(ShapeType::RECTANGLE);
        plainLoaded->FromJson(plainJson);
        ctx.Check(!plainJson.contains(QStringLiteral("fillImage")) && !plainLoaded->HasFillImage(),
                  QStringLiteral("没有图片填充的图元不写 fillImage 键，载入按纯色无图"));

        delete saved;
        delete loaded;
        delete plain;
        delete plainLoaded;
    }

    // 8) Excalidraw 导出：图片填充降级为纯色 fillColor，并给出一次降级提示。
    {
        auto* exportShape = MakePlainShape(ShapeType::RECTANGLE, QSizeF(120, 80), YELLOW);
        exportShape->SetStyle(QColor(20, 20, 20), YELLOW, 2.0);
        exportShape->SetFillImage(SolidImage(GREEN, 24), ImageFill::Mode::STRETCH);
        QJsonArray items;
        items.append(exportShape->ToJson());
        QJsonObject document;
        document.insert(QStringLiteral("items"), items);

        QStringList warnings;
        const QJsonObject scene = Excalidraw::StoryBoardToScene(document, &warnings);
        const QJsonArray elements = scene.value(QStringLiteral("elements")).toArray();
        const QJsonObject element = elements.isEmpty() ? QJsonObject() : elements.at(0).toObject();
        const bool backgroundColorIsPlain =
            element.value(QStringLiteral("backgroundColor")).toString() == YELLOW.name(QColor::HexRgb);
        const bool hasNote = warnings.join(QLatin1Char('\n')).contains(QStringLiteral("图片填充导出为纯色"));
        ctx.Check(
            backgroundColorIsPlain && hasNote, QStringLiteral("Excalidraw 导出把图片填充降级为纯色并提示"),
            QStringLiteral("background=%1 note=%2")
                .arg(element.value(QStringLiteral("backgroundColor")).toString(), warnings.join(QLatin1Char('|'))));
        delete exportShape;
    }

    // 9) 合并图元：结果不继承图片填充（与需求 15 的合并口径一致）。
    {
        CanvasScene mergeScene;
        mergeScene.SetMode(ToolMode::EDIT);
        QUndoStack mergeUndo;
        mergeScene.SetUndoStack(&mergeUndo);
        auto* leftRect = MakePlainShape(ShapeType::RECTANGLE, QSizeF(80, 80), RED);
        leftRect->setPos(QPointF(0, 0));
        auto* rightRect = MakePlainShape(ShapeType::RECTANGLE, QSizeF(80, 80), BLUE);
        rightRect->setPos(QPointF(40, 0));
        rightRect->SetFillImage(SolidImage(GREEN, 16), ImageFill::Mode::STRETCH);
        mergeScene.AddItemsWithUndo({leftRect, rightRect}, QStringLiteral("Add merge items"));
        leftRect->setSelected(true);
        rightRect->setSelected(true);
        const bool merged = BoolOps::Apply(&mergeScene, BoolOps::Operation::UNION, QStringLiteral("合并"));
        const QList<CanvasItem*> selected = mergeScene.SelectedCanvasItems();
        auto* composite = selected.isEmpty() ? nullptr : dynamic_cast<PolygonItem*>(selected.constFirst());
        ctx.Check(merged && composite && !composite->HasFillImage(), QStringLiteral("合并图元的结果不继承图片填充"),
                  QStringLiteral("merged=%1 composite=%2 hasImage=%3")
                      .arg(merged ? 1 : 0)
                      .arg(composite ? 1 : 0)
                      .arg(composite && composite->HasFillImage() ? 1 : 0));
    }
}
