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


void selftest::runPropertyDialogTests(Context& ctx)
{
    CanvasScene& viewScene = ctx.viewScene;
    CanvasView& view = *ctx.view;
    int& itemPropertyRequests = ctx.itemPropertyRequests;

    auto* propertyShape = new ShapeItem(ShapeType::RoundedRectangle);
    propertyShape->setStyle(QColor(QStringLiteral("#1D4ED8")), QColor(QStringLiteral("#BFDBFE")), 6.0);
    propertyShape->setFill(QColor(QStringLiteral("#BFDBFE")), FillStyle::GridHatch);
    propertyShape->setContentSize(QSizeF(180.0, 120.0));
    propertyShape->setPos(QPointF(360, 620));
    viewScene.addItemsWithUndo({propertyShape}, QStringLiteral("Add property shape"));
    view.fitAll();
    QApplication::processEvents();

    const int requestsBeforeShape = itemPropertyRequests;
    const QPoint shapePoint = view.mapFromScene(propertyShape->sceneContentRect().center());
    QMouseEvent shapeDoubleClick(QEvent::MouseButtonDblClick, QPointF(shapePoint),
                                 QPointF(view.viewport()->mapToGlobal(shapePoint)), Qt::LeftButton, Qt::LeftButton,
                                 Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &shapeDoubleClick);
    QApplication::processEvents();
    ctx.check(itemPropertyRequests == requestsBeforeShape + 1, QStringLiteral("双击图形请求属性弹窗"));

    {
        ItemPropertiesDialog dialog(&viewScene, propertyShape, &view);
        dialog.show();
        QApplication::processEvents();
        QLabel* typeValue = dialog.findChild<QLabel*>(QStringLiteral("ItemPropertiesTypeValue"));
        QLineEdit* nameValue = dialog.findChild<QLineEdit*>(QStringLiteral("ItemPropertiesNameValue"));
        QComboBox* layerValue = dialog.findChild<QComboBox*>(QStringLiteral("ItemPropertiesLayerValue"));
        QSpinBox* heightValue = dialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesHeightValue"));
        QComboBox* fillStyleValue = dialog.findChild<QComboBox*>(QStringLiteral("ItemPropertiesFillStyleValue"));
        QToolButton* fillColorValue = dialog.findChild<QToolButton*>(QStringLiteral("ItemPropertiesFillColorValue"));
        QToolButton* strokeColorValue =
            dialog.findChild<QToolButton*>(QStringLiteral("ItemPropertiesStrokeColorValue"));
        QDoubleSpinBox* strokeWidthValue =
            dialog.findChild<QDoubleSpinBox*>(QStringLiteral("ItemPropertiesStrokeWidthValue"));
        QDoubleSpinBox* positionXValue =
            dialog.findChild<QDoubleSpinBox*>(QStringLiteral("ItemPropertiesPositionXValue"));
        QDoubleSpinBox* widthValue = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("ItemPropertiesWidthValue"));
        ctx.check(typeValue && typeValue->text() == propertyShape->displayName(), QStringLiteral("属性窗显示图元类型"));
        ctx.check(uiText(QStringLiteral("Item name")) == QStringLiteral("图元名称"),
                  QStringLiteral("图元名称属性有中文翻译"), uiText(QStringLiteral("Item name")));
        ctx.check(nameValue && nameValue->isEnabled() && nameValue->text() == propertyShape->name(),
                  QStringLiteral("属性窗显示图元名称且可编辑"));
        ctx.check(layerValue && layerValue->isEnabled() &&
                      layerValue->currentText() ==
                          ItemPropertiesDialog::layerDescription(&viewScene, propertyShape->layerId()),
                  QStringLiteral("属性窗显示并可编辑图元所在图层"));
        ctx.check(heightValue && heightValue->isEnabled() && heightValue->value() == propertyShape->height(),
                  QStringLiteral("属性窗显示并可编辑层内高度"));
        ctx.check(fillStyleValue && fillStyleValue->isEnabled() &&
                      fillStyleValue->currentText() == fillStyleName(propertyShape->fillStyle()),
                  QStringLiteral("属性窗显示并可编辑填充样式"),
                  fillStyleValue ? fillStyleValue->currentText() : QString());
        ctx.check(fillColorValue && fillColorValue->isEnabled(), QStringLiteral("属性窗填充颜色可编辑"));
        ctx.check(strokeColorValue && strokeColorValue->isEnabled(), QStringLiteral("属性窗描边颜色可编辑"));
        ctx.check(strokeWidthValue && strokeWidthValue->isEnabled() &&
                      qFuzzyCompare(strokeWidthValue->value(), propertyShape->strokeWidth()),
                  QStringLiteral("属性窗描边宽度可编辑"));
        ctx.check(positionXValue && positionXValue->isEnabled() && widthValue && widthValue->isEnabled(),
                  QStringLiteral("属性窗位置和尺寸可编辑"));
        dialog.close();
        QApplication::processEvents();
    }

    // 需求 24：除图元类型外，属性弹窗的修改要真正落到图元，并且一次提交只占一条撤销记录。
    {
        CanvasScene editScene;
        editScene.applyCanvasSize(QSizeF(800.0, 600.0));
        editScene.setMode(ToolMode::Edit);
        QUndoStack editUndo;
        editScene.setUndoStack(&editUndo);

        auto* firstShape = new ShapeItem(ShapeType::Rectangle);
        firstShape->setStyle(QColor(QStringLiteral("#1E293B")), QColor(QStringLiteral("#93C5FD")), 3.0);
        firstShape->setFill(QColor(QStringLiteral("#93C5FD")), FillStyle::Solid);
        firstShape->setContentSize(QSizeF(120.0, 80.0));
        firstShape->setPos(QPointF(40.0, 50.0));
        editScene.addItemsWithUndo({firstShape}, QStringLiteral("Add property test shape"));
        const QString firstAutoName = firstShape->rawName();
        ctx.check(firstAutoName == QStringLiteral("矩形 1"), QStringLiteral("新图元自动生成名称"), firstAutoName);
        const int firstLayerId = firstShape->layerId();
        const int firstHeight = firstShape->height();
        const ShapeItemState firstState = firstShape->state();

        ctx.check(editScene.addLayerWithUndo(QStringLiteral("属性目标层")), QStringLiteral("创建属性编辑目标图层"));
        const int targetLayerId = editScene.activeLayerId();
        auto* secondShape = new ShapeItem(ShapeType::Ellipse);
        secondShape->setStyle(QColor(QStringLiteral("#334155")), QColor(QStringLiteral("#FDE68A")), 2.0);
        secondShape->setContentSize(QSizeF(100.0, 100.0));
        secondShape->setPos(QPointF(240.0, 80.0));
        editScene.addItemsWithUndo({secondShape}, QStringLiteral("Add target layer shape"));
        ctx.check(secondShape->rawName() == QStringLiteral("椭圆 1"), QStringLiteral("不同图元类型各自从 1 编号"),
                  secondShape->rawName());

        auto* thirdShape = new ShapeItem(ShapeType::Rectangle);
        thirdShape->setContentSize(QSizeF(90.0, 60.0));
        thirdShape->setPos(QPointF(360.0, 200.0));
        editScene.addItemsWithUndo({thirdShape}, QStringLiteral("Add second rectangle"));
        ctx.check(thirdShape->rawName() == QStringLiteral("矩形 2"), QStringLiteral("同类图元名称后缀自动递增"),
                  thirdShape->rawName());

        const int undoCountBeforeEdit = editUndo.count();
        {
            ItemPropertiesDialog dialog(&editScene, firstShape);
            dialog.show();
            QApplication::processEvents();
            QComboBox* layerValue = dialog.findChild<QComboBox*>(QStringLiteral("ItemPropertiesLayerValue"));
            QLineEdit* nameValue = dialog.findChild<QLineEdit*>(QStringLiteral("ItemPropertiesNameValue"));
            QSpinBox* heightValue = dialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesHeightValue"));
            QComboBox* fillStyleValue = dialog.findChild<QComboBox*>(QStringLiteral("ItemPropertiesFillStyleValue"));
            QToolButton* fillColorValue =
                dialog.findChild<QToolButton*>(QStringLiteral("ItemPropertiesFillColorValue"));
            QToolButton* strokeColorValue =
                dialog.findChild<QToolButton*>(QStringLiteral("ItemPropertiesStrokeColorValue"));
            QDoubleSpinBox* strokeWidthValue =
                dialog.findChild<QDoubleSpinBox*>(QStringLiteral("ItemPropertiesStrokeWidthValue"));
            QDoubleSpinBox* positionXValue =
                dialog.findChild<QDoubleSpinBox*>(QStringLiteral("ItemPropertiesPositionXValue"));
            QDoubleSpinBox* positionYValue =
                dialog.findChild<QDoubleSpinBox*>(QStringLiteral("ItemPropertiesPositionYValue"));
            QDoubleSpinBox* widthValue = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("ItemPropertiesWidthValue"));
            QDoubleSpinBox* heightSizeValue =
                dialog.findChild<QDoubleSpinBox*>(QStringLiteral("ItemPropertiesHeightSizeValue"));
            QDoubleSpinBox* rotationValue =
                dialog.findChild<QDoubleSpinBox*>(QStringLiteral("ItemPropertiesRotationValue"));
            ctx.check(layerValue && heightValue && nameValue && fillStyleValue && fillColorValue && strokeColorValue &&
                          strokeWidthValue && positionXValue && positionYValue && widthValue && heightSizeValue &&
                          rotationValue,
                      QStringLiteral("属性窗编辑控件齐全"));
            ctx.check(rotationValue && qFuzzyCompare(rotationValue->value(), firstShape->rotationAngle()),
                      QStringLiteral("属性窗显示图元旋转角度"));

            if (nameValue) {
                nameValue->setText(QStringLiteral("Hero panel"));
            }
            if (layerValue) {
                const int targetIndex = layerValue->findData(targetLayerId);
                ctx.check(targetIndex >= 0, QStringLiteral("属性窗图层下拉包含目标图层"));
                if (targetIndex >= 0) {
                    layerValue->setCurrentIndex(targetIndex);
                }
            }
            QApplication::processEvents();
            if (heightValue) {
                heightValue->setValue(2);
            }
            if (fillStyleValue) {
                fillStyleValue->setCurrentIndex(fillStyleValue->findData(static_cast<int>(FillStyle::Honeycomb)));
            }
            if (fillColorValue) {
                fillColorValue->setProperty("itemColor", QColor(QStringLiteral("#22C55E")));
            }
            if (strokeColorValue) {
                strokeColorValue->setProperty("itemColor", QColor(QStringLiteral("#DC2626")));
            }
            if (strokeWidthValue) {
                strokeWidthValue->setValue(9.0);
            }
            if (positionXValue) {
                positionXValue->setValue(150.0);
            }
            if (positionYValue) {
                positionYValue->setValue(210.0);
            }
            if (widthValue) {
                widthValue->setValue(240.0);
            }
            if (heightSizeValue) {
                heightSizeValue->setValue(160.0);
            }
            if (rotationValue) {
                rotationValue->setValue(30.0);
            }
            dialog.accept();
            QApplication::processEvents();
        }

        ctx.check(editUndo.count() == undoCountBeforeEdit + 1, QStringLiteral("属性窗修改只生成一条撤销记录"));
        ctx.check(firstShape->layerId() == targetLayerId && firstShape->height() == 2,
                  QStringLiteral("属性窗修改可落到图层和层内高度"));
        ctx.check(firstShape->fillStyle() == FillStyle::Honeycomb &&
                      firstShape->fillColor() == QColor(QStringLiteral("#22C55E")),
                  QStringLiteral("属性窗修改可落到填充样式和填充颜色"));
        ctx.check(firstShape->strokeColor() == QColor(QStringLiteral("#DC2626")) &&
                      qFuzzyCompare(firstShape->strokeWidth(), 9.0),
                  QStringLiteral("属性窗修改可落到描边颜色和描边宽度"));
        ctx.check(firstShape->pos() == QPointF(150.0, 210.0) && firstShape->contentSize() == QSizeF(240.0, 160.0),
                  QStringLiteral("属性窗修改可落到位置和尺寸"));
        ctx.check(firstShape->rawName() == QStringLiteral("Hero panel"), QStringLiteral("属性窗修改可落到图元名称"));
        ctx.check(qFuzzyCompare(firstShape->rotationAngle(), 30.0), QStringLiteral("属性窗修改可落到图元旋转角度"));

        editUndo.undo();
        QApplication::processEvents();
        ctx.check(firstShape->layerId() == firstLayerId && firstShape->height() == firstHeight,
                  QStringLiteral("属性窗修改可整体撤销图层和高度"));
        ctx.check(qFuzzyCompare(firstShape->rotationAngle(), 0.0), QStringLiteral("属性窗修改可撤销旋转角度"));
        ctx.check(firstShape->state() == firstState, QStringLiteral("属性窗修改可整体撤销样式和几何"));
        ctx.check(firstShape->rawName() == firstAutoName, QStringLiteral("属性窗修改可撤销图元名称"));

        editUndo.redo();
        QApplication::processEvents();
        ctx.check(firstShape->layerId() == targetLayerId && firstShape->height() == 2,
                  QStringLiteral("属性窗修改可整体重做"));
        ctx.check(firstShape->rawName() == QStringLiteral("Hero panel"), QStringLiteral("属性窗修改可重做图元名称"));
        ctx.check(qFuzzyCompare(firstShape->rotationAngle(), 30.0), QStringLiteral("属性窗修改可重做旋转角度"));

        ctx.check(qFuzzyCompare(CanvasItem::snapRotation(12.4), 10.0) &&
                      qFuzzyCompare(CanvasItem::snapRotation(-12.4), -10.0),
                  QStringLiteral("旋转手柄角度按 5° 步长吸附"));
        const QRectF rotationFrame = firstShape->contentRect();
        const QPointF rotationHandlePoint(rotationFrame.center().x(), rotationFrame.top() - 25.0);
        editScene.clearSelection();
        firstShape->setSelected(true);
        ctx.check(firstShape->shape().contains(rotationHandlePoint), QStringLiteral("选中单个图元时显示旋转手柄"));
        firstShape->setSelected(false);
        ctx.check(!firstShape->shape().contains(rotationHandlePoint), QStringLiteral("未选中图元时不显示旋转手柄"));
        firstShape->setSelected(true);
        const int rotateUndoBefore = editUndo.count();
        const int rotatedCount = editScene.rotateSelected(45.0);
        ctx.check(rotatedCount == 1 && qFuzzyCompare(firstShape->rotationAngle(), 75.0) &&
                      editUndo.count() == rotateUndoBefore + 1,
                  QStringLiteral("旋转快捷键对选中图元增加 45°"));
        editUndo.undo();
        ctx.check(qFuzzyCompare(firstShape->rotationAngle(), 30.0), QStringLiteral("旋转快捷键可整体撤销"));
        editUndo.redo();
        ctx.check(qFuzzyCompare(firstShape->rotationAngle(), 75.0), QStringLiteral("旋转快捷键可重做"));
        ctx.check(editScene.rotateSelected(-45.0) == 1 && qFuzzyCompare(firstShape->rotationAngle(), 30.0),
                  QStringLiteral("逆时针旋转快捷键减少 45°"));

        // 需求 35：低缩放下旋转手柄必须仍在 boundingRect 与场景命中范围内。
        {
            CanvasView lowZoomView(&editScene);
            lowZoomView.resize(900, 700);
            lowZoomView.show();
            QApplication::processEvents();
            for (int i = 0; i < 5; ++i) {
                lowZoomView.zoomOut();
            }
            const qreal lowZoom = lowZoomView.zoom();
            ctx.check(lowZoom < 0.6, QStringLiteral("低缩放测试视图确实缩小"));
            firstShape->setSelected(true);
            const QRectF rotationFrame = firstShape->contentRect();
            const qreal lowZoomHandleSize = qBound(6.0, 9.0 / lowZoom, 40.0);
            const QPointF lowZoomHandle(rotationFrame.center().x(),
                                        rotationFrame.top() - (lowZoomHandleSize + 16.0 / lowZoom));
            ctx.check(firstShape->boundingRect().contains(lowZoomHandle),
                      QStringLiteral("低缩放下旋转手柄位于 boundingRect 内"));
            ctx.check(firstShape->shape().contains(lowZoomHandle),
                      QStringLiteral("低缩放下旋转手柄仍在 shape 命中范围内"));
            ctx.check(editScene.items(firstShape->mapToScene(lowZoomHandle)).contains(firstShape),
                      QStringLiteral("低缩放下旋转手柄可被场景命中"));
            lowZoomView.hide();
        }
        editScene.clearSelection();

        QJsonObject namedJson = firstShape->toJson();
        ShapeItem loadedNamedShape;
        loadedNamedShape.fromJson(namedJson);
        ctx.check(loadedNamedShape.rawName() == QStringLiteral("Hero panel"), QStringLiteral("图元名称持久化到画布"));
        ctx.check(qFuzzyCompare(loadedNamedShape.rotationAngle(), 30.0), QStringLiteral("图元旋转角度持久化到画布"));

        const int noopUndoBefore = editUndo.count();
        {
            ItemPropertiesDialog dialog(&editScene, firstShape);
            dialog.show();
            QApplication::processEvents();
            dialog.accept();
            QApplication::processEvents();
        }
        ctx.check(editUndo.count() == noopUndoBefore, QStringLiteral("属性窗未修改时确定不产生撤销记录"));

        auto* imageItem = new ImageItem();
        imageItem->setImage(QPixmap(24, 24), QStringLiteral("old.png"));
        imageItem->setContentSize(QSizeF(80.0, 80.0));
        imageItem->setPos(QPointF(400.0, 200.0));
        editScene.addItemsWithUndo({imageItem}, QStringLiteral("Add property image"));
        ctx.check(imageItem->rawName() == QStringLiteral("图片 1"), QStringLiteral("图片图元自动名称不使用文件名"),
                  imageItem->rawName());
        const int imageUndoBefore = editUndo.count();
        {
            ItemPropertiesDialog dialog(&editScene, imageItem);
            dialog.show();
            QApplication::processEvents();
            QLineEdit* imageName = dialog.findChild<QLineEdit*>(QStringLiteral("ItemPropertiesImageValue"));
            QLabel* typeValue = dialog.findChild<QLabel*>(QStringLiteral("ItemPropertiesTypeValue"));
            QLabel* heading = dialog.findChild<QLabel*>(QStringLiteral("ItemPropertiesHeading"));
            ctx.check(imageName && imageName->isEnabled() && imageName->text() == QStringLiteral("old.png"),
                      QStringLiteral("图片属性窗图片名称可编辑"));
            ctx.check(typeValue && typeValue->text() == uiText(QStringLiteral("Image")) && heading &&
                          heading->text() == imageItem->name(),
                      QStringLiteral("图片属性窗标题和类型使用图元名称与类型名"));
            if (imageName) {
                imageName->setText(QStringLiteral("new.png"));
            }
            dialog.accept();
            QApplication::processEvents();
        }
        ctx.check(imageItem->imageName() == QStringLiteral("new.png") && editUndo.count() == imageUndoBefore + 1,
                  QStringLiteral("图片属性窗修改图片名称并生成一条撤销记录"));
        editUndo.undo();
        QApplication::processEvents();
        ctx.check(imageItem->imageName() == QStringLiteral("old.png"), QStringLiteral("图片属性窗修改可撤销"));

        const int imageBodyUndoIndex = editUndo.index();
        {
            ItemPropertiesDialog dialog(&editScene, imageItem);
            dialog.show();
            QApplication::processEvents();
            QPushButton* replaceButton =
                dialog.findChild<QPushButton*>(QStringLiteral("ItemPropertiesReplaceImageValue"));
            ctx.check(replaceButton && replaceButton->isEnabled() && dialog.isModal(),
                      QStringLiteral("图片属性窗提供替换图片按钮且属性窗为模态"));
            QFile iconFile(QStringLiteral(":/icons/app.png"));
            const bool iconRead = iconFile.open(QIODevice::ReadOnly);
            const QByteArray pngBytes = iconRead ? iconFile.readAll() : QByteArray();
            const QString replacementPath =
                QDir(ctx.workDirectory).filePath(QStringLiteral("chprops-replacement-probe.png"));
            QFile probeFile(replacementPath);
            const bool probeWritten =
                probeFile.open(QIODevice::WriteOnly) && probeFile.write(pngBytes) == pngBytes.size();
            probeFile.close();
            QString probeError;
            const bool probeLoaded =
                probeWritten && !pngBytes.isEmpty() && dialog.loadImageFromFile(replacementPath, &probeError);
            QFile::remove(replacementPath);
            ctx.check(!pngBytes.isEmpty(), QStringLiteral("图片替换探针可读取应用图标资源"),
                      QString::number(pngBytes.size()));
            ctx.check(probeWritten, QStringLiteral("图片替换探针可写入临时图片"), replacementPath);
            ctx.check(probeLoaded, QStringLiteral("图片属性窗可从文件载入替换图片"),
                      probeLoaded ? QStringLiteral("ok") : probeError);
            QPixmap replacement(12, 12);
            replacement.fill(Qt::blue);
            dialog.setReplacementImage(replacement);
            dialog.accept();
            QApplication::processEvents();
        }
        ctx.check(imageItem->image().size() == QSize(12, 12), QStringLiteral("图片本体替换生效"),
                  QStringLiteral("%1x%2").arg(imageItem->image().width()).arg(imageItem->image().height()));
        ctx.check(imageItem->contentSize() == QSizeF(12.0, 12.0), QStringLiteral("图片本体替换后重置为原始像素尺寸"),
                  QStringLiteral("%1x%2").arg(imageItem->contentSize().width()).arg(imageItem->contentSize().height()));
        ctx.check(editUndo.index() == imageBodyUndoIndex + 1, QStringLiteral("图片本体替换进入属性撤销记录"),
                  QStringLiteral("%1 / %2").arg(imageBodyUndoIndex).arg(editUndo.index()));
        editUndo.undo();
        QApplication::processEvents();
        ctx.check(imageItem->image().size() == QSize(24, 24), QStringLiteral("图片本体替换可撤销"));
        ctx.check(imageItem->contentSize() == QSizeF(80.0, 80.0), QStringLiteral("图片本体撤销恢复旧尺寸"));

        // 需求 42：非编辑模式打开属性窗时字段只读，关闭不会写回。
        const int readOnlyUndoBefore = editUndo.count();
        const QString readOnlyImageName = imageItem->imageName();
        {
            ItemPropertiesDialog dialog(&editScene, imageItem, nullptr, false);
            dialog.show();
            QApplication::processEvents();
            QLineEdit* readOnlyName = dialog.findChild<QLineEdit*>(QStringLiteral("ItemPropertiesImageValue"));
            QPushButton* readOnlyReplace =
                dialog.findChild<QPushButton*>(QStringLiteral("ItemPropertiesReplaceImageValue"));
            QLabel* readOnlyHint = dialog.findChild<QLabel*>(QStringLiteral("ItemPropertiesReadOnlyHint"));
            ctx.check(dialog.isModal() && !dialog.isEditable(), QStringLiteral("属性窗在只读口径下仍为模态"));
            ctx.check(readOnlyName && !readOnlyName->isEnabled() && readOnlyReplace && !readOnlyReplace->isEnabled(),
                      QStringLiteral("非编辑模式属性窗字段与替换按钮不可编辑"));
            ctx.check(readOnlyHint != nullptr, QStringLiteral("非编辑模式属性窗显示只读提示"));
            if (readOnlyName) {
                readOnlyName->setText(QStringLiteral("read-only-probe.png"));
            }
            dialog.accept();
            QApplication::processEvents();
        }
        ctx.check(imageItem->imageName() == readOnlyImageName && editUndo.count() == readOnlyUndoBefore,
                  QStringLiteral("只读属性窗关闭后不写回图元"));

        QFont propertyFont(QStringLiteral("Microsoft YaHei UI"));
        propertyFont.setPointSize(16);
        auto* textItem = new TextItem();
        textItem->setTextFormat(QStringLiteral("属性文字"), propertyFont, QColor(QStringLiteral("#0F766E")));
        textItem->setContentSize(QSizeF(200.0, 50.0));
        textItem->setPos(QPointF(420.0, 300.0));
        editScene.addItemsWithUndo({textItem}, QStringLiteral("Add property text"));
        ctx.check(textItem->rawName() == QStringLiteral("文字 1"), QStringLiteral("文字图元自动名称不使用文字内容"),
                  textItem->rawName());

        const int textPropertyUndoBefore = editUndo.count();
        const QSizeF textSizeBefore = textItem->contentSize();
        {
            ItemPropertiesDialog dialog(&editScene, textItem);
            dialog.show();
            QApplication::processEvents();
            QPlainTextEdit* textValue = dialog.findChild<QPlainTextEdit*>(QStringLiteral("ItemPropertiesTextValue"));
            ctx.check(textValue && textValue->isEnabled() && textValue->toPlainText() == QStringLiteral("属性文字"),
                      QStringLiteral("文字属性窗可编辑文字正文"));
            if (textValue) {
                textValue->setPlainText(QStringLiteral("属性文字已更新"));
            }
            dialog.accept();
            QApplication::processEvents();
        }
        ctx.check(textItem->text() == QStringLiteral("属性文字已更新") &&
                      editUndo.count() == textPropertyUndoBefore + 1,
                  QStringLiteral("文字正文修改进入属性撤销记录"));
        ctx.check(textItem->contentSize() == TextItem::measure(textItem->text(), textItem->textFont()),
                  QStringLiteral("文字正文修改后重置为自然尺寸"),
                  QStringLiteral("%1x%2").arg(textItem->contentSize().width()).arg(textItem->contentSize().height()));
        editUndo.undo();
        QApplication::processEvents();
        ctx.check(textItem->text() == QStringLiteral("属性文字"), QStringLiteral("文字正文修改可撤销"));
        ctx.check(textItem->contentSize() == textSizeBefore, QStringLiteral("文字正文撤销恢复旧尺寸"));

        auto* firstNamingStroke = new StrokeItem();
        firstNamingStroke->beginStroke(QPointF(90.0, 420.0), QColor(QStringLiteral("#0F766E")), 5.0);
        firstNamingStroke->appendScenePoint(QPointF(150.0, 470.0));
        firstNamingStroke->appendScenePoint(QPointF(210.0, 440.0));
        firstNamingStroke->endStroke();
        editScene.addItemsWithUndo({firstNamingStroke}, QStringLiteral("Add numbered stroke"));
        auto* secondNamingStroke = new StrokeItem();
        secondNamingStroke->beginStroke(QPointF(260.0, 420.0), QColor(QStringLiteral("#0F766E")), 5.0);
        secondNamingStroke->appendScenePoint(QPointF(320.0, 470.0));
        secondNamingStroke->appendScenePoint(QPointF(380.0, 440.0));
        secondNamingStroke->endStroke();
        editScene.addItemsWithUndo({secondNamingStroke}, QStringLiteral("Add second numbered stroke"));
        ctx.check(firstNamingStroke->rawName() == QStringLiteral("笔画 1") &&
                      secondNamingStroke->rawName() == QStringLiteral("笔画 2"),
                  QStringLiteral("笔画图元名称后缀自动递增"),
                  firstNamingStroke->rawName() + QStringLiteral(" / ") + secondNamingStroke->rawName());

        int strokeUndoBefore = editUndo.count();
        firstNamingStroke->setName(QString());
        {
            ItemPropertiesDialog dialog(&editScene, firstNamingStroke);
            dialog.show();
            QApplication::processEvents();
            QLineEdit* strokeNameValue = dialog.findChild<QLineEdit*>(QStringLiteral("ItemPropertiesNameValue"));
            QLabel* strokeTypeValue = dialog.findChild<QLabel*>(QStringLiteral("ItemPropertiesTypeValue"));
            ctx.check(!firstNamingStroke->rawName().isEmpty() &&
                          firstNamingStroke->rawName().startsWith(QStringLiteral("笔画 ")),
                      QStringLiteral("打开属性窗时为无名图元补上自动编号"), firstNamingStroke->rawName());
            ctx.check(strokeNameValue && strokeNameValue->isEnabled() &&
                          strokeNameValue->text() == firstNamingStroke->name(),
                      QStringLiteral("笔画属性窗显示可编辑的图元名称"),
                      strokeNameValue ? strokeNameValue->text() : QStringLiteral("<missing>"));
            ctx.check(strokeTypeValue && strokeTypeValue->text() == firstNamingStroke->typeDisplayName(),
                      QStringLiteral("笔画属性窗图元类型只读显示当前类型"));
            dialog.reject();
            QApplication::processEvents();
        }
        ctx.check(editUndo.count() == strokeUndoBefore, QStringLiteral("只打开并取消笔画属性窗不产生撤销记录"));
        int unnamedItemCount = 0;
        for (CanvasItem* item : editScene.canvasItems()) {
            if (!item || item->rawName().isEmpty()) {
                ++unnamedItemCount;
            }
        }
        ctx.check(unnamedItemCount == 0, QStringLiteral("场景内所有图元都有名称"), QString::number(unnamedItemCount));
        const QColor textColorBefore = textItem->textColor();
        const int textUndoBefore = editUndo.count();
        {
            ItemPropertiesDialog dialog(&editScene, textItem);
            dialog.show();
            QApplication::processEvents();
            QToolButton* textColorValue =
                dialog.findChild<QToolButton*>(QStringLiteral("ItemPropertiesTextColorValue"));
            QLabel* typeValue = dialog.findChild<QLabel*>(QStringLiteral("ItemPropertiesTypeValue"));
            QLabel* heading = dialog.findChild<QLabel*>(QStringLiteral("ItemPropertiesHeading"));
            ctx.check(typeValue && typeValue->text() == uiText(QStringLiteral("Text")) && heading &&
                          heading->text() == textItem->name(),
                      QStringLiteral("文字属性窗标题和类型使用图元名称与类型名"));
            if (textColorValue) {
                textColorValue->setProperty("itemColor", QColor(QStringLiteral("#DB2777")));
            }
            dialog.accept();
            QApplication::processEvents();
        }
        ctx.check(textItem->textColor() == QColor(QStringLiteral("#DB2777")) && editUndo.count() == textUndoBefore + 1,
                  QStringLiteral("文字属性窗修改文字颜色并生成一条撤销记录"));
        editUndo.undo();
        QApplication::processEvents();
        ctx.check(textItem->textColor() == textColorBefore, QStringLiteral("文字属性窗修改可撤销"));

        {
            ItemPropertiesDialog dialog(&editScene, textItem);
            dialog.show();
            QApplication::processEvents();
            ctx.check(!dialog.findChild<QCheckBox*>(QStringLiteral("ItemPropertiesBoldValue")) &&
                          !dialog.findChild<QCheckBox*>(QStringLiteral("ItemPropertiesItalicValue")) &&
                          !dialog.findChild<QCheckBox*>(QStringLiteral("ItemPropertiesUnderlineValue")) &&
                          !dialog.findChild<QCheckBox*>(QStringLiteral("ItemPropertiesStrikeOutValue")),
                      QStringLiteral("文字属性窗不再显示文字样式选项"));
            dialog.close();
            QApplication::processEvents();
        }

        CanvasScene reloadedNames;
        ctx.check(reloadedNames.fromJson(editScene.toJson()), QStringLiteral("自动名称随画布重新载入"));
        QStringList reloadedNameList;
        for (CanvasItem* item : reloadedNames.canvasItems()) {
            reloadedNameList.append(item->rawName());
        }
        ctx.check(reloadedNameList.contains(QStringLiteral("Hero panel")) &&
                      reloadedNameList.contains(QStringLiteral("矩形 2")) &&
                      reloadedNameList.contains(QStringLiteral("椭圆 1")) &&
                      reloadedNameList.contains(QStringLiteral("图片 1")) &&
                      reloadedNameList.contains(QStringLiteral("文字 1")),
                  QStringLiteral("自动名称画布往返不重复编号"), reloadedNameList.join(QStringLiteral(" / ")));
    }
}
