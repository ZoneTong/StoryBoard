#include "BraceItem.h"
#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CurveItem.h"
#include "SelfTest.h"
#include "ShapeItem.h"
#include "ShapePathBuilder.h"
#include "TextItem.h"
#include "selftest/SelftestHelpers.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QFont>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QPixmap>
#include <QPointF>
#include <QSizeF>
#include <QStringList>
#include <QVector>
#include <QtMath>

namespace {

// 需求 14：插入图片统一为「矩形图元 + 背景图片」，演示场景沿用同一构造。
ShapeItem* addImageRect(CanvasScene& scene, int layerId, const QPixmap& pixmap, const QSizeF& size, const QPointF& pos)
{
    auto* item = createShapeItem(ShapeType::RECTANGLE);
    item->setLayerId(layerId);
    item->setStyle(QColor(Qt::transparent), QColor(Qt::transparent), 1.0);
    item->setFillStyle(FillStyle::NONE);
    item->setFillImage(pixmap, ImageFill::Mode::STRETCH);
    item->setPos(pos);
    item->setContentSize(size);
    scene.attachItem(item);
    return item;
}

} // namespace
using namespace selftest;

bool renderDemoCanvas(const QString& outputPath)
{
    CanvasScene scene;
    populateDemoScene(scene);

    const QImage image = scene.renderImage(scene.canvasRect(), 1.0);
    if (image.isNull()) {
        return false;
    }
    return image.save(outputPath);
}

bool writeDemoCanvas(const QString& outputPath)
{
    CanvasScene scene;
    populateDemoScene(scene);

    QDir().mkpath(QFileInfo(outputPath).absolutePath());
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QJsonDocument document(scene.toJson());
    const QByteArray data = document.toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size()) {
        file.close();
        return false;
    }
    file.close();
    return file.error() == QFileDevice::NoError;
}

