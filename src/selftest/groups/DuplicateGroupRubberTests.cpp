#include "app/I18n.h"
#include "app/Settings.h"
#include "canvas/CanvasView.h"
#include "io/CustomShapeTemplates.h"
#include "items/CanvasTypes.h"
#include "items/lines/CurveItem.h"
#include "items/polygon/PolygonItem.h"
#include "items/shapes/ShapeAbstractItem.h"
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


void selftest::RunDuplicateItemTests(Context& ctx)
{
    // 图元拷贝：副本偏移、名称自动接续，并整体进入一条撤销记录。
    {
        CanvasScene duplicateScene;
        duplicateScene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        duplicateScene.SetMode(ToolMode::EDIT);
        QUndoStack duplicateUndo;
        duplicateScene.SetUndoStack(&duplicateUndo);

        auto* source = CreateShapeItem(ShapeType::RECTANGLE);
        source->SetContentSize(QSizeF(120.0, 80.0));
        source->setPos(QPointF(40.0, 50.0));
        duplicateScene.AddItemsWithUndo({source}, QStringLiteral("Add duplicate source"));
        ctx.Check(source->CustomName() == QStringLiteral("矩形 1"), QStringLiteral("拷贝源图元使用自动名称"),
                  source->CustomName());
        source->setSelected(true);

        const int countBefore = duplicateScene.CanvasItemCount();
        const int undoBefore = duplicateUndo.count();
        const int duplicated = duplicateScene.DuplicateSelectedItems();
        ctx.Check(duplicated == 1 && duplicateScene.CanvasItemCount() == countBefore + 1,
                  QStringLiteral("拷贝选中的图元"), QString::number(duplicated));

        CanvasItem* copy = nullptr;
        for (CanvasItem* item : duplicateScene.CanvasItems()) {
            if (item != source && item->CustomName() == QStringLiteral("矩形 2")) {
                copy = item;
            }
        }
        ctx.Check(copy != nullptr, QStringLiteral("副本名称自动接续为同类下一个编号"));
        ctx.Check(copy && copy->pos() == source->pos() + QPointF(20.0, 20.0), QStringLiteral("副本相对源图元偏移"),
                  copy ? QStringLiteral("%1,%2").arg(copy->pos().x()).arg(copy->pos().y())
                       : QStringLiteral("<missing>"));
        ctx.Check(!duplicateScene.IsItemNameAvailable(QStringLiteral("矩形 1"), copy) &&
                      duplicateScene.IsItemNameAvailable(QStringLiteral("自定义名称"), copy),
                  QStringLiteral("画布内名称唯一性可查询"));
        ctx.Check(duplicateUndo.count() == undoBefore + 1, QStringLiteral("拷贝图元只产生一条撤销记录"));

        duplicateUndo.undo();
        QApplication::processEvents();
        ctx.Check(duplicateScene.CanvasItemCount() == countBefore, QStringLiteral("撤销拷贝删除副本"));
        duplicateUndo.redo();
        QApplication::processEvents();
        ctx.Check(duplicateScene.CanvasItemCount() == countBefore + 1 && copy && copy->scene() == &duplicateScene &&
                      copy->CustomName() == QStringLiteral("矩形 2"),
                  QStringLiteral("重做拷贝恢复副本和名称"));
    }

    // 自定义名称拷贝：Hero panel / Hero panel 2 之后取 Hero panel 3，不产生重名。
    {
        CanvasScene customNameScene;
        customNameScene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        customNameScene.SetMode(ToolMode::EDIT);
        QUndoStack customNameUndo;
        customNameScene.SetUndoStack(&customNameUndo);

        auto* first = CreateShapeItem(ShapeType::RECTANGLE);
        first->SetContentSize(QSizeF(100.0, 60.0));
        customNameScene.AddItemsWithUndo({first}, QStringLiteral("Add first custom item"));
        first->SetName(QStringLiteral("Hero panel"));

        auto* second = CreateShapeItem(ShapeType::ELLIPSE);
        second->SetContentSize(QSizeF(90.0, 90.0));
        customNameScene.AddItemsWithUndo({second}, QStringLiteral("Add second custom item"));
        second->SetName(QStringLiteral("Hero panel 2"));

        first->setSelected(true);
        const int duplicated = customNameScene.DuplicateSelectedItems();
        CanvasItem* copy = nullptr;
        for (CanvasItem* item : customNameScene.CanvasItems()) {
            if (item != first && item != second && item->CustomName() == QStringLiteral("Hero panel 3")) {
                copy = item;
            }
        }
        ctx.Check(duplicated == 1 && copy != nullptr, QStringLiteral("自定义名称拷贝时取下一个唯一名称"));
    }

    // 载入或旧文件里已有的重名，在补名时一并修复。
    {
        CanvasScene repairScene;
        auto* first = CreateShapeItem(ShapeType::RECTANGLE);
        first->SetContentSize(QSizeF(80.0, 60.0));
        repairScene.AddItemsWithUndo({first}, QStringLiteral("Add repair item"));
        auto* second = CreateShapeItem(ShapeType::RECTANGLE);
        second->SetContentSize(QSizeF(70.0, 50.0));
        repairScene.AddItemsWithUndo({second}, QStringLiteral("Add repair item"));
        first->SetName(QStringLiteral("Same name"));
        second->SetName(QStringLiteral("Same name"));
        repairScene.AssignAutoItemNames();
        const QString firstName = first->CustomName();
        const QString secondName = second->CustomName();
        ctx.Check(firstName != secondName &&
                      (firstName == QStringLiteral("Same name") || firstName == QStringLiteral("Same name 2")) &&
                      (secondName == QStringLiteral("Same name") || secondName == QStringLiteral("Same name 2")),
                  QStringLiteral("补名时修复画布内已有重名"), firstName + QStringLiteral(" / ") + secondName);
    }
}

