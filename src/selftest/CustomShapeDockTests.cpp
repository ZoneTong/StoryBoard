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


void selftest::runCustomShapeDockTests(Context& ctx)
{
    // 需求：选中图元工具卡（边框样式 / 透明度 / 矩形圆角 / 图层与高度）与画布格式自定义图形。
    {
        ShapeItem styled(ShapeType::Rectangle);
        styled.setContentSize(QSizeF(200, 120));
        styled.setStyle(QColor(QStringLiteral("#1E293B")), QColor(QStringLiteral("#F97316")), 6.0);
        styled.setStrokeStyle(Qt::DashLine);
        styled.setOpacity(0.42);
        styled.setCornerRadius(18.0);

        ShapeItem reloaded;
        reloaded.fromJson(styled.toJson());
        ctx.check(reloaded.strokeStyle() == Qt::DashLine && qAbs(reloaded.opacity() - 0.42) < 0.001 &&
                      qAbs(reloaded.cornerRadius() - 18.0) < 0.001,
                  QStringLiteral("图形边框样式、透明度与圆角随画布保存"),
                  QStringLiteral("style=%1 opacity=%2 radius=%3")
                      .arg(static_cast<int>(reloaded.strokeStyle()))
                      .arg(reloaded.opacity())
                      .arg(reloaded.cornerRadius()));

        const int squareElements = ShapeItem::pathFor(ShapeType::Rectangle, QRectF(0, 0, 100, 80), 0.0).elementCount();
        const int roundElements = ShapeItem::pathFor(ShapeType::Rectangle, QRectF(0, 0, 100, 80), 20.0).elementCount();
        ctx.check(roundElements > squareElements, QStringLiteral("矩形圆角半径改变轮廓"),
                  QStringLiteral("square=%1 round=%2").arg(squareElements).arg(roundElements));

        QPainterPath unit;
        unit.addRect(QRectF(0.0, 0.0, 1.0, 1.0));
        ShapeItem custom;
        custom.setCustomShape(QStringLiteral("Badge"), unit);
        custom.setContentSize(QSizeF(160, 100));
        ShapeItem customReloaded;
        customReloaded.fromJson(custom.toJson());
        const QRectF customBounds = customReloaded.currentPath(QRectF(0, 0, 160, 100)).boundingRect();
        ctx.check(customReloaded.isCustomShape() && customReloaded.displayName() == QStringLiteral("Badge") &&
                      qAbs(customBounds.width() - 160.0) < 0.01 && qAbs(customBounds.height() - 100.0) < 0.01,
                  QStringLiteral("自定义图形轮廓随画布保存并按内容矩形缩放"),
                  QStringLiteral("custom=%1 name=%2 w=%3")
                      .arg(customReloaded.isCustomShape() ? 1 : 0)
                      .arg(customReloaded.displayName())
                      .arg(customBounds.width()));
        customReloaded.setShapeType(ShapeType::Ellipse);
        ctx.check(!customReloaded.isCustomShape(), QStringLiteral("切换内置图形时清除自定义轮廓"));

        // 自定义图形目录：配置同级的 shapes 子目录，扫描 .sbcanvas 并按文件名命名。
        const QString shapesConfig = QDir(ctx.workDirectory).filePath(QStringLiteral("custom-shape-config.ini"));
        qputenv("STORYBOARD_CONFIG", shapesConfig.toLocal8Bit());
        const QString shapesDirectory = AppSettings::shapesDir();
        ctx.check(QFileInfo(shapesDirectory).absolutePath() == QFileInfo(shapesConfig).absolutePath() &&
                      QFileInfo(shapesDirectory).fileName() == QStringLiteral("shapes"),
                  QStringLiteral("自定义图形目录位于配置文件同级的 shapes 子目录"), shapesDirectory);

        ShapeItem templateShape(ShapeType::Star);
        templateShape.setPos(QPointF(12, 18));
        templateShape.setContentSize(QSizeF(160, 100));
        QJsonObject templateRoot;
        templateRoot.insert(QStringLiteral("format"), QStringLiteral("StoryBoardCanvas"));
        templateRoot.insert(QStringLiteral("items"), QJsonArray{templateShape.toJson()});
        const QString templatePath = QDir(shapesDirectory).filePath(QStringLiteral("probe-badge.sbcanvas"));
        for (const QString& stale : QDir(shapesDirectory).entryList({QStringLiteral("*.sbcanvas")}, QDir::Files)) {
            QFile::remove(QDir(shapesDirectory).filePath(stale));
        }
        QFile templateFile(templatePath);
        const bool templateWritten = templateFile.open(QIODevice::WriteOnly);
        if (templateWritten) {
            templateFile.write(QJsonDocument(templateRoot).toJson(QJsonDocument::Indented));
            templateFile.close();
        }
        QStringList templateErrors;
        const QList<CustomShapeTemplate> templates = loadCustomShapeTemplates(shapesDirectory, &templateErrors);
        const CustomShapeTemplate* probeTemplate = nullptr;
        for (const CustomShapeTemplate& entry : templates) {
            if (entry.name == QStringLiteral("probe-badge")) {
                probeTemplate = &entry;
            }
        }
        const bool templateOk = probeTemplate != nullptr && templateErrors.isEmpty();
        const QRectF unitBounds = templateOk ? probeTemplate->unitPath.boundingRect() : QRectF();
        ctx.check(templateOk && probeTemplate->aspectRatio > 1.0 && qAbs(unitBounds.left()) < 0.01 &&
                      qAbs(unitBounds.top()) < 0.01 && qAbs(unitBounds.width() - 1.0) < 0.01 &&
                      qAbs(unitBounds.height() - 1.0) < 0.01,
                  QStringLiteral("自定义图形模板按文件名命名并归一化到单位坐标"),
                  QStringLiteral("count=%1 name=%2 bounds=%3,%4 %5x%6")
                      .arg(templates.size())
                      .arg(templateOk ? probeTemplate->name : QString())
                      .arg(unitBounds.left())
                      .arg(unitBounds.top())
                      .arg(unitBounds.width())
                      .arg(unitBounds.height()));

        for (const QString& stale : QDir(shapesDirectory).entryList({QStringLiteral("*.sbcanvas")}, QDir::Files)) {
            QFile::remove(QDir(shapesDirectory).filePath(stale));
        }

        // 部署目录（可执行文件旁）的 shapes 也要参与扫描，同名模板按目录顺序优先。
        const QString bundledDir = AppSettings::bundledShapesDir();
        {
            // 随包分发的示例模板必须能解析，否则用户装完看不到「自定义图形」示例。
            const QString sampleDir =
                QDir(QStringLiteral(STORYBOARD_SOURCE_DIR)).filePath(QStringLiteral("examples/shapes"));
            QStringList sampleErrors;
            const QList<CustomShapeTemplate> samples = loadCustomShapeTemplates(sampleDir, &sampleErrors);
            ctx.check(
                !samples.isEmpty() && sampleErrors.isEmpty(), QStringLiteral("随包示例模板可解析并出现在自定义图形里"),
                QStringLiteral("count=%1 errors=%2").arg(samples.size()).arg(sampleErrors.join(QStringLiteral(";"))));
        }
        const QStringList scanDirs = AppSettings::shapesDirs();
        ctx.check(!scanDirs.isEmpty() && scanDirs.contains(shapesDirectory) && scanDirs.contains(bundledDir),
                  QStringLiteral("自定义图形同时扫描配置文件同级与部署目录旁的 shapes"),
                  QStringLiteral("dirs=%1").arg(scanDirs.join(QStringLiteral("|"))));
        {
            // 合并多目录时用临时目录，别往真实部署目录里写测试文件，否则包里会留下残留。
            const QString secondDir = QDir(ctx.workDirectory).filePath(QStringLiteral("deployed-shapes"));
            QDir().mkpath(secondDir);
            QJsonObject deployedRoot;
            deployedRoot.insert(QStringLiteral("format"), QStringLiteral("StoryBoardCanvas"));
            ShapeItem deployedShape(ShapeType::Ellipse);
            deployedShape.setContentSize(QSizeF(90, 90));
            deployedRoot.insert(QStringLiteral("items"), QJsonArray{deployedShape.toJson()});
            const QString deployedPath = QDir(secondDir).filePath(QStringLiteral("deployed-leaf.sbcanvas"));
            QFile deployedFile(deployedPath);
            if (deployedFile.open(QIODevice::WriteOnly)) {
                deployedFile.write(QJsonDocument(deployedRoot).toJson(QJsonDocument::Indented));
                deployedFile.close();
            }
            // 同名模板放两份：配置文件同级的那份应当胜出。
            QFile duplicateUser(QDir(shapesDirectory).filePath(QStringLiteral("deployed-leaf.sbcanvas")));
            if (duplicateUser.open(QIODevice::WriteOnly)) {
                duplicateUser.write(QJsonDocument(templateRoot).toJson(QJsonDocument::Indented));
                duplicateUser.close();
            }
            QStringList mergedErrors;
            const QList<CustomShapeTemplate> merged =
                loadCustomShapeTemplatesFromDirs({shapesDirectory, secondDir}, &mergedErrors);
            int deployedCount = 0;
            QString winnerPath;
            for (const CustomShapeTemplate& entry : merged) {
                if (entry.name == QStringLiteral("deployed-leaf")) {
                    ++deployedCount;
                    winnerPath = entry.filePath;
                }
            }
            ctx.check(deployedCount == 1 &&
                          QFileInfo(winnerPath).absolutePath() == QDir(shapesDirectory).absolutePath(),
                      QStringLiteral("同名模板只保留配置文件同级的那一份"),
                      QStringLiteral("count=%1 winner=%2").arg(deployedCount).arg(winnerPath));
            QFile::remove(deployedPath);
            QFile::remove(duplicateUser.fileName());
        }

        CanvasScene panelScene;
        ItemDockPanel panel(&panelScene);
        // 紧凑化后不再有折叠分组小标题：断言整卡只由标题行 + 内容滚动区组成，
        // 图形 / 颜色 / 填充 / 边框 / 透明度 / 圆角 / 图层各行控件直接平铺在卡内。
        ctx.check(panel.findChildren<QToolButton*>(QStringLiteral("ItemSectionToggle")).isEmpty(),
                  QStringLiteral("编辑图元卡已去掉折叠分组小标题"));
        ctx.check(panel.findChildren<QToolButton*>(QStringLiteral("ItemShapeButton")).size() >= 6 &&
                      panel.findChildren<QToolButton*>(QStringLiteral("ItemColorSwatch")).size() >= 16 &&
                      panel.findChildren<QToolButton*>(QStringLiteral("ItemFillStyleButton")).size() == 7 &&
                      panel.findChildren<QToolButton*>(QStringLiteral("ItemBorderStyleButton")).size() == 4,
                  QStringLiteral("编辑图元卡平铺图形 / 颜色 / 填充 / 边框等工具"),
                  QStringLiteral("shapes=%1 colors=%2 fills=%3 borders=%4")
                      .arg(panel.findChildren<QToolButton*>(QStringLiteral("ItemShapeButton")).size())
                      .arg(panel.findChildren<QToolButton*>(QStringLiteral("ItemColorSwatch")).size())
                      .arg(panel.findChildren<QToolButton*>(QStringLiteral("ItemFillStyleButton")).size())
                      .arg(panel.findChildren<QToolButton*>(QStringLiteral("ItemBorderStyleButton")).size()));
        ctx.check(panel.findChildren<QToolButton*>(QStringLiteral("ItemLayerUpButton")).size() == 1 &&
                      panel.findChildren<QToolButton*>(QStringLiteral("ItemLayerDownButton")).size() == 1 &&
                      panel.findChildren<QToolButton*>(QStringLiteral("ItemHeightUpButton")).size() == 1 &&
                      panel.findChildren<QToolButton*>(QStringLiteral("ItemHeightDownButton")).size() == 1,
                  QStringLiteral("工具卡提供图层与层内高度 ±1 按钮"));

        QWidget* content = panel.findChild<QWidget*>(QStringLiteral("ItemPanelContent"));
        QWidget* fillStyleRow = panel.findChild<QWidget*>(QStringLiteral("ItemFillStyleRow"));
        QWidget* fillColorRow = panel.findChild<QWidget*>(QStringLiteral("ItemFillColorRow"));
        QWidget* borderStyleRow = panel.findChild<QWidget*>(QStringLiteral("ItemBorderStyleRow"));
        QWidget* borderWidthRow = panel.findChild<QWidget*>(QStringLiteral("ItemBorderWidthRow"));
        QWidget* borderColorRow = panel.findChild<QWidget*>(QStringLiteral("ItemBorderColorRow"));
        const int fillStyleIndex = content && content->layout() && fillStyleRow ? content->layout()->indexOf(fillStyleRow) : -1;
        const int fillColorIndex = content && content->layout() && fillColorRow ? content->layout()->indexOf(fillColorRow) : -1;
        const int borderStyleIndex =
            content && content->layout() && borderStyleRow ? content->layout()->indexOf(borderStyleRow) : -1;
        const int borderWidthIndex =
            content && content->layout() && borderWidthRow ? content->layout()->indexOf(borderWidthRow) : -1;
        const int borderColorIndex =
            content && content->layout() && borderColorRow ? content->layout()->indexOf(borderColorRow) : -1;
        ctx.check(fillStyleIndex >= 0 && fillColorIndex > fillStyleIndex && borderStyleIndex > fillColorIndex &&
                      borderColorIndex > borderStyleIndex && borderWidthIndex > borderColorIndex,
                  QStringLiteral("编辑图元卡按填充样式、填充颜色、边框样式、边框颜色、边框宽度排列"),
                  QStringLiteral("%1/%2/%3/%4/%5")
                      .arg(fillStyleIndex)
                      .arg(fillColorIndex)
                      .arg(borderStyleIndex)
                      .arg(borderColorIndex)
                      .arg(borderWidthIndex));
        ctx.check(ItemDockPanel::tr("Border color") == QStringLiteral("边框颜色") &&
                      ItemDockPanel::tr("Height +1") == QStringLiteral("高度 +1") &&
                      ItemDockPanel::tr("Layer and height") == QStringLiteral("图层与高度"),
                  QStringLiteral("编辑图元卡的工具提示都有中文译文"),
                  QStringLiteral("stroke=%1 height=%2")
                      .arg(ItemDockPanel::tr("Border color"))
                      .arg(ItemDockPanel::tr("Height +1")));
        bool priorityTipFound = false;
        for (QToolButton* rowIcon : panel.findChildren<QToolButton*>(QStringLiteral("ItemRowIcon"))) {
            if (rowIcon->toolTip().contains(QStringLiteral("优先级"))) {
                priorityTipFound = true;
                break;
            }
        }
        ctx.check(priorityTipFound, QStringLiteral("工具卡标明优先级先按图层再按层内高度"));

        qunsetenv("STORYBOARD_CONFIG");
    }
}