void populateDemoScene(CanvasScene& scene)
{
    scene.applyCanvasSize(QSizeF(1600, 1000));

    CanvasBackground background;
    background.type = BackgroundType::COLOR;
    background.color = QColor(QStringLiteral("#F7F3EA"));
    scene.setCanvasBackground(background);

    const int frameLayer = scene.createLayer(QStringLiteral("分镜框"));
    const int shotLayer = scene.createLayer(QStringLiteral("镜头画面"));
    const int inkLayer = scene.createLayer(QStringLiteral("注释与标题"));
    scene.removeLayerWithUndo(CanvasScene::BackgroundLayerId + 1);
    scene.setActiveLayer(inkLayer);

    QFont titleFont(QStringLiteral("Microsoft YaHei UI"));
    titleFont.setPointSize(52);
    titleFont.setBold(true);
    auto* titleItem = new TextItem();
    titleItem->setLayerId(inkLayer);
    titleItem->setTextFormat(QStringLiteral("STORYBOARD  /  分镜画板"), titleFont, QColor(QStringLiteral("#1F2937")));
    titleItem->setContentSize(QSizeF(980, 80));
    titleItem->setPos(QPointF(78, 48));
    scene.attachItem(titleItem);

    QFont labelFont(QStringLiteral("Microsoft YaHei UI"));
    labelFont.setPointSize(20);
    labelFont.setBold(true);
    const QPixmap subtitle = makeTextImage(QStringLiteral("三个镜头 · 一个下午的故事"),
                                           QColor(QStringLiteral("#78716C")), labelFont, 720, 40);
    addImageRect(scene, inkLayer, subtitle, subtitle.size(), QPointF(82, 126));

    const QColor ink(QStringLiteral("#1F2937"));
    const QColor accent(QStringLiteral("#EA580C"));
    const QColor teal(QStringLiteral("#0F766E"));

    const QVector<QRectF> panels = {QRectF(80, 210, 440, 660), QRectF(580, 210, 440, 660), QRectF(1080, 210, 440, 660)};
    const QList<QColor> shotStarts = {QColor(QStringLiteral("#93C5FD")), QColor(QStringLiteral("#FCA5A5")),
                                      QColor(QStringLiteral("#86EFAC"))};
    const QList<QColor> shotEnds = {QColor(QStringLiteral("#1D4ED8")), QColor(QStringLiteral("#B91C1C")),
                                    QColor(QStringLiteral("#047857"))};
    const QStringList shotLabels = {QStringLiteral("SHOT 01"), QStringLiteral("SHOT 02"), QStringLiteral("SHOT 03")};

    for (int i = 0; i < panels.size(); ++i) {
        const QRectF& panel = panels.at(i);
        auto* frame = createShapeItem(ShapeType::RECTANGLE);
        frame->setLayerId(frameLayer);
        frame->setStyle(ink, QColor(Qt::transparent), 5.0);
        frame->setPos(panel.topLeft());
        frame->setContentSize(panel.size());
        scene.attachItem(frame);

        const QPixmap shot = makeShotImage(380, 230, shotLabels.at(i), shotStarts.at(i), shotEnds.at(i));
        addImageRect(scene, shotLayer, shot, QSizeF(380, 230), QPointF(panel.left() + 30, panel.top() + 24));

        QFont numberFont(QStringLiteral("Microsoft YaHei UI"));
        numberFont.setPointSize(26);
        numberFont.setBold(true);
        const QPixmap number = makeTextImage(QStringLiteral("0%1").arg(i + 1), ink, numberFont, 90, 44);
        addImageRect(scene, shotLayer, number, QSizeF(70, 40), QPointF(panel.left() + 32, panel.bottom() - 84));

        QFont captionFont(QStringLiteral("Microsoft YaHei UI"));
        captionFont.setPointSize(16);
        const QStringList captions = {QStringLiteral("主角推门进入，光从背后打来。"),
                                      QStringLiteral("发现桌上的旧照片，定格。"),
                                      QStringLiteral("转身离开，门缓缓合上。")};
        const QPixmap caption = makeTextImage(captions.at(i), QColor(QStringLiteral("#57534E")), captionFont, 420, 34);
        addImageRect(scene, shotLayer, caption, QSizeF(376, 30), QPointF(panel.left() + 32, panel.bottom() - 40));
    }

    // Hand-drawn annotations: stick figure, arrows and a few accents.
    auto* firstArrow = createShapeItem(ShapeType::ARROW);
    firstArrow->setLayerId(inkLayer);
    firstArrow->setStyle(accent, QColor(Qt::transparent), 5.0);
    firstArrow->setPos(QPointF(520, 522));
    firstArrow->setContentSize(QSizeF(66, 36));
    scene.attachItem(firstArrow);

    auto* secondArrow = createShapeItem(ShapeType::ARROW);
    secondArrow->setLayerId(inkLayer);
    secondArrow->setStyle(accent, QColor(Qt::transparent), 5.0);
    secondArrow->setPos(QPointF(1020, 522));
    secondArrow->setContentSize(QSizeF(66, 36));
    scene.attachItem(secondArrow);

    auto* headRing = createShapeItem(ShapeType::ELLIPSE);
    headRing->setLayerId(inkLayer);
    headRing->setStyle(teal, QColor(Qt::transparent), 3.0);
    headRing->setPos(QPointF(258, 510));
    headRing->setContentSize(QSizeF(84, 84));
    scene.attachItem(headRing);

    auto* star = createShapeItem(ShapeType::STAR);
    star->setLayerId(inkLayer);
    QColor starFill = accent;
    starFill.setAlpha(62);
    star->setStyle(accent, starFill, 3.0);
    star->setPos(QPointF(1538, 306));
    star->setContentSize(QSizeF(54, 54));
    scene.attachItem(star);

    // 需求 13：花括号是独立的描线图元，几何采样下沉到 BraceItem::setBraceRect。
    auto* brace = new BraceItem();
    brace->setBraceRect(QRectF(1040, 884, 44, 92), accent, 5.0);
    brace->setLayerId(inkLayer);
    scene.attachItem(brace);

    QVector<QPointF> figure;
    const QPointF headCenter(300, 552);
    constexpr qreal pi = 3.14159265358979323846;
    for (int i = 0; i <= 24; ++i) {
        const qreal angle = 2.0 * pi * i / 24.0;
        figure.append(headCenter + QPointF(qCos(angle) * 26.0, qSin(angle) * 26.0));
    }
    addStroke(scene, figure, teal, 4.0, inkLayer);
    addLine(scene, QPointF(300, 578), QPointF(300, 650), teal, 4.0, inkLayer);
    addLine(scene, QPointF(300, 604), QPointF(260, 628), teal, 4.0, inkLayer);
    addLine(scene, QPointF(300, 604), QPointF(342, 626), teal, 4.0, inkLayer);
    addLine(scene, QPointF(300, 650), QPointF(270, 696), teal, 4.0, inkLayer);
    addLine(scene, QPointF(300, 650), QPointF(330, 696), teal, 4.0, inkLayer);

    QVector<QPointF> underline;
    for (int i = 0; i <= 30; ++i) {
        const qreal x = 1080 + i * 13.0;
        underline.append(QPointF(x, 890 + qSin(i * 0.45) * 4.0));
    }
    addStroke(scene, underline, accent, 5.0, inkLayer);

    QFont noteFont(QStringLiteral("Microsoft YaHei UI"));
    noteFont.setPointSize(18);
    noteFont.setBold(true);
    const QPixmap note =
        makeTextImage(QStringLiteral("KEEP THE MOMENT"), QColor(QStringLiteral("#9A3412")), noteFont, 330, 40);
    addImageRect(scene, inkLayer, note, QSizeF(300, 36), QPointF(1098, 920));
}
