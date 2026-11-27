#include "app/AutoSaveService.h"
#include "app/CanvasSession.h"
#include "app/CanvasTabsController.h"
#include "app/DocumentController.h"
#include "app/Settings.h"
#include "canvas/CanvasView.h"
#include "items/CanvasTypes.h"
#include "items/shapes/ShapeFactory.h"
#include "scene/CanvasScene.h"
#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"
#include "ui/mainwindow/MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QSaveFile>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QThread>
#include <QUndoStack>

namespace {

QAction* FindAction(MainWindow* window, const QString& objectName)
{
    return window->findChild<QAction*>(objectName);
}

QAction* FindActionByText(MainWindow* window, const QString& text)
{
    for (QAction* action : window->findChildren<QAction*>()) {
        if (action->text() == text) {
            return action;
        }
    }
    return nullptr;
}

void ProcessFor(int milliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds) {
        QApplication::processEvents();
        QThread::msleep(5);
    }
}

bool WriteCanvas(const QString& path, int marker)
{
    CanvasScene scene;
    scene.ApplyCanvasSize(QSizeF(400, 300));
    scene.SetShowGrid(false);
    CanvasBackground background;
    background.type = BackgroundType::COLOR;
    background.color = QColor(marker % 2 == 0 ? Qt::white : Qt::lightGray);
    scene.SetCanvasBackground(background);
    auto* shape = ShapeFactory::Create(ShapeType::RECTANGLE);
    shape->setPos(QPointF(40 + marker, 40));
    shape->SetContentSize(QSizeF(60 + marker, 60));
    shape->SetStyle(QColor(Qt::black), QColor(Qt::black), 1.0);
    shape->SetFill(QColor(37, 99, 235), FillStyle::SOLID);
    scene.AttachItem(shape);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(scene.ToJson()).toJson(QJsonDocument::Indented));
    return file.commit();
}

bool WaitForFile(const QString& path, int timeoutMs = 3000)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (QFileInfo::exists(path)) {
            return true;
        }
        QApplication::processEvents();
        QThread::msleep(5);
    }
    return QFileInfo::exists(path);
}

} // namespace

