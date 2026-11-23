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
#include "items/shapes/ShapeRegistry.h"
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

void selftest::RunDockPanelItemStyleTests(Context& ctx)
{
    // 样式卡动作链：圆角 / 边框 / 填充 / 旋转复制 / 对齐合并行 / 组合 / 曲线描线。
    const QString dockConfig = QDir(ctx.workDirectory).filePath(QStringLiteral("dock-item-style-config.ini"));
    QFile::remove(dockConfig);
    qputenv("STORYBOARD_CONFIG", dockConfig.toLocal8Bit());
    MainWindow dockWindow;
    dockWindow.resize(1200, 800);
    dockWindow.show();
    QApplication::processEvents();
    CanvasScene* dockScene = dockWindow.Scene();
    CanvasView* dockView = dockWindow.findChild<CanvasView*>();
    if (dockView) {
        dockView->SetMode(ToolMode::EDIT);
    }
    QApplication::processEvents();
    auto* insertedShape = ShapeRegistry::Create(ShapeType::RECTANGLE);
    insertedShape->setPos(QPointF(160.0, 140.0));
    insertedShape->SetContentSize(QSizeF(140.0, 100.0));
    dockScene->AddItemsWithUndo({insertedShape}, QStringLiteral("Add rectangle"));
    insertedShape->setSelected(true);
    QApplication::processEvents();
    ShapeAbstractItem* selectedShape = dockScene->SelectedCanvasItems().isEmpty()
                                           ? nullptr
                                           : dynamic_cast<ShapeAbstractItem*>(dockScene->SelectedCanvasItems().first());
    PropertiesPanel* dockPanel = dockWindow.findChild<PropertiesPanel*>(QStringLiteral("PropertiesPanel"));
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
        // 需求 15 在候选头部插入“无”，按钮顺序跟随 ItemStyleVisuals::BorderStyles()，
        // 断言不能再按位置取“虚线”，必须按候选表定位。
        const int dashedBorderButtonIndex = ItemStyleVisuals::BorderStyles().indexOf(Qt::DashLine);
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
        ctx.Check(qAbs(selectedShape->ItemOpacity() - 0.45) < 0.01, QStringLiteral("工具卡透明度即时作用到图形"),
                  QString::number(selectedShape->ItemOpacity()));
        dockScene->UndoStack()->undo();
        ctx.Check(qAbs(selectedShape->ItemOpacity() - 1.0) < 0.01, QStringLiteral("撤销透明度修改恢复原值"),
                  QString::number(selectedShape->ItemOpacity()));

        if (dashedBorderButtonIndex >= 0 && borderButtons.size() > dashedBorderButtonIndex) {
            borderButtons.at(dashedBorderButtonIndex)->click();
            QApplication::processEvents();
        }
        ctx.Check(selectedShape->StrokeStyle() == Qt::DashLine, QStringLiteral("工具卡边框样式作用到图形"));
        if (fillButtons.size() > 2) {
            fillButtons.at(2)->click();
            QApplication::processEvents();
        }
        ctx.Check(selectedShape->fillStyle() == FillStyle::HACHURE, QStringLiteral("工具卡填充样式作用到图形"));

        if (cornerSpin) {
            cornerSpin->setValue(24);
            QApplication::processEvents();
        }
        ctx.Check(qAbs(selectedShape->CornerRadius() - 24.0) < 0.01, QStringLiteral("矩形选中时圆角设置可用"),
                  QString::number(selectedShape->CornerRadius()));

        const int heightBefore = selectedShape->ZOrder();
        if (heightUp) {
            heightUp->click();
            QApplication::processEvents();
        }
        ctx.Check(selectedShape->ZOrder() == heightBefore + 1, QStringLiteral("工具卡高度 +1 按钮生效"),
                  QStringLiteral("%1->%2").arg(heightBefore).arg(selectedShape->ZOrder()));
        dockScene->UndoStack()->undo();
        ctx.Check(selectedShape->ZOrder() == heightBefore, QStringLiteral("撤销高度 +1 恢复原高度"));

        // 边框颜色 / 宽度同时是画笔的当前颜色与宽度，改完描边后画笔立即沿用。
        QWidget* borderWidthWidget = dockPanel->findChild<QWidget*>(QStringLiteral("ItemBorderWidthRow"));
        QSpinBox* borderWidthSpin = borderWidthWidget
                                        ? borderWidthWidget->findChild<QSpinBox*>(QStringLiteral("ItemBorderWidthSpin"))
                                        : nullptr;
        if (borderWidthSpin) {
            borderWidthSpin->setValue(9);
            QApplication::processEvents();
        }
        ctx.Check(borderWidthSpin && borderWidthSpin->minimum() == 1 && borderWidthSpin->maximum() == 64 &&
                      borderWidthSpin->singleStep() == 1 && borderWidthSpin->suffix() == QStringLiteral(" px"),
                  QStringLiteral("卡片描线宽度与工具栏使用同一 SpinBox 规格"),
                  QStringLiteral("min=%1 max=%2 step=%3 suffix=%4")
                      .arg(borderWidthSpin ? borderWidthSpin->minimum() : -1)
                      .arg(borderWidthSpin ? borderWidthSpin->maximum() : -1)
                      .arg(borderWidthSpin ? borderWidthSpin->singleStep() : 0)
                      .arg(borderWidthSpin ? borderWidthSpin->suffix() : QStringLiteral("-")));
        ctx.Check(qAbs(selectedShape->StrokeWidth() - 9.0) < 0.01 && dockView &&
                      qAbs(dockView->BrushWidth() - 9.0) < 0.01,
                  QStringLiteral("边框宽度同时成为画笔宽度"),
                  QStringLiteral("stroke=%1 brush=%2")
                      .arg(selectedShape->StrokeWidth())
                      .arg(dockView ? dockView->BrushWidth() : -1.0));
        QWidget* borderColorWidget = dockPanel->findChild<QWidget*>(QStringLiteral("ItemBorderColorRow"));
        const QList<QToolButton*> strokeSwatches =
            borderColorWidget ? borderColorWidget->findChildren<QToolButton*>(QStringLiteral("ItemColorSwatch"))
                              : QList<QToolButton*>();
        const QColor brushColorTarget(QStringLiteral("#EF4444"));
        if (strokeSwatches.size() > 1) {
            strokeSwatches.at(1)->click();
            QApplication::processEvents();
        }
        ctx.Check(selectedShape->StrokeColor() == brushColorTarget && dockView &&
                      dockView->BrushColor() == brushColorTarget,
                  QStringLiteral("边框颜色同时成为画笔颜色"),
                  QStringLiteral("stroke=%1 brush=%2")
                      .arg(selectedShape->StrokeColor().name())
                      .arg(dockView ? dockView->BrushColor().name() : QStringLiteral("-")));

        ctx.Check(copyButton && deleteButton && rotateCwButton && rotateCcwButton && copyButton->isVisible() &&
                      deleteButton->isVisible() && rotateCwButton->isVisible() && rotateCcwButton->isVisible(),
                  QStringLiteral("单选时编辑面板列出复制 / 删除 / 旋转按钮"));
        ctx.Check(alignRowButtons.size() == 8 && morphButtons.size() == 6 && groupButton && ungroupButton &&
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
        ctx.Check(visibleCardButtons >= 4 && unboundCardButtons == 0,
                  QStringLiteral("卡片所有可见按钮都关联了具体动作"),
                  QStringLiteral("visible=%1 unbound=%2").arg(visibleCardButtons).arg(unboundCardButtons));

        // 卡片按钮不是摆设：逐一验证旋转与复制按钮真的执行主窗口动作，且可撤销。
        if (rotateCwButton) {
            const qreal rotationBefore = selectedShape->rotation();
            rotateCwButton->click();
            QApplication::processEvents();
            const qreal rotationAfter = selectedShape->rotation();
            dockScene->UndoStack()->undo();
            QApplication::processEvents();
            ctx.Check(
                qAbs(rotationAfter - rotationBefore - 45.0) < 0.01 &&
                    qAbs(selectedShape->rotation() - rotationBefore) < 0.01,
                QStringLiteral("卡片旋转按钮真正旋转图元且可撤销"),
                QStringLiteral("%1->%2->%3").arg(rotationBefore).arg(rotationAfter).arg(selectedShape->rotation()));
            selectedShape->setSelected(true);
            QApplication::processEvents();
        }
        if (copyButton) {
            const int countBeforeCopy = dockScene->CanvasItemCount();
            copyButton->click();
            QApplication::processEvents();
            const int countAfterCopy = dockScene->CanvasItemCount();
            dockScene->UndoStack()->undo();
            QApplication::processEvents();
            ctx.Check(countAfterCopy == countBeforeCopy + 1 && dockScene->CanvasItemCount() == countBeforeCopy,
                      QStringLiteral("卡片复制按钮真正复制图元且可撤销"),
                      QStringLiteral("%1->%2->%3")
                          .arg(countBeforeCopy)
                          .arg(countAfterCopy)
                          .arg(dockScene->CanvasItemCount()));
            selectedShape->setSelected(true);
            QApplication::processEvents();
        }
        if (rotateCcwButton) {
            const qreal rotationBefore = selectedShape->rotation();
            rotateCcwButton->click();
            QApplication::processEvents();
            const qreal rotationAfter = selectedShape->rotation();
            dockScene->UndoStack()->undo();
            QApplication::processEvents();
            ctx.Check(
                qAbs(rotationAfter - rotationBefore + 45.0) < 0.01 &&
                    qAbs(selectedShape->rotation() - rotationBefore) < 0.01,
                QStringLiteral("卡片逆时针旋转按钮真正旋转图元且可撤销"),
                QStringLiteral("%1->%2->%3").arg(rotationBefore).arg(rotationAfter).arg(selectedShape->rotation()));
            selectedShape->setSelected(true);
            QApplication::processEvents();
        }

        // 多选时卡片把对齐 8 项与合并图元 6 项直接展开成图标按钮行，并复用菜单栏同一批动作。
        auto* secondShape = ShapeRegistry::Create(ShapeType::RECTANGLE);
        secondShape->setPos(QPointF(420.0, 240.0));
        secondShape->SetContentSize(QSizeF(120.0, 90.0));
        secondShape->SetStyle(QColor(QStringLiteral("#1D4ED8")), QColor(29, 78, 216, 40), 3.0);
        dockScene->AddItemsWithUndo({secondShape}, QStringLiteral("Add second shape"));
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
        ctx.Check(visibleAlign == 6, QStringLiteral("两个图元时对齐行展开 6 个可用模式"),
                  QString::number(visibleAlign));
        ctx.Check(visibleMorph == 5, QStringLiteral("多选时合并图元五项展开成一行"));
        ctx.Check(groupButton->isVisible() && !ungroupButton->isVisible() &&
                      groupButton->property("cardActionText").toString() == QStringLiteral("图元分组"),
                  QStringLiteral("多选未分组时显示图元分组、不显示解散分组"));

        // 对齐行按钮真正调用对齐动作；对齐后可撤销并恢复两个图元。
        if (!alignRowButtons.isEmpty()) {
            alignRowButtons.first()->click();
            QApplication::processEvents();
            const bool aligned =
                qAbs(selectedShape->sceneBoundingRect().left() - secondShape->sceneBoundingRect().left()) < 0.5;
            dockScene->UndoStack()->undo();
            QApplication::processEvents();
            ctx.Check(aligned, QStringLiteral("卡片对齐按钮真正对齐图元"),
                      QStringLiteral("left=%1/%2")
                          .arg(selectedShape->sceneBoundingRect().left())
                          .arg(secondShape->sceneBoundingRect().left()));
            selectedShape->setSelected(true);
            secondShape->setSelected(true);
            QApplication::processEvents();
        }
        // 合并图元行按钮真正执行布尔运算：两个不相交矩形联合后按连续块替换为两个多边形
        // 图元（新语义不再强行捏成单个），撤销恢复原矩形。
        if (!morphButtons.isEmpty()) {
            const int countBeforeMorph = dockScene->CanvasItemCount();
            morphButtons.first()->click();
            QApplication::processEvents();
            bool hasComposite = false;
            int compositeCount = 0;
            for (CanvasItem* item : dockScene->CanvasItems()) {
                if (dynamic_cast<PolygonItem*>(item)) {
                    hasComposite = true;
                    ++compositeCount;
                }
            }
            const bool merged = hasComposite && compositeCount == 2 && dockScene->CanvasItemCount() == countBeforeMorph;
            dockScene->UndoStack()->undo();
            QApplication::processEvents();
            ctx.Check(merged && dockScene->CanvasItemCount() == countBeforeMorph,
                      QStringLiteral("卡片合并图元按钮真正合并图元且可撤销"),
                      QStringLiteral("count=%1->%2 composite=%3")
                          .arg(countBeforeMorph)
                          .arg(dockScene->CanvasItemCount())
                          .arg(compositeCount));
            selectedShape->setSelected(true);
            secondShape->setSelected(true);
            QApplication::processEvents();
        }

        const int groupedCount = dockScene->GroupSelectedItems();
        // 组合不改选中集合，重新点选一次让主窗口按新的组归属刷新动作可用状态。
        dockScene->clearSelection();
        selectedShape->setSelected(true);
        secondShape->setSelected(true);
        QApplication::processEvents();
        ctx.Check(groupedCount == 2 && ungroupButton->isVisible() && !groupButton->isVisible() &&
                      ungroupButton->property("cardActionText").toString() == QStringLiteral("解散分组"),
                  QStringLiteral("选中整组时只显示解散分组"));

        // 组 + 组外图元混合选中：再套一层组合与取消组合同时可用（分组支持多层）。
        auto* thirdShape = ShapeRegistry::Create(ShapeType::ELLIPSE);
        thirdShape->setPos(QPointF(560.0, 320.0));
        thirdShape->SetContentSize(QSizeF(100.0, 80.0));
        dockScene->AddItemsWithUndo({thirdShape}, QStringLiteral("Add third shape"));
        selectedShape->setSelected(true);
        secondShape->setSelected(true);
        thirdShape->setSelected(true);
        QApplication::processEvents();
        ctx.Check(groupButton->isVisible() && ungroupButton->isVisible() &&
                      groupButton->property("cardActionText").toString() == QStringLiteral("图元分组") &&
                      ungroupButton->property("cardActionText").toString() == QStringLiteral("解散分组"),
                  QStringLiteral("组与普通图元混合选中时图元分组和解散分组都显示"));

        // 删除按钮单独用一个临时图元验证，避免删掉后面还要复用的图元。
        auto* deleteProbe = ShapeRegistry::Create(ShapeType::RECTANGLE);
        deleteProbe->setPos(QPointF(640.0, 360.0));
        deleteProbe->SetContentSize(QSizeF(80.0, 60.0));
        dockScene->AddItemsWithUndo({deleteProbe}, QStringLiteral("Add delete probe"));
        dockScene->clearSelection();
        deleteProbe->setSelected(true);
        QApplication::processEvents();
        const int countBeforeDelete = dockScene->CanvasItemCount();
        if (deleteButton) {
            deleteButton->click();
            QApplication::processEvents();
        }
        const int countAfterDelete = dockScene->CanvasItemCount();
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(countAfterDelete == countBeforeDelete - 1 && dockScene->CanvasItemCount() == countBeforeDelete,
                  QStringLiteral("卡片删除按钮真正删除图元且可撤销"),
                  QStringLiteral("%1->%2->%3")
                      .arg(countBeforeDelete)
                      .arg(countAfterDelete)
                      .arg(dockScene->CanvasItemCount()));


        // 曲线复用描线颜色 / 宽度 / 样式与透明度：只选曲线时描线样式行可用。
        auto* selectedStroke = new CurveItem();
        selectedStroke->SetStrokePoints({QPointF(60.0, 300.0), QPointF(180.0, 300.0)},
                                        QColor(QStringLiteral("#2563EB")), 6.0);
        dockScene->AddItemsWithUndo({selectedStroke}, QStringLiteral("Add stroke"));
        dockScene->clearSelection();
        selectedStroke->setSelected(true);
        QApplication::processEvents();

        QWidget* borderStyleWidget = dockPanel->findChild<QWidget*>(QStringLiteral("ItemBorderStyleRow"));
        ctx.Check(borderStyleWidget && borderStyleWidget->isVisible(), QStringLiteral("只选曲线时显示描线样式行"));
        ctx.Check(borderWidthSpin && borderWidthSpin->isEnabled() &&
                      qAbs(borderWidthSpin->value() - selectedStroke->StrokeWidth()) < 0.01 && opacitySpin &&
                      opacitySpin->isEnabled(),
                  QStringLiteral("只选曲线时描线宽度与透明度可用并按曲线回显"),
                  QStringLiteral("width=%1 spin=%2")
                      .arg(selectedStroke->StrokeWidth())
                      .arg(borderWidthSpin ? borderWidthSpin->value() : -1.0));

        if (strokeSwatches.size() > 1) {
            strokeSwatches.at(1)->click();
            QApplication::processEvents();
        }
        ctx.Check(selectedStroke->StrokeColor() == QColor(QStringLiteral("#EF4444")),
                  QStringLiteral("卡片描线颜色直接改变曲线颜色"), selectedStroke->StrokeColor().name());
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(selectedStroke->StrokeColor() == QColor(QStringLiteral("#2563EB")),
                  QStringLiteral("撤销描线颜色恢复曲线原色"), selectedStroke->StrokeColor().name());

        if (borderWidthSpin) {
            borderWidthSpin->setValue(11);
            QApplication::processEvents();
        }
        ctx.Check(qAbs(selectedStroke->StrokeWidth() - 11.0) < 0.01, QStringLiteral("卡片描线宽度直接改变曲线宽度"),
                  QString::number(selectedStroke->StrokeWidth()));
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(qAbs(selectedStroke->StrokeWidth() - 6.0) < 0.01, QStringLiteral("撤销描线宽度恢复曲线原宽度"),
                  QString::number(selectedStroke->StrokeWidth()));

        if (dashedBorderButtonIndex >= 0 && borderButtons.size() > dashedBorderButtonIndex) {
            const Qt::PenStyle styleBefore = selectedStroke->strokePattern();
            borderButtons.at(dashedBorderButtonIndex)->click();
            QApplication::processEvents();
            ctx.Check(selectedStroke->strokePattern() != styleBefore, QStringLiteral("曲线可通过编辑卡切换描线样式"));
            dockScene->UndoStack()->undo();
            QApplication::processEvents();
            ctx.Check(selectedStroke->strokePattern() == styleBefore, QStringLiteral("撤销描线样式恢复曲线原样式"));
        }

        if (opacitySpin) {
            opacitySpin->setValue(55);
            QApplication::processEvents();
        }
        ctx.Check(qAbs(selectedStroke->ItemOpacity() - 0.55) < 0.01, QStringLiteral("透明度作用到曲线"),
                  QString::number(selectedStroke->ItemOpacity()));
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(qAbs(selectedStroke->ItemOpacity() - 1.0) < 0.01, QStringLiteral("撤销透明度恢复曲线原值"),
                  QString::number(selectedStroke->ItemOpacity()));

        selectedShape->setSelected(true);
        selectedStroke->setSelected(true);
        QApplication::processEvents();
        ctx.Check(borderStyleWidget && borderStyleWidget->isVisible(),
                  QStringLiteral("图形与曲线混合选中时保留描线样式行"));
        if (opacitySpin) {
            opacitySpin->setValue(30);
            QApplication::processEvents();
        }
        ctx.Check(
            qAbs(selectedShape->ItemOpacity() - 0.3) < 0.01 && qAbs(selectedStroke->ItemOpacity() - 0.3) < 0.01,
            QStringLiteral("透明度同时作用到图形和曲线"),
            QStringLiteral("shape=%1 stroke=%2").arg(selectedShape->ItemOpacity()).arg(selectedStroke->ItemOpacity()));
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(qAbs(selectedShape->ItemOpacity() - 1.0) < 0.01 && qAbs(selectedStroke->ItemOpacity() - 1.0) < 0.01,
                  QStringLiteral("撤销透明度同时恢复图形和曲线"));
    }
    qunsetenv("STORYBOARD_CONFIG");
}
