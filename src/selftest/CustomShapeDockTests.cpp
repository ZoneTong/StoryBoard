#include "Alignment.h"
#include "BoolOps.h"
#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CanvasView.h"
#include "Commands.h"
#include "CompositeShapeItem.h"
#include "CurveItem.h"
#include "CustomShapePanel.h"
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
        // 需求 7：归一化预留 2% 内边距，轮廓落在 0.02–0.98 之间而不是贴边。
        ctx.check(templateOk && probeTemplate->aspectRatio > 1.0 && unitBounds.left() > 0.0 &&
                      unitBounds.left() < 0.03 && unitBounds.top() > 0.0 && unitBounds.top() < 0.03 &&
                      unitBounds.right() > 0.97 && unitBounds.right() < 1.0 && unitBounds.bottom() > 0.97 &&
                      unitBounds.bottom() < 1.0,
                  QStringLiteral("自定义图形模板按文件名命名并归一化到单位坐标（含 2% 内边距）"),
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

            // 需求 8：25 个儿童故事简笔画模板分四个子目录随包分发。
            const QStringList expectedFolders = {QStringLiteral("小动物"), QStringLiteral("自然"),
                                                 QStringLiteral("物件"), QStringLiteral("人物与精灵")};
            QStringList allNames;
            QStringList childErrors;
            int folderCount = 0;
            for (const QString& folder : expectedFolders) {
                const QString childDir = QDir(sampleDir).filePath(folder);
                if (!QDir(childDir).exists()) {
                    continue;
                }
                ++folderCount;
                for (const CustomShapeTemplate& entry : loadCustomShapeTemplates(childDir, &childErrors)) {
                    allNames.append(entry.name);
                }
            }
            const QStringList expectedNames = {
                QStringLiteral("兔子"),    QStringLiteral("小猫"),   QStringLiteral("小狗"),    QStringLiteral("小鸭"),
                QStringLiteral("小鱼"),    QStringLiteral("小鸟"),   QStringLiteral("蝴蝶"),    QStringLiteral("云"),
                QStringLiteral("太阳"),    QStringLiteral("月亮"),   QStringLiteral("星星"),    QStringLiteral("山"),
                QStringLiteral("树"),      QStringLiteral("小树苗"), QStringLiteral("小船"),    QStringLiteral("帆船"),
                QStringLiteral("房子"),    QStringLiteral("火箭"),   QStringLiteral("爱心"),    QStringLiteral("瓶子"),
                QStringLiteral("花朵"),    QStringLiteral("小人"),   QStringLiteral("小人-跑"), QStringLiteral("精灵"),
                QStringLiteral("星星精灵")};
            bool namesMatch = allNames.size() == expectedNames.size();
            for (const QString& name : expectedNames) {
                if (!allNames.contains(name)) {
                    namesMatch = false;
                }
            }
            ctx.check(folderCount == 4 && namesMatch && childErrors.isEmpty(),
                      QStringLiteral("四个子目录共 25 个儿童故事模板且名称齐全"),
                      QStringLiteral("folders=%1 count=%2 errors=%3")
                          .arg(folderCount)
                          .arg(allNames.size())
                          .arg(childErrors.join(QStringLiteral(";"))));
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
        // 颜色 / 填充 / 边框 / 透明度 / 圆角 / 图层各行控件直接平铺在卡内。
        ctx.check(panel.findChildren<QToolButton*>(QStringLiteral("ItemSectionToggle")).isEmpty(),
                  QStringLiteral("编辑图元卡已去掉折叠分组小标题"));
        ctx.check(panel.findChildren<QToolButton*>(QStringLiteral("ItemShapeButton")).isEmpty() &&
                      panel.findChildren<QToolButton*>(QStringLiteral("ItemCustomShapeButton")).isEmpty(),
                  QStringLiteral("编辑图元卡已移除常见图形展开行"));
        ctx.check(panel.findChildren<QToolButton*>(QStringLiteral("ItemColorSwatch")).size() >= 16 &&
                      panel.findChildren<QToolButton*>(QStringLiteral("ItemFillStyleButton")).size() == 11 &&
                      panel.findChildren<QToolButton*>(QStringLiteral("ItemBorderStyleButton")).size() == 5,
                  QStringLiteral("编辑图元卡平铺颜色 / 填充 / 边框等工具"),
                  QStringLiteral("colors=%1 fills=%2 borders=%3")
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
        const int fillStyleIndex =
            content && content->layout() && fillStyleRow ? content->layout()->indexOf(fillStyleRow) : -1;
        const int fillColorIndex =
            content && content->layout() && fillColorRow ? content->layout()->indexOf(fillColorRow) : -1;
        const int borderStyleIndex =
            content && content->layout() && borderStyleRow ? content->layout()->indexOf(borderStyleRow) : -1;
        const int borderWidthIndex =
            content && content->layout() && borderWidthRow ? content->layout()->indexOf(borderWidthRow) : -1;
        const int borderColorIndex =
            content && content->layout() && borderColorRow ? content->layout()->indexOf(borderColorRow) : -1;
        // 需求 1 行序：填充样式 → 填充颜色 → 描线样式 → 描线颜色 → 描线宽度。
        ctx.check(fillStyleIndex >= 0 && fillColorIndex > fillStyleIndex && borderStyleIndex > fillColorIndex &&
                      borderColorIndex > borderStyleIndex && borderWidthIndex > borderColorIndex,
                  QStringLiteral("编辑图元卡按填充样式、填充颜色、描线样式、描线颜色、描线宽度排列"),
                  QStringLiteral("%1/%2/%3/%4/%5")
                      .arg(fillStyleIndex)
                      .arg(fillColorIndex)
                      .arg(borderStyleIndex)
                      .arg(borderColorIndex)
                      .arg(borderWidthIndex));
        // 新增的选中操作行统一排在「图层与高度」行下方。
        QWidget* selectionRows = panel.findChild<QWidget*>(QStringLiteral("ItemSelectionRowsCard"));
        QWidget* arrangeCard = panel.findChild<QWidget*>(QStringLiteral("ItemArrangeCard"));
        const int selectionIndex =
            content && content->layout() && selectionRows ? content->layout()->indexOf(selectionRows) : -1;
        const int arrangeIndex =
            content && content->layout() && arrangeCard ? content->layout()->indexOf(arrangeCard) : -1;
        ctx.check(arrangeIndex >= 0 && selectionIndex > arrangeIndex, QStringLiteral("新增操作行位于图层与高度行下方"),
                  QStringLiteral("arrange=%1 selection=%2").arg(arrangeIndex).arg(selectionIndex));
        ctx.check(ItemDockPanel::tr("Border color") == QStringLiteral("描线颜色") &&
                      ItemDockPanel::tr("Border width") == QStringLiteral("描线宽度") &&
                      ItemDockPanel::tr("Height +1") == QStringLiteral("高度 +1") &&
                      ItemDockPanel::tr("Layer and height") == QStringLiteral("图层与高度"),
                  QStringLiteral("编辑图元卡把颜色与宽度统一称为描线颜色 / 描线宽度"),
                  QStringLiteral("stroke=%1 height=%2")
                      .arg(ItemDockPanel::tr("Border color"))
                      .arg(ItemDockPanel::tr("Height +1")));

        // 选中只描边 / 非填充图元时隐藏填充两行，混入可填充图元再恢复。
        panelScene.setMode(ToolMode::Edit);
        auto* strokeOnlyShape = new ShapeItem(ShapeType::Line);
        strokeOnlyShape->setContentSize(QSizeF(140.0, 1.0));
        panelScene.addItemsWithUndo({strokeOnlyShape}, QStringLiteral("Add line"));
        strokeOnlyShape->setSelected(true);
        QApplication::processEvents();
        ctx.check(fillStyleRow && fillStyleRow->isHidden() && fillColorRow && fillColorRow->isHidden(),
                  QStringLiteral("选中不支持填充的图元时隐藏填充颜色与填充样式行"));
        auto* fillableShape = new ShapeItem(ShapeType::Rectangle);
        fillableShape->setContentSize(QSizeF(120.0, 80.0));
        panelScene.addItemsWithUndo({fillableShape}, QStringLiteral("Add rectangle"));
        fillableShape->setSelected(true);
        strokeOnlyShape->setSelected(true);
        QApplication::processEvents();
        ctx.check(fillStyleRow && !fillStyleRow->isHidden() && fillColorRow && !fillColorRow->isHidden(),
                  QStringLiteral("混入可填充图元时恢复填充颜色与填充样式行"));
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
        QFile::remove(dockConfig);
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

        QDockWidget* itemDock = dockWindow.findChild<QDockWidget*>(QStringLiteral("ItemDock"));
        ItemDockPanel* dockPanel =
            itemDock ? itemDock->findChild<ItemDockPanel*>(QStringLiteral("ItemPanel")) : nullptr;
        QMenu* dockCustomMenu = dockWindow.findChild<QMenu*>(QStringLiteral("CustomShapeMenu"));
        ctx.check(itemDock && dockPanel && !itemDock->isFloating() &&
                      dockWindow.dockWidgetArea(itemDock) == Qt::LeftDockWidgetArea && itemDock->isVisible(),
                  QStringLiteral("编辑图元卡首次启动显示并固定在窗口左侧"));
        const int preferredCardWidth = dockPanel ? dockPanel->preferredWidth() : 0;
        const int actualCardWidth = itemDock ? itemDock->width() : -1;
        // 宽度既要覆盖内容，也不能明显宽于内容，否则右侧会留下空白。
        const bool dockWidthFits =
            actualCardWidth >= preferredCardWidth - 2 && actualCardWidth <= preferredCardWidth + 2;
        ctx.check(itemDock && dockPanel && preferredCardWidth >= 260 && preferredCardWidth <= 320 && dockWidthFits,
                  QStringLiteral("编辑图元卡宽度贴合内容且不超出 320px"),
                  QStringLiteral("preferred=%1 actual=%2").arg(preferredCardWidth).arg(actualCardWidth));
        // 需求 7：自定义图形改为浮动面板，模板以按钮形式出现在标签页里；菜单只留触发器。
        dockWindow.openCustomShapePanel(nullptr);
        QApplication::processEvents();
        auto* shapePanel = dockWindow.findChild<CustomShapePanel*>(QStringLiteral("CustomShapePanel"));
        QStringList panelTexts;
        QTabWidget* shapeTabs = shapePanel ? shapePanel->tabWidget() : nullptr;
        if (shapePanel) {
            for (QToolButton* button :
                 shapePanel->findChildren<QToolButton*>(QStringLiteral("CustomShapeTemplateButton"))) {
                panelTexts.append(button->text());
            }
        }
        ctx.check(shapePanel && shapeTabs && shapeTabs->count() >= 1 &&
                      panelTexts.contains(QStringLiteral("dock-probe")),
                  QStringLiteral("自定义图形浮动面板按文件夹出标签页并列出模板"),
                  QStringLiteral("tabs=%1 texts=%2")
                      .arg(shapeTabs ? shapeTabs->count() : -1)
                      .arg(panelTexts.join(QStringLiteral("|"))));
        ctx.check(dockCustomMenu && dockCustomMenu->actions().size() == 1 &&
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
        ctx.check(!panelTitles.contains(QStringLiteral("空目录")) && panelTitles.contains(QStringLiteral("坏文件")),
                  QStringLiteral("空目录不出页、有坏文件的目录保留页"),
                  QStringLiteral("titles=%1").arg(panelTitles.join(QStringLiteral("|"))));
        ctx.check(brokenLabel && brokenLabel->styleSheet().contains(QStringLiteral("#9CA3AF")),
                  QStringLiteral("坏文件在页底以灰字提示"),
                  QStringLiteral("label=%1 style=%2")
                      .arg(brokenLabel ? QStringLiteral("yes") : QStringLiteral("no"))
                      .arg(brokenLabel ? brokenLabel->styleSheet() : QString()));
        const int itemsBeforeKey = dockWindow.scene()->canvasItemCount();
        bool keyInserted = false;
        if (shapePanel && keyTabIndex >= 0) {
            shapePanel->tabWidget()->setCurrentIndex(keyTabIndex);
            QApplication::processEvents();
            QList<QToolButton*> keyButtons = shapePanel->tabWidget()->currentWidget()->findChildren<QToolButton*>(
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
                              dockWindow.scene()->canvasItemCount() > itemsBeforeKey;
            }
        }
        ctx.check(keyInserted, QStringLiteral("方向键移动焦点、回车插入模板"),
                  QStringLiteral("items %1->%2").arg(itemsBeforeKey).arg(dockWindow.scene()->canvasItemCount()));
        // 插入会落到画布上，断言后立刻撤回，免得影响后面按选中项数量走的检查。
        if (keyInserted) {
            while (dockWindow.scene()->canvasItemCount() > itemsBeforeKey &&
                   dockWindow.scene()->undoStack()->canUndo()) {
                dockWindow.scene()->undoStack()->undo();
            }
            dockWindow.scene()->clearSelection();
            QApplication::processEvents();
        }
        ctx.check(dockWindow.findChild<QAction*>(QStringLiteral("ItemDockToggleAction")) != nullptr,
                  QStringLiteral("视图菜单提供编辑图元卡开关"));

        CanvasScene* dockScene = dockWindow.scene();
        // 卡片不再有常见图形行，插入改为走场景接口（等价于「插入」菜单插入矩形）；
        // 默认是查看模式，先切到编辑模式，图元才可被选中。
        CanvasView* dockView = dockWindow.findChild<CanvasView*>();
        if (dockView) {
            dockView->setMode(ToolMode::Edit);
        }
        QApplication::processEvents();
        const int beforeCount = dockScene->canvasItemCount();
        auto* insertedShape = new ShapeItem(ShapeType::Rectangle);
        insertedShape->setPos(QPointF(160.0, 140.0));
        insertedShape->setContentSize(QSizeF(140.0, 100.0));
        dockScene->addItemsWithUndo({insertedShape}, QStringLiteral("Add rectangle"));
        insertedShape->setSelected(true);
        QApplication::processEvents();
        ShapeItem* selectedShape = dockScene->selectedCanvasItems().isEmpty()
                                       ? nullptr
                                       : dynamic_cast<ShapeItem*>(dockScene->selectedCanvasItems().first());
        ctx.check(selectedShape == insertedShape && dockScene->canvasItemCount() == beforeCount + 1 &&
                      itemDock->isVisible(),
                  QStringLiteral("插入矩形后编辑图元卡随选中出现并保持显示"),
                  QStringLiteral("same=%1 count=%2/%3 visible=%4 sel=%5 inScene=%6")
                      .arg(selectedShape == insertedShape ? 1 : 0)
                      .arg(dockScene->canvasItemCount())
                      .arg(beforeCount + 1)
                      .arg(itemDock->isVisible() ? 1 : 0)
                      .arg(insertedShape->isSelected() ? 1 : 0)
                      .arg(dockScene->canvasItems().contains(insertedShape) ? 1 : 0));

        // 选中图元后可见行变多，卡片宽度保持启动时的值，不随选中来回跳动。
        QApplication::processEvents();
        const int selectedActualWidth = itemDock ? itemDock->width() : -1;
        ctx.check(qAbs(selectedActualWidth - preferredCardWidth) <= 2,
                  QStringLiteral("切换选中后编辑图元卡宽度保持不变"),
                  QStringLiteral("start=%1 actual=%2").arg(preferredCardWidth).arg(selectedActualWidth));

        QDockWidget* layerDock = dockWindow.findChild<QDockWidget*>(QStringLiteral("LayerDock"));
        QDockWidget* viewModeDock = dockWindow.findChild<QDockWidget*>(QStringLiteral("ViewModeDock"));
        QToolBar* modeBar = dockWindow.findChild<QToolBar*>(QStringLiteral("ModeToolBar"));
        ctx.check(viewModeDock && modeBar && viewModeDock->widget() == modeBar,
                  QStringLiteral("视图模式栏作为右上角卡片"));
        // 默认停靠在左侧，不属于悬浮窗口。
        ctx.check(itemDock && !itemDock->isFloating() && dockWindow.dockWidgetArea(itemDock) == Qt::LeftDockWidgetArea,
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
            // 需求 4：圆角上限改为短边一半，不能再按固定的 400 识别，按对象名取。
            QSpinBox* cornerSpin = dockPanel->findChild<QSpinBox*>(QStringLiteral("ItemValueCornerSpin"));
            for (QSpinBox* spin : valueSpins) {
                if (spin->maximum() == 100) {
                    opacitySpin = spin;
                }
            }
            const QList<QToolButton*> borderButtons =
                dockPanel->findChildren<QToolButton*>(QStringLiteral("ItemBorderStyleButton"));
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

            // 边框颜色 / 宽度同时是画笔的当前颜色与宽度，改完描边后画笔立即沿用。
            QWidget* borderWidthWidget = dockPanel->findChild<QWidget*>(QStringLiteral("ItemBorderWidthRow"));
            QSpinBox* borderWidthSpin =
                borderWidthWidget ? borderWidthWidget->findChild<QSpinBox*>(QStringLiteral("ItemBorderWidthSpin"))
                                  : nullptr;
            if (borderWidthSpin) {
                borderWidthSpin->setValue(9);
                QApplication::processEvents();
            }
            ctx.check(borderWidthSpin && borderWidthSpin->minimum() == 1 && borderWidthSpin->maximum() == 64 &&
                          borderWidthSpin->singleStep() == 1 && borderWidthSpin->suffix() == QStringLiteral(" px"),
                      QStringLiteral("卡片描线宽度与工具栏使用同一 SpinBox 规格"),
                      QStringLiteral("min=%1 max=%2 step=%3 suffix=%4")
                          .arg(borderWidthSpin ? borderWidthSpin->minimum() : -1)
                          .arg(borderWidthSpin ? borderWidthSpin->maximum() : -1)
                          .arg(borderWidthSpin ? borderWidthSpin->singleStep() : 0)
                          .arg(borderWidthSpin ? borderWidthSpin->suffix() : QStringLiteral("-")));
            ctx.check(qAbs(selectedShape->strokeWidth() - 9.0) < 0.01 && dockView &&
                          qAbs(dockView->brushWidth() - 9.0) < 0.01,
                      QStringLiteral("边框宽度同时成为画笔宽度"),
                      QStringLiteral("stroke=%1 brush=%2")
                          .arg(selectedShape->strokeWidth())
                          .arg(dockView ? dockView->brushWidth() : -1.0));
            QWidget* borderColorWidget = dockPanel->findChild<QWidget*>(QStringLiteral("ItemBorderColorRow"));
            const QList<QToolButton*> strokeSwatches =
                borderColorWidget ? borderColorWidget->findChildren<QToolButton*>(QStringLiteral("ItemColorSwatch"))
                                  : QList<QToolButton*>();
            const QColor brushColorTarget(QStringLiteral("#EF4444"));
            if (strokeSwatches.size() > 1) {
                strokeSwatches.at(1)->click();
                QApplication::processEvents();
            }
            ctx.check(selectedShape->strokeColor() == brushColorTarget && dockView &&
                          dockView->brushColor() == brushColorTarget,
                      QStringLiteral("边框颜色同时成为画笔颜色"),
                      QStringLiteral("stroke=%1 brush=%2")
                          .arg(selectedShape->strokeColor().name())
                          .arg(dockView ? dockView->brushColor().name() : QStringLiteral("-")));

            ctx.check(copyButton && deleteButton && rotateCwButton && rotateCcwButton && copyButton->isVisible() &&
                          deleteButton->isVisible() && rotateCwButton->isVisible() && rotateCcwButton->isVisible(),
                      QStringLiteral("单选时编辑图元卡列出复制 / 删除 / 旋转按钮"));
            ctx.check(alignRowButtons.size() == 8 && morphButtons.size() == 6 && groupButton && ungroupButton &&
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
            ctx.check(visibleCardButtons >= 4 && unboundCardButtons == 0,
                      QStringLiteral("卡片所有可见按钮都关联了具体动作"),
                      QStringLiteral("visible=%1 unbound=%2").arg(visibleCardButtons).arg(unboundCardButtons));

            // 卡片按钮不是摆设：逐一验证旋转与复制按钮真的执行主窗口动作，且可撤销。
            if (rotateCwButton) {
                const qreal rotationBefore = selectedShape->rotation();
                rotateCwButton->click();
                QApplication::processEvents();
                const qreal rotationAfter = selectedShape->rotation();
                dockScene->undoStack()->undo();
                QApplication::processEvents();
                ctx.check(
                    qAbs(rotationAfter - rotationBefore - 45.0) < 0.01 &&
                        qAbs(selectedShape->rotation() - rotationBefore) < 0.01,
                    QStringLiteral("卡片旋转按钮真正旋转图元且可撤销"),
                    QStringLiteral("%1->%2->%3").arg(rotationBefore).arg(rotationAfter).arg(selectedShape->rotation()));
                selectedShape->setSelected(true);
                QApplication::processEvents();
            }
            if (copyButton) {
                const int countBeforeCopy = dockScene->canvasItemCount();
                copyButton->click();
                QApplication::processEvents();
                const int countAfterCopy = dockScene->canvasItemCount();
                dockScene->undoStack()->undo();
                QApplication::processEvents();
                ctx.check(countAfterCopy == countBeforeCopy + 1 && dockScene->canvasItemCount() == countBeforeCopy,
                          QStringLiteral("卡片复制按钮真正复制图元且可撤销"),
                          QStringLiteral("%1->%2->%3")
                              .arg(countBeforeCopy)
                              .arg(countAfterCopy)
                              .arg(dockScene->canvasItemCount()));
                selectedShape->setSelected(true);
                QApplication::processEvents();
            }
            if (rotateCcwButton) {
                const qreal rotationBefore = selectedShape->rotation();
                rotateCcwButton->click();
                QApplication::processEvents();
                const qreal rotationAfter = selectedShape->rotation();
                dockScene->undoStack()->undo();
                QApplication::processEvents();
                ctx.check(
                    qAbs(rotationAfter - rotationBefore + 45.0) < 0.01 &&
                        qAbs(selectedShape->rotation() - rotationBefore) < 0.01,
                    QStringLiteral("卡片逆时针旋转按钮真正旋转图元且可撤销"),
                    QStringLiteral("%1->%2->%3").arg(rotationBefore).arg(rotationAfter).arg(selectedShape->rotation()));
                selectedShape->setSelected(true);
                QApplication::processEvents();
            }

            // 多选时卡片把对齐 8 项与合并图元 6 项直接展开成图标按钮行，并复用菜单栏同一批动作。
            auto* secondShape = new ShapeItem(ShapeType::Rectangle);
            secondShape->setPos(QPointF(420.0, 240.0));
            secondShape->setContentSize(QSizeF(120.0, 90.0));
            secondShape->setStyle(QColor(QStringLiteral("#1D4ED8")), QColor(29, 78, 216, 40), 3.0);
            dockScene->addItemsWithUndo({secondShape}, QStringLiteral("Add second shape"));
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
            ctx.check(visibleAlign == 6, QStringLiteral("两个图元时对齐行展开 6 个可用模式"),
                      QString::number(visibleAlign));
            ctx.check(visibleMorph == 5, QStringLiteral("多选时合并图元五项展开成一行"));
            ctx.check(groupButton->isVisible() && !ungroupButton->isVisible() &&
                          groupButton->property("cardActionText").toString() == QStringLiteral("图元分组"),
                      QStringLiteral("多选未分组时显示图元分组、不显示解散分组"));

            // 对齐行按钮真正调用对齐动作；对齐后可撤销并恢复两个图元。
            if (!alignRowButtons.isEmpty()) {
                alignRowButtons.first()->click();
                QApplication::processEvents();
                const bool aligned =
                    qAbs(selectedShape->sceneBoundingRect().left() - secondShape->sceneBoundingRect().left()) < 0.5;
                dockScene->undoStack()->undo();
                QApplication::processEvents();
                ctx.check(aligned, QStringLiteral("卡片对齐按钮真正对齐图元"),
                          QStringLiteral("left=%1/%2")
                              .arg(selectedShape->sceneBoundingRect().left())
                              .arg(secondShape->sceneBoundingRect().left()));
                selectedShape->setSelected(true);
                secondShape->setSelected(true);
                QApplication::processEvents();
            }
            // 合并图元行按钮真正执行布尔运算：联合后两个矩形合并成一个多边形，撤销恢复。
            if (!morphButtons.isEmpty()) {
                const int countBeforeMorph = dockScene->canvasItemCount();
                morphButtons.first()->click();
                QApplication::processEvents();
                bool hasComposite = false;
                for (CanvasItem* item : dockScene->canvasItems()) {
                    if (dynamic_cast<CompositeShapeItem*>(item)) {
                        hasComposite = true;
                        break;
                    }
                }
                const bool merged = dockScene->canvasItemCount() == countBeforeMorph - 1 && hasComposite;
                dockScene->undoStack()->undo();
                QApplication::processEvents();
                ctx.check(merged && dockScene->canvasItemCount() == countBeforeMorph,
                          QStringLiteral("卡片合并图元按钮真正合并图元且可撤销"),
                          QStringLiteral("count=%1->%2 composite=%3")
                              .arg(countBeforeMorph)
                              .arg(dockScene->canvasItemCount())
                              .arg(hasComposite ? 1 : 0));
                selectedShape->setSelected(true);
                secondShape->setSelected(true);
                QApplication::processEvents();
            }

            const int groupedCount = dockScene->groupSelectedItems();
            // 组合不改选中集合，重新点选一次让主窗口按新的组归属刷新动作可用状态。
            dockScene->clearSelection();
            selectedShape->setSelected(true);
            secondShape->setSelected(true);
            QApplication::processEvents();
            ctx.check(groupedCount == 2 && ungroupButton->isVisible() && !groupButton->isVisible() &&
                          ungroupButton->property("cardActionText").toString() == QStringLiteral("解散分组"),
                      QStringLiteral("选中整组时只显示解散分组"));

            // 组 + 组外图元混合选中：再套一层组合与取消组合同时可用（分组支持多层）。
            auto* thirdShape = new ShapeItem(ShapeType::Ellipse);
            thirdShape->setPos(QPointF(560.0, 320.0));
            thirdShape->setContentSize(QSizeF(100.0, 80.0));
            dockScene->addItemsWithUndo({thirdShape}, QStringLiteral("Add third shape"));
            selectedShape->setSelected(true);
            secondShape->setSelected(true);
            thirdShape->setSelected(true);
            QApplication::processEvents();
            ctx.check(groupButton->isVisible() && ungroupButton->isVisible() &&
                          groupButton->property("cardActionText").toString() == QStringLiteral("图元分组") &&
                          ungroupButton->property("cardActionText").toString() == QStringLiteral("解散分组"),
                      QStringLiteral("组与普通图元混合选中时图元分组和解散分组都显示"));

            // 删除按钮单独用一个临时图元验证，避免删掉后面还要复用的图元。
            auto* deleteProbe = new ShapeItem(ShapeType::Rectangle);
            deleteProbe->setPos(QPointF(640.0, 360.0));
            deleteProbe->setContentSize(QSizeF(80.0, 60.0));
            dockScene->addItemsWithUndo({deleteProbe}, QStringLiteral("Add delete probe"));
            dockScene->clearSelection();
            deleteProbe->setSelected(true);
            QApplication::processEvents();
            const int countBeforeDelete = dockScene->canvasItemCount();
            if (deleteButton) {
                deleteButton->click();
                QApplication::processEvents();
            }
            const int countAfterDelete = dockScene->canvasItemCount();
            dockScene->undoStack()->undo();
            QApplication::processEvents();
            ctx.check(countAfterDelete == countBeforeDelete - 1 && dockScene->canvasItemCount() == countBeforeDelete,
                      QStringLiteral("卡片删除按钮真正删除图元且可撤销"),
                      QStringLiteral("%1->%2->%3")
                          .arg(countBeforeDelete)
                          .arg(countAfterDelete)
                          .arg(dockScene->canvasItemCount()));


            // 曲线复用描线颜色 / 宽度 / 样式与透明度：只选曲线时描线样式行可用。
            auto* selectedStroke = new CurveItem();
            selectedStroke->setStrokePoints({QPointF(60.0, 300.0), QPointF(180.0, 300.0)},
                                            QColor(QStringLiteral("#2563EB")), 6.0);
            dockScene->addItemsWithUndo({selectedStroke}, QStringLiteral("Add stroke"));
            dockScene->clearSelection();
            selectedStroke->setSelected(true);
            QApplication::processEvents();

            QWidget* borderStyleWidget = dockPanel->findChild<QWidget*>(QStringLiteral("ItemBorderStyleRow"));
            ctx.check(borderStyleWidget && borderStyleWidget->isVisible(), QStringLiteral("只选曲线时显示描线样式行"));
            ctx.check(borderWidthSpin && borderWidthSpin->isEnabled() &&
                          qAbs(borderWidthSpin->value() - selectedStroke->strokeWidth()) < 0.01 && opacitySpin &&
                          opacitySpin->isEnabled(),
                      QStringLiteral("只选曲线时描线宽度与透明度可用并按曲线回显"),
                      QStringLiteral("width=%1 spin=%2")
                          .arg(selectedStroke->strokeWidth())
                          .arg(borderWidthSpin ? borderWidthSpin->value() : -1.0));

            if (strokeSwatches.size() > 1) {
                strokeSwatches.at(1)->click();
                QApplication::processEvents();
            }
            ctx.check(selectedStroke->strokeColor() == QColor(QStringLiteral("#EF4444")),
                      QStringLiteral("卡片描线颜色直接改变曲线颜色"), selectedStroke->strokeColor().name());
            dockScene->undoStack()->undo();
            QApplication::processEvents();
            ctx.check(selectedStroke->strokeColor() == QColor(QStringLiteral("#2563EB")),
                      QStringLiteral("撤销描线颜色恢复曲线原色"), selectedStroke->strokeColor().name());

            if (borderWidthSpin) {
                borderWidthSpin->setValue(11);
                QApplication::processEvents();
            }
            ctx.check(qAbs(selectedStroke->strokeWidth() - 11.0) < 0.01, QStringLiteral("卡片描线宽度直接改变曲线宽度"),
                      QString::number(selectedStroke->strokeWidth()));
            dockScene->undoStack()->undo();
            QApplication::processEvents();
            ctx.check(qAbs(selectedStroke->strokeWidth() - 6.0) < 0.01, QStringLiteral("撤销描线宽度恢复曲线原宽度"),
                      QString::number(selectedStroke->strokeWidth()));

            if (borderButtons.size() > 1) {
                const Qt::PenStyle styleBefore = selectedStroke->strokeStyle();
                borderButtons.at(1)->click();
                QApplication::processEvents();
                ctx.check(selectedStroke->strokeStyle() != styleBefore, QStringLiteral("曲线可通过编辑卡切换描线样式"));
                dockScene->undoStack()->undo();
                QApplication::processEvents();
                ctx.check(selectedStroke->strokeStyle() == styleBefore, QStringLiteral("撤销描线样式恢复曲线原样式"));
            }

            if (opacitySpin) {
                opacitySpin->setValue(55);
                QApplication::processEvents();
            }
            ctx.check(qAbs(selectedStroke->opacity() - 0.55) < 0.01, QStringLiteral("透明度作用到曲线"),
                      QString::number(selectedStroke->opacity()));
            dockScene->undoStack()->undo();
            QApplication::processEvents();
            ctx.check(qAbs(selectedStroke->opacity() - 1.0) < 0.01, QStringLiteral("撤销透明度恢复曲线原值"),
                      QString::number(selectedStroke->opacity()));

            selectedShape->setSelected(true);
            selectedStroke->setSelected(true);
            QApplication::processEvents();
            ctx.check(borderStyleWidget && borderStyleWidget->isVisible(),
                      QStringLiteral("图形与曲线混合选中时保留描线样式行"));
            if (opacitySpin) {
                opacitySpin->setValue(30);
                QApplication::processEvents();
            }
            ctx.check(
                qAbs(selectedShape->opacity() - 0.3) < 0.01 && qAbs(selectedStroke->opacity() - 0.3) < 0.01,
                QStringLiteral("透明度同时作用到图形和曲线"),
                QStringLiteral("shape=%1 stroke=%2").arg(selectedShape->opacity()).arg(selectedStroke->opacity()));
            dockScene->undoStack()->undo();
            QApplication::processEvents();
            ctx.check(qAbs(selectedShape->opacity() - 1.0) < 0.01 && qAbs(selectedStroke->opacity() - 1.0) < 0.01,
                      QStringLiteral("撤销透明度同时恢复图形和曲线"));

            // 多边形也要有填充 / 描线属性：选中后五行可见，按钮改动真正落到图层并可撤销。
            CompositeShapeItem::Layer polygonLayer;
            polygonLayer.path.addRect(0.0, 0.0, 140.0, 90.0);
            polygonLayer.fill = QColor(QStringLiteral("#3B82F6"));
            polygonLayer.stroke = QColor(QStringLiteral("#1E293B"));
            polygonLayer.strokeWidth = 4.0;
            polygonLayer.role = CompositeShapeItem::LayerRole::FilledArea;
            auto* polygon = new CompositeShapeItem({polygonLayer});
            polygon->setPos(QPointF(240.0, 240.0));
            dockScene->addItemsWithUndo({polygon}, QStringLiteral("Add polygon"));
            dockScene->clearSelection();
            polygon->setSelected(true);
            QApplication::processEvents();

            QWidget* fillStyleRow = dockPanel->findChild<QWidget*>(QStringLiteral("ItemFillStyleRow"));
            QWidget* fillColorRow = dockPanel->findChild<QWidget*>(QStringLiteral("ItemFillColorRow"));
            QWidget* borderColorRow = dockPanel->findChild<QWidget*>(QStringLiteral("ItemBorderColorRow"));
            QWidget* borderWidthRow = dockPanel->findChild<QWidget*>(QStringLiteral("ItemBorderWidthRow"));
            ctx.check(fillStyleRow && fillStyleRow->isVisible() && fillColorRow && fillColorRow->isVisible() &&
                          borderStyleWidget && borderStyleWidget->isVisible() && borderColorRow &&
                          borderColorRow->isVisible() && borderWidthRow && borderWidthRow->isVisible(),
                      QStringLiteral("选中多边形时填充与描线五行全部可见"));

            const FillStyle polygonFillBefore = polygon->fillStyle();
            if (fillButtons.size() > 2) {
                fillButtons.at(2)->click();
                QApplication::processEvents();
            }
            ctx.check(polygon->fillStyle() == FillStyle::Hachure, QStringLiteral("编辑卡填充样式作用到多边形"),
                      QString::number(static_cast<int>(polygon->fillStyle())));
            dockScene->undoStack()->undo();
            QApplication::processEvents();
            ctx.check(polygon->fillStyle() == polygonFillBefore, QStringLiteral("撤销填充样式恢复多边形原样式"));

            const QList<QToolButton*> fillSwatches =
                fillColorRow ? fillColorRow->findChildren<QToolButton*>(QStringLiteral("ItemColorSwatch"))
                             : QList<QToolButton*>();
            const QColor polygonFillBeforeColor = polygon->fillColor();
            if (fillSwatches.size() > 1) {
                fillSwatches.at(1)->click();
                QApplication::processEvents();
            }
            ctx.check(polygon->fillColor() == QColor(QStringLiteral("#EF4444")),
                      QStringLiteral("编辑卡填充颜色作用到多边形"), polygon->fillColor().name());
            dockScene->undoStack()->undo();
            QApplication::processEvents();
            ctx.check(polygon->fillColor() == polygonFillBeforeColor, QStringLiteral("撤销填充颜色恢复多边形原色"),
                      polygon->fillColor().name());

            const Qt::PenStyle polygonPenBefore = polygon->strokeStyle();
            if (borderButtons.size() > 1) {
                borderButtons.at(1)->click();
                QApplication::processEvents();
            }
            ctx.check(polygon->strokeStyle() != polygonPenBefore, QStringLiteral("编辑卡描线样式作用到多边形"));
            dockScene->undoStack()->undo();
            QApplication::processEvents();
            ctx.check(polygon->strokeStyle() == polygonPenBefore, QStringLiteral("撤销描线样式恢复多边形原样式"));

            const QColor polygonStrokeBefore = polygon->strokeColor();
            const QList<QToolButton*> polygonStrokeSwatches =
                borderColorRow ? borderColorRow->findChildren<QToolButton*>(QStringLiteral("ItemColorSwatch"))
                               : QList<QToolButton*>();
            if (polygonStrokeSwatches.size() > 1) {
                polygonStrokeSwatches.at(1)->click();
                QApplication::processEvents();
            }
            ctx.check(polygon->strokeColor() == QColor(QStringLiteral("#EF4444")),
                      QStringLiteral("编辑卡描线颜色作用到多边形"), polygon->strokeColor().name());
            dockScene->undoStack()->undo();
            QApplication::processEvents();
            ctx.check(polygon->strokeColor() == polygonStrokeBefore, QStringLiteral("撤销描线颜色恢复多边形原色"),
                      polygon->strokeColor().name());

            const qreal polygonWidthBefore = polygon->strokeWidth();
            QSpinBox* polygonWidthSpin =
                borderWidthRow ? borderWidthRow->findChild<QSpinBox*>(QStringLiteral("ItemBorderWidthSpin")) : nullptr;
            if (polygonWidthSpin) {
                polygonWidthSpin->setValue(9);
                QApplication::processEvents();
            }
            ctx.check(qAbs(polygon->strokeWidth() - 9.0) < 0.01, QStringLiteral("编辑卡描线宽度作用到多边形"),
                      QString::number(polygon->strokeWidth()));
            dockScene->undoStack()->undo();
            QApplication::processEvents();
            ctx.check(qAbs(polygon->strokeWidth() - polygonWidthBefore) < 0.01,
                      QStringLiteral("撤销描线宽度恢复多边形原宽度"), QString::number(polygon->strokeWidth()));

            // 属性窗同样识别多边形的填充 / 描线字段，确认后只占一条撤销记录。
            ItemPropertiesDialog polygonDialog(dockScene, polygon);
            QComboBox* dialogFillStyle =
                polygonDialog.findChild<QComboBox*>(QStringLiteral("ItemPropertiesFillStyleValue"));
            QSpinBox* dialogStrokeWidth =
                polygonDialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesStrokeWidthValue"));
            const FillStyle dialogFillBefore = polygon->fillStyle();
            const qreal dialogWidthBefore = polygon->strokeWidth();
            if (dialogFillStyle) {
                const int hachureIndex = dialogFillStyle->findData(static_cast<int>(FillStyle::Hachure));
                if (hachureIndex >= 0) {
                    dialogFillStyle->setCurrentIndex(hachureIndex);
                }
            }
            if (dialogStrokeWidth) {
                dialogStrokeWidth->setValue(12);
            }
            polygonDialog.accept();
            QApplication::processEvents();
            ctx.check(polygon->fillStyle() == FillStyle::Hachure && qAbs(polygon->strokeWidth() - 12.0) < 0.01,
                      QStringLiteral("属性窗填充样式与描线宽度作用到多边形"),
                      QStringLiteral("fill=%1 width=%2")
                          .arg(static_cast<int>(polygon->fillStyle()))
                          .arg(polygon->strokeWidth()));
            dockScene->undoStack()->undo();
            QApplication::processEvents();
            ctx.check(polygon->fillStyle() == dialogFillBefore &&
                          qAbs(polygon->strokeWidth() - dialogWidthBefore) < 0.01,
                      QStringLiteral("撤销属性窗修改同时恢复多边形填充与描线"));

            // 图形与多边形混合选中时，一次填充色修改只占一条撤销记录。
            dockScene->clearSelection();
            selectedShape->setSelected(true);
            polygon->setSelected(true);
            QApplication::processEvents();
            const QColor mixedShapeFillBefore = selectedShape->fillColor();
            const QColor mixedPolygonFillBefore = polygon->fillColor();
            const int mixedUndoBefore = dockScene->undoStack()->index();
            if (fillSwatches.size() > 2) {
                fillSwatches.at(2)->click();
                QApplication::processEvents();
            }
            const int mixedUndoAfter = dockScene->undoStack()->index();
            ctx.check(selectedShape->fillColor() != mixedShapeFillBefore &&
                          polygon->fillColor() != mixedPolygonFillBefore && mixedUndoAfter == mixedUndoBefore + 1,
                      QStringLiteral("混合选中图形与多边形时填充色只占一条撤销记录"),
                      QStringLiteral("undo=%1->%2 shape=%3->%4 polygon=%5->%6 selected=%7")
                          .arg(mixedUndoBefore)
                          .arg(mixedUndoAfter)
                          .arg(mixedShapeFillBefore.name())
                          .arg(selectedShape->fillColor().name())
                          .arg(mixedPolygonFillBefore.name())
                          .arg(polygon->fillColor().name())
                          .arg(dockScene->selectedCanvasItems().size()));
            dockScene->undoStack()->undo();
            QApplication::processEvents();
            ctx.check(selectedShape->fillColor() == mixedShapeFillBefore &&
                          polygon->fillColor() == mixedPolygonFillBefore,
                      QStringLiteral("撤销混合填充同时恢复图形与多边形原色"));

            // 文字 / 图片内容行与卡片操作按钮：只对相应类型显示，并且写入能撤销。
            auto* cardText = new TextItem();
            cardText->setTextFormat(QStringLiteral("卡片文字"), QFont(QStringLiteral("Arial"), 12),
                                    QColor(QStringLiteral("#1E293B")));
            cardText->setPos(QPointF(420.0, 430.0));
            dockScene->addItemsWithUndo({cardText}, QStringLiteral("Add card text"));
            dockScene->clearSelection();
            cardText->setSelected(true);
            QApplication::processEvents();

            QWidget* textContentRow = itemDock->findChild<QWidget*>(QStringLiteral("ItemTextContentRow"));
            QWidget* textFontRow = itemDock->findChild<QWidget*>(QStringLiteral("ItemTextFontRow"));
            QWidget* textColorRow = itemDock->findChild<QWidget*>(QStringLiteral("ItemTextColorRow"));
            QWidget* imageRow = itemDock->findChild<QWidget*>(QStringLiteral("ItemImageRow"));
            QToolButton* propertiesButton = itemDock->findChild<QToolButton*>(QStringLiteral("ItemPropertiesButton"));
            QToolButton* smoothButton = itemDock->findChild<QToolButton*>(QStringLiteral("ItemSmoothButton"));
            ctx.check(textContentRow && textContentRow->isVisible() && textFontRow && textFontRow->isVisible() &&
                          textColorRow && textColorRow->isVisible(),
                      QStringLiteral("选中文字时显示正文 / 字体 / 颜色行"));
            ctx.check(imageRow && !imageRow->isVisible(), QStringLiteral("选中文字时隐藏图片行"));
            ctx.check(propertiesButton && propertiesButton->isVisible(), QStringLiteral("选中文字时显示图元属性按钮"));
            ctx.check(smoothButton && !smoothButton->isVisible(), QStringLiteral("选中文字时隐藏简化曲线按钮"));

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
            ctx.check(propertyDialogOpened, QStringLiteral("卡片图元属性按钮真正打开属性窗"));

            // 透明度不仅作用于图形和曲线，文字等所有图元都走同一属性。
            if (opacitySpin) {
                opacitySpin->setValue(40);
                QApplication::processEvents();
            }
            ctx.check(qAbs(cardText->itemOpacity() - 0.4) < 0.01, QStringLiteral("卡片透明度作用于文字图元"),
                      QString::number(cardText->itemOpacity()));
            dockScene->undoStack()->undo();
            QApplication::processEvents();
            ctx.check(qAbs(cardText->itemOpacity() - 1.0) < 0.01, QStringLiteral("撤销透明度恢复文字原值"),
                      QString::number(cardText->itemOpacity()));

            auto* textEdit = itemDock->findChild<QLineEdit*>(QStringLiteral("ItemTextContentEdit"));
            if (textEdit) {
                textEdit->setText(QStringLiteral("改后文字"));
                QMetaObject::invokeMethod(textEdit, "editingFinished", Qt::DirectConnection);
                QApplication::processEvents();
            }
            ctx.check(cardText->text() == QStringLiteral("改后文字"), QStringLiteral("卡片正文直接修改文字"),
                      cardText->text());
            dockScene->undoStack()->undo();
            QApplication::processEvents();
            ctx.check(cardText->text() == QStringLiteral("卡片文字"), QStringLiteral("撤销正文恢复原文"),
                      cardText->text());

            auto* fontSizeSpin = itemDock->findChild<QSpinBox*>(QStringLiteral("ItemTextFontSizeSpin"));
            if (fontSizeSpin) {
                fontSizeSpin->setValue(20);
                QApplication::processEvents();
            }
            ctx.check(cardText->textFont().pointSize() == 20, QStringLiteral("卡片字号直接修改文字字体"),
                      QString::number(cardText->textFont().pointSize()));
            dockScene->undoStack()->undo();
            QApplication::processEvents();
            ctx.check(cardText->textFont().pointSize() == 12, QStringLiteral("撤销字号恢复原字体"),
                      QString::number(cardText->textFont().pointSize()));

            const QList<QToolButton*> textSwatches =
                itemDock->findChildren<QToolButton*>(QStringLiteral("ItemTextColorSwatch"));
            if (textSwatches.size() > 1) {
                textSwatches.at(1)->click();
                QApplication::processEvents();
            }
            ctx.check(cardText->textColor() == QColor(QStringLiteral("#EF4444")),
                      QStringLiteral("卡片文字颜色直接修改文字"), cardText->textColor().name());
            dockScene->undoStack()->undo();
            QApplication::processEvents();
            ctx.check(cardText->textColor() == QColor(QStringLiteral("#1E293B")),
                      QStringLiteral("撤销文字颜色恢复原色"), cardText->textColor().name());

            auto* cardImage = new ImageItem();
            QPixmap imagePixmap(32, 24);
            imagePixmap.fill(QColor(QStringLiteral("#2563EB")));
            cardImage->setImage(imagePixmap, QStringLiteral("图A"));
            cardImage->setPos(QPointF(620.0, 430.0));
            dockScene->addItemsWithUndo({cardImage}, QStringLiteral("Add card image"));
            dockScene->clearSelection();
            cardImage->setSelected(true);
            QApplication::processEvents();
            ctx.check(imageRow && imageRow->isVisible(), QStringLiteral("选中图片时显示图片行"));
            ctx.check(textContentRow && !textContentRow->isVisible() && textFontRow && !textFontRow->isVisible() &&
                          textColorRow && !textColorRow->isVisible(),
                      QStringLiteral("选中图片时隐藏文字行"));

            auto* imageNameEdit = itemDock->findChild<QLineEdit*>(QStringLiteral("ItemImageNameEdit"));
            if (imageNameEdit) {
                imageNameEdit->setText(QStringLiteral("图B"));
                QMetaObject::invokeMethod(imageNameEdit, "editingFinished", Qt::DirectConnection);
                QApplication::processEvents();
            }
            ctx.check(cardImage->imageName() == QStringLiteral("图B"), QStringLiteral("卡片图片名称直接修改图元"),
                      cardImage->imageName());
            dockScene->undoStack()->undo();
            QApplication::processEvents();
            ctx.check(cardImage->imageName() == QStringLiteral("图A"), QStringLiteral("撤销图片名称恢复原名"),
                      cardImage->imageName());

            QPixmap replacement(64, 48);
            replacement.fill(QColor(QStringLiteral("#16A34A")));
            dockPanel->replaceSelectedImageWith(replacement, QStringLiteral("替换图"));
            QApplication::processEvents();
            ctx.check(cardImage->image().size() == QSize(64, 48) && cardImage->imageName() == QStringLiteral("替换图"),
                      QStringLiteral("卡片替换图片同时更新像素与名称"),
                      QStringLiteral("%1x%2 %3")
                          .arg(cardImage->image().width())
                          .arg(cardImage->image().height())
                          .arg(cardImage->imageName()));
            dockScene->undoStack()->undo();
            QApplication::processEvents();
            ctx.check(cardImage->image().size() == QSize(32, 24) && cardImage->imageName() == QStringLiteral("图A"),
                      QStringLiteral("撤销替换图片恢复原像素与名称"),
                      QStringLiteral("%1x%2 %3")
                          .arg(cardImage->image().width())
                          .arg(cardImage->image().height())
                          .arg(cardImage->imageName()));

            // 图片不参与合并图元：文字与图片混合选中时只保留对齐 / 分组，合并图元入口禁用。
            cardText->setSelected(true);
            cardImage->setSelected(true);
            QApplication::processEvents();
            ctx.check(!alignRowButtons.isEmpty() && alignRowButtons.first()->isVisible() && !morphButtons.isEmpty() &&
                          !morphButtons.first()->isVisible() && groupButton && groupButton->isVisible(),
                      QStringLiteral("文字与图片混合选中隐藏合并图元入口，保留对齐 / 分组"));

            // 只选曲线时显示简化曲线按钮，点击后平滑结果进入撤销栈。
            auto* smoothStroke = new CurveItem();
            QVector<QPointF> jitterPoints;
            for (int i = 0; i <= 8; ++i) {
                jitterPoints.append(QPointF(520.0 + i * 6.0, 470.0 + (i % 2) * 3.0));
            }
            smoothStroke->setStrokePoints(jitterPoints, QColor(QStringLiteral("#334155")), 5.0);
            dockScene->addItemsWithUndo({smoothStroke}, QStringLiteral("Add smooth stroke"));
            dockScene->clearSelection();
            QApplication::processEvents();
            QWidget* fillStyleRowWhenEmpty = itemDock->findChild<QWidget*>(QStringLiteral("ItemFillStyleRow"));
            QWidget* borderWidthRowWhenEmpty = itemDock->findChild<QWidget*>(QStringLiteral("ItemBorderWidthRow"));
            ctx.check(fillStyleRowWhenEmpty && fillStyleRowWhenEmpty->isHidden() && borderWidthRowWhenEmpty &&
                          borderWidthRowWhenEmpty->isHidden(),
                      QStringLiteral("无图元选中时隐藏图元编辑选项"));
            smoothStroke->setSelected(true);
            QApplication::processEvents();
            auto* smoothingSpin = itemDock->window()->findChild<QSpinBox*>(QStringLiteral("BrushSmoothingSpin"));
            if (smoothingSpin) {
                smoothingSpin->setValue(10.0);
                QApplication::processEvents();
            }
            ctx.check(smoothButton && smoothButton->isVisible(), QStringLiteral("选中曲线时显示简化曲线按钮"));
            const int undoIndexBeforeSmooth = dockScene->undoStack()->index();
            if (smoothButton) {
                smoothButton->click();
                QApplication::processEvents();
            }
            ctx.check(dockScene->undoStack()->index() > undoIndexBeforeSmooth,
                      QStringLiteral("卡片简化曲线按钮真正执行简化且可撤销"),
                      QStringLiteral("index=%1->%2").arg(undoIndexBeforeSmooth).arg(dockScene->undoStack()->index()));

            // 悬浮卡片跟随选中状态：清空选中后隐藏，重新选中再出现。
            itemDock->setFloating(true);
            QApplication::processEvents();
            dockScene->clearSelection();
            QApplication::processEvents();
            ctx.check(!itemDock->isVisible(), QStringLiteral("悬浮时清空选中隐藏编辑图元卡"));
            selectedShape->setSelected(true);
            QApplication::processEvents();
            ctx.check(itemDock->isVisible(), QStringLiteral("悬浮时有选中显示编辑图元卡"));

            // 拖回主窗口停靠后卡片常驻，不再随选中状态整张显隐。
            itemDock->setFloating(false);
            QApplication::processEvents();
            ctx.check(!itemDock->isFloating() && itemDock->isVisible(), QStringLiteral("拖回主窗口后停靠并保持显示"));
            dockScene->clearSelection();
            QApplication::processEvents();
            ctx.check(itemDock->isVisible(), QStringLiteral("停靠时清空选中不隐藏编辑图元卡"));
            ctx.check(layerDock && layerDock->height() < dockWindow.height() - 100,
                      QStringLiteral("编辑图元卡停靠时图层卡不撑满窗口高度"));
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
