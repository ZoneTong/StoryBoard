#include "CanvasItem.h"
#include "CanvasScene.h"
#include "MainWindow.h"
#include "ShapeItem.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"

#include <QAction>
#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeySequence>
#include <QList>
#include <QPointF>
#include <QSizeF>
#include <QUndoStack>
#include <QWidget>

namespace {

// 功能键只看绑定键是否正常工作；副键组合只要不与任何功能冲突即可，不整组断言。
QAction* findActionByObjectName(QWidget* window, const QString& objectName)
{
    return window->findChild<QAction*>(objectName);
}

QAction* findActionByText(QWidget* window, const QString& text)
{
    for (QAction* action : window->findChildren<QAction*>()) {
        if (action->text() == text) {
            return action;
        }
    }
    return nullptr;
}

} // namespace

void selftest::runClipboardShortcutTests(Context& ctx)
{
    // 剪贴板序列化：选中图元写进 JSON，跨场景粘贴时重建、偏移、清分组，名称唯一。
    {
        CanvasScene sourceScene;
        sourceScene.applyCanvasSize(QSizeF(800.0, 600.0));
        sourceScene.setMode(ToolMode::EDIT);
        QUndoStack sourceUndo;
        sourceScene.setUndoStack(&sourceUndo);

        auto* source = createShapeItem(ShapeType::RECTANGLE);
        source->setContentSize(QSizeF(120.0, 80.0));
        source->setPos(QPointF(40.0, 50.0));
        sourceScene.addItemsWithUndo({source}, QStringLiteral("Add clipboard source"));
        source->setSelected(true);

        const QJsonArray clipboard = sourceScene.clipboardItems();
        ctx.check(clipboard.size() == 1, QStringLiteral("复制序列化选中的可编辑图元"),
                  QString::number(clipboard.size()));

        CanvasScene pasteScene;
        pasteScene.applyCanvasSize(QSizeF(800.0, 600.0));
        pasteScene.setMode(ToolMode::EDIT);
        QUndoStack pasteUndo;
        pasteScene.setUndoStack(&pasteUndo);

        const int pasted = pasteScene.pasteClipboardItems(clipboard);
        ctx.check(pasted == 1 && pasteScene.canvasItemCount() == 1, QStringLiteral("粘贴把剪贴板图元重建到画布"),
                  QString::number(pasted));

        CanvasItem* copy = nullptr;
        for (CanvasItem* item : pasteScene.canvasItems()) {
            copy = item;
        }
        // 需求 2：粘贴落点重新计算到视图/画布中心区域，不再固定叠在源图元旁边。
        const QPointF pastedCenter = copy ? copy->sceneBoundingRect().center() : QPointF();
        const bool insideCanvas = pasteScene.canvasRect().contains(pastedCenter);
        const bool movedAwayFromSource =
            copy && (pastedCenter - source->sceneBoundingRect().center()).manhattanLength() > 1.0;
        ctx.check(copy && insideCanvas && movedAwayFromSource, QStringLiteral("粘贴副本落在画布内并离开源图元位置"),
                  QStringLiteral("%1,%2").arg(pastedCenter.x()).arg(pastedCenter.y()));
        ctx.check(copy && copy->groupId() == 0, QStringLiteral("粘贴副本不带原分组关系"));
        ctx.check(pasteUndo.count() == 1, QStringLiteral("粘贴只产生一条撤销记录"));

        // 方向键微调：编辑模式按偏移移动选中图元，撤销可还原。
        if (copy) {
            copy->setSelected(true);
            const QPointF before = copy->pos();
            const int nudged = pasteScene.nudgeSelected(QPointF(5.0, 0.0));
            ctx.check(nudged == 1 && copy->pos() == before + QPointF(5.0, 0.0),
                      QStringLiteral("方向键微调移动选中图元"),
                      QStringLiteral("%1,%2").arg(copy->pos().x()).arg(copy->pos().y()));
            ctx.check(pasteUndo.count() == 2, QStringLiteral("微调只产生一条几何撤销记录"));
            pasteUndo.undo();
            QApplication::processEvents();
            ctx.check(copy->pos() == before, QStringLiteral("撤销微调恢复图元位置"));
        }
    }

    // 默认键位映射：按通用软件标准或直觉核对每个动作唯一的那个键。
    {
        MainWindow shortcutWindow;

        QAction* copyAction = findActionByObjectName(&shortcutWindow, QStringLiteral("CopyItemsAction"));
        QAction* cutAction = findActionByObjectName(&shortcutWindow, QStringLiteral("CutItemsAction"));
        QAction* pasteAction = findActionByObjectName(&shortcutWindow, QStringLiteral("PasteItemsAction"));
        ctx.check(copyAction && copyAction->shortcut() == QKeySequence(QStringLiteral("Ctrl+C")),
                  QStringLiteral("复制绑定 Ctrl+C"));
        ctx.check(cutAction && cutAction->shortcut() == QKeySequence(QStringLiteral("Ctrl+X")),
                  QStringLiteral("剪切绑定 Ctrl+X"));
        ctx.check(pasteAction && pasteAction->shortcut() == QKeySequence(QStringLiteral("Ctrl+V")),
                  QStringLiteral("粘贴绑定 Ctrl+V"));

        ctx.check(findActionByText(&shortcutWindow, QStringLiteral("查看模式")) &&
                      findActionByText(&shortcutWindow, QStringLiteral("查看模式"))->shortcut() ==
                          QKeySequence(QStringLiteral("H")),
                  QStringLiteral("概览/手型模式绑定 H"));
        ctx.check(findActionByText(&shortcutWindow, QStringLiteral("编辑模式")) &&
                      findActionByText(&shortcutWindow, QStringLiteral("编辑模式"))->shortcut() ==
                          QKeySequence(QStringLiteral("V")),
                  QStringLiteral("选择/编辑模式绑定 V（PS / Figma 指针工具）"));
        ctx.check(findActionByText(&shortcutWindow, QStringLiteral("画笔模式")) &&
                      findActionByText(&shortcutWindow, QStringLiteral("画笔模式"))->shortcut() ==
                          QKeySequence(QStringLiteral("B")),
                  QStringLiteral("画笔模式绑定 B"));
        ctx.check(findActionByText(&shortcutWindow, QStringLiteral("文字模式")) &&
                      findActionByText(&shortcutWindow, QStringLiteral("文字模式"))->shortcut() ==
                          QKeySequence(QStringLiteral("T")),
                  QStringLiteral("文字模式绑定 T"));

        QAction* insertAction = findActionByText(&shortcutWindow, QStringLiteral("插入图片…"));
        ctx.check(insertAction && insertAction->shortcut() == QKeySequence(QStringLiteral("Ctrl+I")),
                  QStringLiteral("插入图片绑定 Ctrl+I"));

        QAction* redoAction = findActionByText(&shortcutWindow, QStringLiteral("重做"));
        ctx.check(redoAction && redoAction->shortcut() == QKeySequence(QStringLiteral("Ctrl+Y")),
                  QStringLiteral("重做绑定 Ctrl+Y"));

        QAction* zoomInAction = findActionByText(&shortcutWindow, QStringLiteral("放大"));
        ctx.check(zoomInAction && zoomInAction->shortcut() == QKeySequence(QStringLiteral("Ctrl+=")),
                  QStringLiteral("放大绑定 Ctrl+="));

        QAction* deleteAction = findActionByText(&shortcutWindow, QStringLiteral("删除选中图元"));
        ctx.check(deleteAction && deleteAction->shortcut() == QKeySequence(QStringLiteral("Delete")),
                  QStringLiteral("删除绑定 Delete"));

        QAction* fitAction = findActionByText(&shortcutWindow, QStringLiteral("回到全景视图"));
        ctx.check(fitAction && fitAction->shortcut() == QKeySequence(QStringLiteral("Ctrl+0")),
                  QStringLiteral("回到全景绑定 Ctrl+0"));

        QAction* quitAction = findActionByText(&shortcutWindow, QStringLiteral("退出"));
        ctx.check(quitAction && quitAction->shortcut() == QKeySequence(QStringLiteral("Alt+F4")),
                  QStringLiteral("退出绑定 Alt+F4"));

        QAction* gridAction = findActionByText(&shortcutWindow, QStringLiteral("辅助网格"));
        ctx.check(gridAction && gridAction->shortcut() == QKeySequence(QStringLiteral("Ctrl+'")),
                  QStringLiteral("网格开关绑定 Ctrl+'"));

        QAction* propsAction = findActionByText(&shortcutWindow, QStringLiteral("图元属性…"));
        ctx.check(propsAction && propsAction->shortcut().isEmpty(), QStringLiteral("图元属性动作不绑定单字母快捷键"));
    }
}
