#include "selftest/core/SelftestEntries.h"

#include "canvas/CanvasView.h"
#include "items/CanvasTypes.h"
#include "items/shapes/ShapeFactory.h"
#include "scene/CanvasScene.h"
#include "selftest/core/SelfTest.h"
#include "selftest/core/SelfTestContext.h"
#include "ui/mainwindow/MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QHash>
#include <QKeyEvent>
#include <QMenuBar>
#include <QSpinBox>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>

namespace {

// 数字键测试用：把 9 个图形动作的触发次数记下来，便于判断键位到底落到了哪个动作。
QHash<QString, int> g_digitHits;

void CountTriggered(const QString& actionName)
{
    g_digitHits[actionName] = g_digitHits.value(actionName, 0) + 1;
}

int HitCount(const QString& actionName)
{
    return g_digitHits.value(actionName, 0);
}

void SendKey(QWidget* target, Qt::Key key)
{
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, QString());
    QApplication::sendEvent(target, &press);
    QApplication::processEvents();
}

} // namespace

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
        int styleButtons = 0;
        for (const QString& name : {QStringLiteral("FullScreenFillStyleButton"),
                                    QStringLiteral("FullScreenFillColorButton"),
                                    QStringLiteral("FullScreenStrokeStyleButton"),
                                    QStringLiteral("FullScreenStrokeColorButton")}) {
            if (QToolButton* button = bar->findChild<QToolButton*>(name)) {
                if (button->isVisible()) {
                    ++styleButtons;
                }
            }
        }
        ctx.Check(styleButtons == 4, QStringLiteral("全屏工具栏保留填充 / 描线样式与颜色按钮"),
                  QStringLiteral("visible=%1").arg(styleButtons));
        QToolButton* customShapeButton =
            bar->findChild<QToolButton*>(QStringLiteral("FullScreenCustomShapeButton"));
        ctx.Check(customShapeButton && customShapeButton->menu() == nullptr && !customShapeButton->icon().isNull(),
                  QStringLiteral("自定义图形按钮有图标且直接开面板，不再挂子菜单"),
                  QStringLiteral("button=%1 menu=%2 icon=%3")
                      .arg(customShapeButton ? 1 : 0)
                      .arg(customShapeButton && customShapeButton->menu() ? 1 : 0)
                      .arg(customShapeButton && !customShapeButton->icon().isNull() ? 1 : 0));
    }

    // 需求 15-3：编辑面板改左侧悬浮，图层面板右下贴底并压矮。
    QDockWidget* propertiesDock = window.findChild<QDockWidget*>(QStringLiteral("PropertiesPanelDock"));
    QDockWidget* layerDock = window.findChild<QDockWidget*>(QStringLiteral("LayerDock"));
    ctx.Check(propertiesDock && propertiesDock->isFloating(), QStringLiteral("全屏期编辑面板改为悬浮"));
    ctx.Check(layerDock && layerDock->isFloating(), QStringLiteral("全屏期图层卡改为悬浮"));
    if (propertiesDock && layerDock) {
        const QRect windowFrame = window.frameGeometry();
        ctx.Check(qAbs(propertiesDock->frameGeometry().left() - windowFrame.left()) <= 40,
                  QStringLiteral("悬浮编辑面板贴左侧"),
                  QStringLiteral("dx=%1").arg(propertiesDock->frameGeometry().left() - windowFrame.left()));
        ctx.Check(propertiesDock->frameGeometry().top() - windowFrame.top() >= 58,
                  QStringLiteral("悬浮编辑面板让开顶部工具栏"),
                  QStringLiteral("dy=%1").arg(propertiesDock->frameGeometry().top() - windowFrame.top()));
        ctx.Check(layerDock->frameGeometry().height() >= 500 && layerDock->frameGeometry().height() <= 640,
                  QStringLiteral("全屏期图层卡多留 10 行冗余"),
                  QStringLiteral("h=%1").arg(layerDock->frameGeometry().height()));
        ctx.Check(qAbs(layerDock->frameGeometry().bottom() - windowFrame.bottom()) <= 40 &&
                      qAbs(layerDock->frameGeometry().right() - windowFrame.right()) <= 40,
                  QStringLiteral("全屏期图层卡贴右下角"),
                  QStringLiteral("right=%1 bottom=%2").arg(layerDock->frameGeometry().right() - windowFrame.right())
                      .arg(layerDock->frameGeometry().bottom() - windowFrame.bottom()));

        // 编辑面板跟随选中显隐：有选中显示、清空选中隐藏。
        window.View()->SetMode(ToolMode::EDIT);
        QApplication::processEvents();
        auto* shape = ShapeFactory::Create(ShapeType::RECTANGLE);
        shape->setPos(QPointF(120.0, 160.0));
        window.Scene()->AddItemsWithUndo({shape}, QStringLiteral("Add full screen shape"));
        shape->setSelected(true);
        QApplication::processEvents();
        ctx.Check(propertiesDock->isVisible(), QStringLiteral("全屏期有选中显示编辑面板"));
        window.Scene()->clearSelection();
        QApplication::processEvents();
        ctx.Check(!propertiesDock->isVisible(), QStringLiteral("全屏期清空选中隐藏编辑面板"));
    }

    // 需求 15-4：数字键 0–9 只在全屏期落到悬浮工具栏的按钮上。
    const QStringList digitActionNames = {QStringLiteral("ShapeAction_ellipse"), QStringLiteral("ShapeAction_line"),
                                          QStringLiteral("ShapePreset_lineArrow"),
                                          QStringLiteral("ShapeAction_triangle"),
                                          QStringLiteral("ShapePreset_roundedRect"), QStringLiteral("ShapeAction_star"),
                                          QStringLiteral("ShapeAction_diamond"), QStringLiteral("ShapeAction_heart"),
                                          QStringLiteral("ShapePreset_braceLeft")};
    for (const QString& name : digitActionNames) {
        if (QAction* action = window.findChild<QAction*>(name)) {
            QObject::connect(action, &QAction::triggered, &window, [name]() { CountTriggered(name); });
        }
    }
    window.View()->setFocus();
    SendKey(&window, Qt::Key_1);
    ctx.Check(HitCount(QStringLiteral("ShapeAction_line")) == 1, QStringLiteral("全屏期数字键 1 插入直线"),
              QStringLiteral("hits=%1").arg(HitCount(QStringLiteral("ShapeAction_line"))));
    ctx.Check(HitCount(QStringLiteral("ShapeAction_ellipse")) == 0, QStringLiteral("数字键只触发对应按钮"));
    // 输入框聚焦时数字键让位：描线宽度输入框拿到焦点后不该再切图形。
    if (QSpinBox* widthSpin = window.findChild<QSpinBox*>(QStringLiteral("FullScreenStrokeWidthSpin"))) {
        widthSpin->setFocus();
        QApplication::processEvents();
        SendKey(&window, Qt::Key_2);
        ctx.Check(HitCount(QStringLiteral("ShapePreset_lineArrow")) == 0,
                  QStringLiteral("输入框聚焦时数字键让位"));
        window.View()->setFocus();
    }

    fullScreenAct->trigger();
    QApplication::processEvents();
    ctx.Check(!window.isFullScreen(), QStringLiteral("再次触发退出全屏"));
    ctx.Check(!window.menuBar()->isHidden(), QStringLiteral("退出后恢复菜单栏"));
    ctx.Check(mainBar && !mainBar->isHidden(), QStringLiteral("退出后恢复主工具栏"));
    ctx.Check(!window.View()->actions().contains(fullScreenAct), QStringLiteral("退出后摘掉临时键位"));

    // 需求 15-3：退出后两张卡要回到停靠形态与原来的停靠侧。
    ctx.Check(propertiesDock && !propertiesDock->isFloating(), QStringLiteral("退出后编辑面板恢复停靠"));
    ctx.Check(propertiesDock && window.dockWidgetArea(propertiesDock) == Qt::LeftDockWidgetArea,
              QStringLiteral("退出后编辑面板回到左侧"));
    ctx.Check(layerDock && !layerDock->isFloating(), QStringLiteral("退出后图层卡恢复停靠"));
    ctx.Check(layerDock && layerDock->height() > 300, QStringLiteral("退出后图层卡恢复原高度"),
              QStringLiteral("h=%1").arg(layerDock ? layerDock->height() : -1));

    // 本段往画布插了图形、也起了一次插入交互：收尾复位并标记干净，关窗时才不会弹保存询问。
    window.View()->FinishCurrentStroke();
    window.View()->SetMode(ToolMode::VIEW);
    if (window.Scene()->UndoStack()) {
        window.Scene()->UndoStack()->setClean();
    }
    QApplication::processEvents();
    window.close();
    QApplication::processEvents();
}
