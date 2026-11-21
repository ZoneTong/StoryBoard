#include "Alignment.h"
#include "BoolOps.h"
#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CanvasView.h"
#include "Commands.h"
#include "CurveItem.h"
#include "CustomShapeTemplates.h"
#include "ExportDialog.h"
#include "I18n.h"
#include "ItemPropertiesDialog.h"
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


void selftest::RunDockOrderPersistenceTests(Context& ctx)
{
    // 右侧卡片的上下顺序：写进设置文件、读回、启动时按保存的顺序恢复。
    {
        const QString orderConfig = QDir(ctx.workDirectory).filePath(QStringLiteral("dock-order-config.ini"));
        QFile::remove(orderConfig);
        qputenv("STORYBOARD_CONFIG", orderConfig.toLocal8Bit());
        AppSettings seededOrder;
        seededOrder.dockOrder = QStringList{QStringLiteral("ViewModeDock"), QStringLiteral("LayerDock")};
        const bool orderSaved = seededOrder.Save();
        AppSettings reloadedOrder;
        reloadedOrder.Load();
        ctx.Check(orderSaved && reloadedOrder.dockOrder == seededOrder.dockOrder,
                  QStringLiteral("卡片顺序写入设置文件并可读回"), reloadedOrder.dockOrder.join(QStringLiteral("|")));

        MainWindow orderWindow;
        orderWindow.resize(1200, 800);
        orderWindow.show();
        QApplication::processEvents();
        QDockWidget* orderLayer = orderWindow.findChild<QDockWidget*>(QStringLiteral("LayerDock"));
        QDockWidget* orderViewMode = orderWindow.findChild<QDockWidget*>(QStringLiteral("ViewModeDock"));
        QApplication::processEvents();
        ctx.Check(orderLayer && orderViewMode &&
                      orderViewMode->mapTo(&orderWindow, QPoint(0, 0)).y() <
                          orderLayer->mapTo(&orderWindow, QPoint(0, 0)).y(),
                  QStringLiteral("启动时按设置文件恢复右侧卡片顺序"));
        const bool orderWindowClosed = orderWindow.close();
        QApplication::processEvents();
        AppSettings savedOrderAfterClose;
        savedOrderAfterClose.Load();
        ctx.Check(orderWindowClosed && savedOrderAfterClose.dockOrder == seededOrder.dockOrder,
                  QStringLiteral("关闭窗口时把卡片顺序写回设置文件"),
                  savedOrderAfterClose.dockOrder.join(QStringLiteral("|")));
        qunsetenv("STORYBOARD_CONFIG");
    }

    // 属性编辑面板：首次启动停靠左侧并显示；拖动成悬浮窗口后保存位置，重启按保存的位置恢复。
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
            QDockWidget* propertiesDock = itemWindow.findChild<QDockWidget*>(QStringLiteral("PropertiesPanelDock"));
            ctx.Check(propertiesDock && !propertiesDock->isFloating() &&
                          itemWindow.dockWidgetArea(propertiesDock) == Qt::LeftDockWidgetArea &&
                          propertiesDock->isVisible(),
                      QStringLiteral("首次启动属性编辑面板显示并固定在窗口左侧"),
                      propertiesDock ? QStringLiteral("floating=%1 area=%2 visible=%3")
                                           .arg(propertiesDock->isFloating() ? 1 : 0)
                                           .arg(static_cast<int>(itemWindow.dockWidgetArea(propertiesDock)))
                                           .arg(propertiesDock->isVisible() ? 1 : 0)
                                     : QString());

            if (propertiesDock) {
                propertiesDock->setFloating(true);
                propertiesDock->resize(300, 480);
                propertiesDock->move(320, 180);
                QApplication::processEvents();
                movedPosition = propertiesDock->pos();
                QElapsedTimer saveWait;
                saveWait.start();
                while (saveWait.elapsed() < 500) {
                    QApplication::processEvents();
                }
                AppSettings movedSettings;
                movedSettings.Load();
                ctx.Check(movedSettings.propertiesDockStateSaved && movedSettings.propertiesDockFloating &&
                              !movedSettings.propertiesDockGeometry.isEmpty(),
                          QStringLiteral("属性编辑面板悬浮后把位置写入设置文件"));
            }
            itemWindow.close();
            QApplication::processEvents();
        }

        {
            MainWindow restoredWindow;
            restoredWindow.resize(1200, 800);
            restoredWindow.show();
            QApplication::processEvents();
            QDockWidget* propertiesDock = restoredWindow.findChild<QDockWidget*>(QStringLiteral("PropertiesPanelDock"));
            const bool positionRestored = propertiesDock && qAbs(propertiesDock->pos().x() - movedPosition.x()) <= 24 &&
                                          qAbs(propertiesDock->pos().y() - movedPosition.y()) <= 24;
            ctx.Check(propertiesDock && propertiesDock->isFloating() && positionRestored,
                      QStringLiteral("重启后按设置恢复属性编辑面板的悬浮位置"),
                      propertiesDock
                          ? QStringLiteral("%1,%2").arg(propertiesDock->pos().x()).arg(propertiesDock->pos().y())
                          : QString());
            restoredWindow.close();
            QApplication::processEvents();
        }
        qunsetenv("STORYBOARD_CONFIG");
    }
}
