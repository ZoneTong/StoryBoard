#include "app/I18n.h"
#include "app/Settings.h"
#include "canvas/CanvasView.h"
#include "io/CustomShapeTemplates.h"
#include "items/CanvasTypes.h"
#include "items/lines/ArcItem.h"
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
#include <QMetaObject>
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
    auto* insertedShape = ShapeFactory::Create(ShapeType::RECTANGLE);
    insertedShape->setPos(QPointF(160.0, 140.0));
    insertedShape->SetContentSize(QSizeF(140.0, 100.0));
    dockScene->AddItemsWithUndo({insertedShape}, QStringLiteral("Add rectangle"));
    insertedShape->setSelected(true);
    QApplication::processEvents();
    ShapeAbstractItem* selectedShape = dockScene->SelectedCanvasItems().isEmpty()
                                           ? nullptr
                                           : dynamic_cast<ShapeAbstractItem*>(dockScene->SelectedCanvasItems().first());
    PropertiesPanel* dockPanel = dockWindow.findChild<PropertiesPanel*>(QStringLiteral("PropertiesPanel"));
    QDockWidget* propertiesDock = dockWindow.findChild<QDockWidget*>(QStringLiteral("PropertiesPanelDock"));
    QDockWidget* layerDock = dockWindow.findChild<QDockWidget*>(QStringLiteral("LayerDock"));
    QScrollArea* propertiesScroll =
        dockPanel ? dockPanel->findChild<QScrollArea*>(QStringLiteral("PropertiesPanelScroll")) : nullptr;
    ctx.Check(propertiesDock && propertiesDock->maximumHeight() > 620,
              QStringLiteral("编辑面板不再固定 620px 上限，跟随窗口高度伸展"));
    ctx.Check(propertiesScroll && propertiesScroll->verticalScrollBarPolicy() == Qt::ScrollBarAsNeeded,
              QStringLiteral("编辑面板仅在高度不足时出现竖向卷动轴"));
    if (selectedShape && dockPanel) {
        // 需求：属性窗里可编辑的身份 / 几何字段，在编辑面板里同样有行且能改。
        QLineEdit* itemNameEdit = dockPanel->findChild<QLineEdit*>(QStringLiteral("ItemNameEdit"));
        QDoubleSpinBox* positionXSpin = dockPanel->findChild<QDoubleSpinBox*>(QStringLiteral("ItemPositionXSpin"));
        QDoubleSpinBox* sizeWidthSpin = dockPanel->findChild<QDoubleSpinBox*>(QStringLiteral("ItemSizeWidthSpin"));
        QDoubleSpinBox* rotationSpin = dockPanel->findChild<QDoubleSpinBox*>(QStringLiteral("ItemRotationSpin"));
        ctx.Check(itemNameEdit && itemNameEdit->isVisible() && positionXSpin && positionXSpin->isVisible() &&
                      sizeWidthSpin && sizeWidthSpin->isVisible() && rotationSpin && rotationSpin->isVisible(),
                  QStringLiteral("编辑面板提供名称 / 位置 / 尺寸 / 旋转行"));
        if (rotationSpin && positionXSpin && sizeWidthSpin) {
            // 几何行的悬停提示走 PropertiesPanel 上下文的 qm 翻译，且行图标不再
            // 泄漏 ItemRotationSpin 这类内部对象名。
            const auto& tip = [](QDoubleSpinBox* spin) { return spin->toolTip(); };
            const QString rotationTip = QCoreApplication::translate("PropertiesPanel", "Rotation");
            const QString positionTip = QCoreApplication::translate("PropertiesPanel", "Position");
            const QString sizeTip = QCoreApplication::translate("PropertiesPanel", "Item size");
            ctx.Check(tip(rotationSpin) == rotationTip && tip(positionXSpin) == positionTip &&
                          tip(sizeWidthSpin) == sizeTip && !tip(rotationSpin).contains(QStringLiteral("Item")),
                      QStringLiteral("编辑面板旋转 / 位置 / 图元尺寸行提示已翻译"),
                      QStringLiteral("%1|%2|%3").arg(tip(rotationSpin), tip(positionXSpin), tip(sizeWidthSpin)));
            QToolButton* rotationIcon =
                rotationSpin->parentWidget()->findChild<QToolButton*>(QStringLiteral("ItemRowIcon"));
            ctx.Check(rotationIcon && !rotationIcon->toolTip().contains(QStringLiteral("ItemRotation")),
                      QStringLiteral("行图标悬停提示不再是内部对象名"),
                      rotationIcon ? rotationIcon->toolTip() : QStringLiteral("(null)"));
        }

        const QString nameBeforeEdit = selectedShape->Name();
        if (itemNameEdit) {
            itemNameEdit->setText(QStringLiteral("面板改名"));
            QMetaObject::invokeMethod(itemNameEdit, "editingFinished");
            QApplication::processEvents();
        }
        ctx.Check(selectedShape->Name() == QStringLiteral("面板改名"), QStringLiteral("编辑面板改名作用到图元"),
                  selectedShape->Name());
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(selectedShape->Name() == nameBeforeEdit, QStringLiteral("撤销改名恢复图元名称"),
                  selectedShape->Name());

        QComboBox* itemLayerCombo = dockPanel->findChild<QComboBox*>(QStringLiteral("ItemLayerCombo"));
        QSpinBox* itemHeightSpin = dockPanel->findChild<QSpinBox*>(QStringLiteral("ItemHeightSpin"));
        ctx.Check(itemLayerCombo && itemLayerCombo->isVisible() && itemHeightSpin && itemHeightSpin->isVisible(),
                  QStringLiteral("编辑面板提供目标图层 / 层内高度行"));
        if (itemLayerCombo && itemLayerCombo->count() > 1) {
            const int layerBeforeEdit = selectedShape->LayerId();
            itemLayerCombo->setCurrentIndex(1);
            QApplication::processEvents();
            ctx.Check(selectedShape->LayerId() != layerBeforeEdit, QStringLiteral("编辑面板图层行把图元移到目标图层"),
                      QStringLiteral("%1->%2").arg(layerBeforeEdit).arg(selectedShape->LayerId()));
            dockScene->UndoStack()->undo();
            QApplication::processEvents();
        }
        if (itemHeightSpin) {
            const int heightBeforeEdit = selectedShape->ZOrder();
            itemHeightSpin->setValue(heightBeforeEdit + 1);
            QApplication::processEvents();
            ctx.Check(selectedShape->ZOrder() == heightBeforeEdit + 1,
                      QStringLiteral("编辑面板高度行把图元改到目标层内高度"),
                      QStringLiteral("%1->%2").arg(heightBeforeEdit).arg(selectedShape->ZOrder()));
            dockScene->UndoStack()->undo();
            QApplication::processEvents();
        }

        const QPointF positionBeforeEdit = selectedShape->pos();
        if (positionXSpin) {
            positionXSpin->setValue(210.0);
            QApplication::processEvents();
        }
        ctx.Check(qAbs(selectedShape->pos().x() - 210.0) < 0.01, QStringLiteral("编辑面板位置行作用到图元"),
                  QString::number(selectedShape->pos().x()));
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(qAbs(selectedShape->pos().x() - positionBeforeEdit.x()) < 0.01,
                  QStringLiteral("撤销位置恢复图元原坐标"));

        const QSizeF sizeBeforeEdit = selectedShape->ContentSize();
        if (sizeWidthSpin) {
            sizeWidthSpin->setValue(170.0);
            QApplication::processEvents();
        }
        ctx.Check(qAbs(selectedShape->ContentSize().width() - 170.0) < 0.01, QStringLiteral("编辑面板尺寸行作用到图元"),
                  QString::number(selectedShape->ContentSize().width()));
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(qAbs(selectedShape->ContentSize().width() - sizeBeforeEdit.width()) < 0.01,
                  QStringLiteral("撤销尺寸恢复图元原大小"));

        const qreal rotationBeforeEdit = selectedShape->RotationAngle();
        if (rotationSpin) {
            rotationSpin->setValue(20.0);
            QApplication::processEvents();
        }
        ctx.Check(qAbs(selectedShape->RotationAngle() - 20.0) < 0.01, QStringLiteral("编辑面板旋转行作用到图元"),
                  QString::number(selectedShape->RotationAngle()));
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(qAbs(selectedShape->RotationAngle() - rotationBeforeEdit) < 0.01,
                  QStringLiteral("撤销旋转恢复图元原角度"));

        // 平行四边形内角与椭圆长短半轴：属性窗里的专属几何字段，面板同样提供。
        auto* panelParallelogram = ShapeFactory::Create(ShapeType::PARALLELOGRAM);
        panelParallelogram->setPos(QPointF(620.0, 150.0));
        dockScene->AddItemsWithUndo({panelParallelogram}, QStringLiteral("Add panel parallelogram"));
        dockScene->clearSelection();
        panelParallelogram->setSelected(true);
        QApplication::processEvents();
        QDoubleSpinBox* parallelogramAngleSpin =
            dockPanel->findChild<QDoubleSpinBox*>(QStringLiteral("ItemInteriorAngleSpin"));
        ctx.Check(parallelogramAngleSpin && parallelogramAngleSpin->isVisible(),
                  QStringLiteral("选中平行四边形时编辑面板显示内角行"));
        if (parallelogramAngleSpin) {
            parallelogramAngleSpin->setValue(75);
            QApplication::processEvents();
        }
        const qreal panelAngle = panelParallelogram->PropertyValue(ItemProperty::PARALLELOGRAM_ANGLE).toDouble();
        ctx.Check(qAbs(panelAngle - 75.0) < 0.01, QStringLiteral("编辑面板内角行作用到平行四边形"),
                  QString::number(panelAngle));
        dockScene->UndoStack()->undo();
        QApplication::processEvents();

        auto* panelEllipse = ShapeFactory::Create(ShapeType::ELLIPSE);
        panelEllipse->setPos(QPointF(620.0, 320.0));
        panelEllipse->SetContentSize(QSizeF(140.0, 100.0));
        dockScene->AddItemsWithUndo({panelEllipse}, QStringLiteral("Add panel ellipse"));
        dockScene->clearSelection();
        panelEllipse->setSelected(true);
        QApplication::processEvents();
        QDoubleSpinBox* majorAxisSpin = dockPanel->findChild<QDoubleSpinBox*>(QStringLiteral("ItemMajorAxisSpin"));
        QDoubleSpinBox* minorAxisSpin = dockPanel->findChild<QDoubleSpinBox*>(QStringLiteral("ItemMinorAxisSpin"));
        ctx.Check(majorAxisSpin && majorAxisSpin->isVisible() && minorAxisSpin && minorAxisSpin->isVisible(),
                  QStringLiteral("选中椭圆时编辑面板显示长短半轴行"));
        if (majorAxisSpin) {
            majorAxisSpin->setValue(90.0);
            QApplication::processEvents();
        }
        ctx.Check(
            qAbs(panelEllipse->ContentSize().width() - 180.0) < 0.01 &&
                qAbs(panelEllipse->ContentSize().height() - 100.0) < 0.01,
            QStringLiteral("编辑面板长半轴行改写椭圆尺寸"),
            QStringLiteral("%1×%2").arg(panelEllipse->ContentSize().width()).arg(panelEllipse->ContentSize().height()));
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(qAbs(panelEllipse->ContentSize().width() - 140.0) < 0.01,
                  QStringLiteral("撤销长半轴修改恢复椭圆尺寸"));

        // 弧线长短半轴：与椭圆共用同一对行（属性交集驱动显隐），写入保持椭圆中心，
        // 走编辑面板的属性表撤销记录。
        auto* panelArc = new ArcItem;
        panelArc->SetArcGeometry(QPointF(620.0, 460.0), QSizeF(200.0, 100.0), 15.0, 140.0, QColor(Qt::black), 2.0);
        dockScene->AddItemsWithUndo({panelArc}, QStringLiteral("Add panel arc"));
        dockScene->clearSelection();
        panelArc->setSelected(true);
        QApplication::processEvents();
        QDoubleSpinBox* arcMajorSpin = dockPanel->findChild<QDoubleSpinBox*>(QStringLiteral("ItemMajorAxisSpin"));
        QDoubleSpinBox* arcMinorSpin = dockPanel->findChild<QDoubleSpinBox*>(QStringLiteral("ItemMinorAxisSpin"));
        ctx.Check(arcMajorSpin && arcMajorSpin->isVisible() && arcMinorSpin && arcMinorSpin->isVisible() &&
                      qAbs(arcMajorSpin->value() - 100.0) < 0.01 && qAbs(arcMinorSpin->value() - 50.0) < 0.01,
                  QStringLiteral("选中弧线时编辑面板显示真实椭圆长短半轴"),
                  QStringLiteral("major=%1 minor=%2")
                      .arg(arcMajorSpin ? arcMajorSpin->value() : -999.0)
                      .arg(arcMinorSpin ? arcMinorSpin->value() : -999.0));
        if (arcMajorSpin) {
            arcMajorSpin->setValue(90.0);
            QApplication::processEvents();
        }
        ctx.Check(qAbs(panelArc->EllipseSize().width() - 180.0) < 0.01 &&
                      qAbs(panelArc->EllipseSize().height() - 100.0) < 0.01,
                  QStringLiteral("编辑面板长半轴行改写弧线椭圆框"),
                  QStringLiteral("%1x%2").arg(panelArc->EllipseSize().width()).arg(panelArc->EllipseSize().height()));
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(qAbs(panelArc->EllipseSize().width() - 200.0) < 0.01 &&
                      qAbs(panelArc->EllipseSize().height() - 100.0) < 0.01,
                  QStringLiteral("撤销弧线长半轴修改恢复椭圆框"));

        // 需求 2：多选也开放旋转 / 位置 / 尺寸行，行序对齐属性窗；
        // 位置与尺寸按首个图元回显，改动以增量套到其余图元，不会把选中项叠在一起。
        auto* secondGeometryShape = ShapeFactory::Create(ShapeType::RECTANGLE);
        secondGeometryShape->setPos(QPointF(420.0, 260.0));
        secondGeometryShape->SetContentSize(QSizeF(100.0, 80.0));
        dockScene->AddItemsWithUndo({secondGeometryShape}, QStringLiteral("Add second geometry shape"));
        dockScene->clearSelection();
        selectedShape->setSelected(true);
        secondGeometryShape->setSelected(true);
        QApplication::processEvents();

        ctx.Check(positionXSpin && positionXSpin->isVisible() && sizeWidthSpin && sizeWidthSpin->isVisible() &&
                      rotationSpin && rotationSpin->isVisible(),
                  QStringLiteral("多选时编辑面板开放旋转 / 位置 / 尺寸行"));
        ctx.Check(itemNameEdit && !itemNameEdit->isVisible(), QStringLiteral("多选时仍隐藏名称等身份行"));

        const int rotationY = rotationSpin ? rotationSpin->mapTo(dockPanel, QPoint(0, 0)).y() : -1;
        const int positionY = positionXSpin ? positionXSpin->mapTo(dockPanel, QPoint(0, 0)).y() : -1;
        const int sizeY = sizeWidthSpin ? sizeWidthSpin->mapTo(dockPanel, QPoint(0, 0)).y() : -1;
        ctx.Check(rotationY >= 0 && rotationY < positionY && positionY < sizeY,
                  QStringLiteral("几何行按旋转 → 位置 → 尺寸排列，与属性窗一致"),
                  QStringLiteral("y=%1/%2/%3").arg(rotationY).arg(positionY).arg(sizeY));

        const QPointF firstPositionBefore = selectedShape->pos();
        const QPointF secondPositionBefore = secondGeometryShape->pos();
        if (positionXSpin) {
            positionXSpin->setValue(positionXSpin->value() + 30.0);
            QApplication::processEvents();
        }
        ctx.Check(qAbs(selectedShape->pos().x() - firstPositionBefore.x() - 30.0) < 0.01 &&
                      qAbs(secondGeometryShape->pos().x() - secondPositionBefore.x() - 30.0) < 0.01,
                  QStringLiteral("多选位置行按增量同时移动两个图元"));
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(qAbs(selectedShape->pos().x() - firstPositionBefore.x()) < 0.01 &&
                      qAbs(secondGeometryShape->pos().x() - secondPositionBefore.x()) < 0.01,
                  QStringLiteral("撤销多选位置移动同时恢复两个图元"));

        // §3.4 中心点：多选时把成员绝对对齐到同一中心，整次编辑只占一条撤销记录。
        QDoubleSpinBox* centerXSpin = dockPanel->findChild<QDoubleSpinBox*>(QStringLiteral("ItemCenterXSpin"));
        QDoubleSpinBox* centerYSpin = dockPanel->findChild<QDoubleSpinBox*>(QStringLiteral("ItemCenterYSpin"));
        ctx.Check(centerXSpin && centerXSpin->isVisible() && centerYSpin && centerYSpin->isVisible(),
                  QStringLiteral("多选对称图形时编辑面板显示中心点行"));
        const QPointF firstCenterBefore = selectedShape->CenterScenePoint();
        const QPointF secondCenterBefore = secondGeometryShape->CenterScenePoint();
        if (centerXSpin) {
            // 中心点写回是绝对值：改一次即把所有选中项对齐到同一中心（含 Y）。
            centerXSpin->setValue(500.0);
            QApplication::processEvents();
        }
        const QPointF firstCenterAfter = selectedShape->CenterScenePoint();
        const QPointF secondCenterAfter = secondGeometryShape->CenterScenePoint();
        ctx.Check(qAbs(firstCenterAfter.x() - 500.0) < 0.01 && qAbs(secondCenterAfter.x() - 500.0) < 0.01 &&
                      qAbs(firstCenterAfter.y() - secondCenterAfter.y()) < 0.01,
                  QStringLiteral("多选中心点行把两个图元对齐到同一中心"),
                  QStringLiteral("%1,%2/%3,%4")
                      .arg(firstCenterAfter.x())
                      .arg(firstCenterAfter.y())
                      .arg(secondCenterAfter.x())
                      .arg(secondCenterAfter.y()));
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(qAbs(selectedShape->CenterScenePoint().x() - firstCenterBefore.x()) < 0.01 &&
                      qAbs(selectedShape->CenterScenePoint().y() - firstCenterBefore.y()) < 0.01 &&
                      qAbs(secondGeometryShape->CenterScenePoint().x() - secondCenterBefore.x()) < 0.01 &&
                      qAbs(secondGeometryShape->CenterScenePoint().y() - secondCenterBefore.y()) < 0.01,
                  QStringLiteral("撤销中心点对齐一条记录恢复两个图元"));

        const QSizeF firstSizeBefore = selectedShape->ContentSize();
        const QSizeF secondSizeBefore = secondGeometryShape->ContentSize();
        if (sizeWidthSpin) {
            sizeWidthSpin->setValue(sizeWidthSpin->value() + 20.0);
            QApplication::processEvents();
        }
        ctx.Check(qAbs(selectedShape->ContentSize().width() - firstSizeBefore.width() - 20.0) < 0.01 &&
                      qAbs(secondGeometryShape->ContentSize().width() - secondSizeBefore.width() - 20.0) < 0.01,
                  QStringLiteral("多选尺寸行按增量同时改宽两个图元"));
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(qAbs(selectedShape->ContentSize().width() - firstSizeBefore.width()) < 0.01 &&
                      qAbs(secondGeometryShape->ContentSize().width() - secondSizeBefore.width()) < 0.01,
                  QStringLiteral("撤销多选尺寸修改同时恢复两个图元"));

        const qreal firstRotationBefore = selectedShape->RotationAngle();
        const qreal secondRotationBefore = secondGeometryShape->RotationAngle();
        if (rotationSpin) {
            rotationSpin->setValue(25.0);
            QApplication::processEvents();
        }
        ctx.Check(qAbs(selectedShape->RotationAngle() - 25.0) < 0.01 &&
                      qAbs(secondGeometryShape->RotationAngle() - 25.0) < 0.01,
                  QStringLiteral("多选旋转行把同一角度套到两个图元"));
        dockScene->UndoStack()->undo();
        QApplication::processEvents();
        ctx.Check(qAbs(selectedShape->RotationAngle() - firstRotationBefore) < 0.01 &&
                      qAbs(secondGeometryShape->RotationAngle() - secondRotationBefore) < 0.01,
                  QStringLiteral("撤销多选旋转同时恢复两个图元"));
        dockScene->clearSelection();
        QApplication::processEvents();

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
        QWidget* dialogFillStyleRow = polygonDialog.findChild<QWidget*>(QStringLiteral("ItemFillStyleRow"));
        QSpinBox* dialogStrokeWidth = polygonDialog.findChild<QSpinBox*>(QStringLiteral("ItemBorderWidthSpin"));
        const FillStyle dialogFillBefore = polygon->fillStyle();
        const qreal dialogWidthBefore = polygon->StrokeWidth();
        if (dialogFillStyleRow) {
            for (QToolButton* button :
                 dialogFillStyleRow->findChildren<QToolButton*>(QStringLiteral("ItemFillStyleButton"))) {
                if (button->property("fillStyle").toInt() == static_cast<int>(FillStyle::HACHURE)) {
                    button->click();
                    break;
                }
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

        auto* cardImage = ShapeFactory::Create(ShapeType::RECTANGLE);
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
        QWidget* fillStyleRowForImage = propertiesDock->findChild<QWidget*>(QStringLiteral("ItemFillStyleRow"));
        QComboBox* fillImageModeCombo = propertiesDock->findChild<QComboBox*>(QStringLiteral("ItemFillImageMode"));
        QToolButton* clearFillImageButton =
            propertiesDock->findChild<QToolButton*>(QStringLiteral("ItemClearFillImageButton"));
        auto rowIconX = [](QWidget* row) {
            auto* icon = row ? row->findChild<QToolButton*>(QStringLiteral("ItemRowIcon")) : nullptr;
            return icon ? icon->x() : -1;
        };
        ctx.Check(fillImageModeCombo && fillImageModeCombo->count() == 3 && fillImageModeCombo->isEnabled() &&
                      clearFillImageButton && clearFillImageButton->isEnabled() &&
                      clearFillImageButton->text() == QCoreApplication::translate("PropertiesPanel", "Clear"),
                  QStringLiteral("背景图片行提供填充方式与清除按钮"),
                  QStringLiteral("combo=%1 count=%2 clear=%3")
                      .arg(fillImageModeCombo ? 1 : 0)
                      .arg(fillImageModeCombo ? fillImageModeCombo->count() : -1)
                      .arg(clearFillImageButton ? 1 : 0));
        // 图片行没有色板行那样的尾部伸缩，必须显式留白，否则行内空隙会均摊，图标与上方各行错位。
        ctx.Check(rowIconX(fillImageRow) >= 0 && rowIconX(fillImageRow) == rowIconX(fillStyleRowForImage),
                  QStringLiteral("背景图片行图标与上方各行左对齐"),
                  QStringLiteral("image=%1 fill=%2").arg(rowIconX(fillImageRow)).arg(rowIconX(fillStyleRowForImage)));
        if (fillImageModeCombo) {
            const int tileIndex = fillImageModeCombo->findData(static_cast<int>(ImageFill::Mode::TILE));
            if (tileIndex >= 0) {
                fillImageModeCombo->setCurrentIndex(tileIndex);
                QApplication::processEvents();
            }
            ctx.Check(cardImage->PropertyValue(ItemProperty::FILL_IMAGE_MODE).toInt() ==
                          static_cast<int>(ImageFill::Mode::TILE),
                      QStringLiteral("卡片切换背景图摆放方式"),
                      QString::number(static_cast<int>(cardImage->FillImageMode())));
            dockScene->UndoStack()->undo();
            QApplication::processEvents();
            ctx.Check(cardImage->FillImageMode() == ImageFill::Mode::STRETCH,
                      QStringLiteral("撤销背景图摆放方式恢复拉伸"),
                      QString::number(static_cast<int>(cardImage->FillImageMode())));
        }
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
        ctx.Check(!propertiesDock->isVisible(), QStringLiteral("悬浮时清空选中隐藏编辑面板"));
        selectedShape->setSelected(true);
        QApplication::processEvents();
        ctx.Check(propertiesDock->isVisible(), QStringLiteral("悬浮时有选中显示编辑面板"));

        // 拖回主窗口停靠后卡片常驻，不再随选中状态整张显隐。
        propertiesDock->setFloating(false);
        QApplication::processEvents();
        ctx.Check(!propertiesDock->isFloating() && propertiesDock->isVisible(),
                  QStringLiteral("拖回主窗口后停靠并保持显示"));
        dockScene->clearSelection();
        QApplication::processEvents();
        ctx.Check(propertiesDock->isVisible(), QStringLiteral("停靠时清空选中不隐藏编辑面板"));
        ctx.Check(layerDock && layerDock->height() < dockWindow.height() - 100,
                  QStringLiteral("编辑面板停靠时图层卡不撑满窗口高度"));
    }
    qunsetenv("STORYBOARD_CONFIG");
}
