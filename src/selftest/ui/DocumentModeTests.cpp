#include "app/I18n.h"
#include "canvas/CanvasView.h"
#include "items/CanvasTypes.h"
#include "items/geometry/LineGlyph.h"
#include "scene/CanvasScene.h"
#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"
#include "selftest/core/SelftestHelpers.h"
#include "ui/mainwindow/MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QFileInfo>
#include <QKeySequence>

void selftest::RunDocumentModeTests(Context& ctx)
{
    MainWindow window;
    window.resize(1000, 720);
    QApplication::processEvents();

    ctx.Check(window.View()->mode() == ToolMode::VIEW, QStringLiteral("主窗口启动默认查看模式"),
              QStringLiteral("mode=%1").arg(static_cast<int>(window.View()->mode())));

    // 新建：即使当前在编辑模式，也要回到查看模式（走真实的 Ctrl+N 动作，而不是私有槽）。
    QAction* newAction = nullptr;
    for (QAction* action : window.findChildren<QAction*>()) {
        if (action && action->shortcut() == QKeySequence(QStringLiteral("Ctrl+N"))) {
            newAction = action;
            break;
        }
    }
    window.View()->SetMode(ToolMode::EDIT);
    if (newAction) {
        newAction->trigger();
    }
    QApplication::processEvents();
    ctx.Check(newAction != nullptr, QStringLiteral("主窗口提供新建画布动作"));
    ctx.Check(window.View()->mode() == ToolMode::VIEW, QStringLiteral("新建画布后回到查看模式"),
              QStringLiteral("mode=%1").arg(static_cast<int>(window.View()->mode())));

    // 打开：同样从编辑模式回到查看模式。
    const QString demoPath = DemoCanvasPath();
    ctx.Check(QFileInfo::exists(demoPath), QStringLiteral("找到示例画布用于打开测试"), demoPath);
    window.View()->SetMode(ToolMode::EDIT);
    const bool opened = window.OpenCanvasFile(demoPath);
    QApplication::processEvents();
    ctx.Check(opened, QStringLiteral("主窗口打开示例画布"));
    ctx.Check(window.View()->mode() == ToolMode::VIEW, QStringLiteral("打开画布后进入查看模式"),
              QStringLiteral("mode=%1").arg(static_cast<int>(window.View()->mode())));

    // 插入线条族：查看模式下插入后自动进编辑模式，并选中新图元。
    window.View()->SetMode(ToolMode::VIEW);
    const int countBefore = window.Scene()->CanvasItemCount();
    window.InsertCurve(LineGlyph::STRAIGHT);
    QApplication::processEvents();
    ctx.Check(window.Scene()->CanvasItemCount() == countBefore + 1 && window.View()->mode() == ToolMode::EDIT,
              QStringLiteral("插入曲线后自动进入编辑模式"),
              QStringLiteral("items=%1 mode=%2")
                  .arg(window.Scene()->CanvasItemCount())
                  .arg(static_cast<int>(window.View()->mode())));
}
