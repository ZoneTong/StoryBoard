#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "ImageItem.h"
#include "SelfTest.h"
#include "ShapeItem.h"
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

    BackgroundState background;
    background.type = BackgroundType::Color;
    background.color = QColor(QStringLiteral("#F7F3EA"));
    scene.applyBackground(background);

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
    auto* subtitleItem = new ImageItem();
    subtitleItem->setLayerId(inkLayer);
    subtitleItem->setImage(subtitle, QStringLiteral("subtitle"));
    subtitleItem->setContentSize(subtitle.size());
    subtitleItem->setPos(QPointF(82, 126));
    scene.attachItem(subtitleItem);

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
        auto* frame = new ShapeItem(ShapeType::Rectangle);
        frame->setLayerId(frameLayer);
        frame->setStyle(ink, QColor(Qt::transparent), 5.0);
        frame->setPos(panel.topLeft());
        frame->setContentSize(panel.size());
        scene.attachItem(frame);

        const QPixmap shot = makeShotImage(380, 230, shotLabels.at(i), shotStarts.at(i), shotEnds.at(i));
        auto* shotItem = new ImageItem();
        shotItem->setLayerId(shotLayer);
        shotItem->setImage(shot, QStringLiteral("shot-%1.png").arg(i + 1));
        shotItem->setContentSize(QSizeF(380, 230));
        shotItem->setPos(panel.left() + 30, panel.top() + 24);
        scene.attachItem(shotItem);

        QFont numberFont(QStringLiteral("Microsoft YaHei UI"));
        numberFont.setPointSize(26);
        numberFont.setBold(true);
        const QPixmap number = makeTextImage(QStringLiteral("0%1").arg(i + 1), ink, numberFont, 90, 44);
        auto* numberItem = new ImageItem();
        numberItem->setLayerId(shotLayer);
        numberItem->setImage(number, QStringLiteral("number"));
        numberItem->setContentSize(QSizeF(70, 40));
        numberItem->setPos(panel.left() + 32, panel.bottom() - 84);
        scene.attachItem(numberItem);

        QFont captionFont(QStringLiteral("Microsoft YaHei UI"));
        captionFont.setPointSize(16);
        const QStringList captions = {QStringLiteral("主角推门进入，光从背后打来。"),
                                      QStringLiteral("发现桌上的旧照片，定格。"),
                                      QStringLiteral("转身离开，门缓缓合上。")};
        const QPixmap caption = makeTextImage(captions.at(i), QColor(QStringLiteral("#57534E")), captionFont, 420, 34);
        auto* captionItem = new ImageItem();
        captionItem->setLayerId(shotLayer);
        captionItem->setImage(caption, QStringLiteral("caption"));
        captionItem->setContentSize(QSizeF(376, 30));
        captionItem->setPos(panel.left() + 32, panel.bottom() - 40);
        scene.attachItem(captionItem);
    }

    // Hand-drawn annotations: stick figure, arrows and a few accents.
    auto* firstArrow = new ShapeItem(ShapeType::Arrow);
    firstArrow->setLayerId(inkLayer);
    firstArrow->setStyle(accent, QColor(Qt::transparent), 5.0);
    firstArrow->setPos(QPointF(520, 522));
    firstArrow->setContentSize(QSizeF(66, 36));
    scene.attachItem(firstArrow);

    auto* secondArrow = new ShapeItem(ShapeType::Arrow);
    secondArrow->setLayerId(inkLayer);
    secondArrow->setStyle(accent, QColor(Qt::transparent), 5.0);
    secondArrow->setPos(QPointF(1020, 522));
    secondArrow->setContentSize(QSizeF(66, 36));
    scene.attachItem(secondArrow);

    auto* headRing = new ShapeItem(ShapeType::Ellipse);
    headRing->setLayerId(inkLayer);
    headRing->setStyle(teal, QColor(Qt::transparent), 3.0);
    headRing->setPos(QPointF(258, 510));
    headRing->setContentSize(QSizeF(84, 84));
    scene.attachItem(headRing);

    auto* star = new ShapeItem(ShapeType::Star);
    star->setLayerId(inkLayer);
    QColor starFill = accent;
    starFill.setAlpha(62);
    star->setStyle(accent, starFill, 3.0);
    star->setPos(QPointF(1538, 306));
    star->setContentSize(QSizeF(54, 54));
    scene.attachItem(star);

    auto* brace = new ShapeItem(ShapeType::BraceLeft);
    brace->setLayerId(inkLayer);
    brace->setStyle(accent, QColor(Qt::transparent), 5.0);
    brace->setPos(QPointF(1040, 884));
    brace->setContentSize(QSizeF(44, 92));
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
    auto* noteItem = new ImageItem();
    noteItem->setLayerId(inkLayer);
    noteItem->setImage(note, QStringLiteral("note"));
    noteItem->setContentSize(QSizeF(300, 36));
    noteItem->setPos(QPointF(1098, 920));
    scene.attachItem(noteItem);
}
