#include "app/I18n.h"
#include "app/Settings.h"
#include "canvas/CanvasView.h"
#include "io/CustomShapeTemplates.h"
#include "items/CanvasTypes.h"
#include "items/lines/CurveItem.h"
#include "items/lines/StraightLineItem.h"
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

void selftest::RunAlignmentTests(Context& ctx)
{
    // 多图元对齐：左对齐后边缘一致、整体一条撤销记录，等间距至少要三个图元。
    {
        CanvasScene alignScene;
        alignScene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        alignScene.SetMode(ToolMode::EDIT);
        QUndoStack alignUndo;
        alignScene.SetUndoStack(&alignUndo);

        auto* leftItem = ShapeFactory::Create(ShapeType::RECTANGLE);
        leftItem->SetContentSize(QSizeF(120.0, 90.0));
        alignScene.AddItemsWithUndo({leftItem}, QStringLiteral("Add align item"));
        auto* rightItem = ShapeFactory::Create(ShapeType::ELLIPSE);
        rightItem->SetContentSize(QSizeF(80.0, 60.0));
        alignScene.AddItemsWithUndo({rightItem}, QStringLiteral("Add align item"));
        leftItem->setPos(QPointF(40.0, 60.0));
        rightItem->setPos(QPointF(240.0, 180.0));

        alignScene.clearSelection();
        leftItem->setSelected(true);
        rightItem->setSelected(true);
        ctx.Check(Alignment::Supports(&alignScene, Alignment::Mode::LEFT) &&
                      !Alignment::Supports(&alignScene, Alignment::Mode::DISTRIBUTE_HORIZONTAL),
                  QStringLiteral("对齐需要两个图元、等间距需要三个图元"));

        const int undoBefore = alignUndo.count();
        const int moved = Alignment::Apply(&alignScene, Alignment::Mode::LEFT, QStringLiteral("Align items left"));
        ctx.Check(
            moved == 1 && qAbs(leftItem->SceneContentRect().left() - rightItem->SceneContentRect().left()) < 0.01,
            QStringLiteral("左对齐后两图元左边缘一致"),
            QStringLiteral("dx=%1").arg(rightItem->SceneContentRect().left() - leftItem->SceneContentRect().left()));
        ctx.Check(alignUndo.count() == undoBefore + 1, QStringLiteral("对齐整体只占一条撤销记录"));
        alignUndo.undo();
        ctx.Check(qAbs(rightItem->pos().x() - 240.0) < 0.01, QStringLiteral("对齐可撤销"));

        // 等间距：三个图元按空隙均分。
        auto* thirdItem = ShapeFactory::Create(ShapeType::RECTANGLE);
        thirdItem->SetContentSize(QSizeF(60.0, 40.0));
        alignScene.AddItemsWithUndo({thirdItem}, QStringLiteral("Add align item"));
        thirdItem->setPos(QPointF(620.0, 300.0));
        alignScene.clearSelection();
        leftItem->setSelected(true);
        rightItem->setSelected(true);
        thirdItem->setSelected(true);
        ctx.Check(Alignment::Supports(&alignScene, Alignment::Mode::DISTRIBUTE_HORIZONTAL),
                  QStringLiteral("三个图元可以水平等间距"));
    }
}

void selftest::RunAlignmentMenuTests(Context& ctx)
{
    // 菜单保留对齐下拉；主工具栏不再放对齐按钮，入口集中到菜单与编辑面板。
    {
        MainWindow alignWindow;
        QMenu* alignMenu = alignWindow.findChild<QMenu*>(QStringLiteral("AlignMenu"));
        QToolButton* alignButton = alignWindow.findChild<QToolButton*>(QStringLiteral("AlignToolButton"));
        ctx.Check(alignMenu && alignMenu->actions().size() == 8, QStringLiteral("编辑菜单提供 8 项对齐动作"),
                  alignMenu ? QString::number(alignMenu->actions().size()) : QString());
        ctx.Check(!alignButton, QStringLiteral("工具栏不再显示对齐按钮"));

        QAction* redoAction = nullptr;
        QAction* zoomInAction = nullptr;
        for (QAction* action : alignWindow.findChildren<QAction*>()) {
            if (action->text() == QStringLiteral("重做")) {
                redoAction = action;
            } else if (action->text() == QStringLiteral("放大")) {
                zoomInAction = action;
            }
        }
        // 功能键只看绑定键生效；副键不整组断言，改键窗口会兜底冲突提示。
        ctx.Check(redoAction && redoAction->shortcut() == QKeySequence(QStringLiteral("Ctrl+Y")),
                  QStringLiteral("重做绑定 Ctrl+Y"), redoAction ? redoAction->shortcut().toString() : QString());
        ctx.Check(zoomInAction && zoomInAction->shortcut() == QKeySequence(QStringLiteral("Ctrl+=")),
                  QStringLiteral("放大绑定 Ctrl+="), zoomInAction ? zoomInAction->shortcut().toString() : QString());
    }
}
