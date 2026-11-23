#include "app/I18n.h"
#include "app/Settings.h"
#include "canvas/CanvasView.h"
#include "io/CustomShapeTemplates.h"
#include "items/CanvasTypes.h"
#include "items/lines/CurveItem.h"
#include "items/lines/StraightLineItem.h"
#include "items/polygon/PolygonItem.h"
#include "items/properties/ItemStyleVisuals.h"
#include "items/shapes/AllShapes.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "items/shapes/ShapeFactory.h"
#include "items/text/TextItem.h"
#include "lineart/LineArtConverter.h"
#include "lineart/LineArtDialog.h"
#include "lineart/LineArtPresets.h"
#include "ops/Alignment.h"
#include "ops/BoolOps.h"
#include "scene/CanvasScene.h"
#include "scene/Commands.h"
#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"
#include "selftest/core/SelftestHelpers.h"
#include "ui/dialogs/ExportDialog.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/panels/customshape/CustomShapePanel.h"
#include "ui/panels/itemdialog/ItemPropertiesDialog.h"
#include "ui/panels/layers/LayerPanel.h"
#include "ui/panels/properties/PropertiesPanel.h"
#include "ui/settings/SettingsDialog.h"

#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>
#include <QImage>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QLineF>
#include <QLinearGradient>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMarginsF>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPolygonF>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSize>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTabWidget>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>
#include <QWheelEvent>
#include <QWidget>
#include <QWidgetAction>
#include <QtMath>
#include <cstdlib>

