#include "items/CanvasTypes.h"
#include "items/geometry/ShapePathBuilder.h"
#include "items/lines/BraceItem.h"
#include "items/lines/CurveItem.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "items/shapes/ShapeFactory.h"
#include "items/text/TextItem.h"
#include "scene/CanvasScene.h"
#include "selftest/core/SelfTest.h"
#include "selftest/core/SelftestHelpers.h"

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
ShapeAbstractItem* AddImageRect(CanvasScene& scene, int layerId, const QPixmap& pixmap, const QSizeF& size,
                                const QPointF& pos)
{
    auto* item = ShapeFactory::Create(ShapeType::RECTANGLE);
    item->SetLayerId(layerId);
    item->SetStyle(QColor(Qt::transparent), QColor(Qt::transparent), 1.0);
    item->SetFillStyle(FillStyle::NONE);
    item->SetFillImage(pixmap, ImageFill::Mode::STRETCH);
    item->setPos(pos);
    item->SetContentSize(size);
    scene.AttachItem(item);
    return item;
}

} // namespace
using namespace selftest;

namespace Demo {

bool Render(const QString& outputPath)
{
    CanvasScene scene;
    Demo::Populate(scene);

    const QImage image = scene.RenderImage(scene.CanvasRect(), 1.0);
    if (image.isNull()) {
        return false;
    }
    return image.save(outputPath);
}

bool Write(const QString& outputPath)
{
    CanvasScene scene;
    Demo::Populate(scene);

    QDir().mkpath(QFileInfo(outputPath).absolutePath());
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QJsonDocument document(scene.ToJson());
    const QByteArray data = document.toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size()) {
        file.close();
        return false;
    }
    file.close();
    return file.error() == QFileDevice::NoError;
}

