#include "Alignment.h"
#include "BoolOps.h"
#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CanvasView.h"
#include "Commands.h"
#include "CompositeShapeItem.h"
#include "CustomShapeTemplates.h"
#include "ExportDialog.h"
#include "I18n.h"
#include "ImageItem.h"
#include "ItemDockPanel.h"
#include "ItemPropertiesDialog.h"
#include "LayerPanel.h"
#include "LineArtConverter.h"
#include "LineArtDialog.h"
#include "LineArtPresets.h"
#include "MainWindow.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "ShapeItem.h"
#include "StrokeItem.h"
#include "TextItem.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"
#include "selftest/SelftestHelpers.h"

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
#include <QLabel>
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


void selftest::runMainWindowShapeMenuTests(Context& ctx)
{
    const QList<ShapeType> shapeTypes = allShapeTypes();

    const QString shapeCanvasPath = QDir(ctx.workDirectory).filePath(QStringLiteral("shapes.sbcanvas"));
    {
        QFile shapeCanvasFile(shapeCanvasPath);
        ctx.check(shapeCanvasFile.open(QIODevice::WriteOnly), QStringLiteral("创建图形画布测试文件"));
        shapeCanvasFile.write(QJsonDocument(ctx.shapeRoot).toJson(QJsonDocument::Indented));
    }

    {
        MainWindow shapeWindow;
        shapeWindow.resize(1082, 800);
        shapeWindow.show();
        QApplication::processEvents();

        QMenu* shapeMenu = shapeWindow.findChild<QMenu*>(QStringLiteral("ShapeMenu"));
        int builtInShapeActions = 0;
        if (shapeMenu) {
            for (QAction* action : shapeMenu->actions()) {
                if (action->objectName().startsWith(QStringLiteral("ShapeAction_"))) {
                    ++builtInShapeActions;
                }
            }
        }
        QMenu* customShapeMenu = shapeWindow.findChild<QMenu*>(QStringLiteral("CustomShapeMenu"));
        ctx.check(shapeMenu && builtInShapeActions == shapeTypes.size() && customShapeMenu &&
                      shapeMenu->actions().contains(customShapeMenu->menuAction()),
                  QStringLiteral("图形菜单包含全部图形并在下方追加自定义图形子菜单"),
                  QStringLiteral("actions=%1 builtIn=%2 custom=%3")
                      .arg(shapeMenu ? shapeMenu->actions().size() : 0)
                      .arg(builtInShapeActions)
                      .arg(customShapeMenu ? 1 : 0));
        // 工具栏在原「图形」入口处把常见图形直接平铺成一排，复用菜单里的同一批动作。
        QToolButton* shapeButton = shapeWindow.findChild<QToolButton*>(QStringLiteral("ShapeButton"));
        QList<QToolButton*> toolbarShapeButtons =
            shapeWindow.findChildren<QToolButton*>(QStringLiteral("ToolBarShapeButton"));
        bool shapeButtonsShareMenu = toolbarShapeButtons.size() == builtInShapeActions && shapeMenu;
        for (QToolButton* button : std::as_const(toolbarShapeButtons)) {
            const QString actionName = button->property("shapeActionName").toString();
            QAction* boundAction = actionName.isEmpty() ? nullptr : shapeWindow.findChild<QAction*>(actionName);
            if (!boundAction || !shapeMenu || !shapeMenu->actions().contains(boundAction)) {
                shapeButtonsShareMenu = false;
                break;
            }
        }
        ctx.check(shapeButtonsShareMenu && !shapeButton, QStringLiteral("主工具栏把常见图形展开成一排按钮"),
                  QStringLiteral("buttons=%1 builtIn=%2").arg(toolbarShapeButtons.size()).arg(builtInShapeActions));

        QToolBar* mainToolBar = shapeWindow.findChild<QToolBar*>(QStringLiteral("MainToolBar"));
        QAction* exportAction = nullptr;
        for (QAction* action : shapeWindow.findChildren<QAction*>()) {
            if (action->text() == QStringLiteral("导出画布…")) {
                exportAction = action;
                break;
            }
        }
        QToolButton* exportButton = mainToolBar && exportAction
                                        ? qobject_cast<QToolButton*>(mainToolBar->widgetForAction(exportAction))
                                        : nullptr;
        QToolButton* alignButton = shapeWindow.findChild<QToolButton*>(QStringLiteral("AlignToolButton"));
        QToolButton* morphButton = shapeWindow.findChild<QToolButton*>(QStringLiteral("BoolToolButton"));
        ctx.check(exportButton && exportButton->isVisible() && exportButton->width() > 0 && exportAction &&
                      !exportAction->icon().isNull(),
                  QStringLiteral("工具栏导出按钮正常显示"),
                  QStringLiteral("visible=%1 width=%2 icon=%3")
                      .arg(exportButton && exportButton->isVisible() ? 1 : 0)
                      .arg(exportButton ? exportButton->width() : 0)
                      .arg(exportAction && !exportAction->icon().isNull() ? 1 : 0));
        ctx.check(alignButton && alignButton->isVisible() && alignButton->width() > 0 && !alignButton->icon().isNull(),
                  QStringLiteral("工具栏对齐下拉按钮正常显示"),
                  QStringLiteral("visible=%1 width=%2 icon=%3")
                      .arg(alignButton && alignButton->isVisible() ? 1 : 0)
                      .arg(alignButton ? alignButton->width() : 0)
                      .arg(alignButton && !alignButton->icon().isNull() ? 1 : 0));
        ctx.check(morphButton && morphButton->isVisible() && morphButton->width() > 0 && !morphButton->icon().isNull(),
                  QStringLiteral("工具栏化形下拉按钮正常显示"),
                  QStringLiteral("visible=%1 width=%2 icon=%3")
                      .arg(morphButton && morphButton->isVisible() ? 1 : 0)
                      .arg(morphButton ? morphButton->width() : 0)
                      .arg(morphButton && !morphButton->icon().isNull() ? 1 : 0));

        QToolButton* customShapeButton =
            shapeWindow.findChild<QToolButton*>(QStringLiteral("ToolBarCustomShapeButton"));
        ctx.check(customShapeButton && customShapeButton->menu() == customShapeMenu && customShapeButton->isVisible(),
                  QStringLiteral("常见图形后追加自定义图形下拉按钮"),
                  QStringLiteral("button=%1 menu=%2 visible=%3")
                      .arg(customShapeButton ? 1 : 0)
                      .arg(customShapeButton && customShapeButton->menu() == customShapeMenu ? 1 : 0)
                      .arg(customShapeButton && customShapeButton->isVisible() ? 1 : 0));

        int toolbarSeparators = 0;
        bool hasShapeTextLabel = false;
        if (mainToolBar) {
            for (QAction* action : mainToolBar->actions()) {
                if (action && action->isSeparator()) {
                    ++toolbarSeparators;
                }
            }
            for (QLabel* label : mainToolBar->findChildren<QLabel*>()) {
                if (label->text() == QStringLiteral("图形")) {
                    hasShapeTextLabel = true;
                    break;
                }
            }
        }
        ctx.check(mainToolBar && toolbarSeparators <= 2 && !hasShapeTextLabel,
                  QStringLiteral("工具栏没有多余的图形文字和分隔符"),
                  QStringLiteral("separators=%1 shapeLabel=%2").arg(toolbarSeparators).arg(hasShapeTextLabel ? 1 : 0));

        bool hasRemovedToolbarEntry = false;
        if (mainToolBar) {
            for (QAction* action : mainToolBar->actions()) {
                if (!action || action->isSeparator()) {
                    continue;
                }
                const QString text = action->text();
                if (text == QStringLiteral("撤销") || text == QStringLiteral("重做") ||
                    text == QStringLiteral("放大") || text == QStringLiteral("缩小") ||
                    text == QStringLiteral("适应画布") || text == QStringLiteral("替换背景图片…") ||
                    text == QStringLiteral("设置…")) {
                    hasRemovedToolbarEntry = true;
                    break;
                }
            }
        }
        ctx.check(mainToolBar && !hasRemovedToolbarEntry,
                  QStringLiteral("主工具栏不再保留撤销 / 重做 / 缩放 / 背景 / 设置按钮"));

        // 需求：辅助网格除了设置里勾选，还要能从视图菜单直接开关，勾选状态与画布一致。
        QAction* gridAction = shapeWindow.findChild<QAction*>(QStringLiteral("GridToggleAction"));
        CanvasScene* gridScene = shapeWindow.scene();
        ctx.check(gridAction && gridAction->isCheckable() && gridScene &&
                      gridAction->isChecked() == gridScene->showGrid(),
                  QStringLiteral("视图菜单提供辅助网格开关"),
                  QStringLiteral("checked=%1 scene=%2")
                      .arg(gridAction && gridAction->isChecked() ? 1 : 0)
                      .arg(gridScene && gridScene->showGrid() ? 1 : 0));
        if (gridAction) {
            // 菜单项文案必须是中文，不能因为上下文/译文缺失而露出英文。
            ctx.check(gridAction->text() == QStringLiteral("辅助网格"), QStringLiteral("辅助网格菜单项显示中文"),
                      gridAction->text());
        }
        if (gridAction && gridScene) {
            const bool gridBefore = gridScene->showGrid();
            gridAction->setChecked(!gridBefore);
            ctx.check(gridScene->showGrid() == !gridBefore, QStringLiteral("辅助网格开关即时生效"),
                      QStringLiteral("scene=%1").arg(gridScene->showGrid() ? 1 : 0));
            gridAction->setChecked(gridBefore);
        }

        QAction* shortcutsAction = nullptr;
        for (QAction* action : shapeWindow.findChildren<QAction*>()) {
            if (action->text() == QStringLiteral("快捷键一览…")) {
                shortcutsAction = action;
            }
        }
        ctx.check(shortcutsAction != nullptr, QStringLiteral("帮助菜单提供快捷键一览入口"));

        QToolButton* fillButton = shapeWindow.findChild<QToolButton*>(QStringLiteral("FillButton"));
        QStringList fillActionTexts;
        if (fillButton && fillButton->menu()) {
            for (QAction* action : fillButton->menu()->actions()) {
                if (!action->isSeparator()) {
                    fillActionTexts.append(action->text());
                }
            }
        }
        ctx.check(fillActionTexts.size() == 7 && fillActionTexts.contains(QStringLiteral("反斜线填充")) &&
                      fillActionTexts.contains(QStringLiteral("正交叉线填充")) &&
                      fillActionTexts.contains(QStringLiteral("六边形填充")),
                  QStringLiteral("填充菜单只保留填充样式"), fillActionTexts.join(QStringLiteral("|")));

        QSpinBox* toolbarSmoothing = shapeWindow.findChild<QSpinBox*>(QStringLiteral("BrushSmoothingSpin"));
        CanvasView* canvasView = shapeWindow.findChild<CanvasView*>();
        ctx.check(toolbarSmoothing && canvasView && toolbarSmoothing->minimum() == 0 &&
                      toolbarSmoothing->maximum() == 30 && toolbarSmoothing->value() == canvasView->strokeSmoothing(),
                  QStringLiteral("画笔工具栏提供可调容差"),
                  QStringLiteral("value=%1 view=%2")
                      .arg(toolbarSmoothing ? toolbarSmoothing->value() : -1)
                      .arg(canvasView ? canvasView->strokeSmoothing() : -1));
        if (toolbarSmoothing) {
            toolbarSmoothing->setValue(0);
            ctx.check(canvasView && canvasView->strokeSmoothing() == 0, QStringLiteral("工具栏容差即时生效"),
                      QStringLiteral("view=%1").arg(canvasView ? canvasView->strokeSmoothing() : -1));
            toolbarSmoothing->setValue(4);
        }
        QToolButton* smoothButton = shapeWindow.findChild<QToolButton*>(QStringLiteral("SmoothStrokeButton"));
        ctx.check(smoothButton && smoothButton->defaultAction() &&
                      smoothButton->defaultAction()->text() == QStringLiteral("平滑笔画"),
                  QStringLiteral("画笔工具栏提供手动平滑按钮"));
        QAction* smoothAction = nullptr;
        for (QAction* action : shapeWindow.findChildren<QAction*>()) {
            if (action->text() == QStringLiteral("平滑笔画")) {
                smoothAction = action;
            }
        }
        ctx.check(smoothAction && smoothAction->shortcut() == QKeySequence(QStringLiteral("Ctrl+Shift+M")),
                  QStringLiteral("编辑菜单提供手动平滑入口与快捷键"));
        QToolButton* iconFillButton = shapeWindow.findChild<QToolButton*>(QStringLiteral("FillButton"));
        QToolButton* iconFillColorButton = shapeWindow.findChild<QToolButton*>(QStringLiteral("FillColorButton"));
        QToolButton* iconStrokeColorButton = shapeWindow.findChild<QToolButton*>(QStringLiteral("StrokeColorButton"));
        const QImage fillStyleImage = iconFillButton ? iconFillButton->icon().pixmap(20, 20).toImage() : QImage();
        const QImage fillColorImage =
            iconFillColorButton ? iconFillColorButton->icon().pixmap(20, 20).toImage() : QImage();
        const QImage strokeColorImage =
            iconStrokeColorButton ? iconStrokeColorButton->icon().pixmap(20, 20).toImage() : QImage();
        ctx.check(!fillStyleImage.isNull() && !fillColorImage.isNull() && !strokeColorImage.isNull() &&
                      fillStyleImage != fillColorImage && fillColorImage != strokeColorImage,
                  QStringLiteral("填充样式、填充颜色与边框颜色使用彼此不同的图标"));

        // 画笔颜色按钮与画笔宽度按钮已从工具栏移除；描线宽度改挂在描线颜色按钮之后。
        QToolButton* removedBrushColorButton = shapeWindow.findChild<QToolButton*>(QStringLiteral("ColorButton"));
        QAction* removedBrushColorAction = shapeWindow.findChild<QAction*>(QStringLiteral("BrushColorAction"));
        QComboBox* strokeWidthCombo = shapeWindow.findChild<QComboBox*>(QStringLiteral("BrushWidthCombo"));
        const int strokeButtonX =
            iconStrokeColorButton ? iconStrokeColorButton->mapTo(&shapeWindow, QPoint(0, 0)).x() : -1;
        const int widthComboX = strokeWidthCombo ? strokeWidthCombo->mapTo(&shapeWindow, QPoint(0, 0)).x() : -1;
        ctx.check(!removedBrushColorButton && !removedBrushColorAction && strokeWidthCombo && iconStrokeColorButton &&
                      widthComboX > strokeButtonX,
                  QStringLiteral("工具栏去掉画笔颜色按钮并在描线按钮后追加描线宽度下拉"),
                  QStringLiteral("color=%1 action=%2 comboX=%3 strokeX=%4")
                      .arg(removedBrushColorButton ? 1 : 0)
                      .arg(removedBrushColorAction ? 1 : 0)
                      .arg(widthComboX)
                      .arg(strokeButtonX));

        QMenu* brushWidthMenu = shapeWindow.findChild<QMenu*>(QStringLiteral("BrushWidthMenu"));
        QComboBox* widthCombo = shapeWindow.findChild<QComboBox*>(QStringLiteral("BrushWidthCombo"));
        ctx.check(brushWidthMenu && brushWidthMenu->actions().size() == 15, QStringLiteral("画笔样式菜单提供粗细选项"),
                  QStringLiteral("actions=%1").arg(brushWidthMenu ? brushWidthMenu->actions().size() : 0));
        ctx.check(brushWidthMenu && widthCombo && brushWidthMenu->title() == QStringLiteral("描线宽度") &&
                      widthCombo->toolTip() == QStringLiteral("描线宽度"),
                  QStringLiteral("菜单与工具栏统一使用描线宽度"),
                  QStringLiteral("menu=%1 combo=%2")
                      .arg(brushWidthMenu ? brushWidthMenu->title() : QStringLiteral("-"))
                      .arg(widthCombo ? widthCombo->toolTip() : QStringLiteral("-")));
        if (brushWidthMenu && widthCombo && !brushWidthMenu->actions().isEmpty()) {
            const int oldWidth = widthCombo->currentData().toInt();
            QAction* twoPixel = nullptr;
            for (QAction* action : brushWidthMenu->actions()) {
                if (action->data().toInt() == 2) {
                    twoPixel = action;
                    break;
                }
            }
            ctx.check(twoPixel && twoPixel->isCheckable(), QStringLiteral("画笔粗细菜单使用互斥可勾选选项"),
                      QStringLiteral("current=%1").arg(oldWidth));
            if (twoPixel) {
                twoPixel->trigger();
                QApplication::processEvents();
                ctx.check(widthCombo->currentData().toInt() == 2 && twoPixel->isChecked(),
                          QStringLiteral("画笔粗细菜单选项同步工具栏下拉"));
                QAction* restore = nullptr;
                for (QAction* action : brushWidthMenu->actions()) {
                    if (action->data().toInt() == oldWidth) {
                        restore = action;
                        break;
                    }
                }
                if (restore) {
                    restore->trigger();
                }
                QApplication::processEvents();
            }
        }

        // 一级「画笔」菜单已删除：描线宽度子菜单与平滑笔画并到「插入」的描线颜色之后，
        // 重复的画笔颜色项不再保留。
        QMenu* brushMenu = shapeWindow.findChild<QMenu*>(QStringLiteral("BrushMenu"));
        QMenu* fillStyleMenu = shapeWindow.findChild<QMenu*>(QStringLiteral("FillStyleMenu"));
        QMenu* insertMenuForOrder = shapeWindow.findChild<QMenu*>(QStringLiteral("InsertMenu"));
        QList<QAction*> insertActionsForOrder;
        if (insertMenuForOrder) {
            insertActionsForOrder = insertMenuForOrder->actions();
        }
        const int strokeColorIndex =
            insertActionsForOrder.indexOf(shapeWindow.findChild<QAction*>(QStringLiteral("BorderColorAction")));
        const int widthMenuIndex =
            insertActionsForOrder.indexOf(brushWidthMenu ? brushWidthMenu->menuAction() : nullptr);
        int smoothActionIndex = -1;
        for (int i = 0; i < insertActionsForOrder.size(); ++i) {
            if (insertActionsForOrder.at(i) && insertActionsForOrder.at(i)->text() == QStringLiteral("平滑笔画")) {
                smoothActionIndex = i;
                break;
            }
        }
        bool repeatedBrushColor = false;
        for (QAction* action : std::as_const(insertActionsForOrder)) {
            if (action && action->objectName() == QStringLiteral("BrushColorAction")) {
                repeatedBrushColor = true;
                break;
            }
        }
        ctx.check(!brushMenu && strokeColorIndex >= 0 && widthMenuIndex > strokeColorIndex &&
                      smoothActionIndex > widthMenuIndex && !repeatedBrushColor,
                  QStringLiteral("描线宽度与平滑笔画移到插入菜单描线颜色之后"),
                  QStringLiteral("brush=%1 stroke=%2 width=%3 smooth=%4 repeat=%5")
                      .arg(brushMenu ? 1 : 0)
                      .arg(strokeColorIndex)
                      .arg(widthMenuIndex)
                      .arg(smoothActionIndex)
                      .arg(repeatedBrushColor ? 1 : 0));
        bool styleMenuGone = shapeWindow.findChild<QMenu*>(QStringLiteral("StyleMenu")) == nullptr &&
                             shapeWindow.findChild<QMenu*>(QStringLiteral("BrushStyleMenu")) == nullptr;
        ctx.check(styleMenuGone, QStringLiteral("样式菜单与画笔样式子菜单不再存在"));

        QList<QAction*> fillActions;
        if (fillStyleMenu) {
            fillActions = fillStyleMenu->actions();
        }
        bool fillHasColor = false;
        int fillMenuItemCount = 0;
        for (QAction* action : fillActions) {
            if (action->isSeparator()) {
                continue;
            }
            ++fillMenuItemCount;
            if (action->objectName() == QStringLiteral("FillColorAction")) {
                fillHasColor = true;
            }
        }
        ctx.check(fillStyleMenu && fillMenuItemCount == 7 && !fillHasColor,
                  QStringLiteral("填充样式子菜单只保留填充样式"), QStringLiteral("actions=%1").arg(fillMenuItemCount));

        QMenu* insertMenu = nullptr;
        for (QAction* action : shapeWindow.menuBar()->actions()) {
            QMenu* menu = action ? action->menu() : nullptr;
            if (menu && menu->objectName() == QStringLiteral("InsertMenu")) {
                insertMenu = menu;
            }
        }
        QList<QAction*> insertActions;
        if (insertMenu) {
            insertActions = insertMenu->actions();
        }
        {
            // 需求：插入图片只归插入菜单，菜单栏里不再重复。
            QStringList insertEntries;
            for (QAction* action : insertActions) {
                if (action && !action->isSeparator()) {
                    insertEntries.append(action->text());
                }
            }
            ctx.check(insertEntries.contains(QStringLiteral("插入图片…")), QStringLiteral("插入菜单提供插入图片"),
                      insertEntries.join(QStringLiteral(" / ")));
            // 需求：原画布菜单的背景项已并入插入菜单。
            ctx.check(insertEntries.contains(QStringLiteral("替换背景图片…")) &&
                          insertEntries.contains(QStringLiteral("设置背景颜色…")) &&
                          insertEntries.contains(QStringLiteral("清除背景")) &&
                          insertEntries.contains(QStringLiteral("背景填充方式")),
                      QStringLiteral("插入菜单包含原画布菜单的背景项"), insertEntries.join(QStringLiteral(" / ")));
        }
        int fillStyleMenuIndex = insertActions.indexOf(fillStyleMenu ? fillStyleMenu->menuAction() : nullptr);
        int shapeMenuIndex = insertActions.indexOf(shapeMenu ? shapeMenu->menuAction() : nullptr);
        ctx.check(fillStyleMenuIndex >= 0 && shapeMenuIndex >= 0,
                  QStringLiteral("插入菜单包含常见图形与填充样式两个子菜单"),
                  QStringLiteral("shape=%1 fillStyle=%2").arg(shapeMenuIndex).arg(fillStyleMenuIndex));

        QAction* outlineAction = shapeWindow.findChild<QAction*>(QStringLiteral("BorderColorAction"));
        ctx.check(outlineAction && outlineAction->text() == QStringLiteral("描线颜色…"),
                  QStringLiteral("主窗口提供描线颜色动作"));

        QAction* ellipseAction =
            shapeMenu ? shapeMenu->findChild<QAction*>(QStringLiteral("ShapeAction_ellipse")) : nullptr;
        if (ellipseAction) {
            ellipseAction->trigger();
            QApplication::processEvents();

            ctx.check(shapeWindow.scene()->canvasItemCount() == 1, QStringLiteral("快速插入图形到当前画板"));
            ShapeItem* inserted = shapeWindow.scene()->canvasItemCount() == 1
                                      ? dynamic_cast<ShapeItem*>(shapeWindow.scene()->canvasItems().constFirst())
                                      : nullptr;
            ctx.check(inserted && inserted->shapeType() == ShapeType::Ellipse,
                      QStringLiteral("快速插入的图元类型为圆形"));
            ctx.check(inserted && inserted->isSelected() && shapeWindow.view()->mode() == ToolMode::Edit,
                      QStringLiteral("快速插入后自动进入编辑模式并选中图形"));
            ctx.check(inserted && inserted->layerId() == shapeWindow.scene()->activeLayerId() &&
                          shapeWindow.scene()->layerHeightsUnique(inserted->layerId()),
                      QStringLiteral("快速插入图形进入当前图层且高度唯一"));
            ctx.check(inserted && shapeWindow.scene()->canvasRect().contains(inserted->sceneContentRect()),
                      QStringLiteral("快速插入图形位于画板范围内"),
                      inserted ? geometryText(inserted->pos(), inserted->contentSize()) : QString());
            ctx.check(inserted && inserted->fillColor().alpha() > 0 && inserted->strokeWidth() > 0.0,
                      QStringLiteral("快速插入图形带有默认描边与填充"));
        }

        QAction* arrowInsertAction =
            shapeMenu ? shapeMenu->findChild<QAction*>(QStringLiteral("ShapeAction_arrow")) : nullptr;
        if (arrowInsertAction) {
            arrowInsertAction->trigger();
            QApplication::processEvents();
            ShapeItem* insertedArrow = nullptr;
            for (CanvasItem* item : shapeWindow.scene()->canvasItems()) {
                if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
                    if (shape->shapeType() == ShapeType::Arrow) {
                        insertedArrow = shape;
                    }
                }
            }
            ctx.check(insertedArrow != nullptr, QStringLiteral("快速插入的图元类型为箭头"));
            ctx.check(insertedArrow && insertedArrow->fillStyle() != FillStyle::None &&
                          insertedArrow->fillColor().alpha() > 0,
                      QStringLiteral("新插入箭头默认填充"));
        }
    }

    {
        MainWindow shapeCanvasWindow;
        ctx.check(shapeCanvasWindow.openCanvasFile(shapeCanvasPath), QStringLiteral("主窗口打开含图形的画布"));
        ctx.check(shapeCanvasWindow.scene()->canvasItemCount() == shapeTypes.size(),
                  QStringLiteral("主窗口载入含图形画布完整内容"));
        int loadedShapeCount = 0;
        for (CanvasItem* item : shapeCanvasWindow.scene()->canvasItems()) {
            if (dynamic_cast<ShapeItem*>(item)) {
                ++loadedShapeCount;
            }
        }
        ctx.check(loadedShapeCount == shapeTypes.size(), QStringLiteral("主窗口识别画布中的图形图元"));
    }

    const QString demoPath = demoCanvasPath();
    if (QFileInfo::exists(demoPath)) {
        MainWindow canvasWindow;
        ctx.check(canvasWindow.openCanvasFile(demoPath), QStringLiteral("主窗口打开含文字的示例画布"));
        ctx.check(canvasWindow.scene()->canvasItemCount() == 27 && canvasWindow.scene()->userLayerCount() == 3,
                  QStringLiteral("主窗口载入示例画布完整内容"));
        bool hasTextItem = false;
        for (CanvasItem* item : canvasWindow.scene()->canvasItems()) {
            if (dynamic_cast<TextItem*>(item)) {
                hasTextItem = true;
                break;
            }
        }
        ctx.check(hasTextItem, QStringLiteral("主窗口载入画布中的文字图元"));
    }
}