void selftest::RunDockPanelMainWindowTests(Context& ctx)
{
    // 工具卡与主窗口联动：选中出现 / 清空隐藏、透明度 / 边框 / 填充 / 高度即时生效并可撤销。
    {
        const QString dockConfig = QDir(ctx.workDirectory).filePath(QStringLiteral("dock-panel-config.ini"));
        QFile::remove(dockConfig);
        qputenv("STORYBOARD_CONFIG", dockConfig.toLocal8Bit());
        const QString dockShapesDir = AppSettings::ShapesDir();
        StarItem menuShape;
        menuShape.SetContentSize(QSizeF(120, 80));
        QJsonObject menuRoot;
        menuRoot.insert(QStringLiteral("format"), QStringLiteral("StoryBoardCanvas"));
        menuRoot.insert(QStringLiteral("items"), QJsonArray{menuShape.ToJson()});
        const QString menuTemplatePath = QDir(dockShapesDir).filePath(QStringLiteral("dock-probe.sbcanvas"));
        for (const QString& stale : QDir(dockShapesDir).entryList({QStringLiteral("*.sbcanvas")}, QDir::Files)) {
            QFile::remove(QDir(dockShapesDir).filePath(stale));
        }
        QFile menuTemplate(menuTemplatePath);
        if (menuTemplate.open(QIODevice::WriteOnly)) {
            menuTemplate.write(QJsonDocument(menuRoot).toJson(QJsonDocument::Indented));
            menuTemplate.close();
        }
        // 面板的三种边界：空目录不出页、坏文件保留页并灰字提示、方向键 + 回车能直接插入。
        const QString emptyFolder = QDir(dockShapesDir).filePath(QStringLiteral("空目录"));
        QDir().mkpath(emptyFolder);
        const QString brokenFolder = QDir(dockShapesDir).filePath(QStringLiteral("坏文件"));
        QDir().mkpath(brokenFolder);
        QFile brokenFile(QDir(brokenFolder).filePath(QStringLiteral("broken.sbcanvas")));
        if (brokenFile.open(QIODevice::WriteOnly)) {
            brokenFile.write("not a canvas");
            brokenFile.close();
        }
        const QString keyFolder = QDir(dockShapesDir).filePath(QStringLiteral("方向键"));
        QDir().mkpath(keyFolder);
        for (const QString& keyName : {QStringLiteral("key-one"), QStringLiteral("key-two")}) {
            QFile keyFile(QDir(keyFolder).filePath(keyName + QStringLiteral(".sbcanvas")));
            if (keyFile.open(QIODevice::WriteOnly)) {
                keyFile.write(QJsonDocument(menuRoot).toJson(QJsonDocument::Indented));
                keyFile.close();
            }
        }

        MainWindow dockWindow;
        dockWindow.resize(1200, 800);
        dockWindow.show();
        QApplication::processEvents();

        QDockWidget* propertiesDock = dockWindow.findChild<QDockWidget*>(QStringLiteral("PropertiesPanelDock"));
        PropertiesPanel* dockPanel =
            propertiesDock ? propertiesDock->findChild<PropertiesPanel*>(QStringLiteral("PropertiesPanel")) : nullptr;
        QMenu* dockCustomMenu = dockWindow.findChild<QMenu*>(QStringLiteral("CustomShapeMenu"));
        ctx.Check(propertiesDock && dockPanel && !propertiesDock->isFloating() &&
                      dockWindow.dockWidgetArea(propertiesDock) == Qt::LeftDockWidgetArea &&
                      propertiesDock->isVisible(),
                  QStringLiteral("编辑面板首次启动显示并固定在窗口左侧"));
        const int preferredCardWidth = dockPanel ? dockPanel->PreferredWidth() : 0;
        const int actualCardWidth = propertiesDock ? propertiesDock->width() : -1;
        // 宽度既要覆盖内容，也不能明显宽于内容，否则右侧会留下空白。
        const bool dockWidthFits =
            actualCardWidth >= preferredCardWidth - 2 && actualCardWidth <= preferredCardWidth + 2;
        ctx.Check(propertiesDock && dockPanel && preferredCardWidth >= 260 && preferredCardWidth <= 320 &&
                      dockWidthFits,
                  QStringLiteral("编辑面板宽度贴合内容且不超出 320px"),
                  QStringLiteral("preferred=%1 actual=%2").arg(preferredCardWidth).arg(actualCardWidth));
        // 需求 7：自定义图形改为浮动面板，模板以按钮形式出现在标签页里；菜单只留触发器。
        dockWindow.OpenCustomShapePanel(nullptr);
        QApplication::processEvents();
        auto* shapePanel = dockWindow.findChild<CustomShapePanel*>(QStringLiteral("CustomShapePanel"));
        QStringList panelTexts;
        QTabWidget* shapeTabs = shapePanel ? shapePanel->TabWidget() : nullptr;
        if (shapePanel) {
            for (QToolButton* button :
                 shapePanel->findChildren<QToolButton*>(QStringLiteral("CustomShapeTemplateButton"))) {
                panelTexts.append(button->text());
            }
        }
        ctx.Check(shapePanel && shapeTabs && shapeTabs->count() >= 1 &&
                      panelTexts.contains(QStringLiteral("dock-probe")),
                  QStringLiteral("自定义图形浮动面板按文件夹出标签页并列出模板"),
                  QStringLiteral("tabs=%1 texts=%2")
                      .arg(shapeTabs ? shapeTabs->count() : -1)
                      .arg(panelTexts.join(QStringLiteral("|"))));
        ctx.Check(dockCustomMenu && dockCustomMenu->actions().size() == 1 &&
                      dockCustomMenu->actions().constFirst()->objectName() == QStringLiteral("CustomShapePanelTrigger"),
                  QStringLiteral("自定义图形菜单只保留面板触发器"),
                  QStringLiteral("actions=%1").arg(dockCustomMenu ? dockCustomMenu->actions().size() : -1));
        if (shapePanel) {
            shapePanel->hide();
        }
        // 三条边界断言：空目录不出页、坏文件灰字、方向键 + 回车插入。
        QStringList panelTitles;
        QWidget* brokenPage = nullptr;
        int keyTabIndex = -1;
        for (int i = 0; shapeTabs && i < shapeTabs->count(); ++i) {
            const QString title = shapeTabs->tabText(i);
            panelTitles.append(title);
            if (title == QStringLiteral("坏文件")) {
                brokenPage = shapeTabs->widget(i);
            }
            if (title == QStringLiteral("方向键")) {
                keyTabIndex = i;
            }
        }
        QLabel* brokenLabel =
            brokenPage ? brokenPage->findChild<QLabel*>(QStringLiteral("CustomShapeBrokenLabel")) : nullptr;
        ctx.Check(!panelTitles.contains(QStringLiteral("空目录")) && panelTitles.contains(QStringLiteral("坏文件")),
                  QStringLiteral("空目录不出页、有坏文件的目录保留页"),
                  QStringLiteral("titles=%1").arg(panelTitles.join(QStringLiteral("|"))));
        ctx.Check(brokenLabel && brokenLabel->styleSheet().contains(QStringLiteral("#9CA3AF")),
                  QStringLiteral("坏文件在页底以灰字提示"),
                  QStringLiteral("label=%1 style=%2")
                      .arg(brokenLabel ? QStringLiteral("yes") : QStringLiteral("no"))
                      .arg(brokenLabel ? brokenLabel->styleSheet() : QString()));
        const int itemsBeforeKey = dockWindow.Scene()->CanvasItemCount();
        bool keyInserted = false;
        if (shapePanel && keyTabIndex >= 0) {
            shapePanel->TabWidget()->setCurrentIndex(keyTabIndex);
            QApplication::processEvents();
            QList<QToolButton*> keyButtons = shapePanel->TabWidget()->currentWidget()->findChildren<QToolButton*>(
                QStringLiteral("CustomShapeTemplateButton"));
            if (!keyButtons.isEmpty()) {
                shapePanel->activateWindow();
                keyButtons.constFirst()->setFocus(Qt::OtherFocusReason);
                QApplication::processEvents();
                QKeyEvent arrowEvent(QKeyEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
                QApplication::sendEvent(shapePanel, &arrowEvent);
                QWidget* focusedAfterArrow = shapePanel->focusWidget();
                QKeyEvent enterEvent(QKeyEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QApplication::sendEvent(shapePanel, &enterEvent);
                QApplication::processEvents();
                keyInserted = focusedAfterArrow &&
                              focusedAfterArrow->objectName() == QStringLiteral("CustomShapeTemplateButton") &&
                              dockWindow.Scene()->CanvasItemCount() > itemsBeforeKey;
            }
        }
        ctx.Check(keyInserted, QStringLiteral("方向键移动焦点、回车插入模板"),
                  QStringLiteral("items %1->%2").arg(itemsBeforeKey).arg(dockWindow.Scene()->CanvasItemCount()));
        // 插入会落到画布上，断言后立刻撤回，免得影响后面按选中项数量走的检查。
        if (keyInserted) {
            while (dockWindow.Scene()->CanvasItemCount() > itemsBeforeKey &&
                   dockWindow.Scene()->UndoStack()->canUndo()) {
                dockWindow.Scene()->UndoStack()->undo();
            }
            dockWindow.Scene()->clearSelection();
            QApplication::processEvents();
        }
        // 需求：导入自定义图形时，只要拆出多个图元、且这些图元还不属于任何图元分组，
        // 就自动归入一个新组；只有单个图元时不建组。分组与插入是同一条撤销记录。
        {
            CanvasScene* importScene = dockWindow.Scene();
            const QString shapesDir = AppSettings::ShapesDir();
            auto rectJson = [](const QPointF& pos, const QSizeF& size) {
                RectangleItem probe;
                probe.setPos(pos);
                probe.SetContentSize(size);
                return probe.ToJson();
            };
            auto writeTemplate = [&shapesDir](const QString& name, const QJsonArray& items) {
                QJsonObject root;
                root.insert(QStringLiteral("format"), QStringLiteral("StoryBoardCanvas"));
                root.insert(QStringLiteral("items"), items);
                const QString path = QDir(shapesDir).filePath(name + QStringLiteral(".sbcanvas"));
                QFile file(path);
                if (!file.open(QIODevice::WriteOnly)) {
                    return QString();
                }
                file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
                file.close();
                return path;
            };
            auto collectAdded = [importScene](const QSet<CanvasItem*>& before) {
                QList<CanvasItem*> added;
                for (CanvasItem* item : importScene->CanvasItems()) {
                    if (!before.contains(item)) {
                        added.append(item);
                    }
                }
                return added;
            };

            // 两个互不相连的矩形 → 两个闭合子路径 → 两个多边形图元，应落到同一个新组。
            const QString twoPartPath = writeTemplate(QStringLiteral("双块探针"),
                                                      QJsonArray{rectJson(QPointF(20.0, 20.0), QSizeF(60.0, 60.0)),
                                                                 rectJson(QPointF(140.0, 20.0), QSizeF(60.0, 60.0))});
            QSet<CanvasItem*> beforeSet;
            for (CanvasItem* item : importScene->CanvasItems()) {
                beforeSet.insert(item);
            }
            const int countBefore = importScene->CanvasItemCount();
            dockWindow.InsertCustomShape(twoPartPath);
            QApplication::processEvents();
            const QList<CanvasItem*> added = collectAdded(beforeSet);
            const int sharedGroup = added.isEmpty() ? 0 : added.constFirst()->GroupId();
            bool sameGroup = added.size() == 2 && sharedGroup > 0;
            for (CanvasItem* item : added) {
                if (item->GroupId() != sharedGroup) {
                    sameGroup = false;
                }
            }
            ctx.Check(sameGroup, QStringLiteral("导入多图元自定义图形时自动归入同一个新组"),
                      QStringLiteral("added=%1 group=%2 ids=%3")
                          .arg(added.size())
                          .arg(sharedGroup)
                          .arg(added.isEmpty() ? QString() : QString::number(added.constFirst()->GroupId())));

            // 一条撤销同时回退图元与组记录（不留孤儿组），重做时组名随图元一起回来。
            const int countAfterInsert = importScene->CanvasItemCount();
            if (importScene->UndoStack()->canUndo()) {
                importScene->UndoStack()->undo();
                QApplication::processEvents();
            }
            const bool undoneOk =
                importScene->CanvasItemCount() == countBefore && !importScene->GroupIds().contains(sharedGroup);
            if (importScene->UndoStack()->canRedo()) {
                importScene->UndoStack()->redo();
                QApplication::processEvents();
            }
            const bool redoneOk = importScene->CanvasItemCount() == countAfterInsert &&
                                  importScene->GroupName(sharedGroup).startsWith(QStringLiteral("双块探针"));
            ctx.Check(undoneOk && redoneOk,
                      QStringLiteral("自定义图形成组与插入同一条撤销（撤销不留空组、重做保留组名）"),
                      QStringLiteral("undo=%1 redo=%2 name=%3")
                          .arg(undoneOk)
                          .arg(redoneOk)
                          .arg(importScene->GroupName(sharedGroup)));

            // 只有一个 part 的模板不强行建单成员组。
            const QString singlePath = writeTemplate(QStringLiteral("单块探针"),
                                                     QJsonArray{rectJson(QPointF(30.0, 30.0), QSizeF(60.0, 60.0))});
            QSet<CanvasItem*> beforeSingle;
            for (CanvasItem* item : importScene->CanvasItems()) {
                beforeSingle.insert(item);
            }
            dockWindow.InsertCustomShape(singlePath);
            QApplication::processEvents();
            const QList<CanvasItem*> addedSingle = collectAdded(beforeSingle);
            ctx.Check(addedSingle.size() == 1 && addedSingle.constFirst()->GroupId() == 0,
                      QStringLiteral("单图元自定义图形不建分组"),
                      QStringLiteral("added=%1 group=%2")
                          .arg(addedSingle.size())
                          .arg(addedSingle.isEmpty() ? -1 : addedSingle.constFirst()->GroupId()));

            // 清理：撤回两次插入，场景回到导入前的图元数与组记录。
            for (int step = 0; step < 2 && importScene->UndoStack()->canUndo(); ++step) {
                importScene->UndoStack()->undo();
                QApplication::processEvents();
            }
            importScene->clearSelection();
            QApplication::processEvents();
            if (!twoPartPath.isEmpty()) {
                QFile::remove(twoPartPath);
            }
            if (!singlePath.isEmpty()) {
                QFile::remove(singlePath);
            }
        }
        ctx.Check(dockWindow.findChild<QAction*>(QStringLiteral("PropertiesPanelToggleAction")) != nullptr,
                  QStringLiteral("视图菜单提供编辑面板开关"));

        CanvasScene* dockScene = dockWindow.Scene();
        // 卡片不再有常见图形行，插入改为走场景接口（等价于「插入」菜单插入矩形）；
        // 默认是查看模式，先切到编辑模式，图元才可被选中。
        CanvasView* dockView = dockWindow.findChild<CanvasView*>();
        if (dockView) {
            dockView->SetMode(ToolMode::EDIT);
        }
        QApplication::processEvents();
        const int beforeCount = dockScene->CanvasItemCount();
        auto* insertedShape = ShapeFactory::Create(ShapeType::RECTANGLE);
        insertedShape->setPos(QPointF(160.0, 140.0));
        insertedShape->SetContentSize(QSizeF(140.0, 100.0));
        dockScene->AddItemsWithUndo({insertedShape}, QStringLiteral("Add rectangle"));
        insertedShape->setSelected(true);
        QApplication::processEvents();
        ShapeAbstractItem* selectedShape =
            dockScene->SelectedCanvasItems().isEmpty()
                ? nullptr
                : dynamic_cast<ShapeAbstractItem*>(dockScene->SelectedCanvasItems().first());
        ctx.Check(selectedShape == insertedShape && dockScene->CanvasItemCount() == beforeCount + 1 &&
                      propertiesDock->isVisible(),
                  QStringLiteral("插入矩形后编辑面板随选中出现并保持显示"),
                  QStringLiteral("same=%1 count=%2/%3 visible=%4 sel=%5 inScene=%6")
                      .arg(selectedShape == insertedShape ? 1 : 0)
                      .arg(dockScene->CanvasItemCount())
                      .arg(beforeCount + 1)
                      .arg(propertiesDock->isVisible() ? 1 : 0)
                      .arg(insertedShape->isSelected() ? 1 : 0)
                      .arg(dockScene->CanvasItems().contains(insertedShape) ? 1 : 0));

        // 选中图元后可见行变多，卡片宽度保持启动时的值，不随选中来回跳动。
        QApplication::processEvents();
        const int selectedActualWidth = propertiesDock ? propertiesDock->width() : -1;
        ctx.Check(qAbs(selectedActualWidth - preferredCardWidth) <= 2, QStringLiteral("切换选中后编辑面板宽度保持不变"),
                  QStringLiteral("start=%1 actual=%2").arg(preferredCardWidth).arg(selectedActualWidth));

        QDockWidget* layerDock = dockWindow.findChild<QDockWidget*>(QStringLiteral("LayerDock"));
        QDockWidget* viewModeDock = dockWindow.findChild<QDockWidget*>(QStringLiteral("ViewModeDock"));
        QToolBar* modeBar = dockWindow.findChild<QToolBar*>(QStringLiteral("ModeToolBar"));
        ctx.Check(viewModeDock && modeBar && viewModeDock->widget() == modeBar,
                  QStringLiteral("视图模式栏作为右上角卡片"));
        // 卡片标题跟随当前模式，不再固定为「查看模式」。
        CanvasView* titleView = dockWindow.findChild<CanvasView*>();
        if (titleView) {
            titleView->SetMode(ToolMode::VIEW);
            QApplication::processEvents();
        }
        ctx.Check(viewModeDock && titleView && viewModeDock->windowTitle() == ToolModeName(ToolMode::VIEW),
                  QStringLiteral("切到查看模式后模式卡标题同步刷新"),
                  viewModeDock ? viewModeDock->windowTitle() : QString());
        if (titleView) {
            titleView->SetMode(ToolMode::EDIT);
            QApplication::processEvents();
        }
        ctx.Check(viewModeDock && titleView && viewModeDock->windowTitle() == ToolModeName(ToolMode::EDIT),
                  QStringLiteral("切到编辑模式后模式卡标题同步刷新"),
                  viewModeDock ? viewModeDock->windowTitle() : QString());
        // 默认停靠在左侧，不属于悬浮窗口。
        ctx.Check(propertiesDock && !propertiesDock->isFloating() &&
                      dockWindow.dockWidgetArea(propertiesDock) == Qt::LeftDockWidgetArea,
                  QStringLiteral("编辑面板默认停靠左侧"));
        ctx.Check(viewModeDock && layerDock && dockWindow.dockWidgetArea(viewModeDock) == Qt::RightDockWidgetArea &&
                      dockWindow.dockWidgetArea(layerDock) == Qt::RightDockWidgetArea,
                  QStringLiteral("视图模式卡与图层卡默认停靠右侧"));
        ctx.Check(viewModeDock && layerDock &&
                      viewModeDock->mapTo(&dockWindow, QPoint(0, 0)).y() <
                          layerDock->mapTo(&dockWindow, QPoint(0, 0)).y(),
                  QStringLiteral("视图模式卡在右上角、图层卡在右下角"));
        ctx.Check(viewModeDock && layerDock && viewModeDock->height() < dockWindow.height() - 100 &&
                      layerDock->height() < dockWindow.height() - 100,
                  QStringLiteral("右侧两张卡片默认高度不占满窗口"));
        for (const QString& stale : QDir(dockShapesDir).entryList({QStringLiteral("*.sbcanvas")}, QDir::Files)) {
            QFile::remove(QDir(dockShapesDir).filePath(stale));
        }
        for (const QString& staleDir : {QStringLiteral("空目录"), QStringLiteral("坏文件"), QStringLiteral("方向键")}) {
            QDir(QDir(dockShapesDir).filePath(staleDir)).removeRecursively();
        }
        qunsetenv("STORYBOARD_CONFIG");
    }
}