void selftest::runDockPanelMainWindowTests(Context& ctx)
{
    // 工具卡与主窗口联动：选中出现 / 清空隐藏、透明度 / 边框 / 填充 / 高度即时生效并可撤销。
    {
        const QString dockConfig = QDir(ctx.workDirectory).filePath(QStringLiteral("dock-panel-config.ini"));
        qputenv("STORYBOARD_CONFIG", dockConfig.toLocal8Bit());
        const QString dockShapesDir = AppSettings::shapesDir();
        ShapeItem menuShape(ShapeType::Star);
        menuShape.setContentSize(QSizeF(120, 80));
        QJsonObject menuRoot;
        menuRoot.insert(QStringLiteral("format"), QStringLiteral("StoryBoardCanvas"));
        menuRoot.insert(QStringLiteral("items"), QJsonArray{menuShape.toJson()});
        const QString menuTemplatePath = QDir(dockShapesDir).filePath(QStringLiteral("dock-probe.sbcanvas"));
        for (const QString& stale : QDir(dockShapesDir).entryList({QStringLiteral("*.sbcanvas")}, QDir::Files)) {
            QFile::remove(QDir(dockShapesDir).filePath(stale));
        }
        QFile menuTemplate(menuTemplatePath);
        if (menuTemplate.open(QIODevice::WriteOnly)) {
            menuTemplate.write(QJsonDocument(menuRoot).toJson(QJsonDocument::Indented));
            menuTemplate.close();
        }

        MainWindow dockWindow;
        dockWindow.resize(1200, 800);
        dockWindow.show();
        QApplication::processEvents();

        QDockWidget* itemDock = dockWindow.findChild<QDockWidget*>(QStringLiteral("ItemDock"));
        ItemDockPanel* dockPanel =
            itemDock ? itemDock->findChild<ItemDockPanel*>(QStringLiteral("ItemPanel")) : nullptr;
        QMenu* dockCustomMenu = dockWindow.findChild<QMenu*>(QStringLiteral("CustomShapeMenu"));
        QStringList customTexts;
        if (dockCustomMenu) {
            for (QAction* action : dockCustomMenu->actions()) {
                customTexts.append(action->text());
            }
        }
        ctx.check(itemDock && dockPanel && itemDock->isVisible() == false,
                  QStringLiteral("左侧提供编辑图元卡且无选中时隐藏"));
        ctx.check(dockCustomMenu && customTexts.contains(QStringLiteral("dock-probe")),
                  QStringLiteral("自定义图形追加到常见图形下方的下拉菜单"), customTexts.join(QStringLiteral("|")));
        ctx.check(dockWindow.findChild<QAction*>(QStringLiteral("ItemDockToggleAction")) != nullptr,
                  QStringLiteral("视图菜单提供编辑图元卡开关"));

        CanvasScene* dockScene = dockWindow.scene();
        const int beforeCount = dockScene->canvasItemCount();
        QList<QToolButton*> shapeButtons;
        if (dockPanel) {
            shapeButtons = dockPanel->findChildren<QToolButton*>(QStringLiteral("ItemShapeButton"));
        }
        if (!shapeButtons.isEmpty()) {
            shapeButtons.first()->click();
        }
        QApplication::processEvents();
        ShapeItem* selectedShape = dockScene->selectedCanvasItems().isEmpty()
                                       ? nullptr
                                       : dynamic_cast<ShapeItem*>(dockScene->selectedCanvasItems().first());
        ctx.check(selectedShape && dockScene->canvasItemCount() == beforeCount + 1 && itemDock->isVisible(),
                  QStringLiteral("工具卡可插入矩形且选中后卡片自动出现"));

        QDockWidget* layerDock = dockWindow.findChild<QDockWidget*>(QStringLiteral("LayerDock"));
        QDockWidget* viewModeDock = dockWindow.findChild<QDockWidget*>(QStringLiteral("ViewModeDock"));
        QToolBar* modeBar = dockWindow.findChild<QToolBar*>(QStringLiteral("ModeToolBar"));
        ctx.check(viewModeDock && modeBar && viewModeDock->widget() == modeBar,
                  QStringLiteral("视图模式栏作为右上角卡片"));
        ctx.check(itemDock && dockWindow.dockWidgetArea(itemDock) == Qt::LeftDockWidgetArea,
                  QStringLiteral("编辑图元卡默认停靠左侧"));
        ctx.check(viewModeDock && layerDock && dockWindow.dockWidgetArea(viewModeDock) == Qt::RightDockWidgetArea &&
                      dockWindow.dockWidgetArea(layerDock) == Qt::RightDockWidgetArea,
                  QStringLiteral("视图模式卡与图层卡默认停靠右侧"));
        ctx.check(viewModeDock && layerDock &&
                      viewModeDock->mapTo(&dockWindow, QPoint(0, 0)).y() <
                          layerDock->mapTo(&dockWindow, QPoint(0, 0)).y(),
                  QStringLiteral("视图模式卡在右上角、图层卡在右下角"));
        ctx.check(viewModeDock && layerDock && viewModeDock->height() < dockWindow.height() - 100 &&
                      layerDock->height() < dockWindow.height() - 100,
                  QStringLiteral("右侧两张卡片默认高度不占满窗口"));

        if (selectedShape && dockPanel) {
            QList<QSpinBox*> valueSpins = dockPanel->findChildren<QSpinBox*>(QStringLiteral("ItemValueSpin"));
            QSpinBox* opacitySpin = nullptr;
            QSpinBox* cornerSpin = nullptr;
            for (QSpinBox* spin : valueSpins) {
                if (spin->maximum() == 100) {
                    opacitySpin = spin;
                } else if (spin->maximum() == 400) {
                    cornerSpin = spin;
                }
            }
            const QList<QToolButton*> borderButtons =
                dockPanel->findChildren<QToolButton*>(QStringLiteral("ItemBorderStyleButton"));
            const QList<QToolButton*> fillButtons =
                dockPanel->findChildren<QToolButton*>(QStringLiteral("ItemFillStyleButton"));
            QToolButton* heightUp = dockPanel->findChild<QToolButton*>(QStringLiteral("ItemHeightUpButton"));

            if (opacitySpin) {
                opacitySpin->setValue(45);
                QApplication::processEvents();
            }
            ctx.check(qAbs(selectedShape->opacity() - 0.45) < 0.01, QStringLiteral("工具卡透明度即时作用到图形"),
                      QString::number(selectedShape->opacity()));
            dockScene->undoStack()->undo();
            ctx.check(qAbs(selectedShape->opacity() - 1.0) < 0.01, QStringLiteral("撤销透明度修改恢复原值"),
                      QString::number(selectedShape->opacity()));

            if (borderButtons.size() > 1) {
                borderButtons.at(1)->click();
                QApplication::processEvents();
            }
            ctx.check(selectedShape->strokeStyle() == Qt::DashLine, QStringLiteral("工具卡边框样式作用到图形"));
            if (fillButtons.size() > 2) {
                fillButtons.at(2)->click();
                QApplication::processEvents();
            }
            ctx.check(selectedShape->fillStyle() == FillStyle::Hachure, QStringLiteral("工具卡填充样式作用到图形"));

            if (cornerSpin) {
                cornerSpin->setValue(24);
                QApplication::processEvents();
            }
            ctx.check(qAbs(selectedShape->cornerRadius() - 24.0) < 0.01, QStringLiteral("矩形选中时圆角设置可用"),
                      QString::number(selectedShape->cornerRadius()));

            const int heightBefore = selectedShape->height();
            if (heightUp) {
                heightUp->click();
                QApplication::processEvents();
            }
            ctx.check(selectedShape->height() == heightBefore + 1, QStringLiteral("工具卡高度 +1 按钮生效"),
                      QStringLiteral("%1->%2").arg(heightBefore).arg(selectedShape->height()));
            dockScene->undoStack()->undo();
            ctx.check(selectedShape->height() == heightBefore, QStringLiteral("撤销高度 +1 恢复原高度"));

            // 多选时卡片提供对齐与合并形状入口，复用菜单栏同一批动作。
            auto* secondShape = new ShapeItem(ShapeType::Rectangle);
            secondShape->setPos(QPointF(420.0, 240.0));
            secondShape->setContentSize(QSizeF(120.0, 90.0));
            secondShape->setStyle(QColor(QStringLiteral("#1D4ED8")), QColor(29, 78, 216, 40), 3.0);
            dockScene->addItemsWithUndo({secondShape}, QStringLiteral("Add second shape"));
            selectedShape->setSelected(true);
            secondShape->setSelected(true);
            QApplication::processEvents();
            QToolButton* alignButton = dockPanel->findChild<QToolButton*>(QStringLiteral("ItemAlignButton"));
            QToolButton* mergeButton = dockPanel->findChild<QToolButton*>(QStringLiteral("ItemMergeButton"));
            ctx.check(alignButton && mergeButton && alignButton->isVisible() && mergeButton->isVisible() &&
                          alignButton->menu() && alignButton->menu()->actions().size() == 8 && mergeButton->menu() &&
                          mergeButton->menu()->actions().size() == 5,
                      QStringLiteral("多选时编辑图元卡提供对齐与合并形状按钮"));

            dockScene->clearSelection();
            QApplication::processEvents();
            ctx.check(!itemDock->isVisible(), QStringLiteral("清空选中后隐藏编辑图元卡"));
            ctx.check(layerDock && layerDock->height() < dockWindow.height() - 100,
                      QStringLiteral("隐藏编辑图元卡后图层卡不撑满窗口高度"));
        }

        for (const QString& stale : QDir(dockShapesDir).entryList({QStringLiteral("*.sbcanvas")}, QDir::Files)) {
            QFile::remove(QDir(dockShapesDir).filePath(stale));
        }
        qunsetenv("STORYBOARD_CONFIG");
    }
}