void Populate(CanvasScene& scene)
{
    scene.ApplyCanvasSize(QSizeF(1600, 1000));

    CanvasBackground background;
    background.type = BackgroundType::COLOR;
    background.color = QColor(QStringLiteral("#F7F3EA"));
    scene.SetCanvasBackground(background);

    scene.AddLayerWithUndo(QStringLiteral("分镜框"));
    const int frameLayer = scene.ActiveLayerId();
    scene.AddLayerWithUndo(QStringLiteral("镜头画面"));
    const int shotLayer = scene.ActiveLayerId();
    scene.AddLayerWithUndo(QStringLiteral("注释与标题"));
    const int inkLayer = scene.ActiveLayerId();
    scene.RemoveLayerWithUndo(CanvasScene::BackgroundLayerId + 1);
    scene.SetActiveLayer(inkLayer);

    QFont titleFont(QStringLiteral("Microsoft YaHei UI"));
    titleFont.setPointSize(52);
    titleFont.setBold(true);
    auto* titleItem = new TextItem();
    titleItem->SetLayerId(inkLayer);
    titleItem->SetTextFormat(QStringLiteral("STORYBOARD  /  分镜画板"), titleFont, QColor(QStringLiteral("#1F2937")));
    titleItem->SetContentSize(QSizeF(980, 80));
    titleItem->setPos(QPointF(78, 48));
    scene.AttachItem(titleItem);

    QFont labelFont(QStringLiteral("Microsoft YaHei UI"));
    labelFont.setPointSize(20);
    labelFont.setBold(true);
    const QPixmap subtitle = MakeTextImage(QStringLiteral("三个镜头 · 一个下午的故事"),
                                           QColor(QStringLiteral("#78716C")), labelFont, 720, 40);
    AddImageRect(scene, inkLayer, subtitle, subtitle.size(), QPointF(82, 126));

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
        auto* frame = ShapeFactory::Create(ShapeType::RECTANGLE);
        frame->SetLayerId(frameLayer);
        frame->SetStyle(ink, QColor(Qt::transparent), 5.0);
        frame->setPos(panel.topLeft());
        frame->SetContentSize(panel.size());
        scene.AttachItem(frame);

        const QPixmap shot = MakeShotImage(380, 230, shotLabels.at(i), shotStarts.at(i), shotEnds.at(i));
        AddImageRect(scene, shotLayer, shot, QSizeF(380, 230), QPointF(panel.left() + 30, panel.top() + 24));

        QFont numberFont(QStringLiteral("Microsoft YaHei UI"));
        numberFont.setPointSize(26);
        numberFont.setBold(true);
        const QPixmap number = MakeTextImage(QStringLiteral("0%1").arg(i + 1), ink, numberFont, 90, 44);
        AddImageRect(scene, shotLayer, number, QSizeF(70, 40), QPointF(panel.left() + 32, panel.bottom() - 84));

        QFont captionFont(QStringLiteral("Microsoft YaHei UI"));
        captionFont.setPointSize(16);
        const QStringList captions = {QStringLiteral("主角推门进入，光从背后打来。"),
                                      QStringLiteral("发现桌上的旧照片，定格。"),
                                      QStringLiteral("转身离开，门缓缓合上。")};
        const QPixmap caption = MakeTextImage(captions.at(i), QColor(QStringLiteral("#57534E")), captionFont, 420, 34);
        AddImageRect(scene, shotLayer, caption, QSizeF(376, 30), QPointF(panel.left() + 32, panel.bottom() - 40));
    }

    // Hand-drawn annotations: stick figure, arrows and a few accents.
    auto* firstArrow = ShapeFactory::Create(ShapeType::ARROW);
    firstArrow->SetLayerId(inkLayer);
    firstArrow->SetStyle(accent, QColor(Qt::transparent), 5.0);
    firstArrow->setPos(QPointF(520, 522));
    firstArrow->SetContentSize(QSizeF(66, 36));
    scene.AttachItem(firstArrow);

    auto* secondArrow = ShapeFactory::Create(ShapeType::ARROW);
    secondArrow->SetLayerId(inkLayer);
    secondArrow->SetStyle(accent, QColor(Qt::transparent), 5.0);
    secondArrow->setPos(QPointF(1020, 522));
    secondArrow->SetContentSize(QSizeF(66, 36));
    scene.AttachItem(secondArrow);

    auto* headRing = ShapeFactory::Create(ShapeType::ELLIPSE);
    headRing->SetLayerId(inkLayer);
    headRing->SetStyle(teal, QColor(Qt::transparent), 3.0);
    headRing->setPos(QPointF(258, 510));
    headRing->SetContentSize(QSizeF(84, 84));
    scene.AttachItem(headRing);

    auto* star = ShapeFactory::Create(ShapeType::STAR);
    star->SetLayerId(inkLayer);
    QColor starFill = accent;
    starFill.setAlpha(62);
    star->SetStyle(accent, starFill, 3.0);
    star->setPos(QPointF(1538, 306));
    star->SetContentSize(QSizeF(54, 54));
    scene.AttachItem(star);

    // 需求 13：花括号是独立的描线图元，几何采样下沉到 BraceItem::setBraceRect。
    auto* brace = new BraceItem();
    brace->SetBraceRect(QRectF(1040, 884, 44, 92), accent, 5.0);
    brace->SetLayerId(inkLayer);
    scene.AttachItem(brace);

    QVector<QPointF> figure;
    const QPointF headCenter(300, 552);
    constexpr qreal PI = 3.14159265358979323846;
    for (int i = 0; i <= 24; ++i) {
        const qreal angle = 2.0 * PI * i / 24.0;
        figure.append(headCenter + QPointF(qCos(angle) * 26.0, qSin(angle) * 26.0));
    }
    AddStroke(scene, figure, teal, 4.0, inkLayer);
    AddLine(scene, QPointF(300, 578), QPointF(300, 650), teal, 4.0, inkLayer);
    AddLine(scene, QPointF(300, 604), QPointF(260, 628), teal, 4.0, inkLayer);
    AddLine(scene, QPointF(300, 604), QPointF(342, 626), teal, 4.0, inkLayer);
    AddLine(scene, QPointF(300, 650), QPointF(270, 696), teal, 4.0, inkLayer);
    AddLine(scene, QPointF(300, 650), QPointF(330, 696), teal, 4.0, inkLayer);

    QVector<QPointF> underline;
    for (int i = 0; i <= 30; ++i) {
        const qreal x = 1080 + i * 13.0;
        underline.append(QPointF(x, 890 + qSin(i * 0.45) * 4.0));
    }
    AddStroke(scene, underline, accent, 5.0, inkLayer);

    QFont noteFont(QStringLiteral("Microsoft YaHei UI"));
    noteFont.setPointSize(18);
    noteFont.setBold(true);
    const QPixmap note =
        MakeTextImage(QStringLiteral("KEEP THE MOMENT"), QColor(QStringLiteral("#9A3412")), noteFont, 330, 40);
    AddImageRect(scene, inkLayer, note, QSizeF(300, 36), QPointF(1098, 920));
}

} // namespace Demo
