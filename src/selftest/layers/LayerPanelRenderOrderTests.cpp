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
#include "ui/panels/itemdialog/ItemPropertiesDialog.h"
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


void selftest::RunLayerPanelTests(Context& ctx)
{
    // Exercise the actual layer panel buttons, not only the scene API.
    CanvasScene panelScene;
    QUndoStack panelUndo;
    panelUndo.setUndoLimit(50);
    panelScene.SetUndoStack(&panelUndo);
    LayerPanel panel(&panelScene);
    panel.resize(240, 360);

    auto* panelList = panel.findChild<QListWidget*>(QStringLiteral("LayerList"));
    QToolButton* panelAdd = nullptr;
    QToolButton* panelRemove = nullptr;
    QToolButton* panelUp = nullptr;
    QToolButton* panelDown = nullptr;
    const QList<QToolButton*> panelButtons = panel.findChildren<QToolButton*>();
    for (QToolButton* button : panelButtons) {
        if (button->text() == QStringLiteral("+")) {
            panelAdd = button;
        } else if (button->text() == QStringLiteral("−")) {
            panelRemove = button;
        } else if (button->text() == QStringLiteral("↑")) {
            panelUp = button;
        } else if (button->text() == QStringLiteral("↓")) {
            panelDown = button;
        }
    }
    ctx.Check(panelList && panelAdd && panelRemove && panelUp && panelDown, QStringLiteral("图层面板创建列表与按钮"));
    ctx.Check(panelList && panelList->count() == 2, QStringLiteral("图层面板显示背景与一个用户图元"));

    if (panelList && panelAdd && panelRemove && panelUp && panelDown) {
        panelAdd->click();
        QApplication::processEvents();
        const int panelTopId = panelScene.ActiveLayerId();
        ctx.Check(panelScene.UserLayerCount() == 2 && panelList->count() == 3 && panelScene.LayerIndex(panelTopId) == 2,
                  QStringLiteral("面板新建图层位于最上层"));

        panelDown->click();
        QApplication::processEvents();
        ctx.Check(panelScene.LayerIndex(panelTopId) == 1, QStringLiteral("面板按钮下移图层"));

        panelUp->click();
        QApplication::processEvents();
        ctx.Check(panelScene.LayerIndex(panelTopId) == 2, QStringLiteral("面板按钮上移图层"));

        auto* panelItem = MakeLineItem(QPointF(10, 10), QPointF(80, 80), QColor(QStringLiteral("#7C3AED")));
        panelScene.AddItemsWithUndo({panelItem}, QStringLiteral("面板测试图元"));
        QApplication::processEvents();
        ctx.Check(!panelRemove->isEnabled(), QStringLiteral("非空图层禁用面板删除按钮"));

        panelUndo.undo();
        QApplication::processEvents();
        ctx.Check(panelRemove->isEnabled(), QStringLiteral("空图层启用面板删除按钮"));

        panelRemove->click();
        QApplication::processEvents();
        ctx.Check(panelScene.UserLayerCount() == 1 && panelList->count() == 2, QStringLiteral("面板按钮删除空图层"));

        panelUndo.undo();
        QApplication::processEvents();
        ctx.Check(panelScene.UserLayerCount() == 2 && panelScene.HasLayer(panelTopId),
                  QStringLiteral("面板删除图层支持撤销"));
    }

    {
        CanvasScene visibilityPanelScene;
        QUndoStack visibilityPanelUndo;
        visibilityPanelScene.SetUndoStack(&visibilityPanelUndo);
        auto* lowerItem = MakeLineItem(QPointF(8, 8), QPointF(60, 60), QColor(QStringLiteral("#0F766E")));
        visibilityPanelScene.AddItemsWithUndo({lowerItem}, QStringLiteral("面板下层图元"));
        const int lowerId = visibilityPanelScene.ActiveLayerId();

        visibilityPanelScene.AddLayerWithUndo(QStringLiteral("面板上层"));
        const int upperId = visibilityPanelScene.ActiveLayerId();
        auto* upperItem = MakeLineItem(QPointF(20, 20), QPointF(80, 80), QColor(QStringLiteral("#EA580C")));
        visibilityPanelScene.AddItemsWithUndo({upperItem}, QStringLiteral("面板上层图元"));

        LayerPanel visibilityPanel(&visibilityPanelScene);
        auto* visibilityList = visibilityPanel.findChild<QListWidget*>(QStringLiteral("LayerList"));
        auto findRow = [visibilityList](int layerId) -> QListWidgetItem* {
            if (!visibilityList) {
                return nullptr;
            }
            for (int row = 0; row < visibilityList->count(); ++row) {
                QListWidgetItem* item = visibilityList->item(row);
                if (item->data(Qt::UserRole).toInt() == layerId) {
                    return item;
                }
            }
            return nullptr;
        };

        auto rowCheckBox = [visibilityList](int layerId) -> QCheckBox* {
            if (!visibilityList) {
                return nullptr;
            }
            for (int row = 0; row < visibilityList->count(); ++row) {
                QListWidgetItem* item = visibilityList->item(row);
                if (item->data(Qt::UserRole).toInt() != layerId) {
                    continue;
                }
                QWidget* widget = visibilityList->itemWidget(item);
                return widget ? widget->findChild<QCheckBox*>(QStringLiteral("LayerVisibleBox")) : nullptr;
            }
            return nullptr;
        };
        auto rowLockButton = [visibilityList](int layerId) -> QToolButton* {
            if (!visibilityList) {
                return nullptr;
            }
            for (int row = 0; row < visibilityList->count(); ++row) {
                QListWidgetItem* item = visibilityList->item(row);
                if (item->data(Qt::UserRole).toInt() != layerId) {
                    continue;
                }
                QWidget* widget = visibilityList->itemWidget(item);
                return widget ? widget->findChild<QToolButton*>(QStringLiteral("LayerLockButton")) : nullptr;
            }
            return nullptr;
        };

        QListWidgetItem* lowerRow = findRow(lowerId);
        QCheckBox* lowerBox = rowCheckBox(lowerId);
        ctx.Check(lowerRow && lowerBox && lowerBox->isChecked(), QStringLiteral("图层面板显示可见勾选状态"));
        if (lowerBox) {
            lowerBox->setChecked(false);
            QApplication::processEvents();
            ctx.Check(!visibilityPanelScene.IsLayerVisible(lowerId), QStringLiteral("图层面板取消勾选隐藏图层"));
            QCheckBox* hiddenBox = rowCheckBox(lowerId);
            QLabel* hiddenLabel = hiddenBox && hiddenBox->parentWidget()
                                      ? hiddenBox->parentWidget()->findChild<QLabel*>(QStringLiteral("LayerRowLabel"))
                                      : nullptr;
            ctx.Check(hiddenBox && !hiddenBox->isChecked() && hiddenLabel &&
                          hiddenLabel->text().contains(QStringLiteral("隐藏")),
                      QStringLiteral("图层面板标记隐藏图层"));

            QListWidgetItem* hiddenRow = findRow(lowerId);
            if (hiddenRow) {
                visibilityList->setCurrentItem(hiddenRow);
                QApplication::processEvents();
                ctx.Check(visibilityPanelScene.ActiveLayerId() == lowerId &&
                              visibilityPanelScene.IsLayerVisible(lowerId),
                          QStringLiteral("选中隐藏图层自动显示"));
                QCheckBox* shownBox = rowCheckBox(lowerId);
                ctx.Check(shownBox && shownBox->isChecked(), QStringLiteral("自动显示后同步勾选状态"));
                if (shownBox) {
                    shownBox->setChecked(false);
                    QApplication::processEvents();
                    ctx.Check(!visibilityPanelScene.IsLayerVisible(lowerId) &&
                                  visibilityPanelScene.ActiveLayerId() == upperId,
                              QStringLiteral("隐藏当前图层后自动选择其他图层"));
                }
            }
        }

        QCheckBox* backgroundBox = rowCheckBox(CanvasScene::BackgroundLayerId);
        ctx.Check(backgroundBox && backgroundBox->isChecked(), QStringLiteral("图层面板显示背景层勾选"));
        if (backgroundBox) {
            backgroundBox->setChecked(false);
            QApplication::processEvents();
            ctx.Check(!visibilityPanelScene.IsLayerVisible(CanvasScene::BackgroundLayerId),
                      QStringLiteral("图层面板可隐藏背景层"));
            QCheckBox* hiddenBackgroundBox = rowCheckBox(CanvasScene::BackgroundLayerId);
            if (hiddenBackgroundBox) {
                hiddenBackgroundBox->setChecked(true);
                QApplication::processEvents();
            }
            ctx.Check(visibilityPanelScene.IsLayerVisible(CanvasScene::BackgroundLayerId),
                      QStringLiteral("图层面板可重新显示背景层"));
        }

        QCheckBox* allVisibleBox = visibilityPanel.findChild<QCheckBox*>(QStringLiteral("LayerVisibleAllBox"));
        ctx.Check(allVisibleBox && allVisibleBox->isTristate(), QStringLiteral("图层面板表头提供三态勾选框"));
        ctx.Check(allVisibleBox && allVisibleBox->checkState() == Qt::PartiallyChecked,
                  QStringLiteral("表头勾选框反映部分图层隐藏"));
        if (allVisibleBox) {
            allVisibleBox->click();
            QApplication::processEvents();
            bool everyLayerVisible = true;
            for (const CanvasLayer& layer : visibilityPanelScene.Layers()) {
                if (!layer.visible) {
                    everyLayerVisible = false;
                }
            }
            ctx.Check(everyLayerVisible && allVisibleBox->checkState() == Qt::Checked,
                      QStringLiteral("表头勾选框可全选显示所有图层"));

            allVisibleBox->click();
            QApplication::processEvents();
            bool everyLayerHidden = true;
            for (const CanvasLayer& layer : visibilityPanelScene.Layers()) {
                if (layer.visible) {
                    everyLayerHidden = false;
                }
            }
            ctx.Check(everyLayerHidden && allVisibleBox->checkState() == Qt::Unchecked,
                      QStringLiteral("表头勾选框可全不选隐藏所有图层"));

            allVisibleBox->click();
            QApplication::processEvents();
            bool restoredAllVisible = true;
            for (const CanvasLayer& layer : visibilityPanelScene.Layers()) {
                if (!layer.visible) {
                    restoredAllVisible = false;
                }
            }
            ctx.Check(restoredAllVisible && allVisibleBox->checkState() == Qt::Checked,
                      QStringLiteral("表头勾选框可再次显示所有图层"));
        }

        QToolButton* upperLock = rowLockButton(upperId);
        if (upperLock) {
            upperLock->click();
            QApplication::processEvents();
            ctx.Check(visibilityPanelScene.IsLayerLocked(upperId) && !visibilityPanelScene.IsItemEditable(upperItem),
                      QStringLiteral("锁定按钮锁定图层并禁止编辑其图元"));
            QToolButton* restoredLock = rowLockButton(upperId);
            if (restoredLock) {
                restoredLock->click();
            }
            QApplication::processEvents();
            ctx.Check(!visibilityPanelScene.IsLayerLocked(upperId), QStringLiteral("再次点击解锁图层"));
        }

        visibilityList->clearSelection();
        for (int row = 0; row < visibilityList->count(); ++row) {
            QListWidgetItem* item = visibilityList->item(row);
            if (item->data(Qt::UserRole).toInt() == upperId) {
                item->setSelected(true);
            }
        }
        ctx.Check(visibilityPanel.SelectedLayerIds().size() == 1 &&
                      visibilityPanel.SelectedLayerIds().first() == upperId,
                  QStringLiteral("图层面板可读取选中的图层集合"));

        upperItem->setSelected(true);
        QApplication::processEvents();
        ctx.Check(visibilityPanelScene.ActiveLayerId() == upperId, QStringLiteral("选中图元时自动选中其所属图层"));
    }
}

