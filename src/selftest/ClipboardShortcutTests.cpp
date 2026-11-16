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

// 无系统级副键的动作只留一个主键；有副键的动作按「主键在前、副键在后」整组校验。
bool hasOnlyShortcut(QAction* action, const QString& shortcut)
{
    if (!action) {
        return false;
    }
    const QList<QKeySequence> sequences = action->shortcuts();
    return sequences.size() == 1 && sequences.constFirst() == QKeySequence(shortcut);
}

bool hasExactShortcuts(QAction* action, const QStringList& shortcuts)
{
    if (!action || action->shortcuts().size() != shortcuts.size()) {
        return false;
    }
    for (int i = 0; i < shortcuts.size(); ++i) {
        if (action->shortcuts().at(i) != QKeySequence(shortcuts.at(i))) {
            return false;
        }
    }
    return true;
}

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
        sourceScene.setMode(ToolMode::Edit);
        QUndoStack sourceUndo;
        sourceScene.setUndoStack(&sourceUndo);

        auto* source = new ShapeItem(ShapeType::Rectangle);
        source->setContentSize(QSizeF(120.0, 80.0));
        source->setPos(QPointF(40.0, 50.0));
        sourceScene.addItemsWithUndo({source}, QStringLiteral("Add clipboard source"));
        source->setSelected(true);

        const QJsonArray clipboard = sourceScene.clipboardItems();
        ctx.check(clipboard.size() == 1, QStringLiteral("复制序列化选中的可编辑图元"),
                  QString::number(clipboard.size()));

        CanvasScene pasteScene;
        pasteScene.applyCanvasSize(QSizeF(800.0, 600.0));
        pasteScene.setMode(ToolMode::Edit);
        QUndoStack pasteUndo;
        pasteScene.setUndoStack(&pasteUndo);

        const int pasted = pasteScene.pasteClipboardItems(clipboard);
        ctx.check(pasted == 1 && pasteScene.canvasItemCount() == 1, QStringLiteral("粘贴把剪贴板图元重建到画布"),
                  QString::number(pasted));

        CanvasItem* copy = nullptr;
        for (CanvasItem* item : pasteScene.canvasItems()) {
            copy = item;
        }
        ctx.check(
            copy && copy->pos() == source->pos() + QPointF(20.0, 20.0), QStringLiteral("粘贴副本相对源图元偏移 20,20"),
            copy ? QStringLiteral("%1,%2").arg(copy->pos().x()).arg(copy->pos().y()) : QStringLiteral("<missing>"));
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
        ctx.check(hasOnlyShortcut(insertAction, QStringLiteral("Ctrl+I")),
                  QStringLiteral("插入图片只用 Ctrl+I 一个键"));

        QAction* redoAction = findActionByText(&shortcutWindow, QStringLiteral("重做"));
        ctx.check(hasExactShortcuts(redoAction, {QStringLiteral("Ctrl+Y"), QStringLiteral("Ctrl+Shift+Z")}),
                  QStringLiteral("重做主键 Ctrl+Y，副键 Ctrl+Shift+Z"));

        QAction* zoomInAction = findActionByText(&shortcutWindow, QStringLiteral("放大"));
        ctx.check(hasExactShortcuts(zoomInAction, {QStringLiteral("Ctrl+="), QStringLiteral("Ctrl++")}),
                  QStringLiteral("放大主键 Ctrl+=，副键 Ctrl++"));

        QAction* deleteAction = findActionByText(&shortcutWindow, QStringLiteral("删除选中图元"));
        ctx.check(hasOnlyShortcut(deleteAction, QStringLiteral("Delete")), QStringLiteral("删除只用 Delete 一个键"));

        QAction* fitAction = findActionByText(&shortcutWindow, QStringLiteral("回到全景视图"));
        ctx.check(hasOnlyShortcut(fitAction, QStringLiteral("Ctrl+0")), QStringLiteral("回到全景只用 Ctrl+0 一个键"));

        QAction* quitAction = findActionByText(&shortcutWindow, QStringLiteral("退出"));
        ctx.check(hasExactShortcuts(quitAction, {QStringLiteral("Alt+F4"), QStringLiteral("Ctrl+Q")}),
                  QStringLiteral("退出主键 Alt+F4，副键 Ctrl+Q"));

        QAction* gridAction = findActionByText(&shortcutWindow, QStringLiteral("辅助网格"));
        ctx.check(gridAction && gridAction->shortcut() == QKeySequence(QStringLiteral("Ctrl+'")),
                  QStringLiteral("网格开关绑定 Ctrl+'"));

        QAction* propsAction = findActionByText(&shortcutWindow, QStringLiteral("图元属性…"));
        ctx.check(propsAction && propsAction->shortcut().isEmpty(), QStringLiteral("图元属性动作不绑定单字母快捷键"));
    }
}
