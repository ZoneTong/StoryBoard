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


void selftest::RunLockedLayerTests(Context& ctx)
{
    CanvasScene& viewScene = ctx.viewScene;
    CanvasView& view = *ctx.view;
    int& itemPropertyRequests = ctx.itemPropertyRequests;

    const int baseLayerId = viewScene.ActiveLayerId();
    ctx.Check(viewScene.AddLayerWithUndo(QStringLiteral("锁定的注释")), QStringLiteral("创建锁定测试图层"));
    const int lockedLayerId = viewScene.ActiveLayerId();
    QFont lockedFont(QStringLiteral("Microsoft YaHei UI"));
    lockedFont.setPointSize(22);
    auto* lockedText = new TextItem();
    lockedText->SetTextFormat(QStringLiteral("锁定图层文字"), lockedFont, QColor(QStringLiteral("#0F766E")));
    lockedText->setPos(QPointF(980, 600));
    viewScene.AddItemsWithUndo({lockedText}, QStringLiteral("添加锁定图层文字"));
    ctx.Check(viewScene.SetLayerLocked(lockedLayerId, true), QStringLiteral("锁定文字所在图层"));
    viewScene.SetActiveLayer(baseLayerId);
    view.FitAll();
    QApplication::processEvents();

    ctx.Check(lockedLayerId != baseLayerId && !lockedText->IsEditable() &&
                  (lockedText->flags() & QGraphicsItem::ItemIsSelectable) == 0,
              QStringLiteral("锁定图层上的文字不可编辑和选择"));
    viewScene.SelectAllItems();
    ctx.Check(!lockedText->isSelected(), QStringLiteral("全选不包含锁定图层图元"));
    viewScene.clearSelection();

    const int requestsBeforeLockedClick = itemPropertyRequests;
    const QPoint lockedPoint = view.mapFromScene(lockedText->SceneContentRect().center());
    QMouseEvent lockedDoubleClick(QEvent::MouseButtonDblClick, QPointF(lockedPoint),
                                  QPointF(view.viewport()->mapToGlobal(lockedPoint)), Qt::LeftButton, Qt::LeftButton,
                                  Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &lockedDoubleClick);
    QApplication::processEvents();
    ctx.Check(itemPropertyRequests == requestsBeforeLockedClick, QStringLiteral("双击锁定图层图元不会弹出属性窗"));

    SendMouseEvent(view.viewport(), QEvent::MouseButtonPress, lockedPoint, Qt::LeftButton, Qt::LeftButton);
    SendMouseEvent(view.viewport(), QEvent::MouseButtonRelease, lockedPoint, Qt::LeftButton, Qt::NoButton);
    QApplication::processEvents();
    ctx.Check(!lockedText->isSelected(), QStringLiteral("锁定图层图元不能被鼠标选中"));

    int lockedLayerMessages = 0;
    QObject::connect(&view, &CanvasView::statusMessage, &view, [&lockedLayerMessages](const QString& message) {
        if (message.contains(QStringLiteral("锁定"))) {
            ++lockedLayerMessages;
        }
    });
    const QPoint lockedGlobal = view.viewport()->mapToGlobal(lockedPoint);
    QContextMenuEvent lockedContext(QContextMenuEvent::Mouse, lockedPoint, lockedGlobal);
    QApplication::sendEvent(view.viewport(), &lockedContext);
    QApplication::processEvents();
    ctx.Check(lockedLayerMessages > 0, QStringLiteral("右键锁定图层图元提示先解锁图层"));
    ctx.Check(viewScene.SetLayerLocked(lockedLayerId, false), QStringLiteral("解锁文字图层"));
    ctx.Check(lockedText->IsEditable(), QStringLiteral("解锁后文字恢复可编辑"));

    view.close();
    QApplication::processEvents();

    ctx.view.reset();
}
