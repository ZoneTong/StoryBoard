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
#include "ShapeItem.h"
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
                  QStringLiteral("属性编辑面板首次启动显示并固定在窗口左侧"));
        const int preferredCardWidth = dockPanel ? dockPanel->PreferredWidth() : 0;
        const int actualCardWidth = propertiesDock ? propertiesDock->width() : -1;
        // 宽度既要覆盖内容，也不能明显宽于内容，否则右侧会留下空白。
        const bool dockWidthFits =
            actualCardWidth >= preferredCardWidth - 2 && actualCardWidth <= preferredCardWidth + 2;
        ctx.Check(propertiesDock && dockPanel && preferredCardWidth >= 260 && preferredCardWidth <= 320 &&
                      dockWidthFits,
                  QStringLiteral("属性编辑面板宽度贴合内容且不超出 320px"),
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
                  QStringLiteral("视图菜单提供属性编辑面板开关"));

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
        ShapeItem* selectedShape = dockScene->SelectedCanvasItems().isEmpty()
                                       ? nullptr
                                       : dynamic_cast<ShapeItem*>(dockScene->SelectedCanvasItems().first());
        ctx.Check(selectedShape == insertedShape && dockScene->CanvasItemCount() == beforeCount + 1 &&
                      propertiesDock->isVisible(),
                  QStringLiteral("插入矩形后属性编辑面板随选中出现并保持显示"),
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
        ctx.Check(qAbs(selectedActualWidth - preferredCardWidth) <= 2,
                  QStringLiteral("切换选中后属性编辑面板宽度保持不变"),
                  QStringLiteral("start=%1 actual=%2").arg(preferredCardWidth).arg(selectedActualWidth));

        QDockWidget* layerDock = dockWindow.findChild<QDockWidget*>(QStringLiteral("LayerDock"));
        QDockWidget* viewModeDock = dockWindow.findChild<QDockWidget*>(QStringLiteral("ViewModeDock"));
        QToolBar* modeBar = dockWindow.findChild<QToolBar*>(QStringLiteral("ModeToolBar"));
        ctx.Check(viewModeDock && modeBar && viewModeDock->widget() == modeBar,
                  QStringLiteral("视图模式栏作为右上角卡片"));
        // 默认停靠在左侧，不属于悬浮窗口。
        ctx.Check(propertiesDock && !propertiesDock->isFloating() &&
                      dockWindow.dockWidgetArea(propertiesDock) == Qt::LeftDockWidgetArea,
                  QStringLiteral("属性编辑面板默认停靠左侧"));
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

        if (selectedShape && dockPanel) {
            QList<QSpinBox*> valueSpins = dockPanel->findChildren<QSpinBox*>(QStringLiteral("ItemValueSpin"));
            QSpinBox* opacitySpin = nullptr;
            // 需求 4：圆角上限改为短边一半，不能再按固定的 400 识别，按对象名取。
            QSpinBox* cornerSpin = dockPanel->findChild<QSpinBox*>(QStringLiteral("ItemValueCornerSpin"));
            for (QSpinBox* spin : valueSpins) {
                if (spin->maximum() == 100) {
                    opacitySpin = spin;
                }
            }
            const QList<QToolButton*> borderButtons =
                dockPanel->findChildren<QToolButton*>(QStringLiteral("ItemBorderStyleButton"));
            // 需求 15 在候选头部插入“无”，按钮顺序跟随 ItemStyleVisuals::BorderStyles()，
            // 断言不能再按位置取“虚线”，必须按候选表定位。
            const int dashedBorderButtonIndex = ItemStyleVisuals::BorderStyles().indexOf(Qt::DashLine);
            const QList<QToolButton*> fillButtons =
                dockPanel->findChildren<QToolButton*>(QStringLiteral("ItemFillStyleButton"));
            QToolButton* heightUp = dockPanel->findChild<QToolButton*>(QStringLiteral("ItemHeightUpButton"));
            QToolButton* copyButton = dockPanel->findChild<QToolButton*>(QStringLiteral("ItemCopyButton"));
            QToolButton* deleteButton = dockPanel->findChild<QToolButton*>(QStringLiteral("ItemDeleteButton"));
            QToolButton* rotateCwButton = dockPanel->findChild<QToolButton*>(QStringLiteral("ItemRotateCwButton"));
            QToolButton* rotateCcwButton = dockPanel->findChild<QToolButton*>(QStringLiteral("ItemRotateCcwButton"));
            QToolButton* groupButton = dockPanel->findChild<QToolButton*>(QStringLiteral("ItemGroupButton"));
            QToolButton* ungroupButton = dockPanel->findChild<QToolButton*>(QStringLiteral("ItemUngroupButton"));
            const QList<QToolButton*> alignRowButtons =
                dockPanel->findChildren<QToolButton*>(QStringLiteral("ItemAlignRowButton"));
            const QList<QToolButton*> morphButtons =
                dockPanel->findChildren<QToolButton*>(QStringLiteral("ItemMorphButton"));

            if (opacitySpin) {
                opacitySpin->setValue(45);
                QApplication::processEvents();
            }
            ctx.Check(qAbs(selectedShape->ItemOpacity() - 0.45) < 0.01, QStringLiteral("工具卡透明度即时作用到图形"),
                      QString::number(selectedShape->ItemOpacity()));
            dockScene->UndoStack()->undo();
            ctx.Check(qAbs(selectedShape->ItemOpacity() - 1.0) < 0.01, QStringLiteral("撤销透明度修改恢复原值"),
                      QString::number(selectedShape->ItemOpacity()));

            if (dashedBorderButtonIndex >= 0 && borderButtons.size() > dashedBorderButtonIndex) {
                borderButtons.at(dashedBorderButtonIndex)->click();
                QApplication::processEvents();
            }
            ctx.Check(selectedShape->StrokeStyle() == Qt::DashLine, QStringLiteral("工具卡边框样式作用到图形"));
            if (fillButtons.size() > 2) {
                fillButtons.at(2)->click();
                QApplication::processEvents();
            }
            ctx.Check(selectedShape->fillStyle() == FillStyle::HACHURE, QStringLiteral("工具卡填充样式作用到图形"));

            if (cornerSpin) {
                cornerSpin->setValue(24);
                QApplication::processEvents();
            }
            ctx.Check(qAbs(selectedShape->CornerRadius() - 24.0) < 0.01, QStringLiteral("矩形选中时圆角设置可用"),
                      QString::number(selectedShape->CornerRadius()));

            const int heightBefore = selectedShape->ZOrder();
            if (heightUp) {
                heightUp->click();
                QApplication::processEvents();
            }
            ctx.Check(selectedShape->ZOrder() == heightBefore + 1, QStringLiteral("工具卡高度 +1 按钮生效"),
                      QStringLiteral("%1->%2").arg(heightBefore).arg(selectedShape->ZOrder()));
            dockScene->UndoStack()->undo();
            ctx.Check(selectedShape->ZOrder() == heightBefore, QStringLiteral("撤销高度 +1 恢复原高度"));

            // 边框颜色 / 宽度同时是画笔的当前颜色与宽度，改完描边后画笔立即沿用。
            QWidget* borderWidthWidget = dockPanel->findChild<QWidget*>(QStringLiteral("ItemBorderWidthRow"));
            QSpinBox* borderWidthSpin =
                borderWidthWidget ? borderWidthWidget->findChild<QSpinBox*>(QStringLiteral("ItemBorderWidthSpin"))
                                  : nullptr;
            if (borderWidthSpin) {
                borderWidthSpin->setValue(9);
                QApplication::processEvents();
            }
            ctx.Check(borderWidthSpin && borderWidthSpin->minimum() == 1 && borderWidthSpin->maximum() == 64 &&
                          borderWidthSpin->singleStep() == 1 && borderWidthSpin->suffix() == QStringLiteral(" px"),
                      QStringLiteral("卡片描线宽度与工具栏使用同一 SpinBox 规格"),
                      QStringLiteral("min=%1 max=%2 step=%3 suffix=%4")
                          .arg(borderWidthSpin ? borderWidthSpin->minimum() : -1)
                          .arg(borderWidthSpin ? borderWidthSpin->maximum() : -1)
                          .arg(borderWidthSpin ? borderWidthSpin->singleStep() : 0)
                          .arg(borderWidthSpin ? borderWidthSpin->suffix() : QStringLiteral("-")));
            ctx.Check(qAbs(selectedShape->StrokeWidth() - 9.0) < 0.01 && dockView &&
                          qAbs(dockView->BrushWidth() - 9.0) < 0.01,
                      QStringLiteral("边框宽度同时成为画笔宽度"),
                      QStringLiteral("stroke=%1 brush=%2")
                          .arg(selectedShape->StrokeWidth())
                          .arg(dockView ? dockView->BrushWidth() : -1.0));
            QWidget* borderColorWidget = dockPanel->findChild<QWidget*>(QStringLiteral("ItemBorderColorRow"));
            const QList<QToolButton*> strokeSwatches =
                borderColorWidget ? borderColorWidget->findChildren<QToolButton*>(QStringLiteral("ItemColorSwatch"))
                                  : QList<QToolButton*>();
            const QColor brushColorTarget(QStringLiteral("#EF4444"));
            if (strokeSwatches.size() > 1) {
                strokeSwatches.at(1)->click();
                QApplication::processEvents();
            }
            ctx.Check(selectedShape->StrokeColor() == brushColorTarget && dockView &&
                          dockView->BrushColor() == brushColorTarget,
                      QStringLiteral("边框颜色同时成为画笔颜色"),
                      QStringLiteral("stroke=%1 brush=%2")
                          .arg(selectedShape->StrokeColor().name())
                          .arg(dockView ? dockView->BrushColor().name() : QStringLiteral("-")));

            ctx.Check(copyButton && deleteButton && rotateCwButton && rotateCcwButton && copyButton->isVisible() &&
                          deleteButton->isVisible() && rotateCwButton->isVisible() && rotateCcwButton->isVisible(),
                      QStringLiteral("单选时属性编辑面板列出复制 / 删除 / 旋转按钮"));
            ctx.Check(alignRowButtons.size() == 8 && morphButtons.size() == 6 && groupButton && ungroupButton &&
                          !alignRowButtons.first()->isVisible() && !morphButtons.first()->isVisible() &&
                          !groupButton->isVisible() && !ungroupButton->isVisible(),
                      QStringLiteral("单选时不显示对齐 / 合并图元 / 组合按钮行"));

            // 卡片里每颗可见按钮都必须绑定具体动作，不能只显示图标。
            const QStringList cardButtonNames = {
                QStringLiteral("ItemCopyButton"),     QStringLiteral("ItemDeleteButton"),
                QStringLiteral("ItemRotateCwButton"), QStringLiteral("ItemRotateCcwButton"),
                QStringLiteral("ItemAlignRowButton"), QStringLiteral("ItemMorphButton"),
                QStringLiteral("ItemGroupButton"),    QStringLiteral("ItemUngroupButton"),
            };
            int visibleCardButtons = 0;
            int unboundCardButtons = 0;
            for (QToolButton* button : dockPanel->findChildren<QToolButton*>()) {
                if (!button || !cardButtonNames.contains(button->objectName()) || !button->isVisible()) {
                    continue;
                }
                ++visibleCardButtons;
                if (button->property("cardActionText").toString().isEmpty()) {
                    ++unboundCardButtons;
                }
            }
            ctx.Check(visibleCardButtons >= 4 && unboundCardButtons == 0,
                      QStringLiteral("卡片所有可见按钮都关联了具体动作"),
                      QStringLiteral("visible=%1 unbound=%2").arg(visibleCardButtons).arg(unboundCardButtons));

            // 卡片按钮不是摆设：逐一验证旋转与复制按钮真的执行主窗口动作，且可撤销。
            if (rotateCwButton) {
                const qreal rotationBefore = selectedShape->rotation();
                rotateCwButton->click();
                QApplication::processEvents();
                const qreal rotationAfter = selectedShape->rotation();
                dockScene->UndoStack()->undo();
                QApplication::processEvents();
                ctx.Check(
                    qAbs(rotationAfter - rotationBefore - 45.0) < 0.01 &&
                        qAbs(selectedShape->rotation() - rotationBefore) < 0.01,
                    QStringLiteral("卡片旋转按钮真正旋转图元且可撤销"),
                    QStringLiteral("%1->%2->%3").arg(rotationBefore).arg(rotationAfter).arg(selectedShape->rotation()));
                selectedShape->setSelected(true);
                QApplication::processEvents();
            }
            if (copyButton) {
                const int countBeforeCopy = dockScene->CanvasItemCount();
                copyButton->click();
                QApplication::processEvents();
                const int countAfterCopy = dockScene->CanvasItemCount();
                dockScene->UndoStack()->undo();
                QApplication::processEvents();
                ctx.Check(countAfterCopy == countBeforeCopy + 1 && dockScene->CanvasItemCount() == countBeforeCopy,
                          QStringLiteral("卡片复制按钮真正复制图元且可撤销"),
                          QStringLiteral("%1->%2->%3")
                              .arg(countBeforeCopy)
                              .arg(countAfterCopy)
                              .arg(dockScene->CanvasItemCount()));
                selectedShape->setSelected(true);
                QApplication::processEvents();
            }
            if (rotateCcwButton) {
                const qreal rotationBefore = selectedShape->rotation();
                rotateCcwButton->click();
                QApplication::processEvents();
                const qreal rotationAfter = selectedShape->rotation();
                dockScene->UndoStack()->undo();
                QApplication::processEvents();
                ctx.Check(
                    qAbs(rotationAfter - rotationBefore + 45.0) < 0.01 &&
                        qAbs(selectedShape->rotation() - rotationBefore) < 0.01,
                    QStringLiteral("卡片逆时针旋转按钮真正旋转图元且可撤销"),
                    QStringLiteral("%1->%2->%3").arg(rotationBefore).arg(rotationAfter).arg(selectedShape->rotation()));
                selectedShape->setSelected(true);
                QApplication::processEvents();
            }

            // 多选时卡片把对齐 8 项与合并图元 6 项直接展开成图标按钮行，并复用菜单栏同一批动作。
            auto* secondShape = CreateShapeItem(ShapeType::RECTANGLE);
            secondShape->setPos(QPointF(420.0, 240.0));
            secondShape->SetContentSize(QSizeF(120.0, 90.0));
            secondShape->SetStyle(QColor(QStringLiteral("#1D4ED8")), QColor(29, 78, 216, 40), 3.0);
            dockScene->AddItemsWithUndo({secondShape}, QStringLiteral("Add second shape"));
            selectedShape->setSelected(true);
            secondShape->setSelected(true);
            QApplication::processEvents();
            int visibleAlign = 0;
            for (const QToolButton* button : alignRowButtons) {
                if (button->isVisible()) {
                    ++visibleAlign;
                }
            }
            int visibleMorph = 0;
            for (const QToolButton* button : morphButtons) {
                if (button->isVisible()) {
                    ++visibleMorph;
                }
            }
            ctx.Check(visibleAlign == 6, QStringLiteral("两个图元时对齐行展开 6 个可用模式"),
                      QString::number(visibleAlign));
            ctx.Check(visibleMorph == 5, QStringLiteral("多选时合并图元五项展开成一行"));
            ctx.Check(groupButton->isVisible() && !ungroupButton->isVisible() &&
                          groupButton->property("cardActionText").toString() == QStringLiteral("图元分组"),
                      QStringLiteral("多选未分组时显示图元分组、不显示解散分组"));

            // 对齐行按钮真正调用对齐动作；对齐后可撤销并恢复两个图元。
            if (!alignRowButtons.isEmpty()) {
                alignRowButtons.first()->click();
                QApplication::processEvents();
                const bool aligned =
                    qAbs(selectedShape->sceneBoundingRect().left() - secondShape->sceneBoundingRect().left()) < 0.5;
                dockScene->UndoStack()->undo();
                QApplication::processEvents();
                ctx.Check(aligned, QStringLiteral("卡片对齐按钮真正对齐图元"),
                          QStringLiteral("left=%1/%2")
                              .arg(selectedShape->sceneBoundingRect().left())
                              .arg(secondShape->sceneBoundingRect().left()));
                selectedShape->setSelected(true);
                secondShape->setSelected(true);
                QApplication::processEvents();
            }
            // 合并图元行按钮真正执行布尔运算：联合后两个矩形合并成一个多边形，撤销恢复。
            if (!morphButtons.isEmpty()) {
                const int countBeforeMorph = dockScene->CanvasItemCount();
                morphButtons.first()->click();
                QApplication::processEvents();
                bool hasComposite = false;
                for (CanvasItem* item : dockScene->CanvasItems()) {
                    if (dynamic_cast<PolygonItem*>(item)) {
                        hasComposite = true;
                        break;
                    }
                }
                const bool merged = dockScene->CanvasItemCount() == countBeforeMorph - 1 && hasComposite;
                dockScene->UndoStack()->undo();
                QApplication::processEvents();
                ctx.Check(merged && dockScene->CanvasItemCount() == countBeforeMorph,
                          QStringLiteral("卡片合并图元按钮真正合并图元且可撤销"),
                          QStringLiteral("count=%1->%2 composite=%3")
                              .arg(countBeforeMorph)
                              .arg(dockScene->CanvasItemCount())
                              .arg(hasComposite ? 1 : 0));
                selectedShape->setSelected(true);
                secondShape->setSelected(true);
                QApplication::processEvents();
            }

            const int groupedCount = dockScene->GroupSelectedItems();
            // 组合不改选中集合，重新点选一次让主窗口按新的组归属刷新动作可用状态。
            dockScene->clearSelection();
            selectedShape->setSelected(true);
            secondShape->setSelected(true);
            QApplication::processEvents();
            ctx.Check(groupedCount == 2 && ungroupButton->isVisible() && !groupButton->isVisible() &&
                          ungroupButton->property("cardActionText").toString() == QStringLiteral("解散分组"),
                      QStringLiteral("选中整组时只显示解散分组"));

            // 组 + 组外图元混合选中：再套一层组合与取消组合同时可用（分组支持多层）。
            auto* thirdShape = CreateShapeItem(ShapeType::ELLIPSE);
            thirdShape->setPos(QPointF(560.0, 320.0));
            thirdShape->SetContentSize(QSizeF(100.0, 80.0));
            dockScene->AddItemsWithUndo({thirdShape}, QStringLiteral("Add third shape"));
            selectedShape->setSelected(true);
            secondShape->setSelected(true);
            thirdShape->setSelected(true);
            QApplication::processEvents();
            ctx.Check(groupButton->isVisible() && ungroupButton->isVisible() &&
                          groupButton->property("cardActionText").toString() == QStringLiteral("图元分组") &&
                          ungroupButton->property("cardActionText").toString() == QStringLiteral("解散分组"),
                      QStringLiteral("组与普通图元混合选中时图元分组和解散分组都显示"));

            // 删除按钮单独用一个临时图元验证，避免删掉后面还要复用的图元。
            auto* deleteProbe = CreateShapeItem(ShapeType::RECTANGLE);
            deleteProbe->setPos(QPointF(640.0, 360.0));
            deleteProbe->SetContentSize(QSizeF(80.0, 60.0));
            dockScene->AddItemsWithUndo({deleteProbe}, QStringLiteral("Add delete probe"));
            dockScene->clearSelection();
            deleteProbe->setSelected(true);
            QApplication::processEvents();
            const int countBeforeDelete = dockScene->CanvasItemCount();
            if (deleteButton) {
                deleteButton->click();
                QApplication::processEvents();
            }
            const int countAfterDelete = dockScene->CanvasItemCount();
            dockScene->UndoStack()->undo();
            QApplication::processEvents();
            ctx.Check(countAfterDelete == countBeforeDelete - 1 && dockScene->CanvasItemCount() == countBeforeDelete,
                      QStringLiteral("卡片删除按钮真正删除图元且可撤销"),
                      QStringLiteral("%1->%2->%3")
                          .arg(countBeforeDelete)
                          .arg(countAfterDelete)
                          .arg(dockScene->CanvasItemCount()));


            // 曲线复用描线颜色 / 宽度 / 样式与透明度：只选曲线时描线样式行可用。
            auto* selectedStroke = new CurveItem();
            selectedStroke->SetStrokePoints({QPointF(60.0, 300.0), QPointF(180.0, 300.0)},
                                            QColor(QStringLiteral("#2563EB")), 6.0);
            dockScene->AddItemsWithUndo({selectedStroke}, QStringLiteral("Add stroke"));
            dockScene->clearSelection();
            selectedStroke->setSelected(true);
            QApplication::processEvents();

            QWidget* borderStyleWidget = dockPanel->findChild<QWidget*>(QStringLiteral("ItemBorderStyleRow"));
            ctx.Check(borderStyleWidget && borderStyleWidget->isVisible(), QStringLiteral("只选曲线时显示描线样式行"));
            ctx.Check(borderWidthSpin && borderWidthSpin->isEnabled() &&
                          qAbs(borderWidthSpin->value() - selectedStroke->StrokeWidth()) < 0.01 && opacitySpin &&
                          opacitySpin->isEnabled(),
                      QStringLiteral("只选曲线时描线宽度与透明度可用并按曲线回显"),
                      QStringLiteral("width=%1 spin=%2")
                          .arg(selectedStroke->StrokeWidth())
                          .arg(borderWidthSpin ? borderWidthSpin->value() : -1.0));

            if (strokeSwatches.size() > 1) {
                strokeSwatches.at(1)->click();
                QApplication::processEvents();
            }
            ctx.Check(selectedStroke->StrokeColor() == QColor(QStringLiteral("#EF4444")),
                      QStringLiteral("卡片描线颜色直接改变曲线颜色"), selectedStroke->StrokeColor().name());
            dockScene->UndoStack()->undo();
            QApplication::processEvents();
            ctx.Check(selectedStroke->StrokeColor() == QColor(QStringLiteral("#2563EB")),
                      QStringLiteral("撤销描线颜色恢复曲线原色"), selectedStroke->StrokeColor().name());

            if (borderWidthSpin) {
                borderWidthSpin->setValue(11);
                QApplication::processEvents();
            }
            ctx.Check(qAbs(selectedStroke->StrokeWidth() - 11.0) < 0.01, QStringLiteral("卡片描线宽度直接改变曲线宽度"),
                      QString::number(selectedStroke->StrokeWidth()));
            dockScene->UndoStack()->undo();
            QApplication::processEvents();
            ctx.Check(qAbs(selectedStroke->StrokeWidth() - 6.0) < 0.01, QStringLiteral("撤销描线宽度恢复曲线原宽度"),
                      QString::number(selectedStroke->StrokeWidth()));

            if (dashedBorderButtonIndex >= 0 && borderButtons.size() > dashedBorderButtonIndex) {
                const Qt::PenStyle styleBefore = selectedStroke->strokePattern();
                borderButtons.at(dashedBorderButtonIndex)->click();
                QApplication::processEvents();
                ctx.Check(selectedStroke->strokePattern() != styleBefore,
                          QStringLiteral("曲线可通过编辑卡切换描线样式"));
                dockScene->UndoStack()->undo();
                QApplication::processEvents();
                ctx.Check(selectedStroke->strokePattern() == styleBefore, QStringLiteral("撤销描线样式恢复曲线原样式"));
            }

            if (opacitySpin) {
                opacitySpin->setValue(55);
                QApplication::processEvents();
            }
            ctx.Check(qAbs(selectedStroke->ItemOpacity() - 0.55) < 0.01, QStringLiteral("透明度作用到曲线"),
                      QString::number(selectedStroke->ItemOpacity()));
            dockScene->UndoStack()->undo();
            QApplication::processEvents();
            ctx.Check(qAbs(selectedStroke->ItemOpacity() - 1.0) < 0.01, QStringLiteral("撤销透明度恢复曲线原值"),
                      QString::number(selectedStroke->ItemOpacity()));

            selectedShape->setSelected(true);
            selectedStroke->setSelected(true);
            QApplication::processEvents();
            ctx.Check(borderStyleWidget && borderStyleWidget->isVisible(),
                      QStringLiteral("图形与曲线混合选中时保留描线样式行"));
            if (opacitySpin) {
                opacitySpin->setValue(30);
                QApplication::processEvents();
            }
            ctx.Check(qAbs(selectedShape->ItemOpacity() - 0.3) < 0.01 &&
                          qAbs(selectedStroke->ItemOpacity() - 0.3) < 0.01,
                      QStringLiteral("透明度同时作用到图形和曲线"),
                      QStringLiteral("shape=%1 stroke=%2")
                          .arg(selectedShape->ItemOpacity())
                          .arg(selectedStroke->ItemOpacity()));
            dockScene->UndoStack()->undo();
            QApplication::processEvents();
            ctx.Check(qAbs(selectedShape->ItemOpacity() - 1.0) < 0.01 &&
                          qAbs(selectedStroke->ItemOpacity() - 1.0) < 0.01,
                      QStringLiteral("撤销透明度同时恢复图形和曲线"));

            // 多边形也要有填充 / 描线属性：选中后五行可见，按钮改动真正落到图层并可撤销。
            PolygonItem::Layer polygonLayer;
            polygonLayer.path.addRect(0.0, 0.0, 140.0, 90.0);
            polygonLayer.fill = QColor(QStringLiteral("#3B82F6"));
            polygonLayer.stroke = QColor(QStringLiteral("#1E293B"));
            polygonLayer.strokeWidth = 4.0;
            polygonLayer.role = PolygonItem::LayerRole::FILLED_AREA;
            auto* polygon = new PolygonItem({polygonLayer});
            polygon->setPos(QPointF(240.0, 240.0));
            dockScene->AddItemsWithUndo({polygon}, QStringLiteral("Add polygon"));
            dockScene->clearSelection();
            polygon->setSelected(true);
            QApplication::processEvents();

            QWidget* fillStyleRow = dockPanel->findChild<QWidget*>(QStringLiteral("ItemFillStyleRow"));
            QWidget* fillColorRow = dockPanel->findChild<QWidget*>(QStringLiteral("ItemFillColorRow"));
            QWidget* borderColorRow = dockPanel->findChild<QWidget*>(QStringLiteral("ItemBorderColorRow"));
            QWidget* borderWidthRow = dockPanel->findChild<QWidget*>(QStringLiteral("ItemBorderWidthRow"));
            ctx.Check(fillStyleRow && fillStyleRow->isVisible() && fillColorRow && fillColorRow->isVisible() &&
                          borderStyleWidget && borderStyleWidget->isVisible() && borderColorRow &&
                          borderColorRow->isVisible() && borderWidthRow && borderWidthRow->isVisible(),
                      QStringLiteral("选中多边形时填充与描线五行全部可见"));

            const FillStyle polygonFillBefore = polygon->fillStyle();
            if (fillButtons.size() > 2) {
                fillButtons.at(2)->click();
                QApplication::processEvents();
            }
            ctx.Check(polygon->fillStyle() == FillStyle::HACHURE, QStringLiteral("编辑卡填充样式作用到多边形"),
                      QString::number(static_cast<int>(polygon->fillStyle())));
            dockScene->UndoStack()->undo();
            QApplication::processEvents();
            ctx.Check(polygon->fillStyle() == polygonFillBefore, QStringLiteral("撤销填充样式恢复多边形原样式"));

            const QList<QToolButton*> fillSwatches =
                fillColorRow ? fillColorRow->findChildren<QToolButton*>(QStringLiteral("ItemColorSwatch"))
                             : QList<QToolButton*>();
            const QColor polygonFillBeforeColor = polygon->FillColor();
            if (fillSwatches.size() > 1) {
                fillSwatches.at(1)->click();
                QApplication::processEvents();
            }
            ctx.Check(polygon->FillColor() == QColor(QStringLiteral("#EF4444")),
                      QStringLiteral("编辑卡填充颜色作用到多边形"), polygon->FillColor().name());
            dockScene->UndoStack()->undo();
            QApplication::processEvents();
            ctx.Check(polygon->FillColor() == polygonFillBeforeColor, QStringLiteral("撤销填充颜色恢复多边形原色"),
                      polygon->FillColor().name());

            const Qt::PenStyle polygonPenBefore = polygon->StrokeStyle();
            if (dashedBorderButtonIndex >= 0 && borderButtons.size() > dashedBorderButtonIndex) {
                borderButtons.at(dashedBorderButtonIndex)->click();
                QApplication::processEvents();
            }
            ctx.Check(polygon->StrokeStyle() != polygonPenBefore, QStringLiteral("编辑卡描线样式作用到多边形"));
            dockScene->UndoStack()->undo();
            QApplication::processEvents();
            ctx.Check(polygon->StrokeStyle() == polygonPenBefore, QStringLiteral("撤销描线样式恢复多边形原样式"));

            const QColor polygonStrokeBefore = polygon->StrokeColor();
            const QList<QToolButton*> polygonStrokeSwatches =
                borderColorRow ? borderColorRow->findChildren<QToolButton*>(QStringLiteral("ItemColorSwatch"))
                               : QList<QToolButton*>();
            if (polygonStrokeSwatches.size() > 1) {
                polygonStrokeSwatches.at(1)->click();
                QApplication::processEvents();
            }
            ctx.Check(polygon->StrokeColor() == QColor(QStringLiteral("#EF4444")),
                      QStringLiteral("编辑卡描线颜色作用到多边形"), polygon->StrokeColor().name());
            dockScene->UndoStack()->undo();
            QApplication::processEvents();
            ctx.Check(polygon->StrokeColor() == polygonStrokeBefore, QStringLiteral("撤销描线颜色恢复多边形原色"),
                      polygon->StrokeColor().name());

            const qreal polygonWidthBefore = polygon->StrokeWidth();
            QSpinBox* polygonWidthSpin =
                borderWidthRow ? borderWidthRow->findChild<QSpinBox*>(QStringLiteral("ItemBorderWidthSpin")) : nullptr;
            if (polygonWidthSpin) {
                polygonWidthSpin->setValue(9);
                QApplication::processEvents();
            }
            ctx.Check(qAbs(polygon->StrokeWidth() - 9.0) < 0.01, QStringLiteral("编辑卡描线宽度作用到多边形"),
                      QString::number(polygon->StrokeWidth()));
            dockScene->UndoStack()->undo();
            QApplication::processEvents();
            ctx.Check(qAbs(polygon->StrokeWidth() - polygonWidthBefore) < 0.01,
                      QStringLiteral("撤销描线宽度恢复多边形原宽度"), QString::number(polygon->StrokeWidth()));

            // 属性窗同样识别多边形的填充 / 描线字段，确认后只占一条撤销记录。
            ItemPropertiesDialog polygonDialog(dockScene, polygon);
            QComboBox* dialogFillStyle =
                polygonDialog.findChild<QComboBox*>(QStringLiteral("ItemPropertiesFillStyleValue"));
            QSpinBox* dialogStrokeWidth =
                polygonDialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesStrokeWidthValue"));
            const FillStyle dialogFillBefore = polygon->fillStyle();
            const qreal dialogWidthBefore = polygon->StrokeWidth();
            if (dialogFillStyle) {
                const int hachureIndex = dialogFillStyle->findData(static_cast<int>(FillStyle::HACHURE));
                if (hachureIndex >= 0) {
                    dialogFillStyle->setCurrentIndex(hachureIndex);
                }
            }
            if (dialogStrokeWidth) {
                dialogStrokeWidth->setValue(12);
            }
            polygonDialog.accept();
            QApplication::processEvents();
            ctx.Check(polygon->fillStyle() == FillStyle::HACHURE && qAbs(polygon->StrokeWidth() - 12.0) < 0.01,
                      QStringLiteral("属性窗填充样式与描线宽度作用到多边形"),
                      QStringLiteral("fill=%1 width=%2")
                          .arg(static_cast<int>(polygon->fillStyle()))
                          .arg(polygon->StrokeWidth()));
            dockScene->UndoStack()->undo();
            QApplication::processEvents();
            ctx.Check(polygon->fillStyle() == dialogFillBefore &&
                          qAbs(polygon->StrokeWidth() - dialogWidthBefore) < 0.01,
                      QStringLiteral("撤销属性窗修改同时恢复多边形填充与描线"));

            // 图形与多边形混合选中时，一次填充色修改只占一条撤销记录。
            dockScene->clearSelection();
            selectedShape->setSelected(true);
            polygon->setSelected(true);
            QApplication::processEvents();
            const QColor mixedShapeFillBefore = selectedShape->FillColor();
            const QColor mixedPolygonFillBefore = polygon->FillColor();
            const int mixedUndoBefore = dockScene->UndoStack()->index();
            if (fillSwatches.size() > 2) {
                fillSwatches.at(2)->click();
                QApplication::processEvents();
            }
            const int mixedUndoAfter = dockScene->UndoStack()->index();
            ctx.Check(selectedShape->FillColor() != mixedShapeFillBefore &&
                          polygon->FillColor() != mixedPolygonFillBefore && mixedUndoAfter == mixedUndoBefore + 1,
                      QStringLiteral("混合选中图形与多边形时填充色只占一条撤销记录"),
                      QStringLiteral("undo=%1->%2 shape=%3->%4 polygon=%5->%6 selected=%7")
                          .arg(mixedUndoBefore)
                          .arg(mixedUndoAfter)
                          .arg(mixedShapeFillBefore.name())
                          .arg(selectedShape->FillColor().name())
                          .arg(mixedPolygonFillBefore.name())
                          .arg(polygon->FillColor().name())
                          .arg(dockScene->SelectedCanvasItems().size()));
            dockScene->UndoStack()->undo();
            QApplication::processEvents();
            ctx.Check(selectedShape->FillColor() == mixedShapeFillBefore &&
                          polygon->FillColor() == mixedPolygonFillBefore,
                      QStringLiteral("撤销混合填充同时恢复图形与多边形原色"));

            // 文字 / 图片内容行与卡片操作按钮：只对相应类型显示，并且写入能撤销。
            auto* cardText = new TextItem();
            cardText->SetTextFormat(QStringLiteral("卡片文字"), QFont(QStringLiteral("Arial"), 12),
                                    QColor(QStringLiteral("#1E293B")));
            cardText->setPos(QPointF(420.0, 430.0));
            dockScene->AddItemsWithUndo({cardText}, QStringLiteral("Add card text"));
            dockScene->clearSelection();
            cardText->setSelected(true);
            QApplication::processEvents();

            QWidget* textContentRow = propertiesDock->findChild<QWidget*>(QStringLiteral("ItemTextContentRow"));
            QWidget* textFontRow = propertiesDock->findChild<QWidget*>(QStringLiteral("ItemTextFontRow"));
            QWidget* textColorRow = propertiesDock->findChild<QWidget*>(QStringLiteral("ItemTextColorRow"));
            QWidget* fillImageRow = propertiesDock->findChild<QWidget*>(QStringLiteral("ItemFillImageRow"));
            QToolButton* propertiesButton =
                propertiesDock->findChild<QToolButton*>(QStringLiteral("ItemPropertiesButton"));
            QToolButton* smoothButton = propertiesDock->findChild<QToolButton*>(QStringLiteral("ItemSmoothButton"));
            ctx.Check(textContentRow && textContentRow->isVisible() && textFontRow && textFontRow->isVisible() &&
                          textColorRow && textColorRow->isVisible(),
                      QStringLiteral("选中文字时显示正文 / 字体 / 颜色行"));
            ctx.Check(fillImageRow && !fillImageRow->isVisible(), QStringLiteral("选中文字时隐藏背景图片行"));
            ctx.Check(propertiesButton && propertiesButton->isVisible(), QStringLiteral("选中文字时显示图元属性按钮"));
            ctx.Check(smoothButton && !smoothButton->isVisible(), QStringLiteral("选中文字时隐藏简化曲线按钮"));

            // 图元属性按钮不是摆设：点击后打开属性窗，测试中立即关闭。
            bool propertyDialogOpened = false;
            QTimer::singleShot(0, [&propertyDialogOpened]() {
                if (auto* dialog = qobject_cast<ItemPropertiesDialog*>(QApplication::activeModalWidget())) {
                    propertyDialogOpened = true;
                    dialog->reject();
                }
            });
            if (propertiesButton) {
                propertiesButton->click();
                QApplication::processEvents();
            }
            ctx.Check(propertyDialogOpened, QStringLiteral("卡片图元属性按钮真正打开属性窗"));

            // 透明度不仅作用于图形和曲线，文字等所有图元都走同一属性。
            if (opacitySpin) {
                opacitySpin->setValue(40);
                QApplication::processEvents();
            }
            ctx.Check(qAbs(cardText->ItemOpacity() - 0.4) < 0.01, QStringLiteral("卡片透明度作用于文字图元"),
                      QString::number(cardText->ItemOpacity()));
            dockScene->UndoStack()->undo();
            QApplication::processEvents();
            ctx.Check(qAbs(cardText->ItemOpacity() - 1.0) < 0.01, QStringLiteral("撤销透明度恢复文字原值"),
                      QString::number(cardText->ItemOpacity()));

            auto* textEdit = propertiesDock->findChild<QLineEdit*>(QStringLiteral("ItemTextContentEdit"));
            if (textEdit) {
                textEdit->setText(QStringLiteral("改后文字"));
                QMetaObject::invokeMethod(textEdit, "editingFinished", Qt::DirectConnection);
                QApplication::processEvents();
            }
            ctx.Check(cardText->Text() == QStringLiteral("改后文字"), QStringLiteral("卡片正文直接修改文字"),
                      cardText->Text());
            dockScene->UndoStack()->undo();
            QApplication::processEvents();
            ctx.Check(cardText->Text() == QStringLiteral("卡片文字"), QStringLiteral("撤销正文恢复原文"),
                      cardText->Text());

            auto* fontSizeSpin = propertiesDock->findChild<QSpinBox*>(QStringLiteral("ItemTextFontSizeSpin"));
            if (fontSizeSpin) {
                fontSizeSpin->setValue(20);
                QApplication::processEvents();
            }
            ctx.Check(cardText->TextFont().pointSize() == 20, QStringLiteral("卡片字号直接修改文字字体"),
                      QString::number(cardText->TextFont().pointSize()));
            dockScene->UndoStack()->undo();
            QApplication::processEvents();
            ctx.Check(cardText->TextFont().pointSize() == 12, QStringLiteral("撤销字号恢复原字体"),
                      QString::number(cardText->TextFont().pointSize()));

            const QList<QToolButton*> textSwatches =
                propertiesDock->findChildren<QToolButton*>(QStringLiteral("ItemTextColorSwatch"));
            if (textSwatches.size() > 1) {
                textSwatches.at(1)->click();
                QApplication::processEvents();
            }
            ctx.Check(cardText->TextColor() == QColor(QStringLiteral("#EF4444")),
                      QStringLiteral("卡片文字颜色直接修改文字"), cardText->TextColor().name());
            dockScene->UndoStack()->undo();
            QApplication::processEvents();
            ctx.Check(cardText->TextColor() == QColor(QStringLiteral("#1E293B")),
                      QStringLiteral("撤销文字颜色恢复原色"), cardText->TextColor().name());

            auto* cardImage = CreateShapeItem(ShapeType::RECTANGLE);
            QPixmap imagePixmap(32, 24);
            imagePixmap.fill(QColor(QStringLiteral("#2563EB")));
            cardImage->SetFillImage(imagePixmap, ImageFill::Mode::STRETCH);
            cardImage->SetContentSize(QSizeF(32, 24));
            cardImage->setPos(QPointF(620.0, 430.0));
            dockScene->AddItemsWithUndo({cardImage}, QStringLiteral("Add card image"));
            dockScene->clearSelection();
            cardImage->setSelected(true);
            QApplication::processEvents();
            ctx.Check(fillImageRow && fillImageRow->isVisible(), QStringLiteral("选中带背景图的矩形时显示背景图片行"));
            ctx.Check(textContentRow && !textContentRow->isVisible() && textFontRow && !textFontRow->isVisible() &&
                          textColorRow && !textColorRow->isVisible(),
                      QStringLiteral("选中图形时隐藏文字行"));

            // 线条类图元不参与合并：选中直线时合并图元入口隐藏。
            auto* cardLine = new StraightLineItem();
            cardLine->SetEndpoints(QPointF(620.0, 470.0), QPointF(710.0, 480.0), QColor(QStringLiteral("#334155")),
                                   3.0);
            dockScene->AddItemsWithUndo({cardLine}, QStringLiteral("Add card line"));
            dockScene->clearSelection();
            cardLine->setSelected(true);
            QApplication::processEvents();
            ctx.Check(!morphButtons.isEmpty() && !morphButtons.first()->isVisible(),
                      QStringLiteral("选中线条时隐藏合并图元入口"));

            // 只选曲线时显示简化曲线按钮，点击后平滑结果进入撤销栈。
            auto* smoothStroke = new CurveItem();
            QVector<QPointF> jitterPoints;
            for (int i = 0; i <= 8; ++i) {
                jitterPoints.append(QPointF(520.0 + i * 6.0, 470.0 + (i % 2) * 3.0));
            }
            smoothStroke->SetStrokePoints(jitterPoints, QColor(QStringLiteral("#334155")), 5.0);
            dockScene->AddItemsWithUndo({smoothStroke}, QStringLiteral("Add smooth stroke"));
            dockScene->clearSelection();
            QApplication::processEvents();
            QWidget* fillStyleRowWhenEmpty = propertiesDock->findChild<QWidget*>(QStringLiteral("ItemFillStyleRow"));
            QWidget* borderWidthRowWhenEmpty =
                propertiesDock->findChild<QWidget*>(QStringLiteral("ItemBorderWidthRow"));
            ctx.Check(fillStyleRowWhenEmpty && fillStyleRowWhenEmpty->isHidden() && borderWidthRowWhenEmpty &&
                          borderWidthRowWhenEmpty->isHidden(),
                      QStringLiteral("无图元选中时隐藏图元编辑选项"));
            smoothStroke->setSelected(true);
            QApplication::processEvents();
            auto* smoothingSpin = propertiesDock->window()->findChild<QSpinBox*>(QStringLiteral("BrushSmoothingSpin"));
            if (smoothingSpin) {
                smoothingSpin->setValue(10.0);
                QApplication::processEvents();
            }
            ctx.Check(smoothButton && smoothButton->isVisible(), QStringLiteral("选中曲线时显示简化曲线按钮"));
            const int undoIndexBeforeSmooth = dockScene->UndoStack()->index();
            if (smoothButton) {
                smoothButton->click();
                QApplication::processEvents();
            }
            ctx.Check(dockScene->UndoStack()->index() > undoIndexBeforeSmooth,
                      QStringLiteral("卡片简化曲线按钮真正执行简化且可撤销"),
                      QStringLiteral("index=%1->%2").arg(undoIndexBeforeSmooth).arg(dockScene->UndoStack()->index()));

            // 悬浮卡片跟随选中状态：清空选中后隐藏，重新选中再出现。
            propertiesDock->setFloating(true);
            QApplication::processEvents();
            dockScene->clearSelection();
            QApplication::processEvents();
            ctx.Check(!propertiesDock->isVisible(), QStringLiteral("悬浮时清空选中隐藏属性编辑面板"));
            selectedShape->setSelected(true);
            QApplication::processEvents();
            ctx.Check(propertiesDock->isVisible(), QStringLiteral("悬浮时有选中显示属性编辑面板"));

            // 拖回主窗口停靠后卡片常驻，不再随选中状态整张显隐。
            propertiesDock->setFloating(false);
            QApplication::processEvents();
            ctx.Check(!propertiesDock->isFloating() && propertiesDock->isVisible(),
                      QStringLiteral("拖回主窗口后停靠并保持显示"));
            dockScene->clearSelection();
            QApplication::processEvents();
            ctx.Check(propertiesDock->isVisible(), QStringLiteral("停靠时清空选中不隐藏属性编辑面板"));
            ctx.Check(layerDock && layerDock->height() < dockWindow.height() - 100,
                      QStringLiteral("属性编辑面板停靠时图层卡不撑满窗口高度"));
        }

        for (const QString& stale : QDir(dockShapesDir).entryList({QStringLiteral("*.sbcanvas")}, QDir::Files)) {
            QFile::remove(QDir(dockShapesDir).filePath(stale));
        }
        for (const QString& staleDir : {QStringLiteral("空目录"), QStringLiteral("坏文件"), QStringLiteral("方向键")}) {
            QDir(QDir(dockShapesDir).filePath(staleDir)).removeRecursively();
        }
        qunsetenv("STORYBOARD_CONFIG");
    }
}