void selftest::RunPaintOrderTests(Context& ctx)
{
    // Verify the actual paint order, not just the numeric z values.
    CanvasScene paintScene;
    paintScene.ApplyCanvasSize(QSizeF(100, 100));
    const QColor redColor(QStringLiteral("#DC2626"));
    const QColor blueColor(QStringLiteral("#2563EB"));

    auto* redItem = ShapeFactory::Create(ShapeType::RECTANGLE);
    redItem->SetStyle(QColor(Qt::transparent), redColor, 1.0);
    redItem->SetContentSize(QSizeF(80, 80));
    redItem->setPos(QPointF(10, 10));
    paintScene.AddItemsWithUndo({redItem}, QStringLiteral("添加红色图元"));

    ctx.Check(paintScene.AddLayerWithUndo(QStringLiteral("蓝色层")), QStringLiteral("渲染测试新建上层"));
    auto* blueItem = ShapeFactory::Create(ShapeType::RECTANGLE);
    blueItem->SetStyle(QColor(Qt::transparent), blueColor, 1.0);
    blueItem->SetContentSize(QSizeF(80, 80));
    blueItem->setPos(QPointF(20, 20));
    paintScene.AddItemsWithUndo({blueItem}, QStringLiteral("添加蓝色图元"));
    const int blueLayerId = paintScene.ActiveLayerId();

    const QImage painted = paintScene.RenderImage(paintScene.CanvasRect(), 1.0);
    ctx.Check(painted.pixelColor(30, 30) == QColor(QStringLiteral("#2563EB")),
              QStringLiteral("上层图元实际遮盖下层图元"));
    ctx.Check(painted.pixelColor(15, 15) == QColor(QStringLiteral("#DC2626")),
              QStringLiteral("未重叠区域保留下层图元"));

    paintScene.MoveLayerWithUndo(blueLayerId, -1);
    const QImage paintedLower = paintScene.RenderImage(paintScene.CanvasRect(), 1.0);
    ctx.Check(paintedLower.pixelColor(30, 30) == QColor(QStringLiteral("#DC2626")),
              QStringLiteral("图层下移后实际遮盖关系反转"));

    const QString demoPath = DemoCanvasPath();
    if (QFileInfo::exists(demoPath)) {
        QFile demoFile(demoPath);
        ctx.Check(demoFile.open(QIODevice::ReadOnly), QStringLiteral("读取示例画布文件"));
        const QJsonDocument demoDocument = QJsonDocument::fromJson(demoFile.readAll());
        CanvasScene demoLoaded;
        QString demoError;
        ctx.Check(demoLoaded.FromJson(demoDocument.object(), &demoError), QStringLiteral("加载示例画布"), demoError);
        ctx.Check(
            demoLoaded.UserLayerCount() == 3 && demoLoaded.ActiveLayerId() == demoLoaded.Layers().last().id &&
                demoLoaded.CanvasItemCount() == 27,
            QStringLiteral("示例画布图层与图元完整"),
            QStringLiteral("layers=%1 items=%2").arg(demoLoaded.UserLayerCount()).arg(demoLoaded.CanvasItemCount()));
    }

    // ---------------------------------------------------------------
}