void selftest::RunGroupTests(Context& ctx)
{
    // 图元分组：同层多选可组合；单个组员仍能单独选中并编辑属性；
    // 拖动任意组员时整组移动，分组与取消分组都可撤销。
    {
        CanvasScene groupScene;
        QUndoStack groupUndo;
        groupScene.SetUndoStack(&groupUndo);
        groupScene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        groupScene.SetMode(ToolMode::EDIT);

        auto* first = CreateShapeItem(ShapeType::RECTANGLE);
        first->SetContentSize(QSizeF(90.0, 70.0));
        first->setPos(QPointF(80.0, 90.0));
        auto* second = CreateShapeItem(ShapeType::ELLIPSE);
        second->SetContentSize(QSizeF(90.0, 70.0));
        second->setPos(QPointF(320.0, 260.0));
        groupScene.AddItemsWithUndo({first, second}, QStringLiteral("Add grouped sources"));

        first->setSelected(true);
        second->setSelected(true);
        const int grouped = groupScene.GroupSelectedItems();
        const int groupId = first->GroupId();
        ctx.Check(
            grouped == 2 && groupId > 0 && second->GroupId() == groupId, QStringLiteral("同层多选图元可组合"),
            QStringLiteral("group=%1 first=%2 second=%3").arg(grouped).arg(first->GroupId()).arg(second->GroupId()));
        ctx.Check(groupScene.CanUngroupSelectedItems(), QStringLiteral("组合后可取消组合"));

        groupUndo.undo();
        ctx.Check(first->GroupId() == 0 && second->GroupId() == 0, QStringLiteral("撤销组合恢复未分组"));
        groupUndo.redo();
        ctx.Check(first->GroupId() == groupId && second->GroupId() == groupId, QStringLiteral("重做组合恢复组标记"));

        // 单个组员仍可成为唯一选中项，属性窗保持可编辑。
        groupScene.clearSelection();
        first->setSelected(true);
        ctx.Check(groupScene.SelectedCanvasItems().size() == 1 && first->IsEditable(),
                  QStringLiteral("组合后单个图元仍可单独选中"));
        ItemPropertiesDialog propertiesDialog(&groupScene, first, nullptr);
        QLineEdit* nameEdit = propertiesDialog.findChild<QLineEdit*>(QStringLiteral("ItemPropertiesNameValue"));
        ctx.Check(nameEdit != nullptr && !nameEdit->isReadOnly(), QStringLiteral("组合后单个图元属性窗仍可编辑"));

        CanvasView groupView(&groupScene);
        groupView.resize(900, 700);
        groupView.show();
        groupView.SetMode(ToolMode::EDIT);
        groupView.FitAll();
        QApplication::processEvents();

        const QPointF firstBefore = first->pos();
        const QPointF secondBefore = second->pos();
        const QPointF firstCenter = first->mapToScene(first->ContentRect().center());
        const QPoint dragStart = groupView.mapFromScene(firstCenter);

        // 右键点中已选图元时，Qt 默认选择逻辑不能把多选收成单选，
        // 否则右键菜单里的“组合选中图元”会一直处于禁用态。
        first->setSelected(true);
        second->setSelected(true);
        SendMouseEvent(groupView.viewport(), QEvent::MouseButtonPress, dragStart, Qt::RightButton, Qt::RightButton);
        SendMouseEvent(groupView.viewport(), QEvent::MouseButtonRelease, dragStart, Qt::RightButton, Qt::NoButton);
        QApplication::processEvents();
        ctx.Check(groupScene.SelectedCanvasItems().size() == 2 && first->isSelected() && second->isSelected(),
                  QStringLiteral("右键已选组员不破坏多选"));

        groupScene.clearSelection();
        first->setSelected(true);
        SendMouseEvent(groupView.viewport(), QEvent::MouseButtonPress, dragStart, Qt::LeftButton, Qt::LeftButton);
        SendMouseEvent(groupView.viewport(), QEvent::MouseMove, dragStart + QPoint(50, 35), Qt::NoButton,
                       Qt::LeftButton);
        SendMouseEvent(groupView.viewport(), QEvent::MouseButtonRelease, dragStart + QPoint(50, 35), Qt::LeftButton,
                       Qt::NoButton);
        QApplication::processEvents();

        const QPointF firstDelta = first->pos() - firstBefore;
        const QPointF secondDelta = second->pos() - secondBefore;
        ctx.Check(QLineF(firstDelta, QPointF()).length() > 1.0 && QLineF(firstDelta, secondDelta).length() < 0.01,
                  QStringLiteral("拖动单个组员时整组一起移动"),
                  QStringLiteral("first=%1,%2 second=%3,%4")
                      .arg(firstDelta.x())
                      .arg(firstDelta.y())
                      .arg(secondDelta.x())
                      .arg(secondDelta.y()));

        groupUndo.undo();
        ctx.Check(first->pos() == firstBefore && second->pos() == secondBefore,
                  QStringLiteral("撤销整组移动恢复所有组员"));
        groupUndo.redo();
        ctx.Check(first->pos() != firstBefore && second->pos() != secondBefore, QStringLiteral("重做整组移动"));

        CanvasScene reloadedGroups;
        ctx.Check(reloadedGroups.FromJson(groupScene.ToJson()), QStringLiteral("分组画布可重新载入"));
        CanvasItem* reloadedFirst = nullptr;
        CanvasItem* reloadedSecond = nullptr;
        for (CanvasItem* item : reloadedGroups.CanvasItems()) {
            if (item->CustomName() == first->CustomName()) {
                reloadedFirst = item;
            }
            if (item->CustomName() == second->CustomName()) {
                reloadedSecond = item;
            }
        }
        ctx.Check(reloadedFirst && reloadedSecond && reloadedFirst->GroupId() > 0 &&
                      reloadedFirst->GroupId() == reloadedSecond->GroupId(),
                  QStringLiteral("分组关系随画布保存和载入"));

        // 只复制组内一个图元时，副本不能误加入原组。
        groupScene.clearSelection();
        first->setSelected(true);
        const int duplicatedGroupMember = groupScene.DuplicateSelectedItems();
        CanvasItem* groupCopy = nullptr;
        for (CanvasItem* item : groupScene.CanvasItems()) {
            if (item != first && item != second && item->CustomName() == QStringLiteral("矩形 2")) {
                groupCopy = item;
            }
        }
        ctx.Check(duplicatedGroupMember == 1 && groupCopy && groupCopy->GroupId() == 0,
                  QStringLiteral("只复制单个组员时副本保持未分组"));

        // 只选中一个组员也能取消整个组。
        groupScene.clearSelection();
        first->setSelected(true);
        const int ungrouped = groupScene.UngroupSelectedItems();
        ctx.Check(ungrouped == 2 && first->GroupId() == 0 && second->GroupId() == 0,
                  QStringLiteral("取消组合会清除整个组"), QStringLiteral("ungrouped=%1").arg(ungrouped));
        groupUndo.undo();
        ctx.Check(first->GroupId() == groupId && second->GroupId() == groupId, QStringLiteral("撤销取消组合恢复整组"));

        groupView.close();
        QApplication::processEvents();
    }
}

