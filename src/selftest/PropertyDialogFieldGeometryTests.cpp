#include "BraceItem.h"
#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CanvasView.h"
#include "CurveItem.h"
#include "I18n.h"
#include "ItemPropertiesDialog.h"
#include "ShapeItem.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"

#include <QApplication>
#include <QColor>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPointF>
#include <QSizeF>
#include <QSpinBox>
#include <QToolButton>
#include <QUndoStack>
#include <QtMath>


void selftest::runPropertyDialogFieldGeometryTests(Context& ctx)
{
    CanvasScene& viewScene = ctx.viewScene;
    CanvasView& view = *ctx.view;
    int& itemPropertyRequests = ctx.itemPropertyRequests;

    // 需求 5：圆角矩形并入矩形，这里用「矩形 + 显式 px 圆角」覆盖原来的圆角形态。
    auto* propertyShape = createShapeItem(ShapeType::RECTANGLE);
    propertyShape->SetCornerRadius(19.2);
    propertyShape->setStyle(QColor(QStringLiteral("#1D4ED8")), QColor(QStringLiteral("#BFDBFE")), 6.0);
    propertyShape->SetFill(QColor(QStringLiteral("#BFDBFE")), FillStyle::GRID_HATCH);
    propertyShape->SetContentSize(QSizeF(180.0, 120.0));
    propertyShape->setPos(QPointF(360, 620));
    viewScene.addItemsWithUndo({propertyShape}, QStringLiteral("Add property shape"));
    view.fitAll();
    QApplication::processEvents();

    const int requestsBeforeShape = itemPropertyRequests;
    const QPoint shapePoint = view.mapFromScene(propertyShape->SceneContentRect().center());
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
        QSpinBox* strokeWidthValue = dialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesStrokeWidthValue"));
        QComboBox* borderStyleValue = dialog.findChild<QComboBox*>(QStringLiteral("ItemPropertiesBorderStyleValue"));
        QSpinBox* opacityValue = dialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesOpacityValue"));
        QSpinBox* cornerRadiusValue = dialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesCornerRadiusValue"));
        QDoubleSpinBox* positionXValue =
            dialog.findChild<QDoubleSpinBox*>(QStringLiteral("ItemPropertiesPositionXValue"));
        QDoubleSpinBox* widthValue = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("ItemPropertiesWidthValue"));
        ctx.check(typeValue && typeValue->text() == propertyShape->DisplayName(), QStringLiteral("属性窗显示图元类型"));
        ctx.check(uiText(QStringLiteral("Item name")) == QStringLiteral("图元名称"),
                  QStringLiteral("图元名称属性有中文翻译"), uiText(QStringLiteral("Item name")));
        ctx.check(nameValue && nameValue->isEnabled() && nameValue->text() == propertyShape->Name(),
                  QStringLiteral("属性窗显示图元名称且可编辑"));
        ctx.check(layerValue && layerValue->isEnabled() &&
                      layerValue->currentText() ==
                          ItemPropertiesDialog::layerDescription(&viewScene, propertyShape->LayerId()),
                  QStringLiteral("属性窗显示并可编辑图元所在图层"));
        ctx.check(heightValue && heightValue->isEnabled() && heightValue->value() == propertyShape->ZOrder(),
                  QStringLiteral("属性窗显示并可编辑层内高度"));
        ctx.check(fillStyleValue && fillStyleValue->isEnabled() &&
                      fillStyleValue->currentText() == fillStyleName(propertyShape->fillStyle()),
                  QStringLiteral("属性窗显示并可编辑填充样式"),
                  fillStyleValue ? fillStyleValue->currentText() : QString());
        ctx.check(fillColorValue && fillColorValue->isEnabled(), QStringLiteral("属性窗填充颜色可编辑"));
        ctx.check(strokeColorValue && strokeColorValue->isEnabled(), QStringLiteral("属性窗边框颜色可编辑"));
        ctx.check(strokeWidthValue && strokeWidthValue->isEnabled() &&
                      qFuzzyCompare(strokeWidthValue->value(), propertyShape->StrokeWidth()) &&
                      strokeWidthValue->minimum() == 1 && strokeWidthValue->maximum() == 64 &&
                      strokeWidthValue->singleStep() == 1 && strokeWidthValue->suffix() == QStringLiteral(" px"),
                  QStringLiteral("属性窗边框宽度可编辑且与工具栏使用同一 SpinBox 规格"));
        ctx.check(borderStyleValue && borderStyleValue->isEnabled() &&
                      borderStyleValue->currentData().toInt() == static_cast<int>(Qt::SolidLine),
                  QStringLiteral("属性窗边框样式可编辑"));
        ctx.check(opacityValue && opacityValue->isEnabled() && opacityValue->value() == 100,
                  QStringLiteral("属性窗透明度可编辑"));
        if (borderStyleValue) {
            borderStyleValue->setCurrentIndex(borderStyleValue->findData(static_cast<int>(Qt::DashLine)));
        }
        if (opacityValue) {
            opacityValue->setValue(45);
        }
        const int roundedDefaultRadius =
            qRound(qMin(propertyShape->contentSize().width(), propertyShape->contentSize().height()) * 0.16);
        ctx.check(cornerRadiusValue && cornerRadiusValue->isEnabled() &&
                      cornerRadiusValue->value() == roundedDefaultRadius && cornerRadiusValue->value() > 0,
                  QStringLiteral("圆角矩形属性窗显示非零默认圆角"),
                  cornerRadiusValue ? QString::number(cornerRadiusValue->value()) : QString());
        if (cornerRadiusValue) {
            cornerRadiusValue->setValue(40);
        }
        ctx.check(positionXValue && positionXValue->isEnabled() && widthValue && widthValue->isEnabled(),
                  QStringLiteral("属性窗位置和尺寸可编辑"));
        dialog.accept();
        QApplication::processEvents();
        ctx.check(qAbs(propertyShape->CornerRadius() - 40.0) < 0.01, QStringLiteral("圆角矩形属性窗可修改圆角"),
                  QString::number(propertyShape->CornerRadius()));
        ctx.check(propertyShape->StrokeStyle() == Qt::DashLine && qAbs(propertyShape->ItemOpacity() - 0.45) < 0.01,
                  QStringLiteral("属性窗修改可落到边框样式和透明度"),
                  QStringLiteral("style=%1 opacity=%2")
                      .arg(static_cast<int>(propertyShape->StrokeStyle()))
                      .arg(propertyShape->ItemOpacity()));
    }

    // 需求 24：除图元类型外，属性弹窗的修改要真正落到图元，并且一次提交只占一条撤销记录。
    {
        CanvasScene& editScene = ctx.propertyScene;
        QUndoStack& editUndo = ctx.propertyUndo;
        editScene.applyCanvasSize(QSizeF(800.0, 600.0));
        editScene.setMode(ToolMode::EDIT);
        editScene.setUndoStack(&editUndo);

        auto* firstShape = createShapeItem(ShapeType::RECTANGLE);
        firstShape->setStyle(QColor(QStringLiteral("#1E293B")), QColor(QStringLiteral("#93C5FD")), 3.0);
        firstShape->SetFill(QColor(QStringLiteral("#93C5FD")), FillStyle::SOLID);
        firstShape->SetContentSize(QSizeF(120.0, 80.0));
        firstShape->setPos(QPointF(40.0, 50.0));
        editScene.addItemsWithUndo({firstShape}, QStringLiteral("Add property test shape"));
        ctx.propertyFirstShape = firstShape;
        const QString firstAutoName = firstShape->CustomName();
        ctx.check(firstAutoName == QStringLiteral("矩形 1"), QStringLiteral("新图元自动生成名称"), firstAutoName);
        const int firstLayerId = firstShape->LayerId();
        const int firstHeight = firstShape->ZOrder();
        const ShapeItemState firstState = firstShape->State();

        ctx.check(editScene.addLayerWithUndo(QStringLiteral("属性目标层")), QStringLiteral("创建属性编辑目标图层"));
        const int targetLayerId = editScene.activeLayerId();
        auto* secondShape = createShapeItem(ShapeType::ELLIPSE);
        secondShape->setStyle(QColor(QStringLiteral("#334155")), QColor(QStringLiteral("#FDE68A")), 2.0);
        secondShape->SetContentSize(QSizeF(100.0, 100.0));
        secondShape->setPos(QPointF(240.0, 80.0));
        editScene.addItemsWithUndo({secondShape}, QStringLiteral("Add target layer shape"));
        ctx.check(secondShape->CustomName() == QStringLiteral("椭圆 1"), QStringLiteral("不同图元类型各自从 1 编号"),
                  secondShape->CustomName());

        auto* thirdShape = createShapeItem(ShapeType::RECTANGLE);
        thirdShape->SetContentSize(QSizeF(90.0, 60.0));
        thirdShape->setPos(QPointF(360.0, 200.0));
        editScene.addItemsWithUndo({thirdShape}, QStringLiteral("Add second rectangle"));
        ctx.check(thirdShape->CustomName() == QStringLiteral("矩形 2"), QStringLiteral("同类图元名称后缀自动递增"),
                  thirdShape->CustomName());

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
            QSpinBox* strokeWidthValue = dialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesStrokeWidthValue"));
            QSpinBox* cornerRadiusValue =
                dialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesCornerRadiusValue"));
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
                          strokeWidthValue && cornerRadiusValue && positionXValue && positionYValue && widthValue &&
                          heightSizeValue && rotationValue,
                      QStringLiteral("属性窗编辑控件齐全"));
            ctx.check(cornerRadiusValue && cornerRadiusValue->isEnabled() && cornerRadiusValue->value() == 0,
                      QStringLiteral("矩形属性窗圆角默认值为 0"));
            if (cornerRadiusValue) {
                cornerRadiusValue->setValue(20);
            }
            ctx.check(rotationValue && qFuzzyCompare(rotationValue->value(), firstShape->RotationAngle()),
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
                fillStyleValue->setCurrentIndex(fillStyleValue->findData(static_cast<int>(FillStyle::HONEYCOMB)));
            }
            if (fillColorValue) {
                fillColorValue->setProperty("itemColor", QColor(QStringLiteral("#22C55E")));
            }
            if (strokeColorValue) {
                strokeColorValue->setProperty("itemColor", QColor(QStringLiteral("#DC2626")));
            }
            if (strokeWidthValue) {
                strokeWidthValue->setValue(9);
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
        ctx.check(firstShape->LayerId() == targetLayerId && firstShape->ZOrder() == 2,
                  QStringLiteral("属性窗修改可落到图层和层内高度"));
        ctx.check(firstShape->fillStyle() == FillStyle::HONEYCOMB &&
                      firstShape->FillColor() == QColor(QStringLiteral("#22C55E")),
                  QStringLiteral("属性窗修改可落到填充样式和填充颜色"));
        ctx.check(firstShape->StrokeColor() == QColor(QStringLiteral("#DC2626")) &&
                      qFuzzyCompare(firstShape->StrokeWidth(), 9.0),
                  QStringLiteral("属性窗修改可落到边框颜色和边框宽度"));
        ctx.check(qAbs(firstShape->CornerRadius() - 20.0) < 0.01, QStringLiteral("矩形属性窗修改可落到圆角半径"),
                  QString::number(firstShape->CornerRadius()));
        ctx.check(firstShape->pos() == QPointF(150.0, 210.0) && firstShape->contentSize() == QSizeF(240.0, 160.0),
                  QStringLiteral("属性窗修改可落到位置和尺寸"));
        ctx.check(firstShape->CustomName() == QStringLiteral("Hero panel"), QStringLiteral("属性窗修改可落到图元名称"));
        ctx.check(qFuzzyCompare(firstShape->RotationAngle(), 30.0), QStringLiteral("属性窗修改可落到图元旋转角度"));

        editUndo.undo();
        QApplication::processEvents();
        ctx.check(firstShape->LayerId() == firstLayerId && firstShape->ZOrder() == firstHeight,
                  QStringLiteral("属性窗修改可整体撤销图层和高度"));
        ctx.check(qFuzzyCompare(firstShape->RotationAngle(), 0.0), QStringLiteral("属性窗修改可撤销旋转角度"));
        ctx.check(firstShape->State() == firstState, QStringLiteral("属性窗修改可整体撤销样式和几何"));
        ctx.check(firstShape->CustomName() == firstAutoName, QStringLiteral("属性窗修改可撤销图元名称"));

        editUndo.redo();
        QApplication::processEvents();
        ctx.check(firstShape->LayerId() == targetLayerId && firstShape->ZOrder() == 2,
                  QStringLiteral("属性窗修改可整体重做"));
        ctx.check(firstShape->CustomName() == QStringLiteral("Hero panel"), QStringLiteral("属性窗修改可重做图元名称"));
        ctx.check(qFuzzyCompare(firstShape->RotationAngle(), 30.0), QStringLiteral("属性窗修改可重做旋转角度"));
    }

    {
        // 需求 27：曲线（花括号除外）与直线一样，属性窗可设置两端箭头样式，
        // 修改进同一条撤销记录并支持撤销 / 重做。
        CanvasScene arrowScene;
        arrowScene.applyCanvasSize(QSizeF(800, 600));
        arrowScene.setMode(ToolMode::EDIT);
        QUndoStack arrowUndo;
        arrowScene.setUndoStack(&arrowUndo);
        auto* curve = new CurveItem();
        curve->SetLayerId(arrowScene.activeLayerId());
        curve->beginStroke(QPointF(100, 100), QColor(QStringLiteral("#111827")), 4.0);
        for (int i = 0; i < 20; ++i) {
            curve->appendScenePoint(QPointF(100.0 + i * 6.0, 100.0 + 10.0 * qSin(i * 0.6)));
        }
        curve->endStroke(0.0);
        arrowScene.attachItem(curve);
        curve->setSelected(true);

        ItemPropertiesDialog dialog(&arrowScene, curve);
        dialog.show();
        QApplication::processEvents();
        QComboBox* startArrow = dialog.findChild<QComboBox*>(QStringLiteral("ItemPropertiesStartArrowValue"));
        QComboBox* endArrow = dialog.findChild<QComboBox*>(QStringLiteral("ItemPropertiesEndArrowValue"));
        ctx.check(startArrow && endArrow && startArrow->isEnabled() && endArrow->isEnabled(),
                  QStringLiteral("曲线属性窗两端箭头可用"));
        ctx.check(startArrow && endArrow &&
                      startArrow->currentData().toInt() == static_cast<int>(curve->StartArrow()) &&
                      endArrow->currentData().toInt() == static_cast<int>(curve->EndArrow()),
                  QStringLiteral("曲线属性窗箭头回显当前样式"));
        if (startArrow) {
            startArrow->setCurrentIndex(startArrow->findData(static_cast<int>(ArrowStyle::TRIANGLE_FILLED)));
        }
        if (endArrow) {
            endArrow->setCurrentIndex(endArrow->findData(static_cast<int>(ArrowStyle::OPEN_ARROW)));
        }
        dialog.accept();
        QApplication::processEvents();
        ctx.check(curve->StartArrow() == ArrowStyle::TRIANGLE_FILLED && curve->EndArrow() == ArrowStyle::OPEN_ARROW,
                  QStringLiteral("曲线属性窗修改可落到两端箭头"));
        arrowUndo.undo();
        ctx.check(curve->StartArrow() == ArrowStyle::NONE && curve->EndArrow() == ArrowStyle::NONE,
                  QStringLiteral("曲线属性窗箭头修改可撤销"));
        arrowUndo.redo();
        ctx.check(curve->StartArrow() == ArrowStyle::TRIANGLE_FILLED && curve->EndArrow() == ArrowStyle::OPEN_ARROW,
                  QStringLiteral("曲线属性窗箭头修改可重做"));

        // 箭头对全部曲线子型开放：花括号同样提供两端箭头行。
        auto* braceCurve = new BraceItem();
        braceCurve->SetLayerId(arrowScene.activeLayerId());
        braceCurve->SetStrokePoints(QVector<QPointF>{QPointF(100, 200), QPointF(130, 240), QPointF(100, 300)},
                                    QColor(QStringLiteral("#111827")), 4.0);
        arrowScene.attachItem(braceCurve);
        braceCurve->setSelected(true);
        ItemPropertiesDialog braceDialog(&arrowScene, braceCurve);
        braceDialog.show();
        QApplication::processEvents();
        QComboBox* braceStart = braceDialog.findChild<QComboBox*>(QStringLiteral("ItemPropertiesStartArrowValue"));
        QComboBox* braceEnd = braceDialog.findChild<QComboBox*>(QStringLiteral("ItemPropertiesEndArrowValue"));
        ctx.check(braceStart && braceEnd && braceStart->isEnabled() && braceEnd->isEnabled(),
                  QStringLiteral("花括号曲线属性窗也提供箭头设置"));
    }
}
