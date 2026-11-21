#include "CanvasScene.h"
#include "CurveItem.h"
#include "I18n.h"
#include "ItemPropertiesDialog.h"
#include "ShapeItem.h"
#include "TextItem.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"

#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QDoubleSpinBox>
#include <QFont>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointF>
#include <QSizeF>
#include <QSpinBox>
#include <QStringList>
#include <QToolButton>
#include <QUndoStack>


void selftest::RunPropertyDialogTextNamingTests(Context& ctx)
{
    CanvasScene& editScene = ctx.propertyScene;
    QUndoStack& editUndo = ctx.propertyUndo;

    QFont propertyFont(QStringLiteral("Microsoft YaHei UI"));
    propertyFont.setPointSize(16);
    auto* textItem = new TextItem();
    textItem->SetTextFormat(QStringLiteral("属性文字"), propertyFont, QColor(QStringLiteral("#0F766E")));
    textItem->SetContentSize(QSizeF(200.0, 50.0));
    textItem->setPos(QPointF(420.0, 300.0));
    editScene.AddItemsWithUndo({textItem}, QStringLiteral("Add property text"));
    ctx.Check(textItem->CustomName() == QStringLiteral("文字 1"), QStringLiteral("文字图元自动名称不使用文字内容"),
              textItem->CustomName());

    const int textPropertyUndoBefore = editUndo.count();
    const QSizeF textSizeBefore = textItem->ContentSize();
    {
        ItemPropertiesDialog dialog(&editScene, textItem);
        dialog.show();
        QApplication::processEvents();
        QPlainTextEdit* textValue = dialog.findChild<QPlainTextEdit*>(QStringLiteral("ItemPropertiesTextValue"));
        ctx.Check(textValue && textValue->isEnabled() && textValue->toPlainText() == QStringLiteral("属性文字"),
                  QStringLiteral("文字属性窗可编辑文字正文"));
        if (textValue) {
            textValue->setPlainText(QStringLiteral("属性文字已更新"));
        }
        dialog.accept();
        QApplication::processEvents();
    }
    ctx.Check(textItem->Text() == QStringLiteral("属性文字已更新") && editUndo.count() == textPropertyUndoBefore + 1,
              QStringLiteral("文字正文修改进入属性撤销记录"));
    ctx.Check(textItem->ContentSize() == TextItem::Measure(textItem->Text(), textItem->TextFont()),
              QStringLiteral("文字正文修改后重置为自然尺寸"),
              QStringLiteral("%1x%2").arg(textItem->ContentSize().width()).arg(textItem->ContentSize().height()));
    editUndo.undo();
    QApplication::processEvents();
    ctx.Check(textItem->Text() == QStringLiteral("属性文字"), QStringLiteral("文字正文修改可撤销"));
    ctx.Check(textItem->ContentSize() == textSizeBefore, QStringLiteral("文字正文撤销恢复旧尺寸"));

    // 文字不是图形，属性窗单独提供透明度行；修改同样进入撤销记录。
    {
        ItemPropertiesDialog dialog(&editScene, textItem);
        dialog.show();
        QApplication::processEvents();
        QSpinBox* opacityValue = dialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesOpacityValue"));
        ctx.Check(opacityValue && opacityValue->isEnabled() && opacityValue->value() == 100,
                  QStringLiteral("文字属性窗提供透明度编辑"),
                  opacityValue ? QString::number(opacityValue->value()) : QString());
        if (opacityValue) {
            opacityValue->setValue(35);
        }
        dialog.accept();
        QApplication::processEvents();
    }
    ctx.Check(qAbs(textItem->ItemOpacity() - 0.35) < 0.01 && editUndo.canUndo(),
              QStringLiteral("文字属性窗修改透明度并可撤销"),
              QStringLiteral("opacity=%1 canUndo=%2").arg(textItem->ItemOpacity()).arg(editUndo.canUndo() ? 1 : 0));
    editUndo.undo();
    QApplication::processEvents();
    ctx.Check(qAbs(textItem->ItemOpacity() - 1.0) < 0.01, QStringLiteral("撤销文字透明度恢复原值"),
              QString::number(textItem->ItemOpacity()));

    auto* firstNamingStroke = new CurveItem();
    firstNamingStroke->BeginStroke(QPointF(90.0, 420.0), QColor(QStringLiteral("#0F766E")), 5.0);
    firstNamingStroke->AppendScenePoint(QPointF(150.0, 470.0));
    firstNamingStroke->AppendScenePoint(QPointF(210.0, 440.0));
    firstNamingStroke->EndStroke();
    editScene.AddItemsWithUndo({firstNamingStroke}, QStringLiteral("Add numbered stroke"));
    auto* secondNamingStroke = new CurveItem();
    secondNamingStroke->BeginStroke(QPointF(260.0, 420.0), QColor(QStringLiteral("#0F766E")), 5.0);
    secondNamingStroke->AppendScenePoint(QPointF(320.0, 470.0));
    secondNamingStroke->AppendScenePoint(QPointF(380.0, 440.0));
    secondNamingStroke->EndStroke();
    editScene.AddItemsWithUndo({secondNamingStroke}, QStringLiteral("Add second numbered stroke"));
    ctx.Check(firstNamingStroke->CustomName() == QStringLiteral("曲线 1") &&
                  secondNamingStroke->CustomName() == QStringLiteral("曲线 2"),
              QStringLiteral("曲线图元名称后缀自动递增"),
              firstNamingStroke->CustomName() + QStringLiteral(" / ") + secondNamingStroke->CustomName());

    int strokeUndoBefore = editUndo.count();
    firstNamingStroke->SetName(QString());
    {
        ItemPropertiesDialog dialog(&editScene, firstNamingStroke);
        dialog.show();
        QApplication::processEvents();
        QLineEdit* strokeNameValue = dialog.findChild<QLineEdit*>(QStringLiteral("ItemPropertiesNameValue"));
        QLabel* strokeTypeValue = dialog.findChild<QLabel*>(QStringLiteral("ItemPropertiesTypeValue"));
        ctx.Check(!firstNamingStroke->CustomName().isEmpty() &&
                      firstNamingStroke->CustomName().startsWith(QStringLiteral("曲线 ")),
                  QStringLiteral("打开属性窗时为无名图元补上自动编号"), firstNamingStroke->CustomName());
        ctx.Check(strokeNameValue && strokeNameValue->isEnabled() &&
                      strokeNameValue->text() == firstNamingStroke->Name(),
                  QStringLiteral("曲线属性窗显示可编辑的图元名称"),
                  strokeNameValue ? strokeNameValue->text() : QStringLiteral("<missing>"));
        ctx.Check(strokeTypeValue && strokeTypeValue->text() == firstNamingStroke->TypeDisplayName(),
                  QStringLiteral("曲线属性窗图元类型只读显示当前类型"));
        dialog.reject();
        QApplication::processEvents();
    }
    ctx.Check(editUndo.count() == strokeUndoBefore, QStringLiteral("只打开并取消曲线属性窗不产生撤销记录"));
    int unnamedItemCount = 0;
    for (CanvasItem* item : editScene.CanvasItems()) {
        if (!item || item->CustomName().isEmpty()) {
            ++unnamedItemCount;
        }
    }
    ctx.Check(unnamedItemCount == 0, QStringLiteral("场景内所有图元都有名称"), QString::number(unnamedItemCount));
    const QColor textColorBefore = textItem->TextColor();
    const int textUndoBefore = editUndo.count();
    {
        ItemPropertiesDialog dialog(&editScene, textItem);
        dialog.show();
        QApplication::processEvents();
        QToolButton* textColorValue = dialog.findChild<QToolButton*>(QStringLiteral("ItemPropertiesTextColorValue"));
        QLabel* typeValue = dialog.findChild<QLabel*>(QStringLiteral("ItemPropertiesTypeValue"));
        QLabel* heading = dialog.findChild<QLabel*>(QStringLiteral("ItemPropertiesHeading"));
        ctx.Check(typeValue && typeValue->text() == UiText(QStringLiteral("Text")) && heading &&
                      heading->text() == textItem->Name(),
                  QStringLiteral("文字属性窗标题和类型使用图元名称与类型名"));
        if (textColorValue) {
            textColorValue->setProperty("itemColor", QColor(QStringLiteral("#DB2777")));
        }
        dialog.accept();
        QApplication::processEvents();
    }
    ctx.Check(textItem->TextColor() == QColor(QStringLiteral("#DB2777")) && editUndo.count() == textUndoBefore + 1,
              QStringLiteral("文字属性窗修改文字颜色并生成一条撤销记录"));
    editUndo.undo();
    QApplication::processEvents();
    ctx.Check(textItem->TextColor() == textColorBefore, QStringLiteral("文字属性窗修改可撤销"));

    {
        ItemPropertiesDialog dialog(&editScene, textItem);
        dialog.show();
        QApplication::processEvents();
        ctx.Check(!dialog.findChild<QCheckBox*>(QStringLiteral("ItemPropertiesBoldValue")) &&
                      !dialog.findChild<QCheckBox*>(QStringLiteral("ItemPropertiesItalicValue")) &&
                      !dialog.findChild<QCheckBox*>(QStringLiteral("ItemPropertiesUnderlineValue")) &&
                      !dialog.findChild<QCheckBox*>(QStringLiteral("ItemPropertiesStrikeOutValue")),
                  QStringLiteral("文字属性窗不再显示文字样式选项"));
        dialog.close();
        QApplication::processEvents();
    }

    CanvasScene reloadedNames;
    ctx.Check(reloadedNames.FromJson(editScene.ToJson()), QStringLiteral("自动名称随画布重新载入"));
    QStringList reloadedNameList;
    for (CanvasItem* item : reloadedNames.CanvasItems()) {
        reloadedNameList.append(item->CustomName());
    }
    ctx.Check(reloadedNameList.contains(QStringLiteral("Hero panel")) &&
                  reloadedNameList.contains(QStringLiteral("矩形 2")) &&
                  reloadedNameList.contains(QStringLiteral("椭圆 1")) &&
                  reloadedNameList.contains(QStringLiteral("文字 1")),
              QStringLiteral("自动名称画布往返不重复编号"), reloadedNameList.join(QStringLiteral(" / ")));

    CanvasScene ellipseScene;
    QUndoStack ellipseUndo;
    ellipseScene.SetUndoStack(&ellipseUndo);
    auto* ellipse = CreateShapeItem(ShapeType::ELLIPSE);
    ellipse->SetContentSize(QSizeF(300.0, 100.0));
    ellipse->setPos(QPointF(100.0, 100.0));
    ellipseScene.AddItemsWithUndo({ellipse}, QStringLiteral("Add ellipse"));
    {
        ItemPropertiesDialog dialog(&ellipseScene, ellipse);
        dialog.show();
        QApplication::processEvents();
        QDoubleSpinBox* majorAxisValue =
            dialog.findChild<QDoubleSpinBox*>(QStringLiteral("ItemPropertiesMajorAxisValue"));
        QDoubleSpinBox* minorAxisValue =
            dialog.findChild<QDoubleSpinBox*>(QStringLiteral("ItemPropertiesMinorAxisValue"));
        ctx.Check(majorAxisValue && minorAxisValue && majorAxisValue->isEnabled() && minorAxisValue->isEnabled() &&
                      qAbs(majorAxisValue->value() - 150.0) < 0.01 && qAbs(minorAxisValue->value() - 50.0) < 0.01,
                  QStringLiteral("椭圆属性窗按内容宽高显示长半轴 / 短半轴"),
                  QStringLiteral("major=%1 minor=%2")
                      .arg(majorAxisValue ? majorAxisValue->value() : -999.0)
                      .arg(minorAxisValue ? minorAxisValue->value() : -999.0));
        if (majorAxisValue) {
            majorAxisValue->setValue(200.0);
        }
        if (minorAxisValue) {
            minorAxisValue->setValue(80.0);
        }
        dialog.accept();
        QApplication::processEvents();
    }
    ctx.Check(qAbs(ellipse->ContentSize().width() - 400.0) < 0.01 &&
                  qAbs(ellipse->ContentSize().height() - 160.0) < 0.01 && qAbs(ellipse->pos().x() - 50.0) < 0.01 &&
                  qAbs(ellipse->pos().y() - 70.0) < 0.01,
              QStringLiteral("修改长短半轴会同步内容尺寸并保持椭圆中心"),
              QStringLiteral("size=%1x%2 pos=%3,%4")
                  .arg(ellipse->ContentSize().width())
                  .arg(ellipse->ContentSize().height())
                  .arg(ellipse->pos().x())
                  .arg(ellipse->pos().y()));

    CanvasScene arcScene;
    QUndoStack arcUndo;
    arcScene.SetUndoStack(&arcUndo);
    auto* sector = CreateShapeItem(ShapeType::SECTOR);
    sector->SetContentSize(QSizeF(200.0, 200.0));
    arcScene.AddItemsWithUndo({sector}, QStringLiteral("Add sector"));
    {
        ItemPropertiesDialog dialog(&arcScene, sector);
        dialog.show();
        QApplication::processEvents();
        QSpinBox* startAngleValue = dialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesStartAngleValue"));
        QSpinBox* sweepAngleValue = dialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesSweepAngleValue"));
        QSpinBox* shapeRadiusValue = dialog.findChild<QSpinBox*>(QStringLiteral("ItemPropertiesShapeRadiusValue"));
        QDoubleSpinBox* majorAxisValue =
            dialog.findChild<QDoubleSpinBox*>(QStringLiteral("ItemPropertiesMajorAxisValue"));
        QDoubleSpinBox* minorAxisValue =
            dialog.findChild<QDoubleSpinBox*>(QStringLiteral("ItemPropertiesMinorAxisValue"));
        // 需求 6：半径行已删除，扇形只保留起始角 / 扫过角，椭圆由内容矩形派生。
        ctx.Check(startAngleValue && sweepAngleValue && !shapeRadiusValue && startAngleValue->isEnabled() &&
                      sweepAngleValue->isEnabled() && startAngleValue->value() == 30 && sweepAngleValue->value() == 120,
                  QStringLiteral("扇形属性窗提供起始角 / 扫过角且不再有半径行"),
                  QStringLiteral("start=%1 sweep=%2 radiusRow=%3")
                      .arg(startAngleValue ? startAngleValue->value() : -999)
                      .arg(sweepAngleValue ? sweepAngleValue->value() : -999)
                      .arg(shapeRadiusValue ? 1 : 0));
        ctx.Check(majorAxisValue && minorAxisValue && qAbs(majorAxisValue->value() - 100.0) < 0.01 &&
                      qAbs(minorAxisValue->value() - 100.0) < 0.01,
                  QStringLiteral("扇形保持椭圆派生并显示长短半轴"));
        if (startAngleValue) {
            startAngleValue->setValue(60);
        }
        if (sweepAngleValue) {
            sweepAngleValue->setValue(200);
        }
        dialog.accept();
        QApplication::processEvents();
    }
    ctx.Check(qAbs(sector->StartAngle() - 60.0) < 0.01 && qAbs(sector->SweepAngle() - 200.0) < 0.01,
              QStringLiteral("属性窗修改可落到扇形角度"),
              QStringLiteral("start=%1 sweep=%2").arg(sector->StartAngle()).arg(sector->SweepAngle()));
}
