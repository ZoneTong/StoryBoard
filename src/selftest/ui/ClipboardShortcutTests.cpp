#include "items/CanvasItem.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "items/shapes/ShapeRegistry.h"
#include "scene/CanvasScene.h"
#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/mainwindow/MainWindowEditActionsController.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeySequence>
#include <QList>
#include <QMimeData>
#include <QPointF>
#include <QSizeF>
#include <QUndoStack>
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

        auto* source = ShapeRegistry::Create(ShapeType::RECTANGLE);
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

        QAction* propsAction = FindActionByText(&shortcutWindow, QStringLiteral("图元属性…"));
        ctx.Check(propsAction && propsAction->shortcut().isEmpty(), QStringLiteral("图元属性动作不绑定单字母快捷键"));
    }
}
