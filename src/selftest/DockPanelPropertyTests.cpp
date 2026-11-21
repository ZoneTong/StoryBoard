#include "Alignment.h"
#include "BoolOps.h"
#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CanvasView.h"
#include "Commands.h"
#include "CurveItem.h"
#include "CustomShapePanel.h"
#include "CustomShapeTemplates.h"
#include "ExportDialog.h"
#include "I18n.h"
#include "ItemPropertiesDialog.h"
#include "ItemStyleVisuals.h"
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
#include "StraightLineItem.h"
#include "TextItem.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"
#include "selftest/SelftestHelpers.h"
#include "shapes/AllShapes.h"

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

void selftest::RunDockPanelPropertyTests(Context& ctx)
{
    // 属性联动：多边形填充描线 / 混合选中撤销 / 文字图片行 / 悬浮与停靠显隐。
    const QString dockConfig = QDir(ctx.workDirectory).filePath(QStringLiteral("dock-panel-content-config.ini"));
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
    auto* insertedShape = CreateShapeItem(ShapeType::RECTANGLE);
    insertedShape->setPos(QPointF(160.0, 140.0));
    insertedShape->SetContentSize(QSizeF(140.0, 100.0));
    dockScene->AddItemsWithUndo({insertedShape}, QStringLiteral("Add rectangle"));
    insertedShape->setSelected(true);
    QApplication::processEvents();
    ShapeItem* selectedShape = dockScene->SelectedCanvasItems().isEmpty()
                                   ? nullptr
                                   : dynamic_cast<ShapeItem*>(dockScene->SelectedCanvasItems().first());
    PropertiesPanel* dockPanel = dockWindow.findChild<PropertiesPanel*>(QStringLiteral("PropertiesPanel"));
    QDockWidget* propertiesDock = dockWindow.findChild<QDockWidget*>(QStringLiteral("PropertiesPanelDock"));
    QDockWidget* layerDock = dockWindow.findChild<QDockWidget*>(QStringLiteral("LayerDock"));
    if (selectedShape && dockPanel) {
        QSpinBox* opacitySpin = nullptr;
        for (QSpinBox* spin : dockPanel->findChildren<QSpinBox*>(QStringLiteral("ItemValueSpin"))) {
            if (spin->maximum() == 100) {
                opacitySpin = spin;
            }
        }
        const QList<QToolButton*> borderButtons =
            dockPanel->findChildren<QToolButton*>(QStringLiteral("ItemBorderStyleButton"));
        const int dashedBorderButtonIndex = ItemStyleVisuals::BorderStyles().indexOf(Qt::DashLine);
        const QList<QToolButton*> fillButtons =
            dockPanel->findChildren<QToolButton*>(QStringLiteral("ItemFillStyleButton"));
        const QList<QToolButton*> morphButtons =
            dockPanel->findChildren<QToolButton*>(QStringLiteral("ItemMorphButton"));
        QWidget* borderStyleWidget = dockPanel->findChild<QWidget*>(QStringLiteral("ItemBorderStyleRow"));
        // 多边形也要有填充 / 描线属性：选中后五行可见，按钮改动真正落到图层并可撤销。
        PolygonItem::Layer polygonLayer;
        polygonLayer.path.addRect(0.0, 0.0, 140.0, 90.0);
        polygonLayer.fill = QColor(QStringLiteral("#3B82F6"));
        polygonLayer.stroke = QColor(QStringLiteral("#1E293B"));
        polygonLayer.strokeWidth = 4.0;
        polygonLayer.role = PolygonItem::LayerRole::FILLED_AREA;
        auto* polygon = new PolygonItem({polygonLayer});
        polygon->setPos(QPointF(240.0, 240.0));
        dockScene->AddItemsWithUndo({polygon}, QStringLiteral("Add polygon"));
        dockScene->clearSelection();
        polygon->setSelected(true);
        QApplication::processEvents();

        QWidget* fillStyleRow = dockPanel->findChild<QWidget*>(QStringLiteral("ItemFillStyleRow"));
        QWidget* fillColorRow = dockPanel->findChild<QWidget*>(QStringLiteral("ItemFillColorRow"));
        QWidget* borderColorRow = dockPanel->findChild<QWidget*>(QStringLiteral("ItemBorderColorRow"));
        QWidget* borderWidthRow = dockPanel->findChild<QWidget*>(QStringLiteral("ItemBorderWidthRow"));
        ctx.Check(fillStyleRow && fillStyleRow->isVisible() && fillColorRow && fillColorRow->isVisible() &&
                      borderStyleWidget && borderStyleWidget->isVisible() && borderColorRow &&
                      borderColorRow->isVisible() && borderWidthRow && borderWidthRow->isVisible(),
                  QStringLiteral("选中多边形时填充与描线五行全部可见"));

        const FillStyle polygonFillBefore = polygon->fillStyle();
        if (fillButtons.size() > 2) {
            fillButtons.at(2)->click();
            QApplication::processEvents();
        }
        ctx.Check(polygon->fillStyle() == FillStyle::HACHURE, QStringLiteral("编辑卡填充样式作用到多边形"),
                  QString::number(static_cast<int>(polygon->fillStyle())));
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(polygon->fillStyle() == polygonFillBefore, QStringLiteral("撤销填充样式恢复多边形原样式"));

        const QList<QToolButton*> fillSwatches =
            fillColorRow ? fillColorRow->findChildren<QToolButton*>(QStringLiteral("ItemColorSwatch"))
                         : QList<QToolButton*>();
        const QColor polygonFillBeforeColor = polygon->FillColor();
        if (fillSwatches.size() > 1) {
            fillSwatches.at(1)->click();
            QApplication::processEvents();
        }
        ctx.Check(polygon->FillColor() == QColor(QStringLiteral("#EF4444")),
                  QStringLiteral("编辑卡填充颜色作用到多边形"), polygon->FillColor().name());
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(polygon->FillColor() == polygonFillBeforeColor, QStringLiteral("撤销填充颜色恢复多边形原色"),
                  polygon->FillColor().name());

        const Qt::PenStyle polygonPenBefore = polygon->StrokeStyle();
        if (dashedBorderButtonIndex >= 0 && borderButtons.size() > dashedBorderButtonIndex) {
            borderButtons.at(dashedBorderButtonIndex)->click();
            QApplication::processEvents();
        }
        ctx.Check(polygon->StrokeStyle() != polygonPenBefore, QStringLiteral("编辑卡描线样式作用到多边形"));
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(polygon->StrokeStyle() == polygonPenBefore, QStringLiteral("撤销描线样式恢复多边形原样式"));

        const QColor polygonStrokeBefore = polygon->StrokeColor();
        const QList<QToolButton*> polygonStrokeSwatches =
            borderColorRow ? borderColorRow->findChildren<QToolButton*>(QStringLiteral("ItemColorSwatch"))
                           : QList<QToolButton*>();
        if (polygonStrokeSwatches.size() > 1) {
            polygonStrokeSwatches.at(1)->click();
            QApplication::processEvents();
        }
        ctx.Check(polygon->StrokeColor() == QColor(QStringLiteral("#EF4444")),
                  QStringLiteral("编辑卡描线颜色作用到多边形"), polygon->StrokeColor().name());
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(polygon->StrokeColor() == polygonStrokeBefore, QStringLiteral("撤销描线颜色恢复多边形原色"),
                  polygon->StrokeColor().name());

        const qreal polygonWidthBefore = polygon->StrokeWidth();
        QSpinBox* polygonWidthSpin =
            borderWidthRow ? borderWidthRow->findChild<QSpinBox*>(QStringLiteral("ItemBorderWidthSpin")) : nullptr;
        if (polygonWidthSpin) {
            polygonWidthSpin->setValue(9);
            QApplication::processEvents();
        }
        ctx.Check(qAbs(polygon->StrokeWidth() - 9.0) < 0.01, QStringLiteral("编辑卡描线宽度作用到多边形"),
                  QString::number(polygon->StrokeWidth()));
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(qAbs(polygon->StrokeWidth() - polygonWidthBefore) < 0.01,
                  QStringLiteral("撤销描线宽度恢复多边形原宽度"), QString::number(polygon->StrokeWidth()));

        // 属性窗同样识别多边形的填充 / 描线字段，确认后只占一条撤销记录。
        ItemPropertiesDialog polygonDialog(dockScene, polygon);
        QComboBox* dialogFillStyle =
            polygonDialog.findChild<QComboBox*>(QStringLiteral("ItemPropertiesFillStyleValue"));
        QSpinBox* dialogStrokeWidth =
            polygonDialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesStrokeWidthValue"));
        const FillStyle dialogFillBefore = polygon->fillStyle();
        const qreal dialogWidthBefore = polygon->StrokeWidth();
        if (dialogFillStyle) {
            const int hachureIndex = dialogFillStyle->findData(static_cast<int>(FillStyle::HACHURE));
            if (hachureIndex >= 0) {
                dialogFillStyle->setCurrentIndex(hachureIndex);
            }
        }
        if (dialogStrokeWidth) {
            dialogStrokeWidth->setValue(12);
        }
        polygonDialog.accept();
        QApplication::processEvents();
        ctx.Check(
            polygon->fillStyle() == FillStyle::HACHURE && qAbs(polygon->StrokeWidth() - 12.0) < 0.01,
            QStringLiteral("属性窗填充样式与描线宽度作用到多边形"),
            QStringLiteral("fill=%1 width=%2").arg(static_cast<int>(polygon->fillStyle())).arg(polygon->StrokeWidth()));
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(polygon->fillStyle() == dialogFillBefore && qAbs(polygon->StrokeWidth() - dialogWidthBefore) < 0.01,
                  QStringLiteral("撤销属性窗修改同时恢复多边形填充与描线"));

        // 图形与多边形混合选中时，一次填充色修改只占一条撤销记录。
        dockScene->clearSelection();
        selectedShape->setSelected(true);
        polygon->setSelected(true);
        QApplication::processEvents();
        const QColor mixedShapeFillBefore = selectedShape->FillColor();
        const QColor mixedPolygonFillBefore = polygon->FillColor();
        const int mixedUndoBefore = dockScene->UndoStack()->index();
        if (fillSwatches.size() > 2) {
            fillSwatches.at(2)->click();
            QApplication::processEvents();
        }
        const int mixedUndoAfter = dockScene->UndoStack()->index();
        ctx.Check(selectedShape->FillColor() != mixedShapeFillBefore &&
                      polygon->FillColor() != mixedPolygonFillBefore && mixedUndoAfter == mixedUndoBefore + 1,
                  QStringLiteral("混合选中图形与多边形时填充色只占一条撤销记录"),
                  QStringLiteral("undo=%1->%2 shape=%3->%4 polygon=%5->%6 selected=%7")
                      .arg(mixedUndoBefore)
                      .arg(mixedUndoAfter)
                      .arg(mixedShapeFillBefore.name())
                      .arg(selectedShape->FillColor().name())
                      .arg(mixedPolygonFillBefore.name())
                      .arg(polygon->FillColor().name())
                      .arg(dockScene->SelectedCanvasItems().size()));
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(selectedShape->FillColor() == mixedShapeFillBefore && polygon->FillColor() == mixedPolygonFillBefore,
                  QStringLiteral("撤销混合填充同时恢复图形与多边形原色"));

        // 文字 / 图片内容行与卡片操作按钮：只对相应类型显示，并且写入能撤销。
        auto* cardText = new TextItem();
        cardText->SetTextFormat(QStringLiteral("卡片文字"), QFont(QStringLiteral("Arial"), 12),
                                QColor(QStringLiteral("#1E293B")));
        cardText->setPos(QPointF(420.0, 430.0));
        dockScene->AddItemsWithUndo({cardText}, QStringLiteral("Add card text"));
        dockScene->clearSelection();
        cardText->setSelected(true);
        QApplication::processEvents();

        QWidget* textContentRow = propertiesDock->findChild<QWidget*>(QStringLiteral("ItemTextContentRow"));
        QWidget* textFontRow = propertiesDock->findChild<QWidget*>(QStringLiteral("ItemTextFontRow"));
        QWidget* textColorRow = propertiesDock->findChild<QWidget*>(QStringLiteral("ItemTextColorRow"));
        QWidget* fillImageRow = propertiesDock->findChild<QWidget*>(QStringLiteral("ItemFillImageRow"));
        QToolButton* propertiesButton = propertiesDock->findChild<QToolButton*>(QStringLiteral("ItemPropertiesButton"));
        QToolButton* smoothButton = propertiesDock->findChild<QToolButton*>(QStringLiteral("ItemSmoothButton"));
        ctx.Check(textContentRow && textContentRow->isVisible() && textFontRow && textFontRow->isVisible() &&
                      textColorRow && textColorRow->isVisible(),
                  QStringLiteral("选中文字时显示正文 / 字体 / 颜色行"));
        ctx.Check(fillImageRow && !fillImageRow->isVisible(), QStringLiteral("选中文字时隐藏背景图片行"));
        ctx.Check(propertiesButton && propertiesButton->isVisible(), QStringLiteral("选中文字时显示图元属性按钮"));
        ctx.Check(smoothButton && !smoothButton->isVisible(), QStringLiteral("选中文字时隐藏简化曲线按钮"));

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
        ctx.Check(propertyDialogOpened, QStringLiteral("卡片图元属性按钮真正打开属性窗"));

        // 透明度不仅作用于图形和曲线，文字等所有图元都走同一属性。
        if (opacitySpin) {
            opacitySpin->setValue(40);
            QApplication::processEvents();
        }
        ctx.Check(qAbs(cardText->ItemOpacity() - 0.4) < 0.01, QStringLiteral("卡片透明度作用于文字图元"),
                  QString::number(cardText->ItemOpacity()));
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(qAbs(cardText->ItemOpacity() - 1.0) < 0.01, QStringLiteral("撤销透明度恢复文字原值"),
                  QString::number(cardText->ItemOpacity()));

        auto* textEdit = propertiesDock->findChild<QLineEdit*>(QStringLiteral("ItemTextContentEdit"));
        if (textEdit) {
            textEdit->setText(QStringLiteral("改后文字"));
            QMetaObject::invokeMethod(textEdit, "editingFinished", Qt::DirectConnection);
            QApplication::processEvents();
        }
        ctx.Check(cardText->Text() == QStringLiteral("改后文字"), QStringLiteral("卡片正文直接修改文字"),
                  cardText->Text());
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(cardText->Text() == QStringLiteral("卡片文字"), QStringLiteral("撤销正文恢复原文"), cardText->Text());

        auto* fontSizeSpin = propertiesDock->findChild<QSpinBox*>(QStringLiteral("ItemTextFontSizeSpin"));
        if (fontSizeSpin) {
            fontSizeSpin->setValue(20);
            QApplication::processEvents();
        }
        ctx.Check(cardText->TextFont().pointSize() == 20, QStringLiteral("卡片字号直接修改文字字体"),
                  QString::number(cardText->TextFont().pointSize()));
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(cardText->TextFont().pointSize() == 12, QStringLiteral("撤销字号恢复原字体"),
                  QString::number(cardText->TextFont().pointSize()));

        const QList<QToolButton*> textSwatches =
            propertiesDock->findChildren<QToolButton*>(QStringLiteral("ItemTextColorSwatch"));
        if (textSwatches.size() > 1) {
            textSwatches.at(1)->click();
            QApplication::processEvents();
        }
        ctx.Check(cardText->TextColor() == QColor(QStringLiteral("#EF4444")),
                  QStringLiteral("卡片文字颜色直接修改文字"), cardText->TextColor().name());
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(cardText->TextColor() == QColor(QStringLiteral("#1E293B")), QStringLiteral("撤销文字颜色恢复原色"),
                  cardText->TextColor().name());

        auto* cardImage = CreateShapeItem(ShapeType::RECTANGLE);
        QPixmap imagePixmap(32, 24);
        imagePixmap.fill(QColor(QStringLiteral("#2563EB")));
        cardImage->SetFillImage(imagePixmap, ImageFill::Mode::STRETCH);
        cardImage->SetContentSize(QSizeF(32, 24));
        cardImage->setPos(QPointF(620.0, 430.0));
        dockScene->AddItemsWithUndo({cardImage}, QStringLiteral("Add card image"));
        dockScene->clearSelection();
        cardImage->setSelected(true);
        QApplication::processEvents();
        ctx.Check(fillImageRow && fillImageRow->isVisible(), QStringLiteral("选中带背景图的矩形时显示背景图片行"));
        ctx.Check(textContentRow && !textContentRow->isVisible() && textFontRow && !textFontRow->isVisible() &&
                      textColorRow && !textColorRow->isVisible(),
                  QStringLiteral("选中图形时隐藏文字行"));

        // 线条类图元不参与合并：选中直线时合并图元入口隐藏。
        auto* cardLine = new StraightLineItem();
        cardLine->SetEndpoints(QPointF(620.0, 470.0), QPointF(710.0, 480.0), QColor(QStringLiteral("#334155")), 3.0);
        dockScene->AddItemsWithUndo({cardLine}, QStringLiteral("Add card line"));
        dockScene->clearSelection();
        cardLine->setSelected(true);
        QApplication::processEvents();
        ctx.Check(!morphButtons.isEmpty() && !morphButtons.first()->isVisible(),
                  QStringLiteral("选中线条时隐藏合并图元入口"));

        // 只选曲线时显示简化曲线按钮，点击后平滑结果进入撤销栈。
        auto* smoothStroke = new CurveItem();
        QVector<QPointF> jitterPoints;
        for (int i = 0; i <= 8; ++i) {
            jitterPoints.append(QPointF(520.0 + i * 6.0, 470.0 + (i % 2) * 3.0));
        }
        smoothStroke->SetStrokePoints(jitterPoints, QColor(QStringLiteral("#334155")), 5.0);
        dockScene->AddItemsWithUndo({smoothStroke}, QStringLiteral("Add smooth stroke"));
        dockScene->clearSelection();
        QApplication::processEvents();
        QWidget* fillStyleRowWhenEmpty = propertiesDock->findChild<QWidget*>(QStringLiteral("ItemFillStyleRow"));
        QWidget* borderWidthRowWhenEmpty = propertiesDock->findChild<QWidget*>(QStringLiteral("ItemBorderWidthRow"));
        ctx.Check(fillStyleRowWhenEmpty && fillStyleRowWhenEmpty->isHidden() && borderWidthRowWhenEmpty &&
                      borderWidthRowWhenEmpty->isHidden(),
                  QStringLiteral("无图元选中时隐藏图元编辑选项"));
        smoothStroke->setSelected(true);
        QApplication::processEvents();
        auto* smoothingSpin = propertiesDock->window()->findChild<QSpinBox*>(QStringLiteral("BrushSmoothingSpin"));
        if (smoothingSpin) {
            smoothingSpin->setValue(10.0);
            QApplication::processEvents();
        }
        ctx.Check(smoothButton && smoothButton->isVisible(), QStringLiteral("选中曲线时显示简化曲线按钮"));
        const int undoIndexBeforeSmooth = dockScene->UndoStack()->index();
        if (smoothButton) {
            smoothButton->click();
            QApplication::processEvents();
        }
        ctx.Check(dockScene->UndoStack()->index() > undoIndexBeforeSmooth,
                  QStringLiteral("卡片简化曲线按钮真正执行简化且可撤销"),
                  QStringLiteral("index=%1->%2").arg(undoIndexBeforeSmooth).arg(dockScene->UndoStack()->index()));

        // 悬浮卡片跟随选中状态：清空选中后隐藏，重新选中再出现。
        propertiesDock->setFloating(true);
        QApplication::processEvents();
        dockScene->clearSelection();
        QApplication::processEvents();
        ctx.Check(!propertiesDock->isVisible(), QStringLiteral("悬浮时清空选中隐藏属性编辑面板"));
        selectedShape->setSelected(true);
        QApplication::processEvents();
        ctx.Check(propertiesDock->isVisible(), QStringLiteral("悬浮时有选中显示属性编辑面板"));

        // 拖回主窗口停靠后卡片常驻，不再随选中状态整张显隐。
        propertiesDock->setFloating(false);
        QApplication::processEvents();
        ctx.Check(!propertiesDock->isFloating() && propertiesDock->isVisible(),
                  QStringLiteral("拖回主窗口后停靠并保持显示"));
        dockScene->clearSelection();
        QApplication::processEvents();
        ctx.Check(propertiesDock->isVisible(), QStringLiteral("停靠时清空选中不隐藏属性编辑面板"));
        ctx.Check(layerDock && layerDock->height() < dockWindow.height() - 100,
                  QStringLiteral("属性编辑面板停靠时图层卡不撑满窗口高度"));
    }
    qunsetenv("STORYBOARD_CONFIG");
}
