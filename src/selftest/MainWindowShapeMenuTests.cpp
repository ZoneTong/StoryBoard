#include "Alignment.h"
#include "BoolOps.h"
#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CanvasView.h"
#include "Commands.h"
#include "CompositeShapeItem.h"
#include "CurveItem.h"
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
#include "StyleController.h"
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
        // 需求 1/6：预置项（45°/60° 直角三角、直线箭头）紧随其基础图形之后，且不计入内建图形数量。
        QAction* lineAction = shapeWindow.findChild<QAction*>(QStringLiteral("ShapeAction_line"));
        QAction* triangleAction = shapeWindow.findChild<QAction*>(QStringLiteral("ShapeAction_triangle"));
        QAction* lineArrowPreset = shapeWindow.findChild<QAction*>(QStringLiteral("ShapePreset_lineArrow"));
        QAction* triangle45Preset = shapeWindow.findChild<QAction*>(QStringLiteral("ShapePreset_triangle45"));
        QAction* triangle60Preset = shapeWindow.findChild<QAction*>(QStringLiteral("ShapePreset_triangle60"));
        bool presetOrderOk =
            shapeMenu && lineAction && triangleAction && lineArrowPreset && triangle45Preset && triangle60Preset;
        if (presetOrderOk) {
            const QList<QAction*> menuActions = shapeMenu->actions();
            const int lineIndex = menuActions.indexOf(lineAction);
            const int triangleIndex = menuActions.indexOf(triangleAction);
            presetOrderOk = menuActions.indexOf(lineArrowPreset) == lineIndex + 1 &&
                            menuActions.indexOf(triangle45Preset) == triangleIndex + 1 &&
                            menuActions.indexOf(triangle60Preset) == triangleIndex + 2;
        }
        ctx.check(presetOrderOk, QStringLiteral("图形菜单内预置项紧跟基础图形（三角形 45°/60°、直线箭头）"));

        QMenu* customShapeMenu = shapeWindow.findChild<QMenu*>(QStringLiteral("CustomShapeMenu"));
        QAction* customTrigger = shapeWindow.findChild<QAction*>(QStringLiteral("CustomShapePanelTrigger"));
        // 自定义图形不再是二级子菜单：入口直接平铺在「常见图形」末尾。
        ctx.check(shapeMenu && builtInShapeActions == shapeTypes.size() && customShapeMenu && customTrigger &&
                      shapeMenu->actions().contains(customTrigger) &&
                      !shapeMenu->actions().contains(customShapeMenu->menuAction()),
                  QStringLiteral("图形菜单包含全部图形，自定义图形入口与常见图形同层"),
                  QStringLiteral("actions=%1 builtIn=%2 custom=%3")
                      .arg(shapeMenu ? shapeMenu->actions().size() : 0)
                      .arg(builtInShapeActions)
                      .arg(customTrigger ? 1 : 0));
        // 工具栏在原「图形」入口处把常见图形与预置项直接平铺成一排，复用菜单里的同一批动作。
        // 预置项：圆角矩形、直线箭头、45°/60° 三角、左右花括号（需求 13 花括号改曲线后仍是 2 枚按钮）。
        const int toolbarExpected = builtInShapeActions + 6;
        QToolButton* shapeButton = shapeWindow.findChild<QToolButton*>(QStringLiteral("ShapeButton"));
        QList<QToolButton*> toolbarShapeButtons =
            shapeWindow.findChildren<QToolButton*>(QStringLiteral("ToolBarShapeButton"));
        bool shapeButtonsShareMenu = toolbarShapeButtons.size() == toolbarExpected && shapeMenu;
        for (QToolButton* button : std::as_const(toolbarShapeButtons)) {
            const QString actionName = button->property("shapeActionName").toString();
            QAction* boundAction = actionName.isEmpty() ? nullptr : shapeWindow.findChild<QAction*>(actionName);
            if (!boundAction || !shapeMenu || !shapeMenu->actions().contains(boundAction)) {
                shapeButtonsShareMenu = false;
                break;
            }
        }
        ctx.check(shapeButtonsShareMenu && !shapeButton, QStringLiteral("主工具栏把常见图形与预置项展开成一排按钮"),
                  QStringLiteral("buttons=%1 expected=%2").arg(toolbarShapeButtons.size()).arg(toolbarExpected));

        // 工具栏顺序是产品口径：气泡/扇形/弧线排在双向箭头之后，直线与花括号收尾。
        const QStringList expectedToolBarOrder = {
            QStringLiteral("ShapeAction_rectangle"),    QStringLiteral("ShapePreset_roundedRect"),
            QStringLiteral("ShapeAction_ellipse"),      QStringLiteral("ShapeAction_triangle"),
            QStringLiteral("ShapePreset_triangle45"),   QStringLiteral("ShapePreset_triangle60"),
            QStringLiteral("ShapeAction_diamond"),      QStringLiteral("ShapeAction_star"),
            QStringLiteral("ShapeAction_star4"),        QStringLiteral("ShapeAction_hexagram"),
            QStringLiteral("ShapeAction_heart"),        QStringLiteral("ShapeAction_arrow"),
            QStringLiteral("ShapeAction_double_arrow"), QStringLiteral("ShapeAction_callout"),
            QStringLiteral("ShapeAction_sector"),       QStringLiteral("ShapeAction_arc"),
            QStringLiteral("ShapeAction_sine_wave"),    QStringLiteral("ShapeAction_flag_wave"),
            QStringLiteral("ShapeAction_scroll"),       QStringLiteral("ShapeAction_cube"),
            QStringLiteral("ShapeAction_cylinder"),     QStringLiteral("ShapeAction_line"),
            QStringLiteral("ShapePreset_lineArrow"),    QStringLiteral("ShapePreset_braceLeft"),
            QStringLiteral("ShapePreset_braceRight"),
        };
        QStringList actualToolBarOrder;
        for (QToolButton* button : std::as_const(toolbarShapeButtons)) {
            actualToolBarOrder.append(button->property("shapeActionName").toString());
        }
        ctx.check(actualToolBarOrder == expectedToolBarOrder,
                  QStringLiteral("工具栏图形顺序：新图形插在弧线之后，直线与花括号在末尾"),
                  actualToolBarOrder.join(QLatin1Char(',')));

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
        ctx.check(!alignButton && !morphButton, QStringLiteral("工具栏不再显示对齐和合并图元按钮"),
                  QStringLiteral("align=%1 morph=%2").arg(alignButton ? 1 : 0).arg(morphButton ? 1 : 0));

        auto* toleranceSpin = shapeWindow.findChild<QDoubleSpinBox*>(QStringLiteral("BrushSmoothingSpin"));
        CanvasView* toleranceView = shapeWindow.findChild<CanvasView*>();
        ctx.check(toleranceSpin && toleranceSpin->minimum() == 0.0 && toleranceSpin->maximum() == 20.0 &&
                      qAbs(toleranceSpin->singleStep() - 0.1) < 0.001 && toleranceSpin->decimals() == 1 &&
                      toleranceView && qAbs(toleranceSpin->value() - toleranceView->strokeSmoothing()) < 0.001 &&
                      toleranceSpin->toolTip().contains(QStringLiteral("画笔")),
                  QStringLiteral("工具栏拟合容差为 0–20、0.1 步进、与画布同步且提示含画笔"),
                  QStringLiteral("spin=%1 range=%2-%3 step=%4 value=%5 tip=%6")
                      .arg(toleranceSpin ? 1 : 0)
                      .arg(toleranceSpin ? toleranceSpin->minimum() : 0.0)
                      .arg(toleranceSpin ? toleranceSpin->maximum() : 0.0)
                      .arg(toleranceSpin ? toleranceSpin->singleStep() : 0.0)
                      .arg(toleranceSpin ? toleranceSpin->value() : 0.0)
                      .arg(toleranceSpin ? toleranceSpin->toolTip() : QString()));

        QToolButton* fillColorButton = shapeWindow.findChild<QToolButton*>(QStringLiteral("FillColorButton"));
        QToolButton* fillStyleButton = shapeWindow.findChild<QToolButton*>(QStringLiteral("FillButton"));
        QToolButton* strokeColorButton = shapeWindow.findChild<QToolButton*>(QStringLiteral("StrokeColorButton"));
        QSpinBox* toolbarStrokeWidthSpin = shapeWindow.findChild<QSpinBox*>(QStringLiteral("StrokeWidthSpin"));
        QToolBar* mainBar = shapeWindow.findChild<QToolBar*>(QStringLiteral("MainToolBar"));
        auto toolbarActionIndex = [mainBar](QWidget* widget) {
            if (!mainBar || !widget) {
                return -1;
            }
            const QList<QAction*> barActions = mainBar->actions();
            for (int i = 0; i < barActions.size(); ++i) {
                QWidget* actionWidget = mainBar->widgetForAction(barActions.at(i));
                if (auto* widgetAction = qobject_cast<QWidgetAction*>(barActions.at(i))) {
                    if (widgetAction->defaultWidget()) {
                        actionWidget = widgetAction->defaultWidget();
                    }
                }
                if (actionWidget == widget) {
                    return i;
                }
            }
            return -1;
        };
        const int toolbarFillColorIndex = toolbarActionIndex(fillColorButton);
        const int toolbarFillStyleIndex = toolbarActionIndex(fillStyleButton);
        const int toolbarStrokeColorIndex = toolbarActionIndex(strokeColorButton);
        const int toolbarStrokeWidthIndex = toolbarActionIndex(toolbarStrokeWidthSpin);
        const int toolbarToleranceIndex = toolbarActionIndex(toleranceSpin);
        ctx.check(toolbarFillColorIndex >= 0 && toolbarFillStyleIndex > toolbarFillColorIndex &&
                      toolbarStrokeColorIndex > toolbarFillStyleIndex &&
                      toolbarStrokeWidthIndex > toolbarStrokeColorIndex &&
                      toolbarToleranceIndex > toolbarStrokeWidthIndex,
                  QStringLiteral("工具栏顺序为填充颜色、填充样式、描线颜色、描线宽度、拟合容差"),
                  QStringLiteral("index=%1,%2,%3,%4,%5")
                      .arg(toolbarFillColorIndex)
                      .arg(toolbarFillStyleIndex)
                      .arg(toolbarStrokeColorIndex)
                      .arg(toolbarStrokeWidthIndex)
                      .arg(toolbarToleranceIndex));

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
        ctx.check(fillActionTexts.size() == 11 && fillActionTexts.contains(QStringLiteral("反斜线填充")) &&
                      fillActionTexts.contains(QStringLiteral("十字线填充")) &&
                      fillActionTexts.contains(QStringLiteral("六边形填充")) &&
                      fillActionTexts.contains(QStringLiteral("水平线填充")) &&
                      fillActionTexts.contains(QStringLiteral("竖线填充")) &&
                      fillActionTexts.contains(QStringLiteral("砖纹填充")) &&
                      fillActionTexts.contains(QStringLiteral("点阵填充")),
                  QStringLiteral("填充菜单只保留填充样式"), fillActionTexts.join(QStringLiteral("|")));

        QDoubleSpinBox* toolbarSmoothing = shapeWindow.findChild<QDoubleSpinBox*>(QStringLiteral("BrushSmoothingSpin"));
        CanvasView* canvasView = shapeWindow.findChild<CanvasView*>();
        if (toolbarSmoothing) {
            toolbarSmoothing->setValue(0.0);
            ctx.check(canvasView && canvasView->strokeSmoothing() == 0, QStringLiteral("工具栏容差即时生效"),
                      QStringLiteral("view=%1").arg(canvasView ? canvasView->strokeSmoothing() : -1));
            toolbarSmoothing->setValue(2.0);
        }
        QToolButton* smoothButton = shapeWindow.findChild<QToolButton*>(QStringLiteral("SmoothStrokeButton"));
        ctx.check(!smoothButton, QStringLiteral("工具栏不提供手动平滑按钮"),
                  QStringLiteral("button=%1").arg(smoothButton ? 1 : 0));
        QAction* smoothAction = nullptr;
        for (QAction* action : shapeWindow.findChildren<QAction*>()) {
            if (action->text() == QStringLiteral("简化曲线")) {
                smoothAction = action;
            }
        }
        ctx.check(smoothAction && smoothAction->shortcut() == QKeySequence(QStringLiteral("Ctrl+Shift+B")),
                  QStringLiteral("菜单提供简化曲线入口与快捷键"));
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

        // 画笔颜色按钮与画笔宽度按钮已从工具栏移除；描线宽度改为描线颜色按钮后的 SpinBox。
        // 图形数量增多后窗口宽度可能触发工具栏溢出扩展，隐藏控件的几何坐标恒为 0，
        // 因此用 MainToolBar 的动作序号（与上文同一套判定）代替像素位置。
        QToolButton* removedBrushColorButton = shapeWindow.findChild<QToolButton*>(QStringLiteral("ColorButton"));
        QAction* removedBrushColorAction = shapeWindow.findChild<QAction*>(QStringLiteral("BrushColorAction"));
        QSpinBox* strokeWidthSpin = shapeWindow.findChild<QSpinBox*>(QStringLiteral("StrokeWidthSpin"));
        const int strokeButtonIndex = toolbarActionIndex(iconStrokeColorButton);
        const int widthSpinIndex = toolbarActionIndex(strokeWidthSpin);
        ctx.check(!removedBrushColorButton && !removedBrushColorAction && strokeWidthSpin && iconStrokeColorButton &&
                      widthSpinIndex > strokeButtonIndex && strokeWidthSpin->singleStep() == 1,
                  QStringLiteral("工具栏去掉画笔颜色按钮并在描线按钮后追加描线宽度 SpinBox"),
                  QStringLiteral("color=%1 action=%2 spinIdx=%3 strokeIdx=%4 step=%5")
                      .arg(removedBrushColorButton ? 1 : 0)
                      .arg(removedBrushColorAction ? 1 : 0)
                      .arg(widthSpinIndex)
                      .arg(strokeButtonIndex)
                      .arg(strokeWidthSpin ? strokeWidthSpin->singleStep() : 0));

        QSpinBox* menuWidthSpin = shapeWindow.findChild<QSpinBox*>(QStringLiteral("MenuStrokeWidthSpin"));
        QAction* widthWidgetAction = shapeWindow.findChild<QAction*>(QStringLiteral("BrushWidthWidgetAction"));
        ctx.check(menuWidthSpin && widthWidgetAction && strokeWidthSpin && menuWidthSpin->singleStep() == 1 &&
                      menuWidthSpin->minimum() == strokeWidthSpin->minimum() &&
                      menuWidthSpin->maximum() == strokeWidthSpin->maximum() &&
                      menuWidthSpin->toolTip() == QStringLiteral("描线宽度"),
                  QStringLiteral("菜单描线宽度与工具栏使用同一规格的 SpinBox"),
                  QStringLiteral("menuSpin=%1 action=%2 step=%3 range=%4-%5 tip=%6")
                      .arg(menuWidthSpin ? 1 : 0)
                      .arg(widthWidgetAction ? 1 : 0)
                      .arg(menuWidthSpin ? menuWidthSpin->singleStep() : 0)
                      .arg(menuWidthSpin ? menuWidthSpin->minimum() : -1)
                      .arg(menuWidthSpin ? menuWidthSpin->maximum() : -1)
                      .arg(menuWidthSpin ? menuWidthSpin->toolTip() : QStringLiteral("-")));
        if (menuWidthSpin && strokeWidthSpin) {
            const int oldWidth = menuWidthSpin->value();
            menuWidthSpin->setValue(2);
            QApplication::processEvents();
            ctx.check(strokeWidthSpin->value() == 2, QStringLiteral("菜单描线宽度 SpinBox 同步工具栏"),
                      QStringLiteral("toolbar=%1 menu=%2").arg(strokeWidthSpin->value()).arg(menuWidthSpin->value()));
            menuWidthSpin->setValue(oldWidth);
            QApplication::processEvents();
        }

        // 一级「画笔」菜单已删除：描线宽度子菜单与平滑曲线并到「插入」的描线颜色之后，
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
        const int widthMenuIndex = insertActionsForOrder.indexOf(widthWidgetAction);
        int smoothActionIndex = -1;
        for (int i = 0; i < insertActionsForOrder.size(); ++i) {
            if (insertActionsForOrder.at(i) && insertActionsForOrder.at(i)->text() == QStringLiteral("简化曲线")) {
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
                  QStringLiteral("描线宽度与简化曲线移到插入菜单描线颜色之后"),
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
        ctx.check(fillStyleMenu && fillMenuItemCount == 11 && !fillHasColor,
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
            // 需求 16：新图元按工具栏当前选择生成——默认纯色填充、未选填充色时
            // 跟随画笔色（与描线同色），描线宽度沿用工具栏取值。
            ctx.check(inserted && inserted->fillStyle() == FillStyle::Solid &&
                          inserted->fillColor() == inserted->strokeColor() && inserted->strokeWidth() > 0.0,
                      QStringLiteral("快速插入图形按工具栏默认样式生成（纯色填充跟随画笔色）"));
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
            // 需求 16：箭头同样按工具栏样式生成（纯色填充跟随画笔色）。
            ctx.check(insertedArrow && insertedArrow->fillStyle() == FillStyle::Solid &&
                          insertedArrow->fillColor() == insertedArrow->strokeColor(),
                      QStringLiteral("新插入箭头按工具栏默认样式生成"));
        }
    }

    {
        MainWindow strokeWindow;
        strokeWindow.show();
        QApplication::processEvents();

        auto* stroke = new CurveItem();
        const QColor initialColor(QStringLiteral("#1E293B"));
        stroke->setStrokePoints({QPointF(40, 40), QPointF(160, 120)}, initialColor, 4.0);
        strokeWindow.scene()->addItemsWithUndo({stroke}, QStringLiteral("Add width stroke"));
        strokeWindow.view()->setMode(ToolMode::Edit);
        strokeWindow.scene()->clearSelection();
        stroke->setSelected(true);
        QApplication::processEvents();

        QSpinBox* widthSpin = strokeWindow.findChild<QSpinBox*>(QStringLiteral("StrokeWidthSpin"));
        QSpinBox* menuWidthSpin = strokeWindow.findChild<QSpinBox*>(QStringLiteral("MenuStrokeWidthSpin"));
        ctx.check(widthSpin && menuWidthSpin, QStringLiteral("工具栏与菜单提供描线宽度入口"),
                  QStringLiteral("toolbar=%1 menu=%2").arg(widthSpin ? 1 : 0).arg(menuWidthSpin ? 1 : 0));
        if (widthSpin && menuWidthSpin) {
            const int originalWidth = menuWidthSpin->value();
            menuWidthSpin->setValue(2);
            QApplication::processEvents();
            ctx.check(qAbs(stroke->strokeWidth() - 2.0) < 0.01 && widthSpin->value() == 2,
                      QStringLiteral("菜单描线宽度改变选中曲线宽度"),
                      QStringLiteral("stroke=%1 toolbar=%2 menu=%3")
                          .arg(stroke->strokeWidth())
                          .arg(widthSpin->value())
                          .arg(menuWidthSpin->value()));

            widthSpin->setValue(7);
            QApplication::processEvents();
            ctx.check(qAbs(stroke->strokeWidth() - 7.0) < 0.01 && strokeWindow.view()->brushWidth() == 7.0 &&
                          menuWidthSpin->value() == 7,
                      QStringLiteral("工具栏描线宽度 SpinBox 步进 1px 并改变选中曲线宽度"),
                      QStringLiteral("stroke=%1 brush=%2 step=%3 menu=%4")
                          .arg(stroke->strokeWidth())
                          .arg(strokeWindow.view()->brushWidth())
                          .arg(widthSpin->singleStep())
                          .arg(menuWidthSpin->value()));
            menuWidthSpin->setValue(originalWidth);
            QApplication::processEvents();
        }

        StyleController styleController(&strokeWindow);
        const QColor changedColor(QStringLiteral("#20C997"));
        styleController.applyStrokeColorToSelection(changedColor);
        ctx.check(stroke->strokeColor() == changedColor, QStringLiteral("描线颜色改变选中曲线颜色"),
                  stroke->strokeColor().name());
        strokeWindow.scene()->undoStack()->undo();
        QApplication::processEvents();
        ctx.check(stroke->strokeColor() == initialColor, QStringLiteral("撤销描线颜色恢复曲线颜色"),
                  stroke->strokeColor().name());
    }

    {
        MainWindow polygonWindow;
        polygonWindow.show();
        QApplication::processEvents();

        CompositeShapeItem::Layer layer;
        layer.path.addRect(0.0, 0.0, 120.0, 80.0);
        layer.fill = QColor(QStringLiteral("#3B82F6"));
        layer.stroke = QColor(QStringLiteral("#1E293B"));
        layer.strokeWidth = 4.0;
        layer.role = CompositeShapeItem::LayerRole::FilledArea;
        auto* polygon = new CompositeShapeItem({layer});
        polygonWindow.scene()->addItemsWithUndo({polygon}, QStringLiteral("Add toolbar polygon"));
        polygonWindow.view()->setMode(ToolMode::Edit);
        polygonWindow.scene()->clearSelection();
        polygon->setSelected(true);
        QApplication::processEvents();

        QToolButton* fillButton = polygonWindow.findChild<QToolButton*>(QStringLiteral("FillButton"));
        QAction* hachureAction = nullptr;
        if (fillButton && fillButton->menu()) {
            for (QAction* action : fillButton->menu()->actions()) {
                if (action->text() == QStringLiteral("反斜线填充")) {
                    hachureAction = action;
                }
            }
        }
        const FillStyle fillBefore = polygon->fillStyle();
        if (hachureAction) {
            hachureAction->trigger();
            QApplication::processEvents();
        }
        ctx.check(polygon->fillStyle() == FillStyle::ReverseHachure, QStringLiteral("工具栏填充样式作用到多边形"),
                  QString::number(static_cast<int>(polygon->fillStyle())));
        polygonWindow.scene()->undoStack()->undo();
        QApplication::processEvents();
        ctx.check(polygon->fillStyle() == fillBefore, QStringLiteral("撤销工具栏填充样式恢复多边形原样式"));

        StyleController styleController(&polygonWindow);
        const QColor changedStroke(QStringLiteral("#20C997"));
        const QColor strokeBefore = polygon->strokeColor();
        styleController.applyStrokeColorToSelection(changedStroke);
        ctx.check(polygon->strokeColor() == changedStroke, QStringLiteral("工具栏描线颜色作用到多边形"),
                  polygon->strokeColor().name());
        polygonWindow.scene()->undoStack()->undo();
        QApplication::processEvents();
        ctx.check(polygon->strokeColor() == strokeBefore, QStringLiteral("撤销工具栏描线颜色恢复多边形原色"));

        QSpinBox* polygonWidthSpin = polygonWindow.findChild<QSpinBox*>(QStringLiteral("StrokeWidthSpin"));
        const qreal widthBefore = polygon->strokeWidth();
        if (polygonWidthSpin) {
            polygonWidthSpin->setValue(9);
            QApplication::processEvents();
        }
        ctx.check(qAbs(polygon->strokeWidth() - 9.0) < 0.01, QStringLiteral("工具栏描线宽度作用到多边形"),
                  QString::number(polygon->strokeWidth()));
        polygonWindow.scene()->undoStack()->undo();
        QApplication::processEvents();
        ctx.check(qAbs(polygon->strokeWidth() - widthBefore) < 0.01,
                  QStringLiteral("撤销工具栏描线宽度恢复多边形原宽度"));
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

    {
        QJsonObject fractionalRoot = ctx.shapeRoot;
        QJsonObject fractionalCanvas = fractionalRoot.value(QStringLiteral("canvas")).toObject();
        fractionalCanvas.insert(QStringLiteral("width"), 800.5);
        fractionalCanvas.insert(QStringLiteral("height"), 600.25);
        fractionalRoot.insert(QStringLiteral("canvas"), fractionalCanvas);
        const QString fractionalPath = QDir(ctx.workDirectory).filePath(QStringLiteral("fractional-canvas.sbcanvas"));
        QFile fractionalFile(fractionalPath);
        ctx.check(fractionalFile.open(QIODevice::WriteOnly), QStringLiteral("创建小数尺寸画布测试文件"));
        fractionalFile.write(QJsonDocument(fractionalRoot).toJson(QJsonDocument::Indented));
        fractionalFile.close();

        MainWindow fractionalWindow;
        ctx.check(fractionalWindow.openCanvasFile(fractionalPath), QStringLiteral("主窗口打开带小数尺寸的画布"));
        ctx.check(qAbs(fractionalWindow.scene()->canvasSize().width() - 800.5) < 0.01 &&
                      qAbs(fractionalWindow.scene()->canvasSize().height() - 600.25) < 0.01,
                  QStringLiteral("小数画布尺寸按原值载入"),
                  QStringLiteral("%1 x %2")
                      .arg(fractionalWindow.scene()->canvasSize().width())
                      .arg(fractionalWindow.scene()->canvasSize().height()));
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
