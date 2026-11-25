#include "app/I18n.h"
#include "app/Settings.h"
#include "canvas/CanvasView.h"
#include "io/CustomShapeTemplates.h"
#include "items/CanvasTypes.h"
#include "items/lines/CurveItem.h"
#include "items/polygon/PolygonItem.h"
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


void selftest::RunLayerExportPanelTests(Context& ctx)
{
    {
        // 需求 15：图层面板选中行有高亮，且重叠图元优先命中已选图层。
        CanvasScene styleScene;
        LayerPanel stylePanel(&styleScene);
        auto* layerList = stylePanel.findChild<QListWidget*>();
        ctx.Check(layerList && layerList->count() >= 2, QStringLiteral("图层面板包含用户图层与背景行"));
        QListWidgetItem* selectedEntry = nullptr;
        for (int row = 0; row < layerList->count(); ++row) {
            QListWidgetItem* entry = layerList->item(row);
            if (entry->data(Qt::UserRole).toInt() == styleScene.ActiveLayerId()) {
                selectedEntry = entry;
            }
        }
        // 高亮改由 QSS 的 selected 属性驱动，这里校验选中行确实带上了该属性。
        ctx.Check(selectedEntry && layerList->itemWidget(selectedEntry) &&
                      layerList->itemWidget(selectedEntry)->property("selected").toBool(),
                  QStringLiteral("选中的图层行带背景高亮"));
        ctx.Check(styleScene.PreferredLayerIds().contains(styleScene.ActiveLayerId()),
                  QStringLiteral("图层面板选中图层同步为首选命中图层"));

        CanvasScene overlapScene;
        overlapScene.SetMode(ToolMode::EDIT);
        const int upperLayerId = overlapScene.CreateLayer(QStringLiteral("上层"));
        auto* lowerItem = ShapeFactory::Create(ShapeType::RECTANGLE);
        lowerItem->SetLayerId(CanvasScene::BackgroundLayerId + 1);
        lowerItem->SetStyle(QColor(QStringLiteral("#2563EB")), QColor(Qt::transparent), 4.0);
        lowerItem->setPos(QPointF(200, 170));
        lowerItem->SetContentSize(QSizeF(200, 140));
        auto* upperItem = ShapeFactory::Create(ShapeType::RECTANGLE);
        upperItem->SetLayerId(upperLayerId);
        upperItem->SetStyle(QColor(QStringLiteral("#DC2626")), QColor(Qt::transparent), 4.0);
        upperItem->setPos(QPointF(200, 170));
        upperItem->SetContentSize(QSizeF(200, 140));
        overlapScene.AddItemsWithUndo({lowerItem, upperItem}, QStringLiteral("重叠图元"));

        CanvasView overlapView(&overlapScene);
        overlapView.resize(640, 460);
        overlapView.SetMode(ToolMode::EDIT);
        overlapView.centerOn(QPointF(300, 240));
        QApplication::processEvents();
        const QPoint overlapPoint = overlapView.mapFromScene(QPointF(300, 240));

        overlapScene.clearSelection();
        overlapScene.SetPreferredLayerIds({CanvasScene::BackgroundLayerId + 1});
        SendMouseEvent(overlapView.viewport(), QEvent::MouseButtonPress, overlapPoint, Qt::LeftButton, Qt::LeftButton);
        SendMouseEvent(overlapView.viewport(), QEvent::MouseButtonRelease, overlapPoint, Qt::LeftButton, Qt::NoButton);
        const QList<CanvasItem*> lowerSelection = overlapScene.SelectedCanvasItems();
        ctx.Check(lowerSelection.size() == 1 && lowerSelection.first() == lowerItem,
                  QStringLiteral("重叠区域优先选中已选图层中的图元"),
                  QStringLiteral("selected=%1").arg(lowerSelection.size()));

        overlapScene.clearSelection();
        overlapScene.SetPreferredLayerIds({upperLayerId});
        SendMouseEvent(overlapView.viewport(), QEvent::MouseButtonPress, overlapPoint, Qt::LeftButton, Qt::LeftButton);
        SendMouseEvent(overlapView.viewport(), QEvent::MouseButtonRelease, overlapPoint, Qt::LeftButton, Qt::NoButton);
        const QList<CanvasItem*> upperSelection = overlapScene.SelectedCanvasItems();
        ctx.Check(upperSelection.size() == 1 && upperSelection.first() == upperItem,
                  QStringLiteral("切换已选图层后优先选中该层图元"));

        // 需求 16：批量高度调整只占一条撤销记录。
        overlapScene.clearSelection();
        lowerItem->setSelected(true);
        upperItem->setSelected(true);
        QUndoStack heightUndo;
        overlapScene.SetUndoStack(&heightUndo);
        const int lowerHeightBefore = lowerItem->ZOrder();
        const int upperHeightBefore = upperItem->ZOrder();
        const int heightIndexBefore = heightUndo.index();
        const int heightChanged = overlapScene.AdjustSelectedHeights(1);
        ctx.Check(heightChanged == 2 && lowerItem->ZOrder() == lowerHeightBefore + 1 &&
                      upperItem->ZOrder() == upperHeightBefore + 1,
                  QStringLiteral("快捷键批量提升选中图元高度"));
        ctx.Check(heightUndo.index() == heightIndexBefore + 1, QStringLiteral("批量高度调整只生成一条撤销记录"));
        heightUndo.undo();
        ctx.Check(lowerItem->ZOrder() == lowerHeightBefore && upperItem->ZOrder() == upperHeightBefore,
                  QStringLiteral("批量高度调整可整体撤销"));
        overlapScene.SetUndoStack(nullptr);
    }

    {
        // 需求 27：合并可见图层导出 / 每个可见图层单独导出。
        CanvasScene layerExportScene;
        CanvasBackground exportBackground;
        exportBackground.type = BackgroundType::COLOR;
        exportBackground.color = QColor(Qt::white);
        layerExportScene.SetCanvasBackground(exportBackground);

        const int redLayer = layerExportScene.CreateLayer(QStringLiteral("红色层"));
        auto* red = ShapeFactory::Create(ShapeType::RECTANGLE);
        red->SetLayerId(redLayer);
        red->SetStyle(QColor(QStringLiteral("#DC2626")), QColor(QStringLiteral("#DC2626")), 1.0);
        red->setPos(QPointF(10, 10));
        red->SetContentSize(QSizeF(40, 40));
        layerExportScene.AttachItem(red);

        const int blueLayer = layerExportScene.CreateLayer(QStringLiteral("蓝色层"));
        auto* blue = ShapeFactory::Create(ShapeType::RECTANGLE);
        blue->SetLayerId(blueLayer);
        blue->SetStyle(QColor(QStringLiteral("#2563EB")), QColor(QStringLiteral("#2563EB")), 1.0);
        blue->setPos(QPointF(100, 100));
        blue->SetContentSize(QSizeF(40, 40));
        layerExportScene.AttachItem(blue);

        const QImage merged = layerExportScene.RenderImage(layerExportScene.CanvasRect(), 1.0);
        ctx.Check(merged.pixelColor(20, 20) == QColor(QStringLiteral("#DC2626")) &&
                      merged.pixelColor(110, 110) == QColor(QStringLiteral("#2563EB")),
                  QStringLiteral("合并导出包含所有可见图层"));
        const QImage redOnly = layerExportScene.RenderLayerImage(layerExportScene.CanvasRect(), 1.0, redLayer);
        const QImage blueOnly = layerExportScene.RenderLayerImage(layerExportScene.CanvasRect(), 1.0, blueLayer);
        ctx.Check(redOnly.pixelColor(20, 20) == QColor(QStringLiteral("#DC2626")) &&
                      redOnly.pixelColor(110, 110) == QColor(Qt::white),
                  QStringLiteral("逐层导出只渲染指定图层（红层）"));
        ctx.Check(blueOnly.pixelColor(110, 110) == QColor(QStringLiteral("#2563EB")) &&
                      blueOnly.pixelColor(20, 20) == QColor(Qt::white),
                  QStringLiteral("逐层导出只渲染指定图层（蓝层）"));

        ExportDialog exportDialog(&layerExportScene, QDir::tempPath());
        auto* modeCombo = exportDialog.findChild<QComboBox*>(QStringLiteral("ExportLayerModeCombo"));
        ctx.Check(modeCombo && modeCombo->count() == 2 && modeCombo->currentIndex() == 0,
                  QStringLiteral("导出对话框默认合并可见图层"));
        const ExportDialog::Options mergedOptions = exportDialog.options();
        ctx.Check(!mergedOptions.separateLayers && mergedOptions.filePaths.size() == 1,
                  QStringLiteral("合并模式只输出一个文件"),
                  QStringLiteral("files=%1").arg(mergedOptions.filePaths.size()));
        if (modeCombo) {
            modeCombo->setCurrentIndex(1);
        }
        const ExportDialog::Options layerOptions = exportDialog.options();
        int visibleLayers = 0;
        for (const CanvasLayer& layer : layerExportScene.Layers()) {
            if (layer.visible) {
                ++visibleLayers;
            }
        }
        bool namesLookRight = layerOptions.separateLayers && layerOptions.filePaths.size() == visibleLayers &&
                              layerOptions.layerIds.size() == visibleLayers;
        for (const QString& path : layerOptions.filePaths) {
            if (!QFileInfo(path).fileName().contains(QStringLiteral("_L"))) {
                namesLookRight = false;
            }
        }
        ctx.Check(namesLookRight, QStringLiteral("逐层模式为每个可见图层生成独立文件"),
                  QStringLiteral("files=%1 visible=%2").arg(layerOptions.filePaths.size()).arg(visibleLayers));

        // 自适应尺寸：默认不勾，范围与比例仍按下拉；勾上后改由视图接管，
        // 输出像素恰好等于全景视图在屏幕上的尺寸，两个旧控件同时失效。
        CanvasView fitView(&layerExportScene);
        fitView.resize(900, 620);
        fitView.show();
        QApplication::processEvents();
        fitView.FitAll();
        ExportDialog fitDialog(&layerExportScene, QDir::tempPath(), nullptr, &fitView);
        QCheckBox* fitCheck = fitDialog.findChild<QCheckBox*>(QStringLiteral("ExportFitViewCheck"));
        auto* fitScope = fitDialog.findChild<QComboBox*>(QStringLiteral("ExportScopeCombo"));
        ctx.Check(fitCheck && fitCheck->isEnabled() && !fitCheck->isChecked(),
                  QStringLiteral("导出弹窗的“自适应尺寸”默认不勾选"));
        const ExportDialog::Options plainOptions = fitDialog.options();
        if (fitCheck) {
            fitCheck->setChecked(true);
        }
        QApplication::processEvents();
        const ExportDialog::Options fitOptions = fitDialog.options();
        const QRect viewportRect = fitView.viewport()->rect();
        const int fitWidth = qCeil(fitOptions.area.width() * fitOptions.scale);
        const int fitHeight = qCeil(fitOptions.area.height() * fitOptions.scale);
        ctx.Check(fitCheck && fitCheck->isChecked() && fitOptions.area != plainOptions.area &&
                      fitOptions.scale != plainOptions.scale && fitScope && !fitScope->isEnabled() &&
                      qAbs(fitWidth - viewportRect.width()) <= 1 && qAbs(fitHeight - viewportRect.height()) <= 1,
                  QStringLiteral("勾选自适应尺寸后输出像素等于全景视图尺寸"),
                  QStringLiteral("fit=%1x%2 view=%3x%4")
                      .arg(fitWidth)
                      .arg(fitHeight)
                      .arg(viewportRect.width())
                      .arg(viewportRect.height()));
    }

    {
        // 需求 28：图层名可编辑，进入撤销栈并随画布持久化。
        CanvasScene renameScene;
        QUndoStack renameUndo;
        renameUndo.setUndoLimit(30);
        renameScene.SetUndoStack(&renameUndo);
        const int renameLayerId = renameScene.CreateLayer(QStringLiteral("初版分镜"));
        const QString beforeName = renameScene.layer(renameLayerId).name;
        ctx.Check(renameScene.RenameLayerWithUndo(renameLayerId, QStringLiteral("终版分镜")),
                  QStringLiteral("图层重命名成功"));
        ctx.Check(renameScene.layer(renameLayerId).name == QStringLiteral("终版分镜"),
                  QStringLiteral("图层重命名立即生效"));
        ctx.Check(renameUndo.count() == 1, QStringLiteral("图层重命名生成一条撤销记录"));
        renameUndo.undo();
        ctx.Check(renameScene.layer(renameLayerId).name == beforeName, QStringLiteral("图层重命名可撤销"));
        renameUndo.redo();
        ctx.Check(renameScene.layer(renameLayerId).name == QStringLiteral("终版分镜"),
                  QStringLiteral("图层重命名可重做"));
        ctx.Check(!renameScene.RenameLayerWithUndo(CanvasScene::BackgroundLayerId, QStringLiteral("不能改名")),
                  QStringLiteral("背景图层名不可编辑"));
        ctx.Check(!renameScene.RenameLayerWithUndo(renameLayerId, QStringLiteral("  ")),
                  QStringLiteral("空图层名被拒绝"));

        CanvasScene reloadedScene;
        ctx.Check(reloadedScene.FromJson(renameScene.ToJson()), QStringLiteral("重命名后的画布可重新载入"));
        ctx.Check(reloadedScene.layer(renameLayerId).name == QStringLiteral("终版分镜"),
                  QStringLiteral("图层名持久化到画布"));

        LayerPanel renamePanel(&renameScene);
        auto* renameList = renamePanel.findChild<QListWidget*>(QStringLiteral("LayerList"));
        QLabel* renameLabel = nullptr;
        if (renameList) {
            for (int row = 0; row < renameList->count(); ++row) {
                if (renameList->item(row)->data(Qt::UserRole).toInt() != renameLayerId) {
                    continue;
                }
                QWidget* rowWidget = renameList->itemWidget(renameList->item(row));
                renameLabel = rowWidget ? rowWidget->findChild<QLabel*>(QStringLiteral("LayerRowLabel")) : nullptr;
                break;
            }
        }
        ctx.Check(renameLabel && renameLabel->property("layerId").toInt() == renameLayerId &&
                      renameLabel->toolTip().contains(QStringLiteral("重命名")),
                  QStringLiteral("图层面板行名提供双击重命名入口"));
        renameScene.SetUndoStack(nullptr);
    }
}
