#include "app/I18n.h"
#include "app/Settings.h"
#include "canvas/CanvasView.h"
#include "io/CustomShapeTemplates.h"
#include "items/CanvasItem.h"
#include "items/CanvasTypes.h"
#include "items/lines/ArcItem.h"
#include "items/lines/CurveItem.h"
#include "items/polygon/PolygonItem.h"
#include "items/properties/ArrowStyle.h"
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
#include "scene/SceneConstants.h"
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
        overlapScene.AddLayerWithUndo(QStringLiteral("上层"));
        const int upperLayerId = overlapScene.ActiveLayerId();
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

        layerExportScene.AddLayerWithUndo(QStringLiteral("红色层"));
        const int redLayer = layerExportScene.ActiveLayerId();
        auto* red = ShapeFactory::Create(ShapeType::RECTANGLE);
        red->SetLayerId(redLayer);
        red->SetStyle(QColor(QStringLiteral("#DC2626")), QColor(QStringLiteral("#DC2626")), 8.0);
        red->setPos(QPointF(10, 10));
        red->SetContentSize(QSizeF(40, 40));
        layerExportScene.AttachItem(red);

        layerExportScene.AddLayerWithUndo(QStringLiteral("蓝色层"));
        const int blueLayer = layerExportScene.ActiveLayerId();
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
        // 自适应尺寸的包围框按图层算：单层只框本层图元，合并模式才是全部可见图元的并集。
        const QRectF redLayerRect = layerExportScene.LayerContentRect(redLayer);
        const QRectF blueLayerRect = layerExportScene.LayerContentRect(blueLayer);
        const QRectF allRect = layerExportScene.ContentRect();
        ctx.Check(!redLayerRect.isEmpty() && !blueLayerRect.isEmpty() && redLayerRect.left() < blueLayerRect.left() &&
                      qAbs(redLayerRect.united(blueLayerRect).width() - allRect.width()) < 0.5 &&
                      redLayerRect.width() < allRect.width(),
                  QStringLiteral("图层包围框按本层图元取值，并集等于全部可见图元包围框"),
                  QStringLiteral("red=%1x%2 blue=%3x%4 all=%5x%6")
                      .arg(redLayerRect.width())
                      .arg(redLayerRect.height())
                      .arg(blueLayerRect.width())
                      .arg(blueLayerRect.height())
                      .arg(allRect.width())
                      .arg(allRect.height()));
        // 描线以路径为中心向外各占半宽，包围框必须把它算进去，否则导出图边缘会被切掉。
        ctx.Check(qAbs(redLayerRect.left() - 6.0) < 0.6 && qAbs(redLayerRect.width() - 48.0) < 0.6,
                  QStringLiteral("图层包围框包含描线半宽"),
                  QStringLiteral("left=%1 width=%2").arg(redLayerRect.left()).arg(redLayerRect.width()));

        // 箭头头长是线宽的 4 倍，翅膀比描线宽得多：0°→180° 的半圆弧线两端箭头
        // 横向各外伸一个头长，包围框不把箭头算进去就会切掉半边。
        CanvasScene arrowScene;
        arrowScene.ApplyCanvasSize(QSizeF(600, 400));
        arrowScene.SetShowGrid(false);
        auto* arrowArc = new ArcItem();
        arrowArc->SetArcGeometry(QPointF(50, 50), QSizeF(200, 200), 0.0, 180.0, QColor(QStringLiteral("#1E293B")),
                                 10.0);
        arrowScene.AttachItem(arrowArc);
        const QRectF plainRect = arrowScene.ContentRect();
        arrowArc->SetArrowStyles(ArrowStyle::TRIANGLE_FILLED, ArrowStyle::TRIANGLE_FILLED);
        const QRectF arrowRect = arrowScene.ContentRect();
        const qreal headLength = 40.0; // 4 × 线宽 10
        ctx.Check(arrowRect.width() > plainRect.width() + headLength * 1.5 && arrowRect.height() >= plainRect.height(),
                  QStringLiteral("导出包围框把两端箭头算进去"),
                  QStringLiteral("plain=%1x%2 arrow=%3x%4")
                      .arg(plainRect.width())
                      .arg(plainRect.height())
                      .arg(arrowRect.width())
                      .arg(arrowRect.height()));
        arrowArc->SetArrowStyles(ArrowStyle::NONE, ArrowStyle::NONE);
        ctx.Check(qAbs(arrowScene.ContentRect().width() - plainRect.width()) < 0.5,
                  QStringLiteral("去掉箭头后包围框回到描线半宽"),
                  QStringLiteral("width=%1 plain=%2").arg(arrowScene.ContentRect().width()).arg(plainRect.width()));

        // 自适应尺寸对所有图元类型都得成立：这里混放“旋转粗描边矩形 + 文字 + 带箭头弧线”，
        // 逐个核对图元实际绘制轮廓都落在包围框里（描边、圆角连接、字形、箭头都不能出框）。
        CanvasScene mixedScene;
        mixedScene.ApplyCanvasSize(QSizeF(800, 600));
        mixedScene.SetShowGrid(false);
        auto* mixedRect = ShapeFactory::Create(ShapeType::RECTANGLE);
        mixedRect->setPos(QPointF(120, 90));
        mixedRect->SetContentSize(QSizeF(120, 80));
        mixedRect->SetStyle(QColor(QStringLiteral("#1E293B")), QColor(QStringLiteral("#1E293B")), 12.0);
        mixedRect->SetRotationAngle(25.0);
        mixedScene.AttachItem(mixedRect);
        auto* mixedText = new TextItem();
        mixedText->SetTextFormat(QStringLiteral("Output size"), QFont(QStringLiteral("Microsoft YaHei"), 22),
                                 QColor(QStringLiteral("#DC2626")));
        mixedText->setPos(QPointF(330, 120));
        mixedScene.AttachItem(mixedText);
        auto* mixedArc = new ArcItem();
        mixedArc->SetArcGeometry(QPointF(420, 260), QSizeF(180, 180), 0.0, 180.0, QColor(QStringLiteral("#2563EB")),
                                 10.0);
        mixedArc->SetArrowStyles(ArrowStyle::TRIANGLE_FILLED, ArrowStyle::TRIANGLE_FILLED);
        mixedScene.AttachItem(mixedArc);
        const QRectF mixedRectBox = mixedScene.ContentRect();
        bool mixedCovered = true;
        QString mixedDetail;
        for (CanvasItem* item : mixedScene.CanvasItems()) {
            const QRectF painted = item->sceneTransform().map(item->PaintedShape()).boundingRect();
            if (painted.isEmpty()) {
                continue;
            }
            const QRectF padded = mixedRectBox.adjusted(-0.5, -0.5, 0.5, 0.5);
            if (!padded.contains(painted)) {
                mixedCovered = false;
                mixedDetail = QStringLiteral("item=%1 painted=%2,%3 %4x%5 box=%6,%7 %8x%9")
                                  .arg(item->Kind())
                                  .arg(painted.left())
                                  .arg(painted.top())
                                  .arg(painted.width())
                                  .arg(painted.height())
                                  .arg(mixedRectBox.left())
                                  .arg(mixedRectBox.top())
                                  .arg(mixedRectBox.width())
                                  .arg(mixedRectBox.height());
                break;
            }
        }
        ctx.Check(mixedCovered, QStringLiteral("自适应尺寸覆盖图形、文字与箭头弧线的绘制轮廓"), mixedDetail);

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

        // 自适应尺寸：默认不勾，范围与比例仍按下拉；勾上后改由画布内容接管，
        // 输出尺寸恰好等于可见图元并集的最小包围框（比例 1），两个旧控件同时失效。
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
        const QRectF contentRect = layerExportScene.ContentRect();
        const qreal fitPad = scene_constants::EXPORT_FIT_PADDING;
        const int fitWidth = qCeil(fitOptions.area.width() * fitOptions.scale);
        const int fitHeight = qCeil(fitOptions.area.height() * fitOptions.scale);
        ctx.Check(fitCheck && fitCheck->isChecked() && fitOptions.area != plainOptions.area &&
                      qAbs(fitOptions.scale - 1.0) < 0.001 && fitScope && !fitScope->isEnabled() &&
                      qAbs(fitOptions.area.left() - (contentRect.left() - fitPad)) < 0.5 &&
                      qAbs(fitOptions.area.top() - (contentRect.top() - fitPad)) < 0.5 &&
                      qAbs(fitWidth - qCeil(contentRect.width() + 2.0 * fitPad)) <= 1 &&
                      qAbs(fitHeight - qCeil(contentRect.height() + 2.0 * fitPad)) <= 1,
                  QStringLiteral("勾选自适应尺寸后输出尺寸等于可见图元包围框加 2px 余量"),
                  QStringLiteral("fit=%1x%2 content=%3x%4 pad=%5")
                      .arg(fitWidth)
                      .arg(fitHeight)
                      .arg(contentRect.width())
                      .arg(contentRect.height())
                      .arg(fitPad));

        // 上次勾过就记住：构造时直接带回来，范围与比例控件一开始就是关的。
        ExportDialog rememberedDialog(&layerExportScene, QDir::tempPath(), nullptr, &fitView, true);
        QCheckBox* rememberedCheck = rememberedDialog.findChild<QCheckBox*>(QStringLiteral("ExportFitViewCheck"));
        auto* rememberedScope = rememberedDialog.findChild<QComboBox*>(QStringLiteral("ExportScopeCombo"));
        const ExportDialog::Options rememberedOptions = rememberedDialog.options();
        ctx.Check(rememberedCheck && rememberedCheck->isChecked() && rememberedOptions.fitViewSize && rememberedScope &&
                      !rememberedScope->isEnabled(),
                  QStringLiteral("上次勾选的自适应尺寸会被记住并回填"),
                  QStringLiteral("checked=%1 scopeEnabled=%2")
                      .arg(rememberedCheck && rememberedCheck->isChecked() ? 1 : 0)
                      .arg(rememberedScope && rememberedScope->isEnabled() ? 1 : 0));
    }

    {
        // 需求 28：图层名可编辑，进入撤销栈并随画布持久化。
        CanvasScene renameScene;
        QUndoStack renameUndo;
        renameUndo.setUndoLimit(30);
        renameScene.SetUndoStack(&renameUndo);
        renameScene.AddLayerWithUndo(QStringLiteral("初版分镜"));
        const int renameLayerId = renameScene.ActiveLayerId();
        // 建层也进了撤销栈；本段只验证重命名自己的撤销记录，先把栈清干净。
        renameUndo.clear();
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
