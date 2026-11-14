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
        shapeWindow.resize(1200, 800);
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
        QToolButton* shapeButton = shapeWindow.findChild<QToolButton*>(QStringLiteral("ShapeButton"));
        ctx.check(shapeButton && shapeButton->menu() == shapeMenu &&
                      shapeButton->popupMode() == QToolButton::InstantPopup,
                  QStringLiteral("主工具栏图形按钮绑定快速插入菜单"));

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

        QMenu* brushWidthMenu = shapeWindow.findChild<QMenu*>(QStringLiteral("BrushWidthMenu"));
        QComboBox* widthCombo = shapeWindow.findChild<QComboBox*>(QStringLiteral("BrushWidthCombo"));
        ctx.check(brushWidthMenu && brushWidthMenu->actions().size() == 15, QStringLiteral("画笔样式菜单提供粗细选项"),
                  QStringLiteral("actions=%1").arg(brushWidthMenu ? brushWidthMenu->actions().size() : 0));
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

        // 需求：“画笔样式”已独立为一级菜单“画笔”，“平滑笔画”也归到该菜单；
        // 填充样式与填充／描边颜色并入“插入”菜单，紧跟在常见图形之后。
        QMenu* brushMenu = shapeWindow.findChild<QMenu*>(QStringLiteral("BrushMenu"));
        QMenu* fillStyleMenu = shapeWindow.findChild<QMenu*>(QStringLiteral("FillStyleMenu"));
        bool brushIsTopLevel = false;
        for (QAction* action : shapeWindow.menuBar()->actions()) {
            QMenu* menu = action ? action->menu() : nullptr;
            if (menu && menu->objectName() == QStringLiteral("BrushMenu")) {
                brushIsTopLevel = true;
            }
        }
        ctx.check(brushMenu && brushIsTopLevel && brushMenu->title() == QStringLiteral("画笔(&B)"),
                  QStringLiteral("画笔是一级菜单"), brushMenu ? brushMenu->title() : QStringLiteral("missing"));
        bool styleMenuGone = shapeWindow.findChild<QMenu*>(QStringLiteral("StyleMenu")) == nullptr &&
                             shapeWindow.findChild<QMenu*>(QStringLiteral("BrushStyleMenu")) == nullptr;
        ctx.check(styleMenuGone, QStringLiteral("样式菜单与画笔样式子菜单不再存在"));

        QList<QAction*> brushActions;
        if (brushMenu) {
            brushActions = brushMenu->actions();
        }
        QList<QAction*> fillActions;
        if (fillStyleMenu) {
            fillActions = fillStyleMenu->actions();
        }
        bool brushHasColor = false;
        bool brushHasOutline = false;
        bool brushHasSmooth = false;
        bool brushHasWidth = brushMenu && brushMenu->findChild<QMenu*>(QStringLiteral("BrushWidthMenu"));
        for (QAction* action : brushActions) {
            if (action->objectName() == QStringLiteral("BrushColorAction")) {
                brushHasColor = true;
            } else if (action->objectName() == QStringLiteral("BorderColorAction")) {
                brushHasOutline = true;
            }
            if (action->text() == QStringLiteral("平滑笔画")) {
                brushHasSmooth = true;
            }
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
        ctx.check(brushMenu && brushHasColor && brushHasWidth && brushHasSmooth && !brushHasOutline,
                  QStringLiteral("画笔菜单包含画笔颜色、粗细选项与平滑笔画"));
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
        ctx.check(outlineAction && outlineAction->text() == QStringLiteral("边框颜色…"),
                  QStringLiteral("主窗口提供边框颜色动作"));

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
