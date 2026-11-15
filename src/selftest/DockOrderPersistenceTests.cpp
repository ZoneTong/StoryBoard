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
#include "CurveItem.h"
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
#include <QElapsedTimer>
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


void selftest::runDockOrderPersistenceTests(Context& ctx)
{
    // 右侧卡片的上下顺序：写进设置文件、读回、启动时按保存的顺序恢复。
    {
        const QString orderConfig = QDir(ctx.workDirectory).filePath(QStringLiteral("dock-order-config.ini"));
        QFile::remove(orderConfig);
        qputenv("STORYBOARD_CONFIG", orderConfig.toLocal8Bit());
        AppSettings seededOrder;
        seededOrder.dockOrder = QStringList{QStringLiteral("ViewModeDock"), QStringLiteral("LayerDock")};
        const bool orderSaved = seededOrder.save();
        AppSettings reloadedOrder;
        reloadedOrder.load();
        ctx.check(orderSaved && reloadedOrder.dockOrder == seededOrder.dockOrder,
                  QStringLiteral("卡片顺序写入设置文件并可读回"), reloadedOrder.dockOrder.join(QStringLiteral("|")));

        MainWindow orderWindow;
        orderWindow.resize(1200, 800);
        orderWindow.show();
        QApplication::processEvents();
        QDockWidget* orderLayer = orderWindow.findChild<QDockWidget*>(QStringLiteral("LayerDock"));
        QDockWidget* orderViewMode = orderWindow.findChild<QDockWidget*>(QStringLiteral("ViewModeDock"));
        QApplication::processEvents();
        ctx.check(orderLayer && orderViewMode &&
                      orderViewMode->mapTo(&orderWindow, QPoint(0, 0)).y() <
                          orderLayer->mapTo(&orderWindow, QPoint(0, 0)).y(),
                  QStringLiteral("启动时按设置文件恢复右侧卡片顺序"));
        const bool orderWindowClosed = orderWindow.close();
        QApplication::processEvents();
        AppSettings savedOrderAfterClose;
        savedOrderAfterClose.load();
        ctx.check(orderWindowClosed && savedOrderAfterClose.dockOrder == seededOrder.dockOrder,
                  QStringLiteral("关闭窗口时把卡片顺序写回设置文件"),
                  savedOrderAfterClose.dockOrder.join(QStringLiteral("|")));
        qunsetenv("STORYBOARD_CONFIG");
    }

    // 编辑图元卡：首次启动停靠左侧并显示；拖动成悬浮窗口后保存位置，重启按保存的位置恢复。
    {
        const QString itemConfig =
            QDir(ctx.workDirectory)
                .filePath(QStringLiteral("item-dock-config-%1.ini").arg(QCoreApplication::applicationPid()));
        QFile::remove(itemConfig);
        qputenv("STORYBOARD_CONFIG", itemConfig.toLocal8Bit());

        QPoint movedPosition;
        {
            MainWindow itemWindow;
            itemWindow.resize(1200, 800);
            itemWindow.show();
            QApplication::processEvents();
            QDockWidget* itemDock = itemWindow.findChild<QDockWidget*>(QStringLiteral("ItemDock"));
            ctx.check(itemDock && !itemDock->isFloating() &&
                          itemWindow.dockWidgetArea(itemDock) == Qt::LeftDockWidgetArea && itemDock->isVisible(),
                      QStringLiteral("首次启动编辑图元卡显示并固定在窗口左侧"),
                      itemDock ? QStringLiteral("floating=%1 area=%2 visible=%3")
                                     .arg(itemDock->isFloating() ? 1 : 0)
                                     .arg(static_cast<int>(itemWindow.dockWidgetArea(itemDock)))
                                     .arg(itemDock->isVisible() ? 1 : 0)
                               : QString());

            if (itemDock) {
                itemDock->setFloating(true);
                itemDock->resize(300, 480);
                itemDock->move(320, 180);
                QApplication::processEvents();
                movedPosition = itemDock->pos();
                QElapsedTimer saveWait;
                saveWait.start();
                while (saveWait.elapsed() < 500) {
                    QApplication::processEvents();
                }
                AppSettings movedSettings;
                movedSettings.load();
                ctx.check(movedSettings.itemDockStateSaved && movedSettings.itemDockFloating &&
                              !movedSettings.itemDockGeometry.isEmpty(),
                          QStringLiteral("编辑图元卡悬浮后把位置写入设置文件"));
            }
            itemWindow.close();
            QApplication::processEvents();
        }

        {
            MainWindow restoredWindow;
            restoredWindow.resize(1200, 800);
            restoredWindow.show();
            QApplication::processEvents();
            QDockWidget* itemDock = restoredWindow.findChild<QDockWidget*>(QStringLiteral("ItemDock"));
            const bool positionRestored = itemDock && qAbs(itemDock->pos().x() - movedPosition.x()) <= 24 &&
                                          qAbs(itemDock->pos().y() - movedPosition.y()) <= 24;
            ctx.check(itemDock && itemDock->isFloating() && positionRestored,
                      QStringLiteral("重启后按设置恢复编辑图元卡的悬浮位置"),
                      itemDock ? QStringLiteral("%1,%2").arg(itemDock->pos().x()).arg(itemDock->pos().y()) : QString());
            restoredWindow.close();
            QApplication::processEvents();
        }
        qunsetenv("STORYBOARD_CONFIG");
    }
}