void selftest::RunRubberBandTests(Context& ctx)
{
    // 框选按图元中心判断：只碰到边缘不选中，框住中心才选中。
    {
        CanvasScene rubberScene;
        rubberScene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        rubberScene.SetMode(ToolMode::EDIT);
        auto* target = CreateShapeItem(ShapeType::RECTANGLE);
        target->SetContentSize(QSizeF(120.0, 80.0));
        target->setPos(QPointF(300.0, 250.0));
        rubberScene.AddItemsWithUndo({target}, QStringLiteral("Add rubber-band target"));

        CanvasView rubberView(&rubberScene);
        rubberView.resize(900, 700);
        rubberView.show();
        rubberView.SetMode(ToolMode::EDIT);
        rubberView.FitAll();
        QApplication::processEvents();

        const QPointF center = target->mapToScene(target->ContentRect().center());
        const QPoint start = rubberView.mapFromScene(center + QPointF(-140.0, -100.0));
        const QPoint edgeEnd = rubberView.mapFromScene(center + QPointF(-15.0, -15.0));
        SendMouseEvent(rubberView.viewport(), QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        SendMouseEvent(rubberView.viewport(), QEvent::MouseMove, edgeEnd, Qt::NoButton, Qt::LeftButton);
        SendMouseEvent(rubberView.viewport(), QEvent::MouseButtonRelease, edgeEnd, Qt::LeftButton, Qt::NoButton);
        QApplication::processEvents();
        ctx.Check(!target->isSelected(), QStringLiteral("框选只碰到图元边缘时不选中"));

        rubberScene.clearSelection();
        const QPoint centerEnd = rubberView.mapFromScene(center + QPointF(15.0, 15.0));
        SendMouseEvent(rubberView.viewport(), QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        SendMouseEvent(rubberView.viewport(), QEvent::MouseMove, centerEnd, Qt::NoButton, Qt::LeftButton);
        SendMouseEvent(rubberView.viewport(), QEvent::MouseButtonRelease, centerEnd, Qt::LeftButton, Qt::NoButton);
        QApplication::processEvents();
        ctx.Check(!target->isSelected(), QStringLiteral("框住中心但图元仍有部分在框外时不选中"));

        rubberScene.clearSelection();
        const QPoint fullEnd = rubberView.mapFromScene(center + QPointF(140.0, 100.0));
        SendMouseEvent(rubberView.viewport(), QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        SendMouseEvent(rubberView.viewport(), QEvent::MouseMove, fullEnd, Qt::NoButton, Qt::LeftButton);
        SendMouseEvent(rubberView.viewport(), QEvent::MouseButtonRelease, fullEnd, Qt::LeftButton, Qt::NoButton);
        QApplication::processEvents();
        ctx.Check(target->isSelected(), QStringLiteral("图元完整落在框内时选中"));

        // 近处框选：框线离开图形只有一两个像素时，Qt 按 shape()（含描边外扩）会判「没框全」，
        // 补漏按 frameRect 再判一次必须仍然选中；这条同时守住「补漏要放在基类之后」的顺序。
        rubberScene.clearSelection();
        rubberView.resetTransform();
        rubberView.centerOn(target);
        QApplication::processEvents();
        const QRectF frame = target->mapRectToScene(target->FrameRect());
        const QPoint nearStart = rubberView.mapFromScene(frame.topLeft() - QPointF(1.5, 1.5));
        const QPoint nearEnd = rubberView.mapFromScene(frame.bottomRight() + QPointF(1.5, 1.5));
        SendMouseEvent(rubberView.viewport(), QEvent::MouseButtonPress, nearStart, Qt::LeftButton, Qt::LeftButton);
        SendMouseEvent(rubberView.viewport(), QEvent::MouseMove, nearEnd, Qt::NoButton, Qt::LeftButton);
        SendMouseEvent(rubberView.viewport(), QEvent::MouseButtonRelease, nearEnd, Qt::LeftButton, Qt::NoButton);
        QApplication::processEvents();
        ctx.Check(target->isSelected(), QStringLiteral("框线紧贴图形（一两个像素余量）时也能框选选中"));

        rubberView.close();
        QApplication::processEvents();
    }
}
