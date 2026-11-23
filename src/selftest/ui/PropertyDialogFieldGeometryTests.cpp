#include "app/I18n.h"
#include "canvas/CanvasView.h"
#include "items/CanvasTypes.h"
#include "items/lines/ArcItem.h"
#include "items/lines/BraceItem.h"
#include "items/lines/CurveItem.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "items/shapes/ShapeRegistry.h"
#include "items/text/TextItem.h"
#include "scene/CanvasScene.h"
#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"
#include "ui/panels/itemdialog/ItemPropertiesDialog.h"

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
#include <QTimer>
#include <QToolButton>
#include <QUndoStack>
#include <QtMath>

namespace {

QToolButton* FindFillStyleButton(QWidget* root, FillStyle style)
{
    for (QToolButton* button : root->findChildren<QToolButton*>(QStringLiteral("ItemFillStyleButton"))) {
        if (button->property("fillStyle").toInt() == static_cast<int>(style)) {
            return button;
        }
    }
    return nullptr;
}

QToolButton* FindPenStyleButton(QWidget* root, Qt::PenStyle style)
{
    for (QToolButton* button : root->findChildren<QToolButton*>(QStringLiteral("ItemBorderStyleButton"))) {
        if (button->property("penStyle").toInt() == static_cast<int>(style)) {
            return button;
        }
    }
    return nullptr;
}

QToolButton* FindSwatch(QWidget* row, const QColor& color)
{
    if (!row) {
        return nullptr;
    }
    for (QToolButton* button : row->findChildren<QToolButton*>(QStringLiteral("ItemColorSwatch"))) {
        if (button->property("swatchColor").value<QColor>().rgb() == color.rgb()) {
            return button;
        }
    }
    return nullptr;
}

bool FillStyleRowHasChecked(QWidget* row, FillStyle style)
{
    QToolButton* button = FindFillStyleButton(row, style);
    return button && button->isChecked();
}

} // namespace


