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


void selftest::runDockOrderPersistenceTests(Context& ctx)
{
    // 右侧卡片的上下顺序：写进设置文件、读回、启动时按保存的顺序恢复。
    {
        const QString orderConfig = QDir(ctx.workDirectory).filePath(QStringLiteral("dock-order-config.ini"));
        QFile::remove(orderConfig);
        qputenv("STORYBOARD_CONFIG", orderConfig.toLocal8Bit());
        AppSettings seededOrder;
        seededOrder.dockOrder = QStringList{QStringLiteral("ItemDock"), QStringLiteral("LayerDock")};
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
        QDockWidget* orderItem = orderWindow.findChild<QDockWidget*>(QStringLiteral("ItemDock"));
        if (orderItem) {
            orderItem->setVisible(true);
        }
        QApplication::processEvents();
        ctx.check(orderLayer && orderItem &&
                      orderItem->mapTo(&orderWindow, QPoint(0, 0)).y() <
                          orderLayer->mapTo(&orderWindow, QPoint(0, 0)).y(),
                  QStringLiteral("启动时按设置文件恢复卡片顺序"));
        const bool orderWindowClosed = orderWindow.close();
        QApplication::processEvents();
        AppSettings savedOrderAfterClose;
        savedOrderAfterClose.load();
        ctx.check(orderWindowClosed && savedOrderAfterClose.dockOrder == seededOrder.dockOrder,
                  QStringLiteral("关闭窗口时把卡片顺序写回设置文件"),
                  savedOrderAfterClose.dockOrder.join(QStringLiteral("|")));
        qunsetenv("STORYBOARD_CONFIG");
    }
}
