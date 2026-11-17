#include "../ExcalidrawFormat.h"
#include "SelfTestContext.h"
#include "SelftestEntries.h"
#include "SelftestHelpers.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

namespace {

QJsonObject element(const QString& type, qreal x, qreal y, qreal w, qreal h)
{
    QJsonObject object;
    object.insert(QStringLiteral("type"), type);
    object.insert(QStringLiteral("x"), x);
    object.insert(QStringLiteral("y"), y);
    object.insert(QStringLiteral("width"), w);
    object.insert(QStringLiteral("height"), h);
    object.insert(QStringLiteral("angle"), 0);
    object.insert(QStringLiteral("strokeColor"), QStringLiteral("#ff0000"));
    object.insert(QStringLiteral("backgroundColor"), QStringLiteral("transparent"));
    object.insert(QStringLiteral("fillStyle"), QStringLiteral("solid"));
    object.insert(QStringLiteral("strokeWidth"), 2);
    object.insert(QStringLiteral("opacity"), 100);
    return object;
}

QJsonObject scene(const QJsonArray& elements)
{
    QJsonObject root;
    root.insert(QStringLiteral("type"), QStringLiteral("excalidraw"));
    root.insert(QStringLiteral("version"), 2);
    root.insert(QStringLiteral("elements"), elements);
    return root;
}

} // namespace

