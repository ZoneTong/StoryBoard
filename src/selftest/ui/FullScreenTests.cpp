#include "selftest/core/SelftestEntries.h"

#include "canvas/CanvasView.h"
#include "selftest/core/SelfTest.h"
#include "selftest/core/SelfTestContext.h"
#include "ui/mainwindow/MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>

void selftest::RunFullScreenTests(Context& ctx)
{
    MainWindow window;
    window.show();
    QApplication::processEvents();

    QAction* fullScreenAct = window.findChild<QAction*>(QStringLiteral("FullScreenAction"));
    ctx.Check(fullScreenAct && fullScreenAct->shortcut() == QKeySequence(Qt::Key_F11),
              QStringLiteral("F11 绑定全屏开关"));
    ctx.Check(QCoreApplication::translate("MainWindow", "Full screen") == QStringLiteral("全屏"),
              QStringLiteral("全屏菜单项有中文译文"));

    QToolBar* mainBar = window.findChild<QToolBar*>(QStringLiteral("MainToolBar"));
    if (!fullScreenAct) {
        return;
    }

    fullScreenAct->trigger();
    QApplication::processEvents();
    ctx.Check(window.menuBar()->isHidden(), QStringLiteral("全屏隐藏菜单栏"));
    ctx.Check(window.statusBar()->isHidden(), QStringLiteral("全屏隐藏状态栏"));
    ctx.Check(mainBar && mainBar->isHidden(), QStringLiteral("全屏隐藏主工具栏"));
    ctx.Check(window.View()->actions().contains(fullScreenAct), QStringLiteral("全屏期 F11 挂在画布视图上"));
    ctx.Check(window.isFullScreen(), QStringLiteral("窗口进入全屏状态"));

    QWidget* bar = window.findChild<QWidget*>(QStringLiteral("FullScreenBar"));
    ctx.Check(bar && bar->isVisible(), QStringLiteral("全屏显示悬浮工具栏"));
    if (bar) {
        ctx.Check(bar->y() >= 32, QStringLiteral("悬浮工具栏离顶边至少一个标题栏"), QStringLiteral("y=%1").arg(bar->y()));
        ctx.Check(qAbs(bar->x() + bar->width() / 2 - window.width() / 2) <= 2,
                  QStringLiteral("悬浮工具栏水平居中"));
        int keyedButtons = 0;
        for (QToolButton* button : bar->findChildren<QToolButton*>()) {
            if (button->objectName().startsWith(QStringLiteral("FullScreenShapeButton"))) {
                ++keyedButtons;
            }
        }
        ctx.Check(keyedButtons == 9, QStringLiteral("悬浮工具栏只放需求列出的 9 个图形"),
                  QStringLiteral("count=%1").arg(keyedButtons));
    }

    fullScreenAct->trigger();
    QApplication::processEvents();
    ctx.Check(!window.isFullScreen(), QStringLiteral("再次触发退出全屏"));
    ctx.Check(!window.menuBar()->isHidden(), QStringLiteral("退出后恢复菜单栏"));
    ctx.Check(mainBar && !mainBar->isHidden(), QStringLiteral("退出后恢复主工具栏"));
    ctx.Check(!window.View()->actions().contains(fullScreenAct), QStringLiteral("退出后摘掉临时键位"));

    window.close();
    QApplication::processEvents();
}
