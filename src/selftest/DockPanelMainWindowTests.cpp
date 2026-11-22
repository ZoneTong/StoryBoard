#include "Alignment.h"
#include "BoolOps.h"
#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CanvasView.h"
#include "Commands.h"
#include "CurveItem.h"
#include "CustomShapePanel.h"
#include "CustomShapeTemplates.h"
#include "ExportDialog.h"
#include "I18n.h"
#include "ItemPropertiesDialog.h"
#include "ItemStyleVisuals.h"
#include "LayerPanel.h"
#include "LineArtConverter.h"
#include "LineArtDialog.h"
#include "LineArtPresets.h"
#include "MainWindow.h"
#include "PolygonItem.h"
#include "PropertiesPanel.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "ShapeAbstractItem.h"
#include "StraightLineItem.h"
#include "TextItem.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"
#include "selftest/SelftestHelpers.h"
#include "shapes/AllShapes.h"

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
        auto* insertedShape = CreateShapeItem(ShapeType::RECTANGLE);
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