void selftest::RunPropertyDialogFieldGeometryTests(Context& ctx)
{
    CanvasScene& viewScene = ctx.viewScene;
    CanvasView& view = *ctx.view;
    int& itemPropertyRequests = ctx.itemPropertyRequests;

    // 需求 5：圆角矩形并入矩形，这里用「矩形 + 显式 px 圆角」覆盖原来的圆角形态。
    auto* propertyShape = ShapeRegistry::Create(ShapeType::RECTANGLE);
    propertyShape->SetCornerRadius(19.2);
    propertyShape->SetStyle(QColor(QStringLiteral("#1D4ED8")), QColor(QStringLiteral("#BFDBFE")), 6.0);
    propertyShape->SetFill(QColor(QStringLiteral("#BFDBFE")), FillStyle::GRID_HATCH);
    propertyShape->SetContentSize(QSizeF(180.0, 120.0));
    propertyShape->setPos(QPointF(360, 620));
    viewScene.AddItemsWithUndo({propertyShape}, QStringLiteral("Add property shape"));
    view.FitAll();
    QApplication::processEvents();

    const int requestsBeforeShape = itemPropertyRequests;
    const QPoint shapePoint = view.mapFromScene(propertyShape->SceneContentRect().center());
    QMouseEvent shapeDoubleClick(QEvent::MouseButtonDblClick, QPointF(shapePoint),
                                 QPointF(view.viewport()->mapToGlobal(shapePoint)), Qt::LeftButton, Qt::LeftButton,
                                 Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &shapeDoubleClick);
    QApplication::processEvents();
    ctx.Check(itemPropertyRequests == requestsBeforeShape + 1, QStringLiteral("双击图形请求属性弹窗"));

    {
        ItemPropertiesDialog dialog(&viewScene, propertyShape, &view);
        dialog.show();
        QApplication::processEvents();
        QLabel* typeValue = dialog.findChild<QLabel*>(QStringLiteral("ItemPropertiesTypeValue"));
        QLineEdit* nameValue = dialog.findChild<QLineEdit*>(QStringLiteral("ItemPropertiesNameValue"));
        QComboBox* layerValue = dialog.findChild<QComboBox*>(QStringLiteral("ItemPropertiesLayerValue"));
        QSpinBox* heightValue = dialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesHeightValue"));
        QWidget* fillStyleRow = dialog.findChild<QWidget*>(QStringLiteral("ItemFillStyleRow"));
        QWidget* fillColorRow = dialog.findChild<QWidget*>(QStringLiteral("ItemFillColorRow"));
        QWidget* strokeColorRow = dialog.findChild<QWidget*>(QStringLiteral("ItemBorderColorRow"));
        QSpinBox* strokeWidthValue = dialog.findChild<QSpinBox*>(QStringLiteral("ItemBorderWidthSpin"));
        QWidget* borderStyleRow = dialog.findChild<QWidget*>(QStringLiteral("ItemBorderStyleRow"));
        QSpinBox* opacityValue = dialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesOpacityValue"));
        QSpinBox* cornerRadiusValue = dialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesCornerRadiusValue"));
        QDoubleSpinBox* positionXValue =
            dialog.findChild<QDoubleSpinBox*>(QStringLiteral("ItemPropertiesPositionXValue"));
        QDoubleSpinBox* widthValue = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("ItemPropertiesWidthValue"));
        ctx.Check(typeValue && typeValue->text() == propertyShape->DisplayName(), QStringLiteral("属性窗显示图元类型"));
        ctx.Check(UiText(QStringLiteral("Item name")) == QStringLiteral("图元名称"),
                  QStringLiteral("图元名称属性有中文翻译"), UiText(QStringLiteral("Item name")));
        ctx.Check(nameValue && nameValue->isEnabled() && nameValue->text() == propertyShape->Name(),
                  QStringLiteral("属性窗显示图元名称且可编辑"));
        ctx.Check(layerValue && layerValue->isEnabled() &&
                      layerValue->currentText() ==
                          ItemPropertiesDialog::LayerDescription(&viewScene, propertyShape->LayerId()),
                  QStringLiteral("属性窗显示并可编辑图元所在图层"));
        ctx.Check(heightValue && heightValue->isEnabled() && heightValue->value() == propertyShape->ZOrder(),
                  QStringLiteral("属性窗显示并可编辑层内高度"));
        ctx.Check(fillStyleRow && fillStyleRow->isVisible() &&
                      FillStyleRowHasChecked(fillStyleRow, propertyShape->fillStyle()),
                  QStringLiteral("属性窗显示并可编辑填充样式"));
        ctx.Check(fillColorRow && fillColorRow->isVisible() &&
                      fillColorRow->findChild<QToolButton*>(QStringLiteral("ItemColorButton")) != nullptr,
                  QStringLiteral("属性窗填充颜色可编辑"));
        ctx.Check(strokeColorRow && strokeColorRow->isVisible() &&
                      strokeColorRow->findChild<QToolButton*>(QStringLiteral("ItemColorButton")) != nullptr,
                  QStringLiteral("属性窗边框颜色可编辑"));
        ctx.Check(strokeWidthValue && strokeWidthValue->isEnabled() &&
                      qFuzzyCompare(strokeWidthValue->value(), propertyShape->StrokeWidth()) &&
                      strokeWidthValue->minimum() == 1 && strokeWidthValue->maximum() == 64 &&
                      strokeWidthValue->singleStep() == 1 && strokeWidthValue->suffix() == QStringLiteral(" px"),
                  QStringLiteral("属性窗边框宽度可编辑且与工具栏使用同一 SpinBox 规格"));
        ctx.Check(borderStyleRow && borderStyleRow->isVisible() &&
                      FindPenStyleButton(borderStyleRow, Qt::SolidLine) != nullptr,
                  QStringLiteral("属性窗边框样式可编辑"));
        ctx.Check(opacityValue && opacityValue->isEnabled() && opacityValue->value() == 100,
                  QStringLiteral("属性窗透明度可编辑"));
        if (QToolButton* dashButton = FindPenStyleButton(borderStyleRow, Qt::DashLine)) {
            dashButton->click();
        }
        if (opacityValue) {
            opacityValue->setValue(45);
        }
        const int roundedDefaultRadius =
            qRound(qMin(propertyShape->ContentSize().width(), propertyShape->ContentSize().height()) * 0.16);
        ctx.Check(cornerRadiusValue && cornerRadiusValue->isEnabled() &&
                      cornerRadiusValue->value() == roundedDefaultRadius && cornerRadiusValue->value() > 0,
                  QStringLiteral("圆角矩形属性窗显示非零默认圆角"),
                  cornerRadiusValue ? QString::number(cornerRadiusValue->value()) : QString());
        if (cornerRadiusValue) {
            cornerRadiusValue->setValue(40);
        }
        ctx.Check(positionXValue && positionXValue->isEnabled() && widthValue && widthValue->isEnabled(),
                  QStringLiteral("属性窗位置和尺寸可编辑"));
        dialog.accept();
        QApplication::processEvents();
        ctx.Check(qAbs(propertyShape->CornerRadius() - 40.0) < 0.01, QStringLiteral("圆角矩形属性窗可修改圆角"),
                  QString::number(propertyShape->CornerRadius()));
        ctx.Check(propertyShape->StrokeStyle() == Qt::DashLine && qAbs(propertyShape->ItemOpacity() - 0.45) < 0.01,
                  QStringLiteral("属性窗修改可落到边框样式和透明度"),
                  QStringLiteral("style=%1 opacity=%2")
                      .arg(static_cast<int>(propertyShape->StrokeStyle()))
                      .arg(propertyShape->ItemOpacity()));
    }

    // 需求 24：除图元类型外，属性弹窗的修改要真正落到图元，并且一次提交只占一条撤销记录。
    {
        CanvasScene& editScene = ctx.propertyScene;
        QUndoStack& editUndo = ctx.propertyUndo;
        editScene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        editScene.SetMode(ToolMode::EDIT);
        editScene.SetUndoStack(&editUndo);

        auto* firstShape = ShapeRegistry::Create(ShapeType::RECTANGLE);
        firstShape->SetStyle(QColor(QStringLiteral("#1E293B")), QColor(QStringLiteral("#93C5FD")), 3.0);
        firstShape->SetFill(QColor(QStringLiteral("#93C5FD")), FillStyle::SOLID);
        firstShape->SetContentSize(QSizeF(120.0, 80.0));
        firstShape->setPos(QPointF(40.0, 50.0));
        editScene.AddItemsWithUndo({firstShape}, QStringLiteral("Add property test shape"));
        ctx.propertyFirstShape = firstShape;
        const QString firstAutoName = firstShape->CustomName();
        ctx.Check(firstAutoName == QStringLiteral("矩形 1"), QStringLiteral("新图元自动生成名称"), firstAutoName);
        const int firstLayerId = firstShape->LayerId();
        const int firstHeight = firstShape->ZOrder();
        const ShapeItemState firstState = firstShape->State();

        ctx.Check(editScene.AddLayerWithUndo(QStringLiteral("属性目标层")), QStringLiteral("创建属性编辑目标图层"));
        const int targetLayerId = editScene.ActiveLayerId();
        auto* secondShape = ShapeRegistry::Create(ShapeType::ELLIPSE);
        secondShape->SetStyle(QColor(QStringLiteral("#334155")), QColor(QStringLiteral("#FDE68A")), 2.0);
        secondShape->SetContentSize(QSizeF(100.0, 100.0));
        secondShape->setPos(QPointF(240.0, 80.0));
        editScene.AddItemsWithUndo({secondShape}, QStringLiteral("Add target layer shape"));
        ctx.Check(secondShape->CustomName() == QStringLiteral("椭圆 1"), QStringLiteral("不同图元类型各自从 1 编号"),
                  secondShape->CustomName());

        auto* thirdShape = ShapeRegistry::Create(ShapeType::RECTANGLE);
        thirdShape->SetContentSize(QSizeF(90.0, 60.0));
        thirdShape->setPos(QPointF(360.0, 200.0));
        editScene.AddItemsWithUndo({thirdShape}, QStringLiteral("Add second rectangle"));
        ctx.Check(thirdShape->CustomName() == QStringLiteral("矩形 2"), QStringLiteral("同类图元名称后缀自动递增"),
                  thirdShape->CustomName());

        const int undoCountBeforeEdit = editUndo.count();
        {
            ItemPropertiesDialog dialog(&editScene, firstShape);
            dialog.show();
            QApplication::processEvents();
            QComboBox* layerValue = dialog.findChild<QComboBox*>(QStringLiteral("ItemPropertiesLayerValue"));
            QLineEdit* nameValue = dialog.findChild<QLineEdit*>(QStringLiteral("ItemPropertiesNameValue"));
            QSpinBox* heightValue = dialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesHeightValue"));
            QWidget* fillStyleRow = dialog.findChild<QWidget*>(QStringLiteral("ItemFillStyleRow"));
            QWidget* fillColorRow = dialog.findChild<QWidget*>(QStringLiteral("ItemFillColorRow"));
            QWidget* strokeColorRow = dialog.findChild<QWidget*>(QStringLiteral("ItemBorderColorRow"));
            QSpinBox* strokeWidthValue = dialog.findChild<QSpinBox*>(QStringLiteral("ItemBorderWidthSpin"));
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
            ctx.Check(layerValue && heightValue && nameValue && fillStyleRow && fillColorRow && strokeColorRow &&
                          strokeWidthValue && cornerRadiusValue && positionXValue && positionYValue && widthValue &&
                          heightSizeValue && rotationValue,
                      QStringLiteral("属性窗编辑控件齐全"));
            ctx.Check(cornerRadiusValue && cornerRadiusValue->isEnabled() && cornerRadiusValue->value() == 0,
                      QStringLiteral("矩形属性窗圆角默认值为 0"));
            if (cornerRadiusValue) {
                cornerRadiusValue->setValue(20);
            }
            ctx.Check(rotationValue && qFuzzyCompare(rotationValue->value(), firstShape->RotationAngle()),
                      QStringLiteral("属性窗显示图元旋转角度"));

            if (nameValue) {
                nameValue->setText(QStringLiteral("Hero panel"));
            }
            if (layerValue) {
                const int targetIndex = layerValue->findData(targetLayerId);
                ctx.Check(targetIndex >= 0, QStringLiteral("属性窗图层下拉包含目标图层"));
                if (targetIndex >= 0) {
                    layerValue->setCurrentIndex(targetIndex);
                }
            }
            QApplication::processEvents();
            if (heightValue) {
                heightValue->setValue(2);
            }
            if (QToolButton* styleButton = FindFillStyleButton(fillStyleRow, FillStyle::HONEYCOMB)) {
                styleButton->click();
            }
            if (QToolButton* fillSwatch = FindSwatch(fillColorRow, QColor(QStringLiteral("#22C55E")))) {
                fillSwatch->click();
            }
            if (QToolButton* strokeSwatch = FindSwatch(strokeColorRow, QColor(QStringLiteral("#EF4444")))) {
                strokeSwatch->click();
            }
            // 描线宽度来自共享的 StrokeWidthRow。
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

        ctx.Check(editUndo.count() == undoCountBeforeEdit + 1, QStringLiteral("属性窗修改只生成一条撤销记录"));
        ctx.Check(firstShape->LayerId() == targetLayerId && firstShape->ZOrder() == 2,
                  QStringLiteral("属性窗修改可落到图层和层内高度"));
        ctx.Check(firstShape->fillStyle() == FillStyle::HONEYCOMB &&
                      firstShape->FillColor() == QColor(QStringLiteral("#22C55E")),
                  QStringLiteral("属性窗修改可落到填充样式和填充颜色"));
        ctx.Check(firstShape->StrokeColor() == QColor(QStringLiteral("#EF4444")) &&
                      qFuzzyCompare(firstShape->StrokeWidth(), 9.0),
                  QStringLiteral("属性窗修改可落到边框颜色和边框宽度"));
        ctx.Check(qAbs(firstShape->CornerRadius() - 20.0) < 0.01, QStringLiteral("矩形属性窗修改可落到圆角半径"),
                  QString::number(firstShape->CornerRadius()));
        ctx.Check(firstShape->pos() == QPointF(150.0, 210.0) && firstShape->ContentSize() == QSizeF(240.0, 160.0),
                  QStringLiteral("属性窗修改可落到位置和尺寸"));
        ctx.Check(firstShape->CustomName() == QStringLiteral("Hero panel"), QStringLiteral("属性窗修改可落到图元名称"));
        ctx.Check(qFuzzyCompare(firstShape->RotationAngle(), 30.0), QStringLiteral("属性窗修改可落到图元旋转角度"));

        editUndo.undo();
        QApplication::processEvents();
        ctx.Check(firstShape->LayerId() == firstLayerId && firstShape->ZOrder() == firstHeight,
                  QStringLiteral("属性窗修改可整体撤销图层和高度"));
        ctx.Check(qFuzzyCompare(firstShape->RotationAngle(), 0.0), QStringLiteral("属性窗修改可撤销旋转角度"));
        ctx.Check(firstShape->State() == firstState, QStringLiteral("属性窗修改可整体撤销样式和几何"));
        ctx.Check(firstShape->CustomName() == firstAutoName, QStringLiteral("属性窗修改可撤销图元名称"));

        editUndo.redo();
        QApplication::processEvents();
        ctx.Check(firstShape->LayerId() == targetLayerId && firstShape->ZOrder() == 2,
                  QStringLiteral("属性窗修改可整体重做"));
        ctx.Check(firstShape->CustomName() == QStringLiteral("Hero panel"), QStringLiteral("属性窗修改可重做图元名称"));
        ctx.Check(qFuzzyCompare(firstShape->RotationAngle(), 30.0), QStringLiteral("属性窗修改可重做旋转角度"));
    }

    {
        // 需求 27：曲线（花括号除外）与直线一样，属性窗可设置两端箭头样式，
        // 修改进同一条撤销记录并支持撤销 / 重做。
        CanvasScene arrowScene;
        arrowScene.ApplyCanvasSize(QSizeF(800, 600));
        arrowScene.SetMode(ToolMode::EDIT);
        QUndoStack arrowUndo;
        arrowScene.SetUndoStack(&arrowUndo);
        auto* curve = new CurveItem();
        curve->SetLayerId(arrowScene.ActiveLayerId());
        curve->BeginStroke(QPointF(100, 100), QColor(QStringLiteral("#111827")), 4.0);
        for (int i = 0; i < 20; ++i) {
            curve->AppendScenePoint(QPointF(100.0 + i * 6.0, 100.0 + 10.0 * qSin(i * 0.6)));
        }
        curve->EndStroke(0.0);
        arrowScene.AttachItem(curve);
        curve->setSelected(true);

        ItemPropertiesDialog dialog(&arrowScene, curve);
        dialog.show();
        QApplication::processEvents();
        QComboBox* startArrow = dialog.findChild<QComboBox*>(QStringLiteral("ItemPropertiesStartArrowValue"));
        QComboBox* endArrow = dialog.findChild<QComboBox*>(QStringLiteral("ItemPropertiesEndArrowValue"));
        ctx.Check(startArrow && endArrow && startArrow->isEnabled() && endArrow->isEnabled(),
                  QStringLiteral("曲线属性窗两端箭头可用"));
        ctx.Check(startArrow && endArrow &&
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
        ctx.Check(curve->StartArrow() == ArrowStyle::TRIANGLE_FILLED && curve->EndArrow() == ArrowStyle::OPEN_ARROW,
                  QStringLiteral("曲线属性窗修改可落到两端箭头"));
        arrowUndo.undo();
        ctx.Check(curve->StartArrow() == ArrowStyle::NONE && curve->EndArrow() == ArrowStyle::NONE,
                  QStringLiteral("曲线属性窗箭头修改可撤销"));
        arrowUndo.redo();
        ctx.Check(curve->StartArrow() == ArrowStyle::TRIANGLE_FILLED && curve->EndArrow() == ArrowStyle::OPEN_ARROW,
                  QStringLiteral("曲线属性窗箭头修改可重做"));

        // 箭头对全部曲线子型开放：花括号同样提供两端箭头行。
        auto* braceCurve = new BraceItem();
        braceCurve->SetLayerId(arrowScene.ActiveLayerId());
        braceCurve->SetStrokePoints(QVector<QPointF>{QPointF(100, 200), QPointF(130, 240), QPointF(100, 300)},
                                    QColor(QStringLiteral("#111827")), 4.0);
        arrowScene.AttachItem(braceCurve);
        braceCurve->setSelected(true);
        ItemPropertiesDialog braceDialog(&arrowScene, braceCurve);
        braceDialog.show();
        QApplication::processEvents();
        QComboBox* braceStart = braceDialog.findChild<QComboBox*>(QStringLiteral("ItemPropertiesStartArrowValue"));
        QComboBox* braceEnd = braceDialog.findChild<QComboBox*>(QStringLiteral("ItemPropertiesEndArrowValue"));
        ctx.Check(braceStart && braceEnd && braceStart->isEnabled() && braceEnd->isEnabled(),
                  QStringLiteral("花括号曲线属性窗也提供箭头设置"));
    }

    {
        // 模态属性窗打开后，点在窗外的画布上应当等同于 Cancel：关闭并丢弃修改。
        CanvasScene outsideScene;
        outsideScene.ApplyCanvasSize(QSizeF(800, 600));
        outsideScene.SetMode(ToolMode::EDIT);
        auto* outsideShape = ShapeRegistry::Create(ShapeType::RECTANGLE);
        outsideShape->SetLayerId(outsideScene.ActiveLayerId());
        outsideScene.AttachItem(outsideShape);
        outsideShape->setSelected(true);

        ItemPropertiesDialog outsideDialog(&outsideScene, outsideShape, &view);
        outsideDialog.move(600, 500);
        bool closedByOutsideClick = false;
        QTimer::singleShot(0, &outsideDialog, [&]() {
            const QPoint canvasPoint = view.mapFromScene(outsideScene.CanvasRect().topLeft() + QPointF(30, 30));
            const QPoint outsideGlobal = view.viewport()->mapToGlobal(canvasPoint);
            const QPoint outsideLocal = view.viewport()->mapFromGlobal(outsideGlobal);
            QMouseEvent press(QEvent::MouseButtonPress, QPointF(outsideLocal), QPointF(outsideGlobal), Qt::LeftButton,
                              Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(view.viewport(), &press);
            closedByOutsideClick = outsideDialog.result() == QDialog::Rejected && !outsideDialog.isVisible();
        });
        // 兜底，避免过滤器失效时把自检卡在模态事件循环里。
        QTimer::singleShot(1500, &outsideDialog, &QDialog::reject);
        outsideDialog.exec();
        ctx.Check(closedByOutsideClick, QStringLiteral("点击属性窗外的画布按取消关闭属性窗"));
    }

    {
        // 单实例复用：同一属性窗依次换绑椭圆 / 文字 / 弧线，行序固定，
        // 行按图元属性有无显隐；填充 / 描线行的行首图标在属性窗内不再显示。
        CanvasScene reuseScene;
        reuseScene.ApplyCanvasSize(QSizeF(800, 600));
        QUndoStack reuseUndo;
        reuseScene.SetUndoStack(&reuseUndo);

        auto* ellipse = ShapeRegistry::Create(ShapeType::ELLIPSE);
        ellipse->SetLayerId(reuseScene.ActiveLayerId());
        ellipse->SetStyle(QColor(QStringLiteral("#1D4ED8")), QColor(QStringLiteral("#BFDBFE")), 4.0);
        ellipse->SetFill(QColor(QStringLiteral("#BFDBFE")), FillStyle::SOLID);
        reuseScene.AttachItem(ellipse);

        QFont reuseFont(QStringLiteral("Microsoft YaHei UI"));
        reuseFont.setPointSize(14);
        auto* reuseText = new TextItem();
        reuseText->SetTextFormat(QStringLiteral("复用文字"), reuseFont, QColor(QStringLiteral("#0F766E")));
        reuseText->SetLayerId(reuseScene.ActiveLayerId());
        reuseScene.AttachItem(reuseText);

        auto* reuseArc = new ArcItem;
        reuseArc->SetArcGeometry(QPointF(50.0, 50.0), QSizeF(160.0, 80.0), 0.0, 180.0, QColor(Qt::black), 2.0);
        reuseArc->SetLayerId(reuseScene.ActiveLayerId());
        reuseScene.AttachItem(reuseArc);

        ItemPropertiesDialog dialog(&reuseScene, ellipse);
        auto* fillStyleRow = dialog.findChild<QWidget*>(QStringLiteral("ItemFillStyleRow"));
        auto* fillColorRow = dialog.findChild<QWidget*>(QStringLiteral("ItemFillColorRow"));
        auto* borderStyleRow = dialog.findChild<QWidget*>(QStringLiteral("ItemBorderStyleRow"));
        auto* reuseTextEdit = dialog.findChild<QWidget*>(QStringLiteral("ItemPropertiesTextValue"));
        auto* sineCyclesRow = dialog.findChild<QWidget*>(QStringLiteral("ItemPropertiesSineCyclesValue"));
        auto* majorAxisRow = dialog.findChild<QWidget*>(QStringLiteral("ItemPropertiesMajorAxisValue"));
        auto* fillIcon = fillStyleRow ? fillStyleRow->findChild<QToolButton*>(QStringLiteral("ItemRowIcon")) : nullptr;
        ctx.Check(fillStyleRow && fillColorRow && borderStyleRow && fillStyleRow->isVisibleTo(&dialog) && fillIcon &&
                      fillIcon->isHidden(),
                  QStringLiteral("椭圆属性窗显示填充 / 描线行且行首图标隐藏"));
        ctx.Check(majorAxisRow && majorAxisRow->isVisibleTo(&dialog) && reuseTextEdit &&
                      !reuseTextEdit->isVisibleTo(&dialog) && sineCyclesRow && !sineCyclesRow->isVisibleTo(&dialog),
                  QStringLiteral("椭圆属性窗显示半轴行、隐藏文字与正弦行"));

        dialog.SetItem(&reuseScene, reuseText, true);
        ctx.Check(fillStyleRow && !fillStyleRow->isVisibleTo(&dialog) && reuseTextEdit &&
                      reuseTextEdit->isVisibleTo(&dialog),
                  QStringLiteral("同一实例换绑文字后填充行隐藏、文字行显示"));

        dialog.SetItem(&reuseScene, reuseArc, true);
        QComboBox* arrowCombo = dialog.findChild<QComboBox*>(QStringLiteral("ItemPropertiesStartArrowValue"));
        auto* arcAngleRow = dialog.findChild<QWidget*>(QStringLiteral("ItemPropertiesStartAngleValue"));
        auto* arcSweepRow = dialog.findChild<QWidget*>(QStringLiteral("ItemPropertiesSweepAngleValue"));
        auto* arcCenterRow = dialog.findChild<QWidget*>(QStringLiteral("ItemPropertiesCenterXValue"));
        ctx.Check(arrowCombo && arrowCombo->isVisibleTo(&dialog) && arcAngleRow && arcAngleRow->isVisibleTo(&dialog) &&
                      arcSweepRow && arcSweepRow->isVisibleTo(&dialog) && arcCenterRow &&
                      arcCenterRow->isVisibleTo(&dialog) && fillStyleRow && !fillStyleRow->isVisibleTo(&dialog),
                  QStringLiteral("同一实例换绑弧线后箭头 / 角度 / 中心点行显示、填充行仍隐藏"));
        ctx.Check(dialog.Item() == reuseArc, QStringLiteral("属性窗换绑后图元指针已更新"));

        // §4.1：属性窗行数固定为 28 行（1 行图元类型 + 27 行按 ItemProperty 注册的属性行）。
        ctx.Check(dialog.PropertyRowCount() == 28, QStringLiteral("属性窗固定 28 行"),
                  QString::number(dialog.PropertyRowCount()));

        // §4.2：内角 A / B 两行复用。三角形两个底角都显示；平行四边形族只显示内角 A。
        auto* triangle = ShapeRegistry::Create(ShapeType::TRIANGLE);
        triangle->SetLayerId(reuseScene.ActiveLayerId());
        reuseScene.AttachItem(triangle);
        auto* interiorA = dialog.findChild<QWidget*>(QStringLiteral("ItemPropertiesInteriorAngleAValue"));
        auto* interiorB = dialog.findChild<QWidget*>(QStringLiteral("ItemPropertiesInteriorAngleBValue"));
        dialog.SetItem(&reuseScene, triangle, true);
        ctx.Check(interiorA && interiorA->isVisibleTo(&dialog) && interiorB && interiorB->isVisibleTo(&dialog),
                  QStringLiteral("三角形属性窗显示内角 A 与内角 B"));

        auto* parallelogram = ShapeRegistry::Create(ShapeType::PARALLELOGRAM);
        parallelogram->SetLayerId(reuseScene.ActiveLayerId());
        reuseScene.AttachItem(parallelogram);
        dialog.SetItem(&reuseScene, parallelogram, true);
        ctx.Check(interiorA && interiorA->isVisibleTo(&dialog) && interiorB && !interiorB->isVisibleTo(&dialog),
                  QStringLiteral("平行四边形属性窗只显示内角 A"));

        // §4.3：行显隐只看属性表。文字没有内角 / 中心点行，矩形有中心点行。
        dialog.SetItem(&reuseScene, reuseText, true);
        auto* centerRow = dialog.findChild<QWidget*>(QStringLiteral("ItemPropertiesCenterXValue"));
        ctx.Check(interiorA && !interiorA->isVisibleTo(&dialog) && centerRow && !centerRow->isVisibleTo(&dialog),
                  QStringLiteral("文字属性窗隐藏内角与中心点行"));

        auto* centerRect = ShapeRegistry::Create(ShapeType::RECTANGLE);
        centerRect->SetLayerId(reuseScene.ActiveLayerId());
        centerRect->SetContentSize(QSizeF(200.0, 100.0));
        centerRect->setPos(QPointF(100.0, 80.0));
        reuseScene.AttachItem(centerRect);
        dialog.SetItem(&reuseScene, centerRect, true);
        QDoubleSpinBox* centerX = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("ItemPropertiesCenterXValue"));
        QDoubleSpinBox* centerY = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("ItemPropertiesCenterYValue"));
        ctx.Check(centerX && centerX->isVisibleTo(&dialog) && qAbs(centerX->value() - 200.0) < 0.01 && centerY &&
                      qAbs(centerY->value() - 130.0) < 0.01,
                  QStringLiteral("矩形属性窗中心点按几何回显"));
        const QSizeF rectSizeBefore = centerRect->ContentSize();
        const qreal rectRotationBefore = centerRect->RotationAngle();
        if (centerX && centerY) {
            centerX->setValue(300.0);
            centerY->setValue(200.0);
        }
        dialog.accept();
        QApplication::processEvents();
        ctx.Check(qAbs(centerRect->CenterScenePoint().x() - 300.0) < 0.01 &&
                      qAbs(centerRect->CenterScenePoint().y() - 200.0) < 0.01,
                  QStringLiteral("属性窗改中心点后中心落在目标坐标"));
        ctx.Check(centerRect->ContentSize() == rectSizeBefore &&
                      qFuzzyCompare(centerRect->RotationAngle() + 1.0, rectRotationBefore + 1.0),
                  QStringLiteral("中心点写回只平移位置、不改尺寸与旋转"));
    }
}