void selftest::runExcalidrawFormatTests(Context& ctx)
{
    // 识别：场景与剪贴板两种包装。
    {
        const QJsonObject clipboard = scene(QJsonArray{});
        QJsonObject edited = clipboard;
        edited.insert(QStringLiteral("type"), QStringLiteral("excalidraw/clipboard"));
        ctx.check(Excalidraw::isScene(clipboard), QStringLiteral("Excalidraw 场景按 type=excalidraw 识别"),
                  QStringLiteral("type=%1").arg(clipboard.value(QStringLiteral("type")).toString()));
        ctx.check(Excalidraw::isClipboard(edited), QStringLiteral("Excalidraw 剪贴板按 type=excalidraw/clipboard 识别"),
                  QStringLiteral("type=%1").arg(edited.value(QStringLiteral("type")).toString()));
        QJsonObject plain;
        ctx.check(!Excalidraw::isScene(plain) && !Excalidraw::isClipboard(plain),
                  QStringLiteral("普通 JSON 不会被当成 Excalidraw 内容"), QStringLiteral("empty object"));
    }

    // 导入：形状 / 文字 / 箭头 / 多段折线 / 不支持类型。
    QStringList warnings;
    QJsonArray elements;
    elements.append(element(QStringLiteral("rectangle"), 10, 20, 120, 60));
    elements.append(element(QStringLiteral("ellipse"), 200, 20, 80, 80));
    elements.append(element(QStringLiteral("diamond"), 300, 20, 80, 60));
    {
        QJsonObject text = element(QStringLiteral("text"), 10, 200, 100, 40);
        text.insert(QStringLiteral("text"), QStringLiteral("你好"));
        text.insert(QStringLiteral("fontSize"), 20.0);
        elements.append(text);
    }
    {
        QJsonObject arrow = element(QStringLiteral("arrow"), 10, 300, 100, 0);
        arrow.insert(QStringLiteral("points"), QJsonArray{QJsonArray{0.0, 0.0}, QJsonArray{100.0, 0.0}});
        arrow.insert(QStringLiteral("startArrowhead"), QJsonValue(QJsonValue::Null));
        arrow.insert(QStringLiteral("endArrowhead"), QStringLiteral("triangle"));
        elements.append(arrow);
    }
    {
        QJsonObject polyline = element(QStringLiteral("arrow"), 200, 300, 100, 60);
        polyline.insert(QStringLiteral("points"),
                        QJsonArray{QJsonArray{0.0, 0.0}, QJsonArray{50.0, 60.0}, QJsonArray{100.0, 0.0}});
        polyline.insert(QStringLiteral("endArrowhead"), QStringLiteral("dot"));
        elements.append(polyline);
    }
    {
        QJsonObject freedraw = element(QStringLiteral("freedraw"), 400, 300, 60, 60);
        freedraw.insert(QStringLiteral("points"), QJsonArray{QJsonArray{0.0, 0.0}, QJsonArray{60.0, 60.0}});
        elements.append(freedraw);
    }
    elements.append(element(QStringLiteral("image"), 500, 400, 40, 40));

    const QJsonArray items = Excalidraw::elementsToItems(elements, &warnings);
    ctx.check(items.size() == 7, QStringLiteral("Excalidraw 元素的导入条数正确（画片图跳过）"),
              QStringLiteral("count=%1 warnings=%2").arg(items.size()).arg(warnings.join(QStringLiteral(";"))));

    auto typeAt = [&items](int index) { return items.at(index).toObject().value(QStringLiteral("type")).toString(); };
    auto shapeTypeAt = [&items](int index) {
        return items.at(index).toObject().value(QStringLiteral("shapeType")).toString();
    };
    ctx.check(typeAt(0) == QStringLiteral("shape") && shapeTypeAt(0) == QStringLiteral("rectangle"),
              QStringLiteral("矩形导入为画板矩形"), QStringLiteral("type=%1").arg(typeAt(0)));
    ctx.check(shapeTypeAt(1) == QStringLiteral("ellipse") && shapeTypeAt(2) == QStringLiteral("diamond"),
              QStringLiteral("椭圆与菱形按原名映射"),
              QStringLiteral("ellipse=%1 diamond=%2").arg(shapeTypeAt(1), shapeTypeAt(2)));
    ctx.check(typeAt(3) == QStringLiteral("text") &&
                  items.at(3).toObject().value(QStringLiteral("text")).toString() == QStringLiteral("你好"),
              QStringLiteral("文字内容原样带入"),
              QStringLiteral("text=%1").arg(items.at(3).toObject().value(QStringLiteral("text")).toString()));
    ctx.check(items.at(3).toObject().value(QStringLiteral("fontSize")).toDouble() > 14.0 &&
                  items.at(3).toObject().value(QStringLiteral("fontSize")).toDouble() < 16.0,
              QStringLiteral("文字字号按 pt≈px/1.333 换算"),
              QStringLiteral("fontSize=%1").arg(items.at(3).toObject().value(QStringLiteral("fontSize")).toDouble()));

    // 两点箭头 → 直线图元 + 末端实心三角；起点无箭头。
    {
        const QJsonObject line = items.at(4).toObject();
        ctx.check(line.value(QStringLiteral("shapeType")).toString() == QStringLiteral("line") &&
                      line.value(QStringLiteral("endArrow")).toString() == QStringLiteral("triangle") &&
                      line.value(QStringLiteral("startArrow")).toString() == QStringLiteral("none"),
                  QStringLiteral("两点箭头导入为直线并保留三角箭头"),
                  QStringLiteral("shapeType=%1 end=%2")
                      .arg(line.value(QStringLiteral("shapeType")).toString(),
                           line.value(QStringLiteral("endArrow")).toString()));
    }
    // 多段箭头 → 曲线图元 + 折线段。
    {
        const QJsonObject curve = items.at(5).toObject();
        ctx.check(curve.value(QStringLiteral("type")).toString() == QStringLiteral("curve") &&
                      curve.value(QStringLiteral("endArrow")).toString() == QStringLiteral("dot") &&
                      curve.value(QStringLiteral("segments")).toArray().size() == 2,
                  QStringLiteral("多段箭头导入为曲线并保留拐点与圆点箭头"),
                  QStringLiteral("type=%1 segs=%2")
                      .arg(curve.value(QStringLiteral("type")).toString())
                      .arg(curve.value(QStringLiteral("segments")).toArray().size()));
    }
    {
        const QJsonObject freehand = items.at(6).toObject();
        ctx.check(freehand.value(QStringLiteral("type")).toString() == QStringLiteral("curve"),
                  QStringLiteral("freedraw 导入为曲线"),
                  QStringLiteral("type=%1").arg(freehand.value(QStringLiteral("type")).toString()));
    }
    ctx.check(!warnings.isEmpty(), QStringLiteral("不支持的元素会留下降级说明"),
              QStringLiteral("count=%1").arg(warnings.size()));

    // 完整场景导入：画布尺寸覆盖元素包围盒，背景取 viewBackgroundColor。
    {
        QJsonObject root = scene(QJsonArray{element(QStringLiteral("rectangle"), 0, 0, 400, 300)});
        QJsonObject appState;
        appState.insert(QStringLiteral("viewBackgroundColor"), QStringLiteral("#ffffff"));
        root.insert(QStringLiteral("appState"), appState);
        QStringList sceneWarnings;
        const QJsonObject document = Excalidraw::sceneToStoryBoard(root, &sceneWarnings);
        const QJsonObject canvas = document.value(QStringLiteral("canvas")).toObject();
        ctx.check(document.value(QStringLiteral("format")).toString() == QStringLiteral("StoryBoardCanvas") &&
                      canvas.value(QStringLiteral("width")).toDouble() >= 400.0 &&
                      canvas.value(QStringLiteral("height")).toDouble() >= 300.0 &&
                      document.value(QStringLiteral("items")).toArray().size() == 1,
                  QStringLiteral("场景导入生成合法画板文档（画布按包围盒扩展）"),
                  QStringLiteral("w=%1 h=%2 items=%3")
                      .arg(canvas.value(QStringLiteral("width")).toDouble())
                      .arg(canvas.value(QStringLiteral("height")).toDouble())
                      .arg(document.value(QStringLiteral("items")).toArray().size()));
        ctx.check(document.value(QStringLiteral("background")).toObject().value(QStringLiteral("color")).toString() ==
                      QStringLiteral("#ffffff"),
                  QStringLiteral("场景背景色带入画板背景"),
                  document.value(QStringLiteral("background")).toObject().value(QStringLiteral("color")).toString());
    }

    // 导出：合法场景骨架 + 直线 / 矩形 / 文字 往返可读。
    {
        QJsonObject item;
        item.insert(QStringLiteral("type"), QStringLiteral("shape"));
        item.insert(QStringLiteral("shapeType"), QStringLiteral("rectangle"));
        item.insert(QStringLiteral("x"), 32.0);
        item.insert(QStringLiteral("y"), 32.0);
        item.insert(QStringLiteral("w"), 64.0);
        item.insert(QStringLiteral("h"), 64.0);
        item.insert(QStringLiteral("strokeColor"), QStringLiteral("#ff1f2937"));
        item.insert(QStringLiteral("fillColor"), QStringLiteral("#00000000"));
        item.insert(QStringLiteral("fillStyle"), QStringLiteral("none"));
        item.insert(QStringLiteral("strokeWidth"), 4.0);
        QJsonObject line = item;
        line.insert(QStringLiteral("shapeType"), QStringLiteral("line"));
        line.insert(QStringLiteral("startArrow"), QStringLiteral("none"));
        line.insert(QStringLiteral("endArrow"), QStringLiteral("open"));
        QJsonObject text;
        text.insert(QStringLiteral("type"), QStringLiteral("text"));
        text.insert(QStringLiteral("x"), 10.0);
        text.insert(QStringLiteral("y"), 120.0);
        text.insert(QStringLiteral("w"), 100.0);
        text.insert(QStringLiteral("h"), 30.0);
        text.insert(QStringLiteral("text"), QStringLiteral("导出"));
        text.insert(QStringLiteral("fontSize"), 15.0);
        text.insert(QStringLiteral("color"), QStringLiteral("#ff1e293b"));

        QJsonObject document;
        document.insert(QStringLiteral("format"), QStringLiteral("StoryBoardCanvas"));
        document.insert(QStringLiteral("version"), 2);
        QJsonObject canvas;
        canvas.insert(QStringLiteral("width"), 1536.0);
        canvas.insert(QStringLiteral("height"), 1024.0);
        document.insert(QStringLiteral("canvas"), canvas);
        QJsonObject background;
        background.insert(QStringLiteral("type"), QStringLiteral("none"));
        document.insert(QStringLiteral("background"), background);
        document.insert(QStringLiteral("items"), QJsonArray{item, line, text});

        QStringList exportWarnings;
        const QJsonObject exported = Excalidraw::storyBoardToScene(document, &exportWarnings);
        const QJsonArray exportedElements = exported.value(QStringLiteral("elements")).toArray();
        ctx.check(exported.value(QStringLiteral("type")).toString() == QStringLiteral("excalidraw") &&
                      exported.value(QStringLiteral("version")).toInt() == 2 && exportedElements.size() == 3,
                  QStringLiteral("导出生成 type/version 正确的场景"),
                  QStringLiteral("type=%1 v=%2 n=%3")
                      .arg(exported.value(QStringLiteral("type")).toString())
                      .arg(exported.value(QStringLiteral("version")).toInt())
                      .arg(exportedElements.size()));
        ctx.check(exported.value(QStringLiteral("appState")).toObject().value(QStringLiteral("gridSize")).toInt() ==
                          32 &&
                      exported.value(QStringLiteral("files")).isObject(),
                  QStringLiteral("导出场景 appState.gridSize=32 且带 files"),
                  exported.value(QStringLiteral("appState")).toObject().value(QStringLiteral("gridSize")).toString());
        const QJsonObject exportedLine = exportedElements.at(1).toObject();
        ctx.check(exportedLine.value(QStringLiteral("type")).toString() == QStringLiteral("arrow") &&
                      exportedLine.value(QStringLiteral("endArrowhead")).toString() == QStringLiteral("arrow") &&
                      exportedLine.value(QStringLiteral("points")).toArray().size() == 2,
                  QStringLiteral("直线箭头导出为 arrow 且末端开口箭头"),
                  exportedLine.value(QStringLiteral("type")).toString());
        const QJsonObject exportedText = exportedElements.at(2).toObject();
        ctx.check(exportedText.value(QStringLiteral("type")).toString() == QStringLiteral("text") &&
                      exportedText.value(QStringLiteral("fontSize")).toDouble() > 19.0 &&
                      exportedText.value(QStringLiteral("fontSize")).toDouble() < 21.0,
                  QStringLiteral("文字导出为 text 且字号按 px≈pt*1.333 换算"),
                  QStringLiteral("fontSize=%1").arg(exportedText.value(QStringLiteral("fontSize")).toDouble()));
        ctx.check(exportedElements.at(0).toObject().value(QStringLiteral("type")).toString() ==
                      QStringLiteral("rectangle"),
                  QStringLiteral("矩形导出为 rectangle"), QStringLiteral("ok"));

        // 往返：导出的场景能再导入回画板文档。
        const QJsonObject roundTrip = Excalidraw::sceneToStoryBoard(exported, nullptr);
        ctx.check(roundTrip.value(QStringLiteral("format")).toString() == QStringLiteral("StoryBoardCanvas") &&
                      roundTrip.value(QStringLiteral("items")).toArray().size() == 3,
                  QStringLiteral("导出结果可再次导入（往返 3 个图元）"),
                  QStringLiteral("items=%1").arg(roundTrip.value(QStringLiteral("items")).toArray().size()));
    }
}
