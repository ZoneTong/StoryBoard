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


void selftest::RunCustomShapeDockTests(Context& ctx)
{
    // 需求：选中图元工具卡（边框样式 / 透明度 / 矩形圆角 / 图层与高度）与画布格式自定义图形。
    {
        RectangleItem styled;
        styled.SetContentSize(QSizeF(200, 120));
        styled.SetStyle(QColor(QStringLiteral("#1E293B")), QColor(QStringLiteral("#F97316")), 6.0);
        styled.SetStrokeStyle(Qt::DashLine);
        styled.SetItemOpacity(0.42);
        styled.SetCornerRadius(18.0);

        RectangleItem reloaded;
        reloaded.FromJson(styled.ToJson());
        ctx.Check(reloaded.StrokeStyle() == Qt::DashLine && qAbs(reloaded.ItemOpacity() - 0.42) < 0.001 &&
                      qAbs(reloaded.CornerRadius() - 18.0) < 0.001,
                  QStringLiteral("图形边框样式、透明度与圆角随画布保存"),
                  QStringLiteral("style=%1 opacity=%2 radius=%3")
                      .arg(static_cast<int>(reloaded.StrokeStyle()))
                      .arg(reloaded.ItemOpacity())
                      .arg(reloaded.CornerRadius()));

        const int squareElements =
            ShapeAbstractItem::PathFor(ShapeType::RECTANGLE, QRectF(0, 0, 100, 80), 0.0).elementCount();
        const int roundElements =
            ShapeAbstractItem::PathFor(ShapeType::RECTANGLE, QRectF(0, 0, 100, 80), 20.0).elementCount();
        ctx.Check(roundElements > squareElements, QStringLiteral("矩形圆角半径改变轮廓"),
                  QStringLiteral("square=%1 round=%2").arg(squareElements).arg(roundElements));

        QPainterPath unit;
        unit.addRect(QRectF(0.0, 0.0, 1.0, 1.0));
        RectangleItem custom;
        custom.SetCustomShape(QStringLiteral("Badge"), unit);
        custom.SetContentSize(QSizeF(160, 100));
        RectangleItem customReloaded;
        customReloaded.FromJson(custom.ToJson());
        const QRectF customBounds = customReloaded.CurrentPath(QRectF(0, 0, 160, 100)).boundingRect();
        ctx.Check(customReloaded.IsCustomShape() && customReloaded.DisplayName() == QStringLiteral("Badge") &&
                      qAbs(customBounds.width() - 160.0) < 0.01 && qAbs(customBounds.height() - 100.0) < 0.01,
                  QStringLiteral("自定义图形轮廓随画布保存并按内容矩形缩放"),
                  QStringLiteral("custom=%1 name=%2 w=%3")
                      .arg(customReloaded.IsCustomShape() ? 1 : 0)
                      .arg(customReloaded.DisplayName())
                      .arg(customBounds.width()));
        customReloaded.SetCustomShape(QString(), QPainterPath());
        ctx.Check(!customReloaded.IsCustomShape(), QStringLiteral("切换内置图形时清除自定义轮廓"));

        // 自定义图形目录：配置同级的 shapes 子目录，扫描 .sbcanvas 并按文件名命名。
        const QString shapesConfig = QDir(ctx.workDirectory).filePath(QStringLiteral("custom-shape-config.ini"));
        qputenv("STORYBOARD_CONFIG", shapesConfig.toLocal8Bit());
        const QString shapesDirectory = AppSettings::ShapesDir();
        ctx.Check(QFileInfo(shapesDirectory).absolutePath() == QFileInfo(shapesConfig).absolutePath() &&
                      QFileInfo(shapesDirectory).fileName() == QStringLiteral("shapes"),
                  QStringLiteral("自定义图形目录位于配置文件同级的 shapes 子目录"), shapesDirectory);

        StarItem templateShape;
        templateShape.setPos(QPointF(12, 18));
        templateShape.SetContentSize(QSizeF(160, 100));
        QJsonObject templateRoot;
        templateRoot.insert(QStringLiteral("format"), QStringLiteral("StoryBoardCanvas"));
        templateRoot.insert(QStringLiteral("items"), QJsonArray{templateShape.ToJson()});
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
        const QList<CustomShapeTemplate> templates = LoadCustomShapeTemplates(shapesDirectory, &templateErrors);
        const CustomShapeTemplate* probeTemplate = nullptr;
        for (const CustomShapeTemplate& entry : templates) {
            if (entry.name == QStringLiteral("probe-badge")) {
                probeTemplate = &entry;
            }
        }
        const bool templateOk = probeTemplate != nullptr && templateErrors.isEmpty();
        const QRectF unitBounds = templateOk ? probeTemplate->unitPath.boundingRect() : QRectF();
        // 需求 7：归一化预留 2% 内边距，轮廓落在 0.02–0.98 之间而不是贴边。
        ctx.Check(templateOk && probeTemplate->aspectRatio > 1.0 && unitBounds.left() > 0.0 &&
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
        const QString bundledDir = AppSettings::BundledShapesDir();
        {
            // 随包分发的示例模板必须能解析，否则用户装完看不到「自定义图形」示例。
            // 顶层只放说明文件，示例模板全部分布在四个分类子目录里。
            const QString sampleDir =
                QDir(QStringLiteral(STORYBOARD_SOURCE_DIR)).filePath(QStringLiteral("examples/shapes"));
            const QStringList expectedFolders = {QStringLiteral("小动物"), QStringLiteral("自然"),
                                                 QStringLiteral("物件"), QStringLiteral("人物与精灵")};
            QStringList sampleErrors;
            QList<CustomShapeTemplate> samples;
            for (const QString& folder : expectedFolders) {
                samples.append(LoadCustomShapeTemplates(QDir(sampleDir).filePath(folder), &sampleErrors));
            }
            ctx.Check(
                !samples.isEmpty() && sampleErrors.isEmpty(), QStringLiteral("随包示例模板可解析并出现在自定义图形里"),
                QStringLiteral("count=%1 errors=%2").arg(samples.size()).arg(sampleErrors.join(QStringLiteral(";"))));

            // 需求 8：25 个儿童故事简笔画模板分四个子目录随包分发。
            QStringList allNames;
            QStringList childErrors;
            int folderCount = 0;
            for (const QString& folder : expectedFolders) {
                const QString childDir = QDir(sampleDir).filePath(folder);
                if (!QDir(childDir).exists()) {
                    continue;
                }
                ++folderCount;
                for (const CustomShapeTemplate& entry : LoadCustomShapeTemplates(childDir, &childErrors)) {
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
            ctx.Check(folderCount == 4 && namesMatch && childErrors.isEmpty(),
                      QStringLiteral("四个子目录共 25 个儿童故事模板且名称齐全"),
                      QStringLiteral("folders=%1 count=%2 errors=%3")
                          .arg(folderCount)
                          .arg(allNames.size())
                          .arg(childErrors.join(QStringLiteral(";"))));
        }
        const QStringList scanDirs = AppSettings::ShapesDirs();
        ctx.Check(!scanDirs.isEmpty() && scanDirs.contains(shapesDirectory) && scanDirs.contains(bundledDir),
                  QStringLiteral("自定义图形同时扫描配置文件同级与部署目录旁的 shapes"),
                  QStringLiteral("dirs=%1").arg(scanDirs.join(QStringLiteral("|"))));
        {
            // 合并多目录时用临时目录，别往真实部署目录里写测试文件，否则包里会留下残留。
            const QString secondDir = QDir(ctx.workDirectory).filePath(QStringLiteral("deployed-shapes"));
            QDir().mkpath(secondDir);
            QJsonObject deployedRoot;
            deployedRoot.insert(QStringLiteral("format"), QStringLiteral("StoryBoardCanvas"));
            EllipseItem deployedShape;
            deployedShape.SetContentSize(QSizeF(90, 90));
            deployedRoot.insert(QStringLiteral("items"), QJsonArray{deployedShape.ToJson()});
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
                LoadCustomShapeTemplatesFromDirs({shapesDirectory, secondDir}, &mergedErrors);
            int deployedCount = 0;
            QString winnerPath;
            for (const CustomShapeTemplate& entry : merged) {
                if (entry.name == QStringLiteral("deployed-leaf")) {
                    ++deployedCount;
                    winnerPath = entry.filePath;
                }
            }
            ctx.Check(deployedCount == 1 &&
                          QFileInfo(winnerPath).absolutePath() == QDir(shapesDirectory).absolutePath(),
                      QStringLiteral("同名模板只保留配置文件同级的那一份"),
                      QStringLiteral("count=%1 winner=%2").arg(deployedCount).arg(winnerPath));
            QFile::remove(deployedPath);
            QFile::remove(duplicateUser.fileName());
        }

        CanvasScene panelScene;
        PropertiesPanel panel(&panelScene);
        // 紧凑化后不再有折叠分组小标题：断言整卡只由标题行 + 内容滚动区组成，
        // 颜色 / 填充 / 边框 / 透明度 / 圆角 / 图层各行控件直接平铺在卡内。
        ctx.Check(panel.findChildren<QToolButton*>(QStringLiteral("ItemSectionToggle")).isEmpty(),
                  QStringLiteral("编辑面板已去掉折叠分组小标题"));
        ctx.Check(panel.findChildren<QToolButton*>(QStringLiteral("ItemShapeButton")).isEmpty() &&
                      panel.findChildren<QToolButton*>(QStringLiteral("ItemCustomShapeButton")).isEmpty(),
                  QStringLiteral("编辑面板已移除常见图形展开行"));
        ctx.Check(panel.findChildren<QToolButton*>(QStringLiteral("ItemColorSwatch")).size() >= 16 &&
                      panel.findChildren<QToolButton*>(QStringLiteral("ItemFillStyleButton")).size() == 9 &&
                      panel.findChildren<QToolButton*>(QStringLiteral("ItemBorderStyleButton")).size() == 5,
                  QStringLiteral("编辑面板平铺颜色 / 填充 / 边框等工具"),
                  QStringLiteral("colors=%1 fills=%2 borders=%3")
                      .arg(panel.findChildren<QToolButton*>(QStringLiteral("ItemColorSwatch")).size())
                      .arg(panel.findChildren<QToolButton*>(QStringLiteral("ItemFillStyleButton")).size())
                      .arg(panel.findChildren<QToolButton*>(QStringLiteral("ItemBorderStyleButton")).size()));
        ctx.Check(panel.findChildren<QToolButton*>(QStringLiteral("ItemLayerUpButton")).size() == 1 &&
                      panel.findChildren<QToolButton*>(QStringLiteral("ItemLayerDownButton")).size() == 1 &&
                      panel.findChildren<QToolButton*>(QStringLiteral("ItemHeightUpButton")).size() == 1 &&
                      panel.findChildren<QToolButton*>(QStringLiteral("ItemHeightDownButton")).size() == 1,
                  QStringLiteral("工具卡提供图层与层内高度 ±1 按钮"));

        QWidget* content = panel.findChild<QWidget*>(QStringLiteral("PropertiesPanelContent"));
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
        ctx.Check(fillStyleIndex >= 0 && fillColorIndex > fillStyleIndex && borderStyleIndex > fillColorIndex &&
                      borderColorIndex > borderStyleIndex && borderWidthIndex > borderColorIndex,
                  QStringLiteral("编辑面板按填充样式、填充颜色、描线样式、描线颜色、描线宽度排列"),
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
        ctx.Check(arrangeIndex >= 0 && selectionIndex > arrangeIndex, QStringLiteral("新增操作行位于图层与高度行下方"),
                  QStringLiteral("arrange=%1 selection=%2").arg(arrangeIndex).arg(selectionIndex));
        ctx.Check(PropertiesPanel::tr("Border color") == QStringLiteral("描线颜色") &&
                      PropertiesPanel::tr("Border width") == QStringLiteral("描线宽度") &&
                      PropertiesPanel::tr("Height +1") == QStringLiteral("高度 +1") &&
                      PropertiesPanel::tr("Layer and height") == QStringLiteral("图层与高度"),
                  QStringLiteral("编辑面板把颜色与宽度统一称为描线颜色 / 描线宽度"),
                  QStringLiteral("stroke=%1 height=%2")
                      .arg(PropertiesPanel::tr("Border color"))
                      .arg(PropertiesPanel::tr("Height +1")));

        // 选中只描边 / 非填充图元时隐藏填充两行，混入可填充图元再恢复。
        panelScene.SetMode(ToolMode::EDIT);
        auto* strokeOnlyShape = new StraightLineItem();
        strokeOnlyShape->SetEndpoints(QPointF(40.0, 40.0), QPointF(180.0, 40.0), QColor(Qt::black), 1.0);
        panelScene.AddItemsWithUndo({strokeOnlyShape}, QStringLiteral("Add line"));
        strokeOnlyShape->setSelected(true);
        QApplication::processEvents();
        ctx.Check(fillStyleRow && fillStyleRow->isHidden() && fillColorRow && fillColorRow->isHidden(),
                  QStringLiteral("选中不支持填充的图元时隐藏填充颜色与填充样式行"));
        auto* fillableShape = ShapeFactory::Create(ShapeType::RECTANGLE);
        fillableShape->SetContentSize(QSizeF(120.0, 80.0));
        panelScene.AddItemsWithUndo({fillableShape}, QStringLiteral("Add rectangle"));
        fillableShape->setSelected(true);
        strokeOnlyShape->setSelected(true);
        QApplication::processEvents();
        ctx.Check(fillStyleRow && fillStyleRow->isHidden() && fillColorRow && fillColorRow->isHidden(),
                  QStringLiteral("混入可填充图元时按属性交集隐藏填充颜色与填充样式行"));
        bool priorityTipFound = false;
        for (QToolButton* rowIcon : panel.findChildren<QToolButton*>(QStringLiteral("ItemRowIcon"))) {
            if (rowIcon->toolTip().contains(QStringLiteral("优先级"))) {
                priorityTipFound = true;
                break;
            }
        }
        ctx.Check(priorityTipFound, QStringLiteral("工具卡标明优先级先按图层再按层内高度"));

        qunsetenv("STORYBOARD_CONFIG");
    }
}
