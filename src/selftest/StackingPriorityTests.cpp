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


void selftest::RunStackingPriorityTests(Context& ctx)
{
    CanvasScene priorityScene;
    priorityScene.ApplyCanvasSize(QSizeF(600, 400));

    auto addPriorityRect = [&priorityScene](const QRectF& rect, const QColor& color) {
        auto* shape = CreateShapeItem(ShapeType::RECTANGLE);
        shape->setPos(rect.topLeft());
        shape->SetContentSize(rect.size());
        shape->SetStyle(color, color, 3.0);
        shape->SetFill(color, FillStyle::SOLID);
        priorityScene.AttachItem(shape);
        return shape;
    };

    auto* coveredRect = addPriorityRect(QRectF(120.0, 120.0, 240.0, 140.0), QColor(QStringLiteral("#DC2626")));
    auto* coverRect = addPriorityRect(QRectF(120.0, 120.0, 240.0, 140.0), QColor(QStringLiteral("#2563EB")));
    ctx.Check(coverRect->zValue() > coveredRect->zValue() && coverRect->ZOrder() > coveredRect->ZOrder(),
              QStringLiteral("同层后加入的图元叠放优先级更高"));

    priorityScene.SetMode(ToolMode::EDIT);
    QList<CanvasItem*> obscured = priorityScene.ObscuredItems();
    ctx.Check(obscured.size() == 1 && obscured.constFirst() == coveredRect,
              QStringLiteral("非查看模式下完全遮盖的低优先级图元会描出轮廓"),
              QStringLiteral("count=%1").arg(obscured.size()));

    auto renderSceneImage = [](CanvasScene& target) {
        QImage image(600, 400, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        QPainter imagePainter(&image);
        target.render(&imagePainter, QRectF(0.0, 0.0, 600.0, 400.0), QRectF(0.0, 0.0, 600.0, 400.0));
        imagePainter.end();
        return image;
    };
    const QImage editRender = renderSceneImage(priorityScene);
    priorityScene.SetMode(ToolMode::VIEW);
    const QImage viewRender = renderSceneImage(priorityScene);
    int changedPixels = 0;
    for (int y = 0; y < editRender.height(); ++y) {
        const QRgb* editLine = reinterpret_cast<const QRgb*>(editRender.constScanLine(y));
        const QRgb* viewLine = reinterpret_cast<const QRgb*>(viewRender.constScanLine(y));
        for (int x = 0; x < editRender.width(); ++x) {
            if (editLine[x] != viewLine[x]) {
                ++changedPixels;
            }
        }
    }
    ctx.Check(changedPixels > 0, QStringLiteral("查看模式完全遮盖，编辑模式能看到被遮盖图元的轮廓"),
              QStringLiteral("changed=%1").arg(changedPixels));
    ctx.Check(priorityScene.ObscuredItems().isEmpty(), QStringLiteral("查看模式下不再描被遮盖图元的轮廓"));

    priorityScene.SetMode(ToolMode::EDIT);
    ctx.Check(priorityScene.AdjustItemHeight(coveredRect, 1), QStringLiteral("调整被遮盖图元的 height"));
    obscured = priorityScene.ObscuredItems();
    ctx.Check(coveredRect->zValue() > coverRect->zValue() && obscured.size() == 1 && obscured.constFirst() == coverRect,
              QStringLiteral("同层 height 更大的图元优先级更高，轮廓提示随之转移"),
              QStringLiteral("count=%1").arg(obscured.size()));

    ctx.Check(priorityScene.AddLayerWithUndo(QStringLiteral("遮挡层")), QStringLiteral("新建更高的遮挡图层"));
    const int overlayLayerId = priorityScene.ActiveLayerId();
    auto* layerCover = addPriorityRect(QRectF(120.0, 120.0, 240.0, 140.0), QColor(QStringLiteral("#059669")));
    ctx.Check(layerCover->LayerId() == overlayLayerId && layerCover->zValue() > coveredRect->zValue(),
              QStringLiteral("更高图层的低 height 图元优先级仍然更高"));
    obscured = priorityScene.ObscuredItems();
    ctx.Check(obscured.contains(coveredRect) && obscured.contains(coverRect) && !obscured.contains(layerCover),
              QStringLiteral("轮廓提示只覆盖被更高优先级图元遮住的图元"),
              QStringLiteral("count=%1").arg(obscured.size()));

    const int coveredLayerId = coveredRect->LayerId();
    ctx.Check(priorityScene.SetLayerVisible(coveredLayerId, false), QStringLiteral("隐藏低优先级图元所在图层"));
    ctx.Check(priorityScene.ObscuredItems().isEmpty(), QStringLiteral("隐藏图层上的图元不参与轮廓提示"));

    CanvasScene partialScene;
    partialScene.ApplyCanvasSize(QSizeF(500, 300));
    auto addPartialRect = [&partialScene](const QRectF& rect, const QColor& color) {
        auto* shape = CreateShapeItem(ShapeType::RECTANGLE);
        shape->setPos(rect.topLeft());
        shape->SetContentSize(rect.size());
        shape->SetStyle(color, color, 3.0);
        shape->SetFill(color, FillStyle::SOLID);
        partialScene.AttachItem(shape);
        return shape;
    };
    auto* partialLower = addPartialRect(QRectF(100.0, 100.0, 200.0, 100.0), QColor(QStringLiteral("#DC2626")));
    addPartialRect(QRectF(240.0, 140.0, 200.0, 100.0), QColor(QStringLiteral("#2563EB")));
    partialScene.SetMode(ToolMode::EDIT);
    obscured = partialScene.ObscuredItems();
    ctx.Check(obscured.size() == 1 && obscured.constFirst() == partialLower,
              QStringLiteral("只要有任何遮挡就描出被遮图元的轮廓"), QStringLiteral("count=%1").arg(obscured.size()));
    partialScene.SetMode(ToolMode::VIEW);
    ctx.Check(partialScene.ObscuredItems().isEmpty(), QStringLiteral("查看模式下小面积遮挡也不描轮廓"));

    // ---------------------------------------------------------------
}
