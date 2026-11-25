#include "io/ExcalidrawFormat.h"
#include "items/properties/TextAttributes.h"
#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"
#include "selftest/core/SelftestHelpers.h"

#include <QColor>
#include <QFontMetricsF>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

namespace {

QJsonObject Element(const QString& type, qreal x, qreal y, qreal w, qreal h)
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

QJsonObject Scene(const QJsonArray& elements)
{
    QJsonObject root;
    root.insert(QStringLiteral("type"), QStringLiteral("excalidraw"));
    root.insert(QStringLiteral("version"), 2);
    root.insert(QStringLiteral("elements"), elements);
    return root;
}

} // namespace

void selftest::RunExcalidrawFormatTests(Context& ctx)
{
    // 识别：场景与剪贴板两种包装。
    {
        const QJsonObject clipboard = Scene(QJsonArray{});
        QJsonObject edited = clipboard;
        edited.insert(QStringLiteral("type"), QStringLiteral("excalidraw/clipboard"));
        ctx.Check(Excalidraw::IsScene(clipboard), QStringLiteral("Excalidraw 场景按 type=excalidraw 识别"),
                  QStringLiteral("type=%1").arg(clipboard.value(QStringLiteral("type")).toString()));
        ctx.Check(Excalidraw::IsClipboard(edited), QStringLiteral("Excalidraw 剪贴板按 type=excalidraw/clipboard 识别"),
                  QStringLiteral("type=%1").arg(edited.value(QStringLiteral("type")).toString()));
        QJsonObject plain;
        ctx.Check(!Excalidraw::IsScene(plain) && !Excalidraw::IsClipboard(plain),
                  QStringLiteral("普通 JSON 不会被当成 Excalidraw 内容"), QStringLiteral("empty object"));
    }

    // 导入：形状 / 文字 / 箭头 / 多段折线 / 不支持类型。
    QStringList warnings;
    QJsonArray elements;
    elements.append(Element(QStringLiteral("rectangle"), 10, 20, 120, 60));
    elements.append(Element(QStringLiteral("ellipse"), 200, 20, 80, 80));
    elements.append(Element(QStringLiteral("diamond"), 300, 20, 80, 60));
    {
        QJsonObject text = Element(QStringLiteral("text"), 10, 200, 100, 40);
        text.insert(QStringLiteral("text"), QStringLiteral("你好"));
        text.insert(QStringLiteral("fontSize"), 20.0);
        elements.append(text);
    }
    {
        QJsonObject arrow = Element(QStringLiteral("arrow"), 10, 300, 100, 0);
        arrow.insert(QStringLiteral("points"), QJsonArray{QJsonArray{0.0, 0.0}, QJsonArray{100.0, 0.0}});
        arrow.insert(QStringLiteral("startArrowhead"), QJsonValue(QJsonValue::Null));
        arrow.insert(QStringLiteral("endArrowhead"), QStringLiteral("triangle"));
        elements.append(arrow);
    }
    {
        QJsonObject polyline = Element(QStringLiteral("arrow"), 200, 300, 100, 60);
        polyline.insert(QStringLiteral("points"),
                        QJsonArray{QJsonArray{0.0, 0.0}, QJsonArray{50.0, 60.0}, QJsonArray{100.0, 0.0}});
        polyline.insert(QStringLiteral("endArrowhead"), QStringLiteral("dot"));
        elements.append(polyline);
    }
    {
        QJsonObject freedraw = Element(QStringLiteral("freedraw"), 400, 300, 60, 60);
        freedraw.insert(QStringLiteral("points"), QJsonArray{QJsonArray{0.0, 0.0}, QJsonArray{60.0, 60.0}});
        elements.append(freedraw);
    }
    elements.append(Element(QStringLiteral("image"), 500, 400, 40, 40));

    const QJsonArray items = Excalidraw::ElementsToItems(elements, &warnings);
    ctx.Check(items.size() == 7, QStringLiteral("Excalidraw 元素的导入条数正确（画片图跳过）"),
              QStringLiteral("count=%1 warnings=%2").arg(items.size()).arg(warnings.join(QStringLiteral(";"))));

    auto typeAt = [&items](int index) { return items.at(index).toObject().value(QStringLiteral("kind")).toString(); };
    auto shapeTypeAt = [&items](int index) {
        return items.at(index).toObject().value(QStringLiteral("shapeType")).toString();
    };
    ctx.Check(typeAt(0) == QStringLiteral("shape") && shapeTypeAt(0) == QStringLiteral("rectangle"),
              QStringLiteral("矩形导入为画板矩形"), QStringLiteral("type=%1").arg(typeAt(0)));
    ctx.Check(shapeTypeAt(1) == QStringLiteral("ellipse") && shapeTypeAt(2) == QStringLiteral("diamond"),
              QStringLiteral("椭圆与菱形都按原生类型导入"),
              QStringLiteral("ellipse=%1 diamond=%2").arg(shapeTypeAt(1), shapeTypeAt(2)));
    // 菱形已是独立类型：外接框原样保留（宽即水平对角线），不再做平行四边形换算。
    {
        const QJsonObject diamond = items.at(2).toObject();
        ctx.Check(qAbs(diamond.value(QStringLiteral("w")).toDouble() - 80.0) < 0.01 &&
                      qAbs(diamond.value(QStringLiteral("h")).toDouble() - 60.0) < 0.01 &&
                      qAbs(diamond.value(QStringLiteral("rotation")).toDouble()) < 0.01 &&
                      !diamond.contains(QStringLiteral("parallelogramAngle")),
                  QStringLiteral("Excalidraw 菱形按原框导入独立菱形类型"),
                  QStringLiteral("w=%1 h=%2 rotation=%3")
                      .arg(diamond.value(QStringLiteral("w")).toDouble())
                      .arg(diamond.value(QStringLiteral("h")).toDouble())
                      .arg(diamond.value(QStringLiteral("rotation")).toDouble()));
    }
    ctx.Check(typeAt(3) == QStringLiteral("text") &&
                  items.at(3).toObject().value(QStringLiteral("text")).toString() == QStringLiteral("你好"),
              QStringLiteral("文字内容原样带入"),
              QStringLiteral("text=%1").arg(items.at(3).toObject().value(QStringLiteral("text")).toString()));
    ctx.Check(items.at(3).toObject().value(QStringLiteral("fontSize")).toDouble() > 14.0 &&
                  items.at(3).toObject().value(QStringLiteral("fontSize")).toDouble() < 16.0,
              QStringLiteral("文字字号按 pt≈px/1.333 换算"),
              QStringLiteral("fontSize=%1").arg(items.at(3).toObject().value(QStringLiteral("fontSize")).toDouble()));

    // 文字框必须容得下首行基线与最后一行字形：Excalidraw 的外框没有画板内边距，
    // 直接照抄会把下半截裁掉（旧版“打开 .excalidraw 只显示上半部分”）。
    {
        const QJsonObject text = items.at(3).toObject();
        const QFont font = TextAttributes::DefaultFont(text.value(QStringLiteral("fontSize")).toDouble(32.0));
        const QFontMetricsF metrics(font);
        const QString content = text.value(QStringLiteral("text")).toString();
        const int lines = content.split(QLatin1Char('\n'), Qt::KeepEmptyParts).size();
        const qreal needed = 12.0 + metrics.ascent() + (lines - 1) * metrics.lineSpacing() + metrics.descent();
        const qreal stored =
            qMax(text.value(QStringLiteral("h")).toDouble(), text.value(QStringLiteral("baseHeight")).toDouble());
        ctx.Check(stored >= needed - 0.5, QStringLiteral("导入的文字框容得下整行字形（不会被裁掉下半截）"),
                  QStringLiteral("stored=%1 needed=%2").arg(stored).arg(needed));
    }
    // 两点箭头 → 直线图元（kind=straightline）+ 末端实心三角；起点无箭头。
    {
        const QJsonObject line = items.at(4).toObject();
        ctx.Check(
            line.value(QStringLiteral("kind")).toString() == QStringLiteral("straightline") &&
                line.value(QStringLiteral("endArrow")).toString() == QStringLiteral("triangle") &&
                line.value(QStringLiteral("startArrow")).toString() == QStringLiteral("none"),
            QStringLiteral("两点箭头导入为直线图元并保留三角箭头"),
            QStringLiteral("kind=%1 end=%2")
                .arg(line.value(QStringLiteral("kind")).toString(), line.value(QStringLiteral("endArrow")).toString()));
    }
    // 粘贴后必须落在画布内：segments 是图元本地坐标，不能把 Excalidraw 的世界坐标原样带走。
    {
        const QJsonObject line = items.at(4).toObject();
        const QJsonArray segments = line.value(QStringLiteral("segments")).toArray();
        qreal maxAbs = 0.0;
        for (const QJsonValue& segment : segments) {
            for (const QJsonValue& value : segment.toArray()) {
                maxAbs = qMax(maxAbs, qAbs(value.toDouble()));
            }
        }
        const qreal span =
            qMax(line.value(QStringLiteral("w")).toDouble(), line.value(QStringLiteral("h")).toDouble()) + 2.0;
        ctx.Check(!segments.isEmpty() && maxAbs <= span, QStringLiteral("直线段保存为图元本地坐标，粘贴后不会飞出画布"),
                  QStringLiteral("maxAbs=%1 span=%2 w=%3 h=%4")
                      .arg(maxAbs)
                      .arg(span)
                      .arg(line.value(QStringLiteral("w")).toDouble())
                      .arg(line.value(QStringLiteral("h")).toDouble()));
    }
    // 多段箭头 → 直线图元 + 折线段。
    {
        const QJsonObject curve = items.at(5).toObject();
        ctx.Check(curve.value(QStringLiteral("kind")).toString() == QStringLiteral("straightline") &&
                      curve.value(QStringLiteral("endArrow")).toString() == QStringLiteral("dot") &&
                      curve.value(QStringLiteral("segments")).toArray().size() == 2,
                  QStringLiteral("多段箭头导入为描线图元并保留拐点与圆点箭头"),
                  QStringLiteral("kind=%1 segs=%2")
                      .arg(curve.value(QStringLiteral("kind")).toString())
                      .arg(curve.value(QStringLiteral("segments")).toArray().size()));
    }
    {
        const QJsonObject freehand = items.at(6).toObject();
        ctx.Check(freehand.value(QStringLiteral("kind")).toString() == QStringLiteral("freehand"),
                  QStringLiteral("freedraw 导入为手绘曲线"),
                  QStringLiteral("kind=%1").arg(freehand.value(QStringLiteral("kind")).toString()));
    }
    ctx.Check(!warnings.isEmpty(), QStringLiteral("不支持的元素会留下降级说明"),
              QStringLiteral("count=%1").arg(warnings.size()));

    // 完整场景导入：画布尺寸覆盖元素包围盒，背景取 viewBackgroundColor。
    {
        QJsonObject root = Scene(QJsonArray{Element(QStringLiteral("rectangle"), 0, 0, 400, 300)});
        QJsonObject appState;
        appState.insert(QStringLiteral("viewBackgroundColor"), QStringLiteral("#ffffff"));
        root.insert(QStringLiteral("appState"), appState);
        QStringList sceneWarnings;
        const QJsonObject document = Excalidraw::SceneToStoryBoard(root, &sceneWarnings);
        const QJsonObject canvas = document.value(QStringLiteral("canvas")).toObject();
        ctx.Check(document.value(QStringLiteral("format")).toString() == QStringLiteral("StoryBoardCanvas") &&
                      canvas.value(QStringLiteral("width")).toDouble() >= 400.0 &&
                      canvas.value(QStringLiteral("height")).toDouble() >= 300.0 &&
                      document.value(QStringLiteral("items")).toArray().size() == 1,
                  QStringLiteral("场景导入生成合法画板文档（画布按包围盒扩展）"),
                  QStringLiteral("w=%1 h=%2 items=%3")
                      .arg(canvas.value(QStringLiteral("width")).toDouble())
                      .arg(canvas.value(QStringLiteral("height")).toDouble())
                      .arg(document.value(QStringLiteral("items")).toArray().size()));
        ctx.Check(document.value(QStringLiteral("background")).toObject().value(QStringLiteral("color")).toString() ==
                      QStringLiteral("#ffffff"),
                  QStringLiteral("场景背景色带入画板背景"),
                  document.value(QStringLiteral("background")).toObject().value(QStringLiteral("color")).toString());
    }

    // 导出：合法场景骨架 + 直线 / 矩形 / 文字 往返可读。
    {
        QJsonObject item;
        item.insert(QStringLiteral("kind"), QStringLiteral("shape"));
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
        line.insert(QStringLiteral("kind"), QStringLiteral("straightline"));
        line.insert(QStringLiteral("segments"), QJsonArray{QJsonArray{32.0, 32.0, 32.0, 32.0, 96.0, 96.0, 96.0, 96.0}});
        line.insert(QStringLiteral("width"), 4.0);
        line.insert(QStringLiteral("startArrow"), QStringLiteral("none"));
        line.insert(QStringLiteral("endArrow"), QStringLiteral("open"));
        QJsonObject text;
        text.insert(QStringLiteral("kind"), QStringLiteral("text"));
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
        const QJsonObject exported = Excalidraw::StoryBoardToScene(document, &exportWarnings);
        const QJsonArray exportedElements = exported.value(QStringLiteral("elements")).toArray();
        ctx.Check(exported.value(QStringLiteral("type")).toString() == QStringLiteral("excalidraw") &&
                      exported.value(QStringLiteral("version")).toInt() == 2 && exportedElements.size() == 3,
                  QStringLiteral("导出生成 type/version 正确的场景"),
                  QStringLiteral("type=%1 v=%2 n=%3")
                      .arg(exported.value(QStringLiteral("type")).toString())
                      .arg(exported.value(QStringLiteral("version")).toInt())
                      .arg(exportedElements.size()));
        // 需求 16：棋盘格边长从 32px 扩大到 64px，导出的 appState.gridSize 跟随画板网格。
        ctx.Check(exported.value(QStringLiteral("appState")).toObject().value(QStringLiteral("gridSize")).toInt() ==
                          64 &&
                      exported.value(QStringLiteral("files")).isObject(),
                  QStringLiteral("导出场景 appState.gridSize=64 且带 files"),
                  exported.value(QStringLiteral("appState")).toObject().value(QStringLiteral("gridSize")).toString());
        const QJsonObject exportedLine = exportedElements.at(1).toObject();
        ctx.Check(exportedLine.value(QStringLiteral("type")).toString() == QStringLiteral("arrow") &&
                      exportedLine.value(QStringLiteral("endArrowhead")).toString() == QStringLiteral("arrow") &&
                      exportedLine.value(QStringLiteral("points")).toArray().size() == 2,
                  QStringLiteral("直线箭头导出为 arrow 且末端开口箭头"),
                  exportedLine.value(QStringLiteral("type")).toString());
        const QJsonObject exportedText = exportedElements.at(2).toObject();
        ctx.Check(exportedText.value(QStringLiteral("type")).toString() == QStringLiteral("text") &&
                      exportedText.value(QStringLiteral("fontSize")).toDouble() > 19.0 &&
                      exportedText.value(QStringLiteral("fontSize")).toDouble() < 21.0,
                  QStringLiteral("文字导出为 text 且字号按 px≈pt*1.333 换算"),
                  QStringLiteral("fontSize=%1").arg(exportedText.value(QStringLiteral("fontSize")).toDouble()));
        ctx.Check(exportedElements.at(0).toObject().value(QStringLiteral("type")).toString() ==
                      QStringLiteral("rectangle"),
                  QStringLiteral("矩形导出为 rectangle"), QStringLiteral("ok"));

        // 往返：导出的场景能再导入回画板文档。
        const QJsonObject roundTrip = Excalidraw::SceneToStoryBoard(exported, nullptr);
        ctx.Check(roundTrip.value(QStringLiteral("format")).toString() == QStringLiteral("StoryBoardCanvas") &&
                      roundTrip.value(QStringLiteral("items")).toArray().size() == 3,
                  QStringLiteral("导出结果可再次导入（往返 3 个图元）"),
                  QStringLiteral("items=%1").arg(roundTrip.value(QStringLiteral("items")).toArray().size()));
    }

    // frame 变成图层：frameId 命中的元素归该层，层名取 frame name。
    {
        QJsonObject frameElement = Element(QStringLiteral("frame"), 0, 0, 300, 200);
        frameElement.insert(QStringLiteral("id"), QStringLiteral("f1"));
        frameElement.insert(QStringLiteral("name"), QStringLiteral("分镜一"));
        QJsonObject inside = Element(QStringLiteral("rectangle"), 10, 10, 100, 40);
        inside.insert(QStringLiteral("frameId"), QStringLiteral("f1"));
        QJsonObject outside = Element(QStringLiteral("rectangle"), 400, 10, 100, 40);
        QJsonArray layers;
        const QJsonArray framedItems =
            Excalidraw::ElementsToItems(QJsonArray{frameElement, inside, outside}, nullptr, &layers);
        const bool layerNamesOk =
            layers.size() == 2 &&
            layers.at(0).toObject().value(QStringLiteral("name")).toString() == QStringLiteral("图层 1") &&
            layers.at(1).toObject().value(QStringLiteral("name")).toString() == QStringLiteral("分镜一");
        ctx.Check(framedItems.size() == 2 && layerNamesOk &&
                      framedItems.at(0).toObject().value(QStringLiteral("layer")).toInt() == 2 &&
                      framedItems.at(1).toObject().value(QStringLiteral("layer")).toInt() == 1,
                  QStringLiteral("frame 建一层，frameId 命中的元素归该层"),
                  QStringLiteral("items=%1 layers=%2 in=%3 out=%4")
                      .arg(framedItems.size())
                      .arg(layers.size())
                      .arg(framedItems.at(0).toObject().value(QStringLiteral("layer")).toInt())
                      .arg(framedItems.at(1).toObject().value(QStringLiteral("layer")).toInt()));
    }

    // 样式 customData：填充样式与填充 / 描线透明度导出后能原样导入回来。
    {
        QJsonObject styled;
        styled.insert(QStringLiteral("kind"), QStringLiteral("shape"));
        styled.insert(QStringLiteral("shapeType"), QStringLiteral("rectangle"));
        styled.insert(QStringLiteral("x"), 0.0);
        styled.insert(QStringLiteral("y"), 0.0);
        styled.insert(QStringLiteral("w"), 100.0);
        styled.insert(QStringLiteral("h"), 60.0);
        styled.insert(QStringLiteral("strokeColor"), QStringLiteral("#c8ff0000"));
        styled.insert(QStringLiteral("fillColor"), QStringLiteral("#8000ff00"));
        styled.insert(QStringLiteral("fillStyle"), QStringLiteral("cross_hatch"));
        styled.insert(QStringLiteral("strokeWidth"), 2.0);
        QJsonObject document;
        document.insert(QStringLiteral("format"), QStringLiteral("StoryBoardCanvas"));
        document.insert(QStringLiteral("items"), QJsonArray{styled});
        const QJsonObject exportedStyled = Excalidraw::StoryBoardToScene(document, nullptr);
        const QJsonObject exportedElement = exportedStyled.value(QStringLiteral("elements")).toArray().at(0).toObject();
        const QJsonObject custom = exportedElement.value(QStringLiteral("customData")).toObject();
        const QJsonObject reimported = Excalidraw::SceneToStoryBoard(exportedStyled, nullptr);
        const QJsonObject reimportedItem = reimported.value(QStringLiteral("items")).toArray().at(0).toObject();
        const QColor reimportedFill(reimportedItem.value(QStringLiteral("fillColor")).toString());
        const QColor reimportedStroke(reimportedItem.value(QStringLiteral("strokeColor")).toString());
        ctx.Check(custom.value(QStringLiteral("fillStyle")).toString() == QStringLiteral("cross_hatch") &&
                      custom.value(QStringLiteral("fillAlpha")).toInt() == 128 &&
                      custom.value(QStringLiteral("strokeAlpha")).toInt() == 200 &&
                      reimportedItem.value(QStringLiteral("fillStyle")).toString() == QStringLiteral("cross_hatch") &&
                      reimportedFill.alpha() == 128 && reimportedStroke.alpha() == 200,
                  QStringLiteral("填充样式与透明度经 customData 原样往返"),
                  QStringLiteral("style=%1 fillAlpha=%2 strokeAlpha=%3 back=%4/%5")
                      .arg(custom.value(QStringLiteral("fillStyle")).toString())
                      .arg(custom.value(QStringLiteral("fillAlpha")).toInt())
                      .arg(custom.value(QStringLiteral("strokeAlpha")).toInt())
                      .arg(reimportedFill.alpha())
                      .arg(reimportedStroke.alpha()));
    }

    // 线型 / 透明度 / 圆角：Excalidraw 的 dashed / dotted 落到画板笔型，
    // line/text 的 opacity 不再丢，圆角导出为合法的 roundness 对象。
    {
        QJsonObject dashedLine = Element(QStringLiteral("line"), 0, 0, 100, 0);
        dashedLine.insert(QStringLiteral("strokeStyle"), QStringLiteral("dashed"));
        dashedLine.insert(QStringLiteral("opacity"), 40);
        dashedLine.insert(QStringLiteral("points"), QJsonArray{QJsonArray{0.0, 0.0}, QJsonArray{100.0, 0.0}});
        QJsonObject fadedText = Element(QStringLiteral("text"), 0, 100, 80, 30);
        fadedText.insert(QStringLiteral("strokeStyle"), QStringLiteral("dotted"));
        fadedText.insert(QStringLiteral("opacity"), 35);
        fadedText.insert(QStringLiteral("text"), QStringLiteral("淡"));
        fadedText.insert(QStringLiteral("fontSize"), 20.0);
        fadedText.insert(QStringLiteral("fontFamily"), 3);
        fadedText.insert(QStringLiteral("textAlign"), QStringLiteral("center"));
        const QJsonObject imported = Excalidraw::SceneToStoryBoard(Scene(QJsonArray{dashedLine, fadedText}), nullptr);
        const QJsonObject lineItem = imported.value(QStringLiteral("items")).toArray().at(0).toObject();
        const QJsonObject textItem = imported.value(QStringLiteral("items")).toArray().at(1).toObject();
        ctx.Check(lineItem.value(QStringLiteral("strokeStyle")).toInt() == 2 &&
                      qAbs(lineItem.value(QStringLiteral("opacity")).toDouble() - 0.4) < 0.01,
                  QStringLiteral("导入 dashed 线与 40% 透明度"),
                  QStringLiteral("style=%1 opacity=%2")
                      .arg(lineItem.value(QStringLiteral("strokeStyle")).toInt())
                      .arg(lineItem.value(QStringLiteral("opacity")).toDouble()));
        ctx.Check(textItem.value(QStringLiteral("opacity")).toDouble() > 0.3 &&
                      textItem.value(QStringLiteral("opacity")).toDouble() < 0.4,
                  QStringLiteral("导入文字透明度"),
                  QStringLiteral("opacity=%1").arg(textItem.value(QStringLiteral("opacity")).toDouble()));
        ctx.Check(textItem.value(QStringLiteral("fontFamily")).toString().contains(QStringLiteral("Cascadia")) &&
                      textItem.value(QStringLiteral("x")).toDouble() > 0.0,
                  QStringLiteral("导入文字字体编号与居中对齐位移"),
                  QStringLiteral("family=%1 x=%2")
                      .arg(textItem.value(QStringLiteral("fontFamily")).toString())
                      .arg(textItem.value(QStringLiteral("x")).toDouble()));

        QJsonObject styledShape;
        styledShape.insert(QStringLiteral("kind"), QStringLiteral("shape"));
        styledShape.insert(QStringLiteral("shapeType"), QStringLiteral("rectangle"));
        styledShape.insert(QStringLiteral("x"), 0.0);
        styledShape.insert(QStringLiteral("y"), 0.0);
        styledShape.insert(QStringLiteral("w"), 80.0);
        styledShape.insert(QStringLiteral("h"), 50.0);
        styledShape.insert(QStringLiteral("strokeWidth"), 2.0);
        styledShape.insert(QStringLiteral("strokeStyle"), 2);
        styledShape.insert(QStringLiteral("cornerRadius"), 12.0);
        QJsonObject document;
        document.insert(QStringLiteral("format"), QStringLiteral("StoryBoardCanvas"));
        document.insert(QStringLiteral("items"), QJsonArray{styledShape});
        const QJsonObject exported = Excalidraw::StoryBoardToScene(document, nullptr);
        const QJsonObject element = exported.value(QStringLiteral("elements")).toArray().at(0).toObject();
        const QJsonObject roundness = element.value(QStringLiteral("roundness")).toObject();
        ctx.Check(element.value(QStringLiteral("strokeStyle")).toString() == QStringLiteral("dashed") &&
                      roundness.value(QStringLiteral("type")).toInt() == 3 &&
                      qAbs(roundness.value(QStringLiteral("value")).toDouble() - 12.0) < 0.01,
                  QStringLiteral("导出虚线笔型与合法圆形圆角"),
                  QStringLiteral("style=%1 roundness=%2")
                      .arg(element.value(QStringLiteral("strokeStyle")).toString())
                      .arg(roundness.value(QStringLiteral("value")).toDouble()));
        ctx.Check(element.contains(QStringLiteral("index")) && element.value(QStringLiteral("created")).toInt() > 0,
                  QStringLiteral("导出补齐 index / created 规范字段"),
                  QStringLiteral("index=%1 created=%2")
                      .arg(element.value(QStringLiteral("index")).toString())
                      .arg(element.value(QStringLiteral("created")).toInt()));

        QJsonObject textDoc;
        QJsonObject exportTextItem;
        exportTextItem.insert(QStringLiteral("kind"), QStringLiteral("text"));
        exportTextItem.insert(QStringLiteral("text"), QStringLiteral("字"));
        exportTextItem.insert(QStringLiteral("fontFamily"), QStringLiteral("Virgil"));
        exportTextItem.insert(QStringLiteral("fontSize"), 12.0);
        textDoc.insert(QStringLiteral("format"), QStringLiteral("StoryBoardCanvas"));
        textDoc.insert(QStringLiteral("items"), QJsonArray{exportTextItem});
        const QJsonObject textElement = Excalidraw::StoryBoardToScene(textDoc, nullptr)
                                            .value(QStringLiteral("elements"))
                                            .toArray()
                                            .at(0)
                                            .toObject();
        ctx.Check(textElement.value(QStringLiteral("fontFamily")).toInt() == 2,
                  QStringLiteral("导出文字字体编号"),
                  QStringLiteral("fontFamily=%1").arg(textElement.value(QStringLiteral("fontFamily")).toInt()));
    }

    // 分组：画板组号与 groupParents 的嵌套链<->Excalidraw groupIds（内层在前）。
    {
        QJsonObject groupedShape;
        groupedShape.insert(QStringLiteral("kind"), QStringLiteral("shape"));
        groupedShape.insert(QStringLiteral("shapeType"), QStringLiteral("rectangle"));
        groupedShape.insert(QStringLiteral("x"), 0.0);
        groupedShape.insert(QStringLiteral("y"), 0.0);
        groupedShape.insert(QStringLiteral("w"), 50.0);
        groupedShape.insert(QStringLiteral("h"), 50.0);
        groupedShape.insert(QStringLiteral("group"), 5);
        QJsonObject parents;
        parents.insert(QStringLiteral("5"), 6);
        QJsonObject document;
        document.insert(QStringLiteral("format"), QStringLiteral("StoryBoardCanvas"));
        document.insert(QStringLiteral("items"), QJsonArray{groupedShape});
        document.insert(QStringLiteral("groupParents"), parents);
        const QJsonObject exported = Excalidraw::StoryBoardToScene(document, nullptr);
        const QJsonArray exportedGroupIds =
            exported.value(QStringLiteral("elements")).toArray().at(0).toObject().value(QStringLiteral("groupIds")).toArray();
        QJsonObject importedParents;
        const QJsonObject imported = Excalidraw::SceneToStoryBoard(exported, nullptr);
        const QJsonArray importedItems = imported.value(QStringLiteral("items")).toArray();
        const bool importedGroupOk = importedItems.size() == 1 &&
                                     importedItems.at(0).toObject().value(QStringLiteral("group")).toInt() > 0;
        const QJsonObject importedParentsJson = imported.value(QStringLiteral("groupParents")).toObject();
        ctx.Check(exportedGroupIds.size() == 2 && exportedGroupIds.at(0).toString() == QStringLiteral("5") &&
                      exportedGroupIds.at(1).toString() == QStringLiteral("6") && importedGroupOk &&
                      !importedParentsJson.isEmpty(),
                  QStringLiteral("分组按内层在前的 groupIds 导出并可导回组链"),
                  QStringLiteral("ids=%1 importedGroup=%2 parents=%3")
                      .arg(exportedGroupIds.size())
                      .arg(importedGroupOk ? QStringLiteral("yes") : QStringLiteral("no"))
                      .arg(importedParentsJson.size()));
    }

    // 填充样式：Excalidraw 原生四档与画板九档的近似映射，customData 保住原值。
    {
        QJsonObject hachure = Element(QStringLiteral("rectangle"), 0, 0, 50, 50);
        hachure.insert(QStringLiteral("backgroundColor"), QStringLiteral("#ff0000"));
        hachure.insert(QStringLiteral("fillStyle"), QStringLiteral("hachure"));
        QJsonObject crossHatch = Element(QStringLiteral("rectangle"), 60, 0, 50, 50);
        crossHatch.insert(QStringLiteral("backgroundColor"), QStringLiteral("#ff0000"));
        crossHatch.insert(QStringLiteral("fillStyle"), QStringLiteral("cross-hatch"));
        QJsonObject dots = Element(QStringLiteral("rectangle"), 120, 0, 50, 50);
        dots.insert(QStringLiteral("backgroundColor"), QStringLiteral("#ff0000"));
        dots.insert(QStringLiteral("fillStyle"), QStringLiteral("dots"));
        const QJsonObject imported = Excalidraw::SceneToStoryBoard(Scene(QJsonArray{hachure, crossHatch, dots}), nullptr);
        const QJsonArray importedItems = imported.value(QStringLiteral("items")).toArray();
        ctx.Check(importedItems.at(0).toObject().value(QStringLiteral("fillStyle")).toString() ==
                          QStringLiteral("hachure") &&
                      importedItems.at(1).toObject().value(QStringLiteral("fillStyle")).toString() ==
                          QStringLiteral("cross_hatch") &&
                      importedItems.at(2).toObject().value(QStringLiteral("fillStyle")).toString() ==
                          QStringLiteral("dots"),
                  QStringLiteral("导入 Excalidraw 填充样式按最接近档落位"),
                  QStringLiteral("styles=%1/%2/%3")
                      .arg(importedItems.at(0).toObject().value(QStringLiteral("fillStyle")).toString(),
                           importedItems.at(1).toObject().value(QStringLiteral("fillStyle")).toString(),
                           importedItems.at(2).toObject().value(QStringLiteral("fillStyle")).toString()));

        QJsonArray exportItems;
        for (const QString& style : {QStringLiteral("reverse_hatch"), QStringLiteral("grid_hatch"),
                                     QStringLiteral("brick")}) {
            QJsonObject shape;
            shape.insert(QStringLiteral("kind"), QStringLiteral("shape"));
            shape.insert(QStringLiteral("shapeType"), QStringLiteral("rectangle"));
            shape.insert(QStringLiteral("x"), 0.0);
            shape.insert(QStringLiteral("y"), 0.0);
            shape.insert(QStringLiteral("w"), 40.0);
            shape.insert(QStringLiteral("h"), 40.0);
            shape.insert(QStringLiteral("fillColor"), QStringLiteral("#ff0000"));
            shape.insert(QStringLiteral("fillStyle"), style);
            exportItems.append(shape);
        }
        QJsonObject document;
        document.insert(QStringLiteral("format"), QStringLiteral("StoryBoardCanvas"));
        document.insert(QStringLiteral("items"), exportItems);
        const QJsonArray elements = Excalidraw::StoryBoardToScene(document, nullptr)
                                        .value(QStringLiteral("elements"))
                                        .toArray();
        const QString reverseStyle = elements.at(0).toObject().value(QStringLiteral("fillStyle")).toString();
        const QString gridStyle = elements.at(1).toObject().value(QStringLiteral("fillStyle")).toString();
        const QString brickStyle = elements.at(2).toObject().value(QStringLiteral("fillStyle")).toString();
        const QString reverseCustom = elements.at(0)
                                          .toObject()
                                          .value(QStringLiteral("customData"))
                                          .toObject()
                                          .value(QStringLiteral("fillStyle"))
                                          .toString();
        ctx.Check(reverseStyle == QStringLiteral("hachure") && gridStyle == QStringLiteral("cross-hatch") &&
                      brickStyle == QStringLiteral("zigzag") && reverseCustom == QStringLiteral("reverse_hatch"),
                  QStringLiteral("导出画板专属填充样式并保留原值"),
                  QStringLiteral("styles=%1/%2/%3 custom=%4")
                      .arg(reverseStyle, gridStyle, brickStyle, reverseCustom));
    }

    // 图层 <-> frame：非默认层导出成 frame，层内图元写 frameId，再导入还原图层。
    {
        QJsonObject layeredShape;
        layeredShape.insert(QStringLiteral("kind"), QStringLiteral("shape"));
        layeredShape.insert(QStringLiteral("shapeType"), QStringLiteral("rectangle"));
        layeredShape.insert(QStringLiteral("x"), 20.0);
        layeredShape.insert(QStringLiteral("y"), 30.0);
        layeredShape.insert(QStringLiteral("w"), 60.0);
        layeredShape.insert(QStringLiteral("h"), 40.0);
        layeredShape.insert(QStringLiteral("layer"), 2);
        QJsonObject layer;
        layer.insert(QStringLiteral("id"), 2);
        layer.insert(QStringLiteral("name"), QStringLiteral("第二层"));
        layer.insert(QStringLiteral("visible"), true);
        layer.insert(QStringLiteral("locked"), false);
        QJsonObject document;
        document.insert(QStringLiteral("format"), QStringLiteral("StoryBoardCanvas"));
        document.insert(QStringLiteral("items"), QJsonArray{layeredShape});
        document.insert(QStringLiteral("layers"), QJsonArray{layer});
        const QJsonObject scene = Excalidraw::StoryBoardToScene(document, nullptr);
        const QJsonArray elements = scene.value(QStringLiteral("elements")).toArray();
        const QJsonObject shapeElement = elements.at(0).toObject();
        const QJsonObject frameElement = elements.at(1).toObject();
        const QJsonObject reimported = Excalidraw::SceneToStoryBoard(scene, nullptr);
        const QJsonObject reimportedItem = reimported.value(QStringLiteral("items")).toArray().at(0).toObject();
        ctx.Check(shapeElement.value(QStringLiteral("frameId")).toString() ==
                          frameElement.value(QStringLiteral("id")).toString() &&
                      frameElement.value(QStringLiteral("type")).toString() == QStringLiteral("frame") &&
                      frameElement.value(QStringLiteral("name")).toString() == QStringLiteral("第二层") &&
                      reimportedItem.value(QStringLiteral("layer")).toInt() == 2,
                  QStringLiteral("图层导出为 frame 并可导回"),
                  QStringLiteral("frameId=%1 name=%2 layer=%3")
                      .arg(shapeElement.value(QStringLiteral("frameId")).toString(),
                           frameElement.value(QStringLiteral("name")).toString())
                      .arg(reimportedItem.value(QStringLiteral("layer")).toInt()));
    }

    // 复制到 Excalidraw：画板图元同时产出 excalidraw/clipboard JSON。
    {
        QJsonObject shape;
        shape.insert(QStringLiteral("kind"), QStringLiteral("shape"));
        shape.insert(QStringLiteral("shapeType"), QStringLiteral("rectangle"));
        shape.insert(QStringLiteral("x"), 10.0);
        shape.insert(QStringLiteral("y"), 20.0);
        shape.insert(QStringLiteral("w"), 60.0);
        shape.insert(QStringLiteral("h"), 40.0);
        QJsonObject document;
        document.insert(QStringLiteral("format"), QStringLiteral("StoryBoardCanvas"));
        document.insert(QStringLiteral("items"), QJsonArray{shape});
        const QJsonObject clipboard = Excalidraw::StoryBoardToClipboard(document);
        const QJsonObject element = clipboard.value(QStringLiteral("elements")).toArray().at(0).toObject();
        ctx.Check(Excalidraw::IsClipboard(clipboard) &&
                      element.value(QStringLiteral("type")).toString() == QStringLiteral("rectangle"),
                  QStringLiteral("复制产出 Excalidraw 剪贴板 JSON"),
                  QStringLiteral("type=%1 elementType=%2")
                      .arg(clipboard.value(QStringLiteral("type")).toString(),
                           element.value(QStringLiteral("type")).toString()));
    }

    // 箭头头补齐：Excalidraw 的 circle / triangle_outline 落到画板的圆点 / 空心三角。
    {
        QJsonObject circleArrow = Element(QStringLiteral("arrow"), 0, 0, 80, 0);
        circleArrow.insert(QStringLiteral("points"), QJsonArray{QJsonArray{0.0, 0.0}, QJsonArray{80.0, 0.0}});
        circleArrow.insert(QStringLiteral("startArrowhead"), QStringLiteral("circle"));
        circleArrow.insert(QStringLiteral("endArrowhead"), QStringLiteral("triangle_outline"));
        const QJsonObject imported =
            Excalidraw::SceneToStoryBoard(Scene(QJsonArray{circleArrow}), nullptr);
        const QJsonObject lineItem = imported.value(QStringLiteral("items")).toArray().at(0).toObject();
        ctx.Check(lineItem.value(QStringLiteral("startArrow")).toString() == QStringLiteral("dot") &&
                      lineItem.value(QStringLiteral("endArrow")).toString() == QStringLiteral("hollow-triangle"),
                  QStringLiteral("导入 circle 与 triangle_outline 箭头头"),
                  QStringLiteral("start=%1 end=%2")
                      .arg(lineItem.value(QStringLiteral("startArrow")).toString(),
                           lineItem.value(QStringLiteral("endArrow")).toString()));
    }

    // 端到端自检：图片 / 分组 / 虚线 / 圆角 / 图层混合导出后再导入，逐项核对。
    {
        QJsonObject rect;
        rect.insert(QStringLiteral("kind"), QStringLiteral("shape"));
        rect.insert(QStringLiteral("shapeType"), QStringLiteral("rectangle"));
        rect.insert(QStringLiteral("x"), 0.0);
        rect.insert(QStringLiteral("y"), 0.0);
        rect.insert(QStringLiteral("w"), 120.0);
        rect.insert(QStringLiteral("h"), 80.0);
        rect.insert(QStringLiteral("strokeStyle"), 2);
        rect.insert(QStringLiteral("cornerRadius"), 16.0);
        rect.insert(QStringLiteral("group"), 5);
        rect.insert(QStringLiteral("fillStyle"), QStringLiteral("dots"));
        rect.insert(QStringLiteral("fillColor"), QStringLiteral("#ff0000"));
        rect.insert(QStringLiteral("layer"), 2);
        QJsonObject picture = rect;
        picture.insert(QStringLiteral("x"), 200.0);
        picture.insert(QStringLiteral("strokeStyle"), 1);
        picture.insert(QStringLiteral("cornerRadius"), 0.0);
        picture.remove(QStringLiteral("group"));
        QJsonObject fillImage;
        fillImage.insert(QStringLiteral("data"), QStringLiteral("QUJD"));
        fillImage.insert(QStringLiteral("mode"), QStringLiteral("stretch"));
        picture.insert(QStringLiteral("fillImage"), fillImage);
        QJsonObject line;
        line.insert(QStringLiteral("kind"), QStringLiteral("straightline"));
        line.insert(QStringLiteral("x"), 0.0);
        line.insert(QStringLiteral("y"), 200.0);
        line.insert(QStringLiteral("w"), 100.0);
        line.insert(QStringLiteral("h"), 0.0);
        line.insert(QStringLiteral("strokeStyle"), 2);
        line.insert(QStringLiteral("endArrow"), QStringLiteral("hollow-triangle"));
        line.insert(QStringLiteral("baseWidth"), 100.0);
        line.insert(QStringLiteral("baseHeight"), 0.0);
        line.insert(QStringLiteral("layer"), 2);
        QJsonArray segments;
        segments.append(QJsonArray{0.0, 0.0, 33.0, 0.0, 67.0, 0.0, 100.0, 0.0});
        line.insert(QStringLiteral("segments"), segments);

        QJsonObject document;
        document.insert(QStringLiteral("format"), QStringLiteral("StoryBoardCanvas"));
        document.insert(QStringLiteral("items"), QJsonArray{rect, picture, line});
        QJsonObject parents;
        parents.insert(QStringLiteral("5"), 6);
        document.insert(QStringLiteral("groupParents"), parents);
        QJsonObject layer;
        layer.insert(QStringLiteral("id"), 2);
        layer.insert(QStringLiteral("name"), QStringLiteral("上层"));
        document.insert(QStringLiteral("layers"), QJsonArray{layer});

        const QJsonObject scene = Excalidraw::StoryBoardToScene(document, nullptr);
        const QJsonObject reimported = Excalidraw::SceneToStoryBoard(scene, nullptr, true);
        const QJsonArray items = reimported.value(QStringLiteral("items")).toArray();
        const QJsonObject backRect = items.at(0).toObject();
        const QJsonObject backPicture = items.at(1).toObject();
        const QJsonObject backLine = items.at(2).toObject();
        ctx.Check(items.size() == 3 && backRect.value(QStringLiteral("strokeStyle")).toInt() == 2 &&
                      qAbs(backRect.value(QStringLiteral("cornerRadius")).toDouble() - 16.0) < 0.01 &&
                      backRect.value(QStringLiteral("group")).toInt() == 5 &&
                      backRect.value(QStringLiteral("fillStyle")).toString() == QStringLiteral("dots"),
                  QStringLiteral("端到端：虚线/圆角/分组/填充样式往返"),
                  QStringLiteral("count=%1 group=%2 fill=%3")
                      .arg(items.size())
                      .arg(backRect.value(QStringLiteral("group")).toInt())
                      .arg(backRect.value(QStringLiteral("fillStyle")).toString()));
        ctx.Check(backPicture.value(QStringLiteral("shapeType")).toString() == QStringLiteral("rectangle") &&
                      backPicture.value(QStringLiteral("fillImage")).toObject().value(QStringLiteral("data")).toString() ==
                          QStringLiteral("QUJD"),
                  QStringLiteral("端到端：图片往返"),
                  QStringLiteral("shape=%1").arg(backPicture.value(QStringLiteral("shapeType")).toString()));
        ctx.Check(backLine.value(QStringLiteral("endArrow")).toString() == QStringLiteral("hollow-triangle") &&
                      backLine.value(QStringLiteral("strokeStyle")).toInt() == 2,
                  QStringLiteral("端到端：箭头头与虚线往返"),
                  QStringLiteral("end=%1 style=%2")
                      .arg(backLine.value(QStringLiteral("endArrow")).toString())
                      .arg(backLine.value(QStringLiteral("strokeStyle")).toInt()));
        const QJsonArray layers = reimported.value(QStringLiteral("layers")).toArray();
        QStringList layerNames;
        for (const QJsonValue& layerValue : layers) {
            layerNames.append(layerValue.toObject().value(QStringLiteral("name")).toString());
        }
        ctx.Check(layerNames.contains(QStringLiteral("上层")), QStringLiteral("端到端：图层名往返"),
                  layerNames.join(QLatin1Char('|')));
    }

    // 图元身份：导入沿用 Excalidraw 元素 id，导出写回同一个 id，绑定关系才能往返。
    {
        QJsonObject source = Element(QStringLiteral("rectangle"), 0, 0, 40, 40);
        source.insert(QStringLiteral("id"), QStringLiteral("shape-1"));
        QJsonObject storyboard = Excalidraw::SceneToStoryBoard(Scene(QJsonArray{source}), nullptr);
        const QJsonObject item = storyboard.value(QStringLiteral("items")).toArray().at(0).toObject();
        const QJsonObject scene = Excalidraw::StoryBoardToScene(storyboard, nullptr);
        const QJsonObject element = scene.value(QStringLiteral("elements")).toArray().at(0).toObject();
        ctx.Check(item.value(QStringLiteral("uid")).toString() == QStringLiteral("shape-1") &&
                      element.value(QStringLiteral("id")).toString() == QStringLiteral("shape-1"),
                  QStringLiteral("Excalidraw 元素 id 与图元 uid 双向沿用"),
                  QStringLiteral("uid=%1 id=%2")
                      .arg(item.value(QStringLiteral("uid")).toString(),
                           element.value(QStringLiteral("id")).toString()));
    }

    // 位图往返：默认跳过 image 并留 warning；勾选后落成“矩形 + 图片填充”，
    // 再导出仍是 image 元素 + files 表，数据不丢。
    {
        QJsonObject imageElement = Element(QStringLiteral("image"), 0, 0, 40, 40);
        imageElement.insert(QStringLiteral("fileId"), QStringLiteral("img1"));
        QJsonObject file;
        file.insert(QStringLiteral("dataURL"), QStringLiteral("data:image/png;base64,QUJD"));
        QJsonObject files;
        files.insert(QStringLiteral("img1"), file);
        QJsonObject withImage = Scene(QJsonArray{imageElement});
        withImage.insert(QStringLiteral("files"), files);
        QStringList skippedWarnings;
        const QJsonObject skipped = Excalidraw::SceneToStoryBoard(withImage, &skippedWarnings, false);
        const QJsonObject kept = Excalidraw::SceneToStoryBoard(withImage, nullptr, true);
        const QJsonObject keptItem = kept.value(QStringLiteral("items")).toArray().at(0).toObject();
        const QJsonObject keptFill = keptItem.value(QStringLiteral("fillImage")).toObject();
        ctx.Check(skipped.isEmpty() && !skippedWarnings.isEmpty() &&
                      keptItem.value(QStringLiteral("kind")).toString() == QStringLiteral("shape") &&
                      keptItem.value(QStringLiteral("shapeType")).toString() == QStringLiteral("rectangle") &&
                      keptFill.value(QStringLiteral("data")).toString() == QStringLiteral("QUJD"),
                  QStringLiteral("位图保留选项：不勾选跳过并提示，勾选后落成矩形图片填充"),
                  QStringLiteral("skipped=%1 kind=%2 data=%3")
                      .arg(skipped.isEmpty() ? QStringLiteral("empty") : QStringLiteral("value"))
                      .arg(keptItem.value(QStringLiteral("kind")).toString())
                      .arg(keptFill.value(QStringLiteral("data")).toString()));

        QStringList exportWarnings;
        const QJsonObject exported = Excalidraw::StoryBoardToScene(kept, &exportWarnings);
        const QJsonObject exportedImage = exported.value(QStringLiteral("elements")).toArray().at(0).toObject();
        const QString fileId = exportedImage.value(QStringLiteral("fileId")).toString();
        const QJsonObject exportedFile = exported.value(QStringLiteral("files")).toObject().value(fileId).toObject();
        const QJsonObject reimported = Excalidraw::SceneToStoryBoard(exported, nullptr, true);
        const QJsonObject reimportedItem = reimported.value(QStringLiteral("items")).toArray().at(0).toObject();
        ctx.Check(exportedImage.value(QStringLiteral("type")).toString() == QStringLiteral("image") &&
                      !fileId.isEmpty() &&
                      exportedFile.value(QStringLiteral("dataURL")).toString() ==
                          QStringLiteral("data:image/png;base64,QUJD") &&
                      reimportedItem.value(QStringLiteral("fillImage")).toObject().value(QStringLiteral("data")).toString() ==
                          QStringLiteral("QUJD"),
                  QStringLiteral("图片导出为 image 元素 + files 并往返保真"),
                  QStringLiteral("fileId=%1 type=%2")
                      .arg(fileId)
                      .arg(exportedImage.value(QStringLiteral("type")).toString()));
    }
}