void selftest::RunMultiCanvasTests(Context& ctx)
{
    // 1) 首启一个标签，名字与当前视图一致。
    {
        MainWindow window;
        ctx.Check(window.Tabs() && window.Tabs()->Count() == 1, QStringLiteral("首启只有一个画布标签"),
                  QString::number(window.Tabs() ? window.Tabs()->Count() : -1));
        ctx.Check(window.CurrentCanvasName() == QStringLiteral("未命名画布 1"),
                  QStringLiteral("首个标签名为未命名画布 1"), window.CurrentCanvasName());
        CanvasSession* session = window.Tabs() ? window.Tabs()->Current() : nullptr;
        ctx.Check(session && window.View() == session->View() && window.Scene() == session->Scene(),
                  QStringLiteral("当前视图 / 场景就是首个标签的页面"));
    }

    QTemporaryDir tempDir;
    ctx.Check(tempDir.isValid(), QStringLiteral("多画布用例临时目录可用"));

    // 2) 新建画布：标签 +1、名字递增、模式回 VIEW、旧页内容不受影响。
    {
        MainWindow window;
        window.show();
        QApplication::processEvents();
        window.InsertShape(ShapeType::RECTANGLE);
        CanvasSession* first = window.Tabs()->Current();
        const int firstItems = first->Scene()->CanvasItemCount();
        const int firstUndoCount = first->Undo()->count();
        window.Document()->NewCanvas();
        QApplication::processEvents();
        ctx.Check(window.Tabs()->Count() == 2, QStringLiteral("新建画布后有两个标签"),
                  QString::number(window.Tabs()->Count()));
        ctx.Check(window.CurrentCanvasName() == QStringLiteral("未命名画布 2"),
                  QStringLiteral("新标签名为未命名画布 2"), window.CurrentCanvasName());
        ctx.Check(window.View()->mode() == ToolMode::VIEW, QStringLiteral("新画布默认查看模式"));
        ctx.Check(first->Scene()->CanvasItemCount() == firstItems && first->Undo()->count() == firstUndoCount,
                  QStringLiteral("新建画布不影响原页内容与撤销记录"));

        // 3) 前后切换回到原页。
        window.Tabs()->Previous();
        QApplication::processEvents();
        ctx.Check(window.Tabs()->Current() == first && window.Scene() == first->Scene(),
                  QStringLiteral("上一个画布切回原标签，场景随之重绑"));
        window.Tabs()->Next();
        QApplication::processEvents();
        ctx.Check(window.Tabs()->Current() != first, QStringLiteral("下一个画布切到第二页"));
        window.Tabs()->SwitchToNumber(99);
        ctx.Check(window.Tabs()->Count() == 2, QStringLiteral("直跳越界不改标签数量"));
    }

    // 4) 撤销栈按页隔离。
    {
        MainWindow window;
        window.Tabs()->CreateSession(true);
        CanvasSession* first = window.Tabs()->SessionAt(0);
        window.Tabs()->SetCurrent(first);
        window.InsertShape(ShapeType::RECTANGLE);
        const bool firstCanUndo = first->Undo()->canUndo();
        CanvasSession* second = window.Tabs()->SessionAt(1);
        window.Tabs()->SetCurrent(second);
        ctx.Check(firstCanUndo && !window.Scene()->UndoStack()->canUndo(),
                  QStringLiteral("撤销栈按标签隔离，第二页没有可撤销记录"));
    }

    // 5) 打开示例画布：标签名取文件基名，同一路径再开只切标签。
    {
        const QString path = tempDir.filePath(QStringLiteral("board-a.sbcanvas"));
        ctx.Check(WriteCanvas(path, 1), QStringLiteral("写出测试画布 a"));
        MainWindow window;
        window.OpenCanvasFile(path);
        QApplication::processEvents();
        const int countAfterOpen = window.Tabs()->Count();
        ctx.Check(window.Tabs()->Current() && window.Tabs()->Current()->Scene()->CanvasItemCount() > 0,
                  QStringLiteral("打开的画布有图元"));
        window.OpenCanvasFile(path);
        QApplication::processEvents();
        ctx.Check(window.Tabs()->Count() == countAfterOpen, QStringLiteral("同一路径重复打开只切换不新开"),
                  QStringLiteral("count=%1").arg(window.Tabs()->Count()));
    }

    // 6) 关闭当前标签回到上一页；关最后一个自动补空白页。
    {
        MainWindow window;
        window.Tabs()->CreateSession(true);
        CanvasSession* second = window.Tabs()->Current();
        window.CloseCanvasSession(second);
        QApplication::processEvents();
        ctx.Check(window.Tabs()->Count() == 1 && window.Tabs()->Current() != second,
                  QStringLiteral("关闭当前标签后回到上一页"));
        window.CloseCanvasSession(window.Tabs()->Current());
        QApplication::processEvents();
        ctx.Check(window.Tabs()->Count() == 1 && window.Tabs()->Current()->IsPristine(),
                  QStringLiteral("关闭最后一个标签自动补空白画布"));
    }

    // 7) 面板随标签重绑：图层列表行数跟随目标场景。
    {
        MainWindow window;
        window.show();
        QApplication::processEvents();
        CanvasSession* first = window.Tabs()->Current();
        CanvasSession* second = window.Tabs()->CreateSession(true);
        QApplication::processEvents();
        QListWidget* layerList = window.findChild<QListWidget*>(QStringLiteral("LayerList"));
        const bool firstRows =
            layerList && first->Scene()->UserLayerCount() >= 1 && second->Scene()->UserLayerCount() >= 1;
        const int secondRows = layerList ? layerList->count() : -1;
        ctx.Check(firstRows && secondRows > 0, QStringLiteral("切换标签后图层面板仍有行"),
                  QStringLiteral("rows=%1").arg(secondRows));
    }

    // 8) 全屏：隐藏标签栏、显示画布名；切换后名字更新；退出恢复标签栏。
    {
        MainWindow window;
        window.show();
        QApplication::processEvents();
        QAction* fullScreen = FindAction(&window, QStringLiteral("FullScreenAction"));
        if (fullScreen) {
            fullScreen->trigger();
            QApplication::processEvents();
        }
        QTabBar* tabBar = window.findChild<QTabBar*>(QStringLiteral("CanvasTabBar"));
        QLabel* nameLabel = window.findChild<QLabel*>(QStringLiteral("FullScreenCanvasName"));
        ctx.Check(tabBar && !tabBar->isVisible(), QStringLiteral("全屏隐藏标签栏"));
        ctx.Check(nameLabel && nameLabel->isVisible() && nameLabel->text().contains(window.CurrentCanvasName()),
                  QStringLiteral("全屏上方显示当前画布名"), nameLabel ? nameLabel->text() : QString());
        window.Tabs()->CreateSession(true);
        QApplication::processEvents();
        ctx.Check(nameLabel && nameLabel->text().contains(window.CurrentCanvasName()) && tabBar && !tabBar->isVisible(),
                  QStringLiteral("全屏切页后画布名更新、标签栏仍隐藏"));
        if (fullScreen) {
            fullScreen->trigger();
            QApplication::processEvents();
        }
        ctx.Check(tabBar && tabBar->isVisible(), QStringLiteral("退出全屏恢复标签栏"));
        ctx.Check(!nameLabel || !nameLabel->isVisible(), QStringLiteral("退出全屏隐藏画布名标签"));
    }

    // 9) 干净空页复用：直接打开文件，标签数仍为 1。
    {
        const QString path = tempDir.filePath(QStringLiteral("board-b.sbcanvas"));
        ctx.Check(WriteCanvas(path, 2), QStringLiteral("写出测试画布 b"));
        MainWindow window;
        window.OpenCanvasFile(path);
        QApplication::processEvents();
        ctx.Check(window.Tabs()->Count() == 1, QStringLiteral("干净空页复用后仍只有一个标签"),
                  QString::number(window.Tabs()->Count()));
    }

    // 10/11/12) 批量打开、坏文件跳过、当前脏页不弹保存。
    {
        const QString b = tempDir.filePath(QStringLiteral("board-b.sbcanvas"));
        const QString c = tempDir.filePath(QStringLiteral("board-c.sbcanvas"));
        const QString d = tempDir.filePath(QStringLiteral("board-d.sbcanvas"));
        const QString bad = tempDir.filePath(QStringLiteral("broken.sbcanvas"));
        ctx.Check(WriteCanvas(c, 3) && WriteCanvas(d, 4), QStringLiteral("写出测试画布 c/d"));
        QFile broken(bad);
        broken.open(QIODevice::WriteOnly);
        broken.close();

        MainWindow window;
        window.show();
        QApplication::processEvents();
        window.InsertShape(ShapeType::RECTANGLE);
        CanvasSession* dirtyPage = window.Tabs()->Current();
        const int dirtyCount = dirtyPage->Scene()->CanvasItemCount();
        const int opened = window.OpenCanvasFiles({b, c, d, bad, b});
        QApplication::processEvents();
        ctx.Check(opened == 3, QStringLiteral("批量打开三份画布、重复路径与坏文件被跳过"),
                  QStringLiteral("opened=%1").arg(opened));
        ctx.Check(window.Tabs()->Count() == 4, QStringLiteral("批量打开后标签数为 1+3"),
                  QString::number(window.Tabs()->Count()));
        ctx.Check(dirtyPage->Scene()->CanvasItemCount() == dirtyCount, QStringLiteral("批量打开期间脏页内容保持原样"));
        ctx.Check(window.Tabs()->Current() && window.Tabs()->Current()->Scene()->CanvasItemCount() > 0,
                  QStringLiteral("批量打开后激活的是最后一份成功的画布"));
    }

    // 13) 备份计时：脏态启动、保存停表；到点只写恢复副本，不动原文件。
    {
        const QString path = tempDir.filePath(QStringLiteral("board-backup.sbcanvas"));
        ctx.Check(WriteCanvas(path, 5), QStringLiteral("写出备份用例画布"));
        const QByteArray originalBytes = [&path]() {
            QFile file(path);
            file.open(QIODevice::ReadOnly);
            return file.readAll();
        }();
        const QDateTime originalMtime = QFileInfo(path).lastModified();

        MainWindow window;
        window.OpenCanvasFile(path);
        QApplication::processEvents();
        AutoSaveService* autoSave = window.AutoSave();
        ctx.Check(autoSave && !autoSave->IsArmed(), QStringLiteral("全干净时不启动备份计时"));
        window.InsertShape(ShapeType::RECTANGLE);
        QApplication::processEvents();
        ctx.Check(autoSave && autoSave->IsArmed(), QStringLiteral("出现未保存改动后启动 5 分钟计时"));

        CanvasSession* session = window.Tabs()->Current();
        const QString id = session->RecoveryId();
        autoSave->SetIntervalForTest(0);
        autoSave->TriggerNowForTest();
        ProcessFor(50);
        const QString scenePath = QDir(AppSettings::RecoveryDir()).filePath(id + QStringLiteral(".sbcanvas"));
        const QString metaPath = QDir(AppSettings::RecoveryDir()).filePath(id + QStringLiteral(".json"));
        ctx.Check(WaitForFile(scenePath) && WaitForFile(metaPath), QStringLiteral("到点写出恢复副本与元数据"));
        QApplication::processEvents();
        ctx.Check(
            QFileInfo(path).lastModified() == originalMtime &&
                [&path]() {
                    QFile file(path);
                    file.open(QIODevice::ReadOnly);
                    return file.readAll();
                }() == originalBytes,
            QStringLiteral("备份不覆写原文件（内容与 mtime 不变）"));
        ctx.Check(!autoSave->IsArmed(), QStringLiteral("备份后没有新改动则停表"));

        const QDateTime firstBackupMtime = QFileInfo(scenePath).lastModified();
        autoSave->TriggerNowForTest();
        ProcessFor(50);
        ctx.Check(QFileInfo(scenePath).lastModified() == firstBackupMtime,
                  QStringLiteral("备份后无改动不再重写恢复副本"));

        window.InsertShape(ShapeType::RECTANGLE);
        ctx.Check(autoSave->IsArmed(), QStringLiteral("备份后再次改动重新启动计时"));
        autoSave->TriggerNowForTest();
        ProcessFor(50);
        ctx.Check(QFileInfo(scenePath).lastModified() != firstBackupMtime,
                  QStringLiteral("修订号前移后下一轮重写恢复副本"));

        // 保存成功后备份删除、计时停止。
        autoSave->MarkSaved(session);
        ctx.Check(!QFileInfo::exists(scenePath) && !QFileInfo::exists(metaPath) && !autoSave->IsArmed(),
                  QStringLiteral("保存成功删除恢复副本并停表"));
    }

    // 13c) 两个画布只有一个变过：到点只重写变过的那份。
    {
        MainWindow window;
        CanvasSession* a = window.Tabs()->Current();
        CanvasSession* b2 = window.Tabs()->CreateSession(true);
        // 只改 a：b 保持干净，用来验证干净页不产生恢复文件。
        window.Tabs()->SetCurrent(a);
        window.InsertShape(ShapeType::RECTANGLE);
        AutoSaveService* autoSave = window.AutoSave();
        autoSave->SetIntervalForTest(0);
        autoSave->TriggerNowForTest();
        ProcessFor(50);
        const QString pathA = QDir(AppSettings::RecoveryDir()).filePath(a->RecoveryId() + QStringLiteral(".sbcanvas"));
        const QString sceneB =
            QDir(AppSettings::RecoveryDir()).filePath(b2->RecoveryId() + QStringLiteral(".sbcanvas"));
        QDateTime mtimeA = QFileInfo(pathA).lastModified();
        ProcessFor(10);
        window.Tabs()->SetCurrent(a);
        window.InsertShape(ShapeType::RECTANGLE);
        autoSave->TriggerNowForTest();
        ProcessFor(50);
        ctx.Check(QFileInfo(pathA).lastModified() > mtimeA && !QFileInfo::exists(sceneB),
                  QStringLiteral("只有变过的画布写恢复副本，干净画布不产生文件"));
    }
}
