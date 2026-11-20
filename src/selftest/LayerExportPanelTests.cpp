#include "Alignment.h"
#include "BoolOps.h"
#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CanvasView.h"
#include "Commands.h"
#include "CurveItem.h"
#include "CustomShapeTemplates.h"
#include "ExportDialog.h"
#include "I18n.h"
#include "ItemPropertiesDialog.h"
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


void selftest::runLayerExportPanelTests(Context& ctx)
{
    {
        // 需求 15：图层面板选中行有高亮，且重叠图元优先命中已选图层。
        CanvasScene styleScene;
        LayerPanel stylePanel(&styleScene);
        auto* layerList = stylePanel.findChild<QListWidget*>();
        ctx.check(layerList && layerList->count() >= 2, QStringLiteral("图层面板包含用户图层与背景行"));
        QListWidgetItem* selectedEntry = nullptr;
        for (int row = 0; row < layerList->count(); ++row) {
            QListWidgetItem* entry = layerList->item(row);
            if (entry->data(Qt::UserRole).toInt() == styleScene.activeLayerId()) {
                selectedEntry = entry;
            }
        }
        // 高亮改由 QSS 的 selected 属性驱动，这里校验选中行确实带上了该属性。
        ctx.check(selectedEntry && layerList->itemWidget(selectedEntry) &&
                      layerList->itemWidget(selectedEntry)->property("selected").toBool(),
                  QStringLiteral("选中的图层行带背景高亮"));
        ctx.check(styleScene.preferredLayerIds().contains(styleScene.activeLayerId()),
                  QStringLiteral("图层面板选中图层同步为首选命中图层"));

        CanvasScene overlapScene;
        overlapScene.setMode(ToolMode::Edit);
        const int upperLayerId = overlapScene.createLayer(QStringLiteral("上层"));
        auto* lowerItem = createShapeItem(ShapeType::Rectangle);
        lowerItem->setLayerId(CanvasScene::BackgroundLayerId + 1);
        lowerItem->setStyle(QColor(QStringLiteral("#2563EB")), QColor(Qt::transparent), 4.0);
        lowerItem->setPos(QPointF(200, 170));
        lowerItem->setContentSize(QSizeF(200, 140));
        auto* upperItem = createShapeItem(ShapeType::Rectangle);
        upperItem->setLayerId(upperLayerId);
        upperItem->setStyle(QColor(QStringLiteral("#DC2626")), QColor(Qt::transparent), 4.0);
        upperItem->setPos(QPointF(200, 170));
        upperItem->setContentSize(QSizeF(200, 140));
        overlapScene.addItemsWithUndo({lowerItem, upperItem}, QStringLiteral("重叠图元"));

        CanvasView overlapView(&overlapScene);
        overlapView.resize(640, 460);
        overlapView.setMode(ToolMode::Edit);
        overlapView.centerOn(QPointF(300, 240));
        QApplication::processEvents();
        const QPoint overlapPoint = overlapView.mapFromScene(QPointF(300, 240));

        overlapScene.clearSelection();
        overlapScene.setPreferredLayerIds({CanvasScene::BackgroundLayerId + 1});
        sendMouseEvent(overlapView.viewport(), QEvent::MouseButtonPress, overlapPoint, Qt::LeftButton, Qt::LeftButton);
        sendMouseEvent(overlapView.viewport(), QEvent::MouseButtonRelease, overlapPoint, Qt::LeftButton, Qt::NoButton);
        const QList<CanvasItem*> lowerSelection = overlapScene.selectedCanvasItems();
        ctx.check(lowerSelection.size() == 1 && lowerSelection.first() == lowerItem,
                  QStringLiteral("重叠区域优先选中已选图层中的图元"),
                  QStringLiteral("selected=%1").arg(lowerSelection.size()));

        overlapScene.clearSelection();
        overlapScene.setPreferredLayerIds({upperLayerId});
        sendMouseEvent(overlapView.viewport(), QEvent::MouseButtonPress, overlapPoint, Qt::LeftButton, Qt::LeftButton);
        sendMouseEvent(overlapView.viewport(), QEvent::MouseButtonRelease, overlapPoint, Qt::LeftButton, Qt::NoButton);
        const QList<CanvasItem*> upperSelection = overlapScene.selectedCanvasItems();
        ctx.check(upperSelection.size() == 1 && upperSelection.first() == upperItem,
                  QStringLiteral("切换已选图层后优先选中该层图元"));

        // 需求 16：批量高度调整只占一条撤销记录。
        overlapScene.clearSelection();
        lowerItem->setSelected(true);
        upperItem->setSelected(true);
        QUndoStack heightUndo;
        overlapScene.setUndoStack(&heightUndo);
        const int lowerHeightBefore = lowerItem->height();
        const int upperHeightBefore = upperItem->height();
        const int heightIndexBefore = heightUndo.index();
        const int heightChanged = overlapScene.adjustSelectedHeights(1);
        ctx.check(heightChanged == 2 && lowerItem->height() == lowerHeightBefore + 1 &&
                      upperItem->height() == upperHeightBefore + 1,
                  QStringLiteral("快捷键批量提升选中图元高度"));
        ctx.check(heightUndo.index() == heightIndexBefore + 1, QStringLiteral("批量高度调整只生成一条撤销记录"));
        heightUndo.undo();
        ctx.check(lowerItem->height() == lowerHeightBefore && upperItem->height() == upperHeightBefore,
                  QStringLiteral("批量高度调整可整体撤销"));
        overlapScene.setUndoStack(nullptr);
    }

    {
        // 需求 27：合并可见图层导出 / 每个可见图层单独导出。
        CanvasScene layerExportScene;
        CanvasBackground exportBackground;
        exportBackground.type = BackgroundType::Color;
        exportBackground.color = QColor(Qt::white);
        layerExportScene.setCanvasBackground(exportBackground);

        const int redLayer = layerExportScene.createLayer(QStringLiteral("红色层"));
        auto* red = createShapeItem(ShapeType::Rectangle);
        red->setLayerId(redLayer);
        red->setStyle(QColor(QStringLiteral("#DC2626")), QColor(QStringLiteral("#DC2626")), 1.0);
        red->setPos(QPointF(10, 10));
        red->setContentSize(QSizeF(40, 40));
        layerExportScene.attachItem(red);

        const int blueLayer = layerExportScene.createLayer(QStringLiteral("蓝色层"));
        auto* blue = createShapeItem(ShapeType::Rectangle);
        blue->setLayerId(blueLayer);
        blue->setStyle(QColor(QStringLiteral("#2563EB")), QColor(QStringLiteral("#2563EB")), 1.0);
        blue->setPos(QPointF(100, 100));
        blue->setContentSize(QSizeF(40, 40));
        layerExportScene.attachItem(blue);

        const QImage merged = layerExportScene.renderImage(layerExportScene.canvasRect(), 1.0);
        ctx.check(merged.pixelColor(20, 20) == QColor(QStringLiteral("#DC2626")) &&
                      merged.pixelColor(110, 110) == QColor(QStringLiteral("#2563EB")),
                  QStringLiteral("合并导出包含所有可见图层"));
        const QImage redOnly = layerExportScene.renderLayerImage(layerExportScene.canvasRect(), 1.0, redLayer);
        const QImage blueOnly = layerExportScene.renderLayerImage(layerExportScene.canvasRect(), 1.0, blueLayer);
        ctx.check(redOnly.pixelColor(20, 20) == QColor(QStringLiteral("#DC2626")) &&
                      redOnly.pixelColor(110, 110) == QColor(Qt::white),
                  QStringLiteral("逐层导出只渲染指定图层（红层）"));
        ctx.check(blueOnly.pixelColor(110, 110) == QColor(QStringLiteral("#2563EB")) &&
                      blueOnly.pixelColor(20, 20) == QColor(Qt::white),
                  QStringLiteral("逐层导出只渲染指定图层（蓝层）"));

        ExportDialog exportDialog(&layerExportScene, QDir::tempPath());
        auto* modeCombo = exportDialog.findChild<QComboBox*>(QStringLiteral("ExportLayerModeCombo"));
        ctx.check(modeCombo && modeCombo->count() == 2 && modeCombo->currentIndex() == 0,
                  QStringLiteral("导出对话框默认合并可见图层"));
        const ExportDialog::Options mergedOptions = exportDialog.options();
        ctx.check(!mergedOptions.separateLayers && mergedOptions.filePaths.size() == 1,
                  QStringLiteral("合并模式只输出一个文件"),
                  QStringLiteral("files=%1").arg(mergedOptions.filePaths.size()));
        if (modeCombo) {
            modeCombo->setCurrentIndex(1);
        }
        const ExportDialog::Options layerOptions = exportDialog.options();
        int visibleLayers = 0;
        for (const CanvasLayer& layer : layerExportScene.layers()) {
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
        ctx.check(namesLookRight, QStringLiteral("逐层模式为每个可见图层生成独立文件"),
                  QStringLiteral("files=%1 visible=%2").arg(layerOptions.filePaths.size()).arg(visibleLayers));
    }

    {
        // 需求 28：图层名可编辑，进入撤销栈并随画布持久化。
        CanvasScene renameScene;
        QUndoStack renameUndo;
        renameUndo.setUndoLimit(30);
        renameScene.setUndoStack(&renameUndo);
        const int renameLayerId = renameScene.createLayer(QStringLiteral("初版分镜"));
        const QString beforeName = renameScene.layer(renameLayerId).name;
        ctx.check(renameScene.renameLayerWithUndo(renameLayerId, QStringLiteral("终版分镜")),
                  QStringLiteral("图层重命名成功"));
        ctx.check(renameScene.layer(renameLayerId).name == QStringLiteral("终版分镜"),
                  QStringLiteral("图层重命名立即生效"));
        ctx.check(renameUndo.count() == 1, QStringLiteral("图层重命名生成一条撤销记录"));
        renameUndo.undo();
        ctx.check(renameScene.layer(renameLayerId).name == beforeName, QStringLiteral("图层重命名可撤销"));
        renameUndo.redo();
        ctx.check(renameScene.layer(renameLayerId).name == QStringLiteral("终版分镜"),
                  QStringLiteral("图层重命名可重做"));
        ctx.check(!renameScene.renameLayerWithUndo(CanvasScene::BackgroundLayerId, QStringLiteral("不能改名")),
                  QStringLiteral("背景图层名不可编辑"));
        ctx.check(!renameScene.renameLayerWithUndo(renameLayerId, QStringLiteral("  ")),
                  QStringLiteral("空图层名被拒绝"));

        CanvasScene reloadedScene;
        ctx.check(reloadedScene.fromJson(renameScene.toJson()), QStringLiteral("重命名后的画布可重新载入"));
        ctx.check(reloadedScene.layer(renameLayerId).name == QStringLiteral("终版分镜"),
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
        ctx.check(renameLabel && renameLabel->property("layerId").toInt() == renameLayerId &&
                      renameLabel->toolTip().contains(QStringLiteral("重命名")),
                  QStringLiteral("图层面板行名提供双击重命名入口"));
        renameScene.setUndoStack(nullptr);
    }
}
