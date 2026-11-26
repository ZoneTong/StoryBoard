#include "io/ExcalidrawFormat.h"
#include "io/ImportExportController.h"
#include "items/CanvasItem.h"
#include "items/lines/LineAbstractItem.h"
#include "items/properties/FillAttributes.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "items/shapes/ShapeFactory.h"
#include "items/text/TextItem.h"
#include "scene/CanvasScene.h"
#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/mainwindow/MainWindowEditActionsController.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QList>
#include <QMimeData>
#include <QPointF>
#include <QSizeF>
#include <QUndoStack>
#include <QUrl>
#include <QWidget>

namespace {

// 功能键只看绑定键是否正常工作；副键组合只要不与任何功能冲突即可，不整组断言。
QAction* FindActionByObjectName(QWidget* window, const QString& objectName)
{
    return window->findChild<QAction*>(objectName);
}

QAction* FindActionByText(QWidget* window, const QString& text)
{
    for (QAction* action : window->findChildren<QAction*>()) {
        if (action->text() == text) {
            return action;
        }
    }
    return nullptr;
}

} // namespace

void selftest::RunClipboardShortcutTests(Context& ctx)
{
    // 剪贴板序列化：选中图元写进 JSON，跨场景粘贴时重建、偏移、清分组，名称唯一。
    {
        CanvasScene sourceScene;
        sourceScene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        sourceScene.SetMode(ToolMode::EDIT);
        QUndoStack sourceUndo;
        sourceScene.SetUndoStack(&sourceUndo);

        auto* source = ShapeFactory::Create(ShapeType::RECTANGLE);
        source->SetContentSize(QSizeF(120.0, 80.0));
        source->setPos(QPointF(40.0, 50.0));
        sourceScene.AddItemsWithUndo({source}, QStringLiteral("Add clipboard source"));
        source->setSelected(true);

        const QJsonArray clipboard = sourceScene.ClipboardItems();
        ctx.Check(clipboard.size() == 1, QStringLiteral("复制序列化选中的可编辑图元"),
                  QString::number(clipboard.size()));

        CanvasScene pasteScene;
        pasteScene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        pasteScene.SetMode(ToolMode::EDIT);
        QUndoStack pasteUndo;
        pasteScene.SetUndoStack(&pasteUndo);

        const int pasted = pasteScene.PasteClipboardItems(clipboard);
        ctx.Check(pasted == 1 && pasteScene.CanvasItemCount() == 1, QStringLiteral("粘贴把剪贴板图元重建到画布"),
                  QString::number(pasted));

        CanvasItem* copy = nullptr;
        for (CanvasItem* item : pasteScene.CanvasItems()) {
            copy = item;
        }
        // 需求 2：粘贴落点重新计算到视图/画布中心区域，不再固定叠在源图元旁边。
        const QPointF pastedCenter = copy ? copy->sceneBoundingRect().center() : QPointF();
        const bool insideCanvas = pasteScene.CanvasRect().contains(pastedCenter);
        const bool movedAwayFromSource =
            copy && (pastedCenter - source->sceneBoundingRect().center()).manhattanLength() > 1.0;
        ctx.Check(copy && insideCanvas && movedAwayFromSource, QStringLiteral("粘贴副本落在画布内并离开源图元位置"),
                  QStringLiteral("%1,%2").arg(pastedCenter.x()).arg(pastedCenter.y()));
        ctx.Check(copy && copy->GroupId() == 0, QStringLiteral("粘贴副本不带原分组关系"));
        ctx.Check(copy && copy->Uid() != source->Uid() && !copy->Uid().isEmpty(),
                  QStringLiteral("粘贴副本重新签发图元身份"));
        ctx.Check(pasteUndo.count() == 1, QStringLiteral("粘贴只产生一条撤销记录"));

        // 方向键微调：编辑模式按偏移移动选中图元，撤销可还原。
        if (copy) {
            copy->setSelected(true);
            const QPointF before = copy->pos();
            const int nudged = pasteScene.NudgeSelected(QPointF(5.0, 0.0));
            ctx.Check(nudged == 1 && copy->pos() == before + QPointF(5.0, 0.0),
                      QStringLiteral("方向键微调移动选中图元"),
                      QStringLiteral("%1,%2").arg(copy->pos().x()).arg(copy->pos().y()));
            ctx.Check(pasteUndo.count() == 2, QStringLiteral("微调只产生一条几何撤销记录"));
            pasteUndo.undo();
            QApplication::processEvents();
            ctx.Check(copy->pos() == before, QStringLiteral("撤销微调恢复图元位置"));
        }
    }

    // Excalidraw 剪贴板：text/plain 与专用 MIME 都要能被粘贴动作识别。
    {
        const QByteArray excalidrawJson =
            R"({"type":"excalidraw/clipboard","elements":[{"id":"r1","type":"rectangle","x":0,"y":0,"width":80,"height":40}]})";
        QMimeData textMime;
        textMime.setText(QString::fromUtf8(excalidrawJson));
        QMimeData excalidrawMime;
        excalidrawMime.setData(QStringLiteral("application/vnd.excalidraw+json"), excalidrawJson);
        QMimeData plainMime;
        plainMime.setText(QStringLiteral("not excalidraw"));
        ctx.Check(MainWindowEditActionsController::CanPaste(&textMime) &&
                      MainWindowEditActionsController::CanPaste(&excalidrawMime) &&
                      !MainWindowEditActionsController::CanPaste(&plainMime),
                  QStringLiteral("粘贴识别 text/plain 与 Excalidraw MIME"));

        MainWindow clipboardWindow;
        clipboardWindow.View()->SetMode(ToolMode::EDIT);
        auto* clipboardMime = new QMimeData;
        clipboardMime->setText(QString::fromUtf8(excalidrawJson));
        QApplication::clipboard()->setMimeData(clipboardMime);
        QApplication::processEvents();
        QAction* pasteAction = FindActionByObjectName(&clipboardWindow, QStringLiteral("PasteItemsAction"));
        ctx.Check(pasteAction && pasteAction->isEnabled(), QStringLiteral("外部复制 Excalidraw 后粘贴动作自动启用"));
        CanvasScene* scene = clipboardWindow.findChild<CanvasScene*>();
        const int beforePaste = scene ? scene->CanvasItemCount() : -1;
        if (pasteAction) {
            pasteAction->trigger();
            QApplication::processEvents();
        }
        ctx.Check(scene && scene->CanvasItemCount() == beforePaste + 1,
                  QStringLiteral("粘贴动作把 Excalidraw 元素落到画布"),
                  QStringLiteral("before=%1 after=%2").arg(beforePaste).arg(scene ? scene->CanvasItemCount() : -1));
        QApplication::clipboard()->clear();
    }

    // Excalidraw 粘贴的还原度：纯色 / 斜线填充样式要落进图元，箭头绑定要指向粘贴后的新 uid。
    {
        const QByteArray sceneJson = R"({"type":"excalidraw/clipboard","elements":[
            {"id":"paste-solid","type":"rectangle","x":0,"y":0,"width":100,"height":60,
             "backgroundColor":"#a5d8ff","fillStyle":"solid","strokeColor":"#1e1e1e","strokeWidth":2},
            {"id":"paste-hachure","type":"rectangle","x":140,"y":0,"width":100,"height":60,
             "backgroundColor":"#fab005","fillStyle":"hachure","strokeColor":"#1e1e1e","strokeWidth":2},
            {"id":"paste-arrow","type":"arrow","x":10,"y":90,"width":90,"height":30,
             "points":[[0,0],[90,30]],"strokeColor":"#1e1e1e","strokeWidth":2,
             "startBinding":{"elementId":"paste-solid","focus":0,"gap":4},"endBinding":null,
             "startArrowhead":null,"endArrowhead":"arrow"},
            {"id":"paste-text","type":"text","x":30,"y":15,"width":40,"height":30,"text":"Hi",
             "originalText":"Hi","fontSize":20,"fontFamily":2,"textAlign":"center","verticalAlign":"middle",
             "containerId":"paste-solid","strokeColor":"#1e1e1e","backgroundColor":"transparent",
             "fillStyle":"solid"}]})";

        MainWindow pasteWindow;
        pasteWindow.View()->SetMode(ToolMode::EDIT);
        auto* pasteMime = new QMimeData;
        pasteMime->setText(QString::fromUtf8(sceneJson));
        QApplication::clipboard()->setMimeData(pasteMime);
        QApplication::processEvents();
        QAction* pasteAction = FindActionByObjectName(&pasteWindow, QStringLiteral("PasteItemsAction"));
        if (pasteAction) {
            pasteAction->trigger();
            QApplication::processEvents();
        }

        CanvasScene* scene = pasteWindow.findChild<CanvasScene*>();
        QStringList fillStyles;
        QString solidUid;
        QString arrowTarget;
        QString textContainer;
        if (scene) {
            for (CanvasItem* item : scene->CanvasItems()) {
                if (auto* shape = dynamic_cast<ShapeAbstractItem*>(item)) {
                    fillStyles.append(FillStyleToString(shape->fillStyle()));
                    if (shape->FillColor().name() == QStringLiteral("#a5d8ff")) {
                        solidUid = shape->Uid();
                    }
                } else if (auto* line = dynamic_cast<LineAbstractItem*>(item)) {
                    arrowTarget = line->StartBinding().value(QStringLiteral("elementId")).toString();
                } else if (auto* text = dynamic_cast<TextItem*>(item)) {
                    textContainer = text->ContainerUid();
                }
            }
        }
        ctx.Check(scene && fillStyles.count(QStringLiteral("solid")) == 1 &&
                      fillStyles.count(QStringLiteral("hachure")) == 1,
                  QStringLiteral("粘贴保留 Excalidraw 矩形的纯色与斜线填充"), fillStyles.join(QLatin1Char(',')));
        ctx.Check(!solidUid.isEmpty() && arrowTarget == solidUid,
                  QStringLiteral("粘贴后箭头绑定改写为粘贴图元的新 uid"),
                  QStringLiteral("target=%1 solidUid=%2").arg(arrowTarget, solidUid));
        ctx.Check(!solidUid.isEmpty() && textContainer == solidUid,
                  QStringLiteral("粘贴后容器文字改写到粘贴图元的新 uid"),
                  QStringLiteral("container=%1 solidUid=%2").arg(textContainer, solidUid));
        QApplication::clipboard()->clear();
    }

    // 粘贴 Excalidraw 的 frame 内容：帧号不能直接当图层号落到目标画布（会撞上同号层），
    // 统一并入当前活动层；容器文字在 ElementsToItems 阶段完成折行。
    {
        const QByteArray frameJson = R"({"type":"excalidraw/clipboard","elements":[
            {"id":"frame-1","type":"frame","x":0,"y":0,"width":320,"height":200,"name":"框 A","frameId":null},
            {"id":"in-frame","type":"rectangle","x":10,"y":10,"width":120,"height":80,"frameId":"frame-1",
             "backgroundColor":"#a5d8ff","fillStyle":"solid","strokeColor":"#1e1e1e","strokeWidth":2},
            {"id":"frame-text","type":"text","x":20,"y":20,"width":200,"height":30,
             "text":"容器里一段很长的文字，粘贴后必须按容器宽度重新折行",
             "originalText":"容器里一段很长的文字，粘贴后必须按容器宽度重新折行","fontSize":20,"fontFamily":2,
             "textAlign":"center","verticalAlign":"middle","containerId":"in-frame",
             "strokeColor":"#1e1e1e","backgroundColor":"transparent","fillStyle":"solid"}]})";

        MainWindow frameWindow;
        frameWindow.View()->SetMode(ToolMode::EDIT);
        CanvasScene* frameScene = frameWindow.findChild<CanvasScene*>();
        const int activeLayer = frameScene ? frameScene->ActiveLayerId() : -1;
        auto* frameMime = new QMimeData;
        frameMime->setText(QString::fromUtf8(frameJson));
        QApplication::clipboard()->setMimeData(frameMime);
        QApplication::processEvents();
        QAction* framePaste = FindActionByObjectName(&frameWindow, QStringLiteral("PasteItemsAction"));
        if (framePaste) {
            framePaste->trigger();
            QApplication::processEvents();
        }

        bool layersOk = frameScene && frameScene->CanvasItemCount() == 2;
        QString containerUid;
        QSizeF textSize;
        qreal boxWidth = 0.0;
        if (frameScene) {
            for (CanvasItem* item : frameScene->CanvasItems()) {
                layersOk = layersOk && item->LayerId() == activeLayer;
                if (auto* shape = dynamic_cast<ShapeAbstractItem*>(item)) {
                    boxWidth = shape->ContentSize().width();
                } else if (auto* text = dynamic_cast<TextItem*>(item)) {
                    containerUid = text->ContainerUid();
                    textSize = text->ContentSize();
                }
            }
        }
        ctx.Check(
            layersOk, QStringLiteral("粘贴 Excalidraw frame 内容并入当前活动层，不撞同号图层"),
            QStringLiteral("active=%1 count=%2").arg(activeLayer).arg(frameScene ? frameScene->CanvasItemCount() : -1));
        ctx.Check(
            !containerUid.isEmpty() && boxWidth > 0.0 && textSize.width() <= boxWidth + 0.5,
            QStringLiteral("粘贴后的容器文字按容器宽度重新折行"),
            QStringLiteral("container=%1 textW=%2 boxW=%3").arg(containerUid).arg(textSize.width()).arg(boxWidth));
        QApplication::clipboard()->clear();
    }

    // 拖放：.excalidraw 与 .sbcanvas 走同一条“打开文档”入口。
    {
        MainWindow dropWindow;
        ImportExportController io(&dropWindow);
        QMimeData canvasMime;
        canvasMime.setUrls({QUrl::fromLocalFile(QStringLiteral("C:/tmp/demo.sbcanvas"))});
        QMimeData excalidrawMime;
        excalidrawMime.setUrls({QUrl::fromLocalFile(QStringLiteral("C:/tmp/demo.excalidraw"))});
        QMimeData imageMime;
        imageMime.setUrls({QUrl::fromLocalFile(QStringLiteral("C:/tmp/demo.png"))});
        QMimeData otherMime;
        otherMime.setUrls({QUrl::fromLocalFile(QStringLiteral("C:/tmp/demo.txt"))});
        ctx.Check(io.AcceptsDropUrls(&canvasMime) && io.AcceptsDropUrls(&excalidrawMime) &&
                      io.AcceptsDropUrls(&imageMime) && !io.AcceptsDropUrls(&otherMime),
                  QStringLiteral("拖放接受 .excalidraw / .sbcanvas / 图片，拒绝其他文件"));
    }

    // 画板自身纯色 / 斜线填充导出到 Excalidraw：元素要带对 fillStyle 与背景色。
    {
        MainWindow sourceWindow;
        sourceWindow.View()->SetMode(ToolMode::EDIT);
        CanvasScene* sourceScene = sourceWindow.findChild<CanvasScene*>();
        auto* solidRect = ShapeFactory::Create(ShapeType::RECTANGLE);
        solidRect->SetFill(QColor(QStringLiteral("#a5d8ff")), FillStyle::SOLID);
        solidRect->SetContentSize(QSizeF(100.0, 60.0));
        solidRect->setPos(QPointF(0.0, 0.0));
        auto* hachureRect = ShapeFactory::Create(ShapeType::RECTANGLE);
        hachureRect->SetFill(QColor(QStringLiteral("#fab005")), FillStyle::HACHURE);
        hachureRect->SetContentSize(QSizeF(100.0, 60.0));
        hachureRect->setPos(QPointF(140.0, 0.0));
        if (sourceScene) {
            sourceScene->AddItemsWithUndo({solidRect, hachureRect}, QStringLiteral("Add fill probe"));
        }
        QStringList exported;
        if (sourceScene) {
            const QJsonObject clipboard = Excalidraw::StoryBoardToClipboard(sourceScene->ToJson());
            for (const QJsonValue& value : clipboard.value(QStringLiteral("elements")).toArray()) {
                const QJsonObject element = value.toObject();
                exported.append(QStringLiteral("%1/%2/%3")
                                    .arg(element.value(QStringLiteral("type")).toString(),
                                         element.value(QStringLiteral("fillStyle")).toString(),
                                         element.value(QStringLiteral("backgroundColor")).toString()));
            }
        }
        ctx.Check(exported.size() == 2 && exported.at(0).contains(QStringLiteral("solid")) &&
                      exported.at(1).contains(QStringLiteral("hachure")),
                  QStringLiteral("画板填充导出写进 Excalidraw 元素"), exported.join(QLatin1String(" | ")));
    }

    // .excalidraw 场景导入（不是剪贴板）也要保留纯色 / 斜线填充。
    {
        const QByteArray sceneText = R"({"type":"excalidraw","version":2,"elements":[
            {"id":"file-solid","type":"rectangle","x":0,"y":0,"width":100,"height":60,
             "backgroundColor":"#a5d8ff","fillStyle":"solid","strokeColor":"#1e1e1e","strokeWidth":2},
            {"id":"file-hachure","type":"rectangle","x":140,"y":0,"width":100,"height":60,
             "backgroundColor":"#fab005","fillStyle":"hachure","strokeColor":"#1e1e1e","strokeWidth":2}]})";
        const QJsonObject scene = QJsonDocument::fromJson(sceneText).object();
        QStringList warnings;
        QJsonObject document = Excalidraw::SceneToStoryBoard(scene, &warnings, true);
        CanvasScene importedScene;
        QString error;
        importedScene.FromJson(document, &error);
        QStringList importedStyles;
        for (CanvasItem* item : importedScene.CanvasItems()) {
            if (auto* shape = dynamic_cast<ShapeAbstractItem*>(item)) {
                importedStyles.append(FillStyleToString(shape->fillStyle()));
            }
        }
        ctx.Check(importedStyles.count(QStringLiteral("solid")) == 1 &&
                      importedStyles.count(QStringLiteral("hachure")) == 1,
                  QStringLiteral("导入 .excalidraw 场景保留纯色 / 斜线填充"),
                  importedStyles.join(QLatin1Char(',')) + QLatin1Char('|') + warnings.join(QLatin1String(" / ")));
    }

    // 倾斜直线：Excalidraw 的 points 是世界坐标，画板 segments 是图元本地坐标；
    // 两者混用时选中框还在画布上，笔迹却被平移一次画到框外（看起来“有名字没线”）。
    {
        const QByteArray diagonal = R"({"type":"excalidraw/clipboard","elements":[{"id":"l1","type":"line",
            "x":420,"y":360,"width":180,"height":90,"points":[[0,0],[180,90]],
            "strokeColor":"#1e293b","strokeWidth":2}]})";
        const QJsonObject clipboard = QJsonDocument::fromJson(diagonal).object();
        const QJsonArray items =
            Excalidraw::ElementsToItems(clipboard.value(QStringLiteral("elements")).toArray(), nullptr);
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        scene.SetMode(ToolMode::EDIT);
        QUndoStack undo;
        scene.SetUndoStack(&undo);
        scene.PasteClipboardItems(items);

        CanvasItem* pasted = nullptr;
        for (CanvasItem* item : scene.CanvasItems()) {
            pasted = item;
        }
        auto* line = dynamic_cast<LineAbstractItem*>(pasted);
        const QVector<QPointF> knots = line ? line->StrokePoints() : QVector<QPointF>();
        qreal maxAbs = 0.0;
        for (const QPointF& knot : knots) {
            maxAbs = qMax(maxAbs, qMax(qAbs(knot.x()), qAbs(knot.y())));
        }
        const qreal span = pasted ? qMax(pasted->ContentSize().width(), pasted->ContentSize().height()) + 2.0 : 0.0;
        ctx.Check(line && knots.size() == 2 && maxAbs <= span,
                  QStringLiteral("粘贴倾斜直线后笔迹与图元框重合（不会画到框外）"),
                  QStringLiteral("knots=%1 maxAbs=%2 span=%3").arg(knots.size()).arg(maxAbs).arg(span));
    }

    // 导出 Excalidraw 入口隐藏：文件菜单没有该动作，复制也不再往剪贴板写 Excalidraw JSON。
    {
        MainWindow exportWindow;
        bool hasExcalidrawAction = false;
        for (QAction* action : exportWindow.findChildren<QAction*>()) {
            if (action->text().contains(QStringLiteral("Excalidraw"), Qt::CaseInsensitive)) {
                hasExcalidrawAction = true;
            }
        }
        ctx.Check(!hasExcalidrawAction, QStringLiteral("文件菜单不再提供导出 Excalidraw"));

        exportWindow.View()->SetMode(ToolMode::EDIT);
        CanvasScene* scene = exportWindow.findChild<CanvasScene*>();
        auto* copiedShape = ShapeFactory::Create(ShapeType::RECTANGLE);
        copiedShape->SetContentSize(QSizeF(100.0, 70.0));
        copiedShape->setPos(QPointF(60.0, 60.0));
        if (scene) {
            scene->AddItemsWithUndo({copiedShape}, QStringLiteral("Add copy source"));
            copiedShape->setSelected(true);
        }
        QApplication::processEvents();
        QAction* copyAction = FindActionByObjectName(&exportWindow, QStringLiteral("CopyItemsAction"));
        if (copyAction) {
            copyAction->trigger();
            QApplication::processEvents();
        }
        const QMimeData* copiedMime = QApplication::clipboard()->mimeData();
        ctx.Check(copiedMime && !copiedMime->hasFormat(QStringLiteral("application/vnd.excalidraw+json")) &&
                      !copiedMime->hasFormat(QStringLiteral("text/plain")),
                  QStringLiteral("复制不再写 Excalidraw 剪贴板"),
                  QStringLiteral("mime=%1").arg(copiedMime ? copiedMime->formats().join(QStringLiteral(";")) : QString()));

        QAction* pasteAction = FindActionByObjectName(&exportWindow, QStringLiteral("PasteItemsAction"));
        const int beforePaste = scene ? scene->CanvasItemCount() : -1;
        if (pasteAction) {
            pasteAction->trigger();
            QApplication::processEvents();
        }
        ctx.Check(scene && scene->CanvasItemCount() == beforePaste + 1,
                  QStringLiteral("复制后仍能粘回画板"),
                  QStringLiteral("before=%1 after=%2").arg(beforePaste).arg(scene ? scene->CanvasItemCount() : -1));
        QApplication::clipboard()->clear();
    }

    // 默认键位映射：按通用软件标准或直觉核对每个动作唯一的那个键。
    {
        MainWindow shortcutWindow;

        QAction* copyAction = FindActionByObjectName(&shortcutWindow, QStringLiteral("CopyItemsAction"));
        QAction* cutAction = FindActionByObjectName(&shortcutWindow, QStringLiteral("CutItemsAction"));
        QAction* pasteAction = FindActionByObjectName(&shortcutWindow, QStringLiteral("PasteItemsAction"));
        ctx.Check(copyAction && copyAction->shortcut() == QKeySequence(QStringLiteral("Ctrl+C")),
                  QStringLiteral("复制绑定 Ctrl+C"));
        ctx.Check(cutAction && cutAction->shortcut() == QKeySequence(QStringLiteral("Ctrl+X")),
                  QStringLiteral("剪切绑定 Ctrl+X"));
        ctx.Check(pasteAction && pasteAction->shortcut() == QKeySequence(QStringLiteral("Ctrl+V")),
                  QStringLiteral("粘贴绑定 Ctrl+V"));

        ctx.Check(FindActionByText(&shortcutWindow, QStringLiteral("查看模式")) &&
                      FindActionByText(&shortcutWindow, QStringLiteral("查看模式"))->shortcut() ==
                          QKeySequence(QStringLiteral("H")),
                  QStringLiteral("概览/手型模式绑定 H"));
        ctx.Check(FindActionByText(&shortcutWindow, QStringLiteral("编辑模式")) &&
                      FindActionByText(&shortcutWindow, QStringLiteral("编辑模式"))->shortcut() ==
                          QKeySequence(QStringLiteral("V")),
                  QStringLiteral("选择/编辑模式绑定 V（PS / Figma 指针工具）"));
        ctx.Check(FindActionByText(&shortcutWindow, QStringLiteral("画笔模式")) &&
                      FindActionByText(&shortcutWindow, QStringLiteral("画笔模式"))->shortcut() ==
                          QKeySequence(QStringLiteral("B")),
                  QStringLiteral("画笔模式绑定 B"));
        ctx.Check(FindActionByText(&shortcutWindow, QStringLiteral("文字模式")) &&
                      FindActionByText(&shortcutWindow, QStringLiteral("文字模式"))->shortcut() ==
                          QKeySequence(QStringLiteral("T")),
                  QStringLiteral("文字模式绑定 T"));

        QAction* insertAction = FindActionByText(&shortcutWindow, QStringLiteral("插入图片…"));
        ctx.Check(insertAction && insertAction->shortcut() == QKeySequence(QStringLiteral("Ctrl+I")),
                  QStringLiteral("插入图片绑定 Ctrl+I"));

        QAction* redoAction = FindActionByText(&shortcutWindow, QStringLiteral("重做"));
        ctx.Check(redoAction && redoAction->shortcut() == QKeySequence(QStringLiteral("Ctrl+Y")),
                  QStringLiteral("重做绑定 Ctrl+Y"));

        QAction* zoomInAction = FindActionByText(&shortcutWindow, QStringLiteral("放大"));
        ctx.Check(zoomInAction && zoomInAction->shortcut() == QKeySequence(QStringLiteral("Ctrl+=")),
                  QStringLiteral("放大绑定 Ctrl+="));

        QAction* deleteAction = FindActionByText(&shortcutWindow, QStringLiteral("删除选中图元"));
        ctx.Check(deleteAction && deleteAction->shortcut() == QKeySequence(QStringLiteral("Delete")),
                  QStringLiteral("删除绑定 Delete"));

        QAction* fitAction = FindActionByText(&shortcutWindow, QStringLiteral("回到全景视图"));
        ctx.Check(fitAction && fitAction->shortcut() == QKeySequence(QStringLiteral("Ctrl+0")),
                  QStringLiteral("回到全景绑定 Ctrl+0"));

        QAction* quitAction = FindActionByText(&shortcutWindow, QStringLiteral("退出"));
        ctx.Check(quitAction && quitAction->shortcut() == QKeySequence(QStringLiteral("Alt+F4")),
                  QStringLiteral("退出绑定 Alt+F4"));

        QAction* gridAction = FindActionByText(&shortcutWindow, QStringLiteral("辅助网格"));
        ctx.Check(gridAction && gridAction->shortcut() == QKeySequence(QStringLiteral("Ctrl+'")),
                  QStringLiteral("网格开关绑定 Ctrl+'"));
    }
}
