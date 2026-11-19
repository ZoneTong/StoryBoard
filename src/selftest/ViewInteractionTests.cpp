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
#include "ItemDockPanel.h"
#include "ItemPropertiesDialog.h"
#include "LayerPanel.h"
#include "LineArtConverter.h"
#include "LineArtDialog.h"
#include "LineArtPresets.h"
#include "LineItem.h"
#include "MainWindow.h"
#include "PolygonItem.h"
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
#include <QKeyEvent>
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


void selftest::runViewInteractionTests(Context& ctx)
{
    CanvasScene& viewScene = ctx.viewScene;
    QUndoStack& viewUndo = ctx.viewUndo;

    viewScene.setUndoStack(&viewUndo);
    viewScene.applyCanvasSize(QSizeF(1200, 800));
    viewScene.setShowGrid(false);

    ctx.view = std::make_unique<CanvasView>(&viewScene);
    CanvasView& view = *ctx.view;
    view.resize(1000, 700);
    view.show();
    QApplication::processEvents();

    ctx.check(view.metaObject()->indexOfSignal("editTextRequested(TextItem*)") < 0,
              QStringLiteral("右键独立编辑文字入口信号已移除"));
    ctx.check(view.mode() == ToolMode::View, QStringLiteral("默认模式为查看模式"));
    view.fitAll();
    QApplication::processEvents();

    view.setBrushColor(QColor(QStringLiteral("#2563EB")));
    view.setBrushWidth(7.0);
    view.setMode(ToolMode::Brush);

    const QPoint brushStart = view.mapFromScene(QPointF(300, 300));
    const QPoint brushEnd = view.mapFromScene(QPointF(700, 500));
    sendMouseEvent(view.viewport(), QEvent::MouseButtonPress, brushStart, Qt::LeftButton, Qt::LeftButton);
    for (int i = 1; i <= 12; ++i) {
        // 加一点手抖，保持这条笔画是「曲线」样本（笔直笔画按需求 8 会归为直线图元）。
        const int jitter = (i % 2 == 0) ? 3 : -3;
        const QPoint point(brushStart.x() + (brushEnd.x() - brushStart.x()) * i / 12,
                           brushStart.y() + (brushEnd.y() - brushStart.y()) * i / 12 + jitter);
        sendMouseEvent(view.viewport(), QEvent::MouseMove, point, Qt::NoButton, Qt::LeftButton);
    }
    sendMouseEvent(view.viewport(), QEvent::MouseButtonRelease, brushEnd, Qt::LeftButton, Qt::NoButton);
    QApplication::processEvents();

    ctx.check(viewScene.canvasItemCount() == 1, QStringLiteral("鼠标绘制生成曲线"));
    CanvasItem* drawnItem = viewScene.canvasItems().value(0);
    auto* drawnStroke = dynamic_cast<CurveItem*>(drawnItem);
    ctx.check(drawnStroke != nullptr, QStringLiteral("绘制图元类型为曲线"));
    if (drawnStroke) {
        ctx.check(drawnStroke->strokeColor() == QColor(QStringLiteral("#2563EB")),
                  QStringLiteral("画笔颜色应用到图元"));
        ctx.check(qAbs(drawnStroke->strokeWidth() - 7.0) < 0.01, QStringLiteral("画笔粗细应用到图元"));
        ctx.check(drawnStroke->layerId() == viewScene.activeLayerId() && drawnStroke->height() == 1,
                  QStringLiteral("鼠标绘制落入当前图层"));
        ctx.check(drawnStroke->contentSize().width() > 300.0 && drawnStroke->contentSize().height() > 150.0,
                  QStringLiteral("绘制曲线内容尺寸覆盖曲线范围"),
                  geometryText(drawnStroke->pos(), drawnStroke->contentSize()));
    }
    ctx.check(viewUndo.count() == 1, QStringLiteral("鼠标绘制进入撤销栈"));
    viewUndo.undo();
    ctx.check(viewScene.canvasItemCount() == 0, QStringLiteral("撤销鼠标绘制"));
    viewUndo.redo();
    ctx.check(viewScene.canvasItemCount() == 1, QStringLiteral("重做鼠标绘制"));

    {
        // 需求 8：笔直的笔画收笔后直接落成直线类型图元。
        view.setBrushColor(QColor(QStringLiteral("#DC2626")));
        view.setBrushWidth(5.0);
        const QPoint lineStart = view.mapFromScene(QPointF(120, 620));
        const QPoint lineEnd = view.mapFromScene(QPointF(520, 620));
        sendMouseEvent(view.viewport(), QEvent::MouseButtonPress, lineStart, Qt::LeftButton, Qt::LeftButton);
        for (int i = 1; i <= 10; ++i) {
            const QPoint point(lineStart.x() + (lineEnd.x() - lineStart.x()) * i / 10, lineStart.y());
            sendMouseEvent(view.viewport(), QEvent::MouseMove, point, Qt::NoButton, Qt::LeftButton);
        }
        sendMouseEvent(view.viewport(), QEvent::MouseButtonRelease, lineEnd, Qt::LeftButton, Qt::NoButton);
        QApplication::processEvents();

        ctx.check(viewScene.canvasItemCount() == 2, QStringLiteral("笔直笔画收笔后新增一个图元"),
                  QString::number(viewScene.canvasItemCount()));
        CanvasItem* straightItem = viewScene.canvasItems().value(1);
        auto* lineShape = dynamic_cast<LineItem*>(straightItem);
        ctx.check(lineShape != nullptr, QStringLiteral("笔直笔画归为直线图元（曲线族的直线子型）"));
        if (lineShape) {
            ctx.check(lineShape->strokeColor() == QColor(QStringLiteral("#DC2626")) &&
                          qAbs(lineShape->strokeWidth() - 5.0) < 0.01,
                      QStringLiteral("直线图元沿用画笔颜色与粗细"));
            // 需求 20：直线必须落在落笔位置——两个端点回到场景坐标后要对上起笔 / 收笔点。
            const QRectF lineBounds = lineShape->sceneBoundingRect();
            const QPointF boundsCenter = lineBounds.center();
            const QVector<QPointF> knots = lineShape->strokePoints();
            const QPointF startScene = knots.isEmpty() ? QPointF() : lineShape->mapToScene(knots.constFirst());
            const QPointF endScene = knots.isEmpty() ? startScene : lineShape->mapToScene(knots.constLast());
            ctx.check(qAbs(boundsCenter.x() - 320.0) < 8.0 && qAbs(boundsCenter.y() - 620.0) < 8.0 &&
                          qAbs(startScene.x() - 120.0) < 6.0 && qAbs(startScene.y() - 620.0) < 6.0 &&
                          qAbs(endScene.x() - 520.0) < 6.0 && qAbs(endScene.y() - 620.0) < 6.0,
                      QStringLiteral("直线图元落在落笔位置"),
                      QStringLiteral("center=(%1,%2) start=(%3,%4) end=(%5,%6) bounds=(%7,%8,%9,%10)")
                          .arg(boundsCenter.x())
                          .arg(boundsCenter.y())
                          .arg(startScene.x())
                          .arg(startScene.y())
                          .arg(endScene.x())
                          .arg(endScene.y())
                          .arg(lineBounds.left())
                          .arg(lineBounds.top())
                          .arg(lineBounds.right())
                          .arg(lineBounds.bottom()));
        }
        viewUndo.undo();
        ctx.check(viewScene.canvasItemCount() == 1, QStringLiteral("直线图元可撤销"));
    }

    {
        // 需求 7：画笔模式按 Esc 退出回编辑模式，正在画的笔画被取消不落库。
        view.setMode(ToolMode::Brush);
        const QPoint escStrokeStart = view.mapFromScene(QPointF(200, 200));
        sendMouseEvent(view.viewport(), QEvent::MouseButtonPress, escStrokeStart, Qt::LeftButton, Qt::LeftButton);
        QApplication::processEvents();
        QKeyEvent escWhileStroke(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(&view, &escWhileStroke);
        QApplication::processEvents();
        ctx.check(view.mode() == ToolMode::Edit, QStringLiteral("画笔模式按 Esc 退出到编辑模式"),
                  QStringLiteral("mode=%1").arg(static_cast<int>(view.mode())));
        ctx.check(viewScene.canvasItemCount() == 1, QStringLiteral("Esc 取消进行中的笔画不落库"),
                  QString::number(viewScene.canvasItemCount()));

        // 空闲画笔模式按 Esc 同样退出。
        view.setMode(ToolMode::Brush);
        QKeyEvent escIdle(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(&view, &escIdle);
        QApplication::processEvents();
        ctx.check(view.mode() == ToolMode::Edit, QStringLiteral("空闲画笔模式按 Esc 也退出到编辑模式"));
    }

    {
        // 画笔收尾自动平滑：轻微手抖的直线收敛成两点，曲线保留形状但点集被简化。
        CurveItem jitterLine;
        jitterLine.beginStroke(QPointF(100, 100), QColor(QStringLiteral("#111827")), 4.0);
        for (int i = 0; i < 40; ++i) {
            const qreal x = 100.0 + i * 5.0;
            const qreal y = 100.0 + (i % 2 == 0 ? 1.5 : -1.5);
            jitterLine.appendScenePoint(QPointF(x, y));
        }
        jitterLine.endStroke(4.0);
        ctx.check(jitterLine.strokePointCount() == 2, QStringLiteral("自动平滑将手绘直线收敛为两点"),
                  QString::number(jitterLine.strokePointCount()));

        CurveItem rawLine;
        rawLine.beginStroke(QPointF(100, 100), QColor(QStringLiteral("#111827")), 4.0);
        for (int i = 0; i < 40; ++i) {
            const qreal x = 100.0 + i * 5.0;
            const qreal y = 100.0 + (i % 2 == 0 ? 1.5 : -1.5);
            rawLine.appendScenePoint(QPointF(x, y));
        }
        rawLine.endStroke(0.0);
        ctx.check(rawLine.strokePointCount() == 41, QStringLiteral("平滑参数为 0 时关闭自动平滑"),
                  QString::number(rawLine.strokePointCount()));

        CurveItem rawCurve;
        rawCurve.beginStroke(QPointF(100, 200), QColor(QStringLiteral("#111827")), 4.0);
        CurveItem smoothCurve;
        smoothCurve.beginStroke(QPointF(100, 200), QColor(QStringLiteral("#111827")), 4.0);
        for (int i = 0; i < 60; ++i) {
            const qreal x = 100.0 + i * 4.0;
            const qreal y = 200.0 + 24.0 * qSin(i * 0.35) + (i % 2 == 0 ? 1.2 : -1.2);
            rawCurve.appendScenePoint(QPointF(x, y));
            smoothCurve.appendScenePoint(QPointF(x, y));
        }
        rawCurve.endStroke(0.0);
        smoothCurve.endStroke(4.0);
        ctx.check(
            smoothCurve.strokePointCount() >= 3 && (smoothCurve.strokePointCount() != rawCurve.strokePointCount() ||
                                                    smoothCurve.strokePoints() != rawCurve.strokePoints()),
            QStringLiteral("自动平滑保留曲线并简化采样点"),
            QStringLiteral("raw=%1 smooth=%2").arg(rawCurve.strokePointCount()).arg(smoothCurve.strokePointCount()));

        CanvasScene manualScene;
        manualScene.applyCanvasSize(QSizeF(800, 600));
        manualScene.setShowGrid(false);
        manualScene.setMode(ToolMode::Edit);
        QUndoStack manualUndo;
        manualScene.setUndoStack(&manualUndo);
        auto* manualStroke = new CurveItem();
        manualStroke->setLayerId(manualScene.activeLayerId());
        manualStroke->beginStroke(QPointF(120, 240), QColor(QStringLiteral("#111827")), 4.0);
        for (int i = 0; i < 50; ++i) {
            const qreal x = 120.0 + i * 4.0;
            const qreal y = 240.0 + 18.0 * qSin(i * 0.4);
            manualStroke->appendScenePoint(QPointF(x, y));
        }
        manualStroke->endStroke(0.0);
        manualScene.attachItem(manualStroke);
        manualStroke->setSelected(true);
        const QVector<QPointF> beforeSmooth = manualStroke->strokePoints();
        const int manuallySmoothed = manualScene.smoothSelectedStrokes(4.0);
        const QVector<QPointF> afterSmooth = manualStroke->strokePoints();
        ctx.check(manuallySmoothed == 1 && manualUndo.count() == 1, QStringLiteral("选中曲线可手动平滑并进入撤销栈"),
                  QStringLiteral("changed=%1 undo=%2").arg(manuallySmoothed).arg(manualUndo.count()));
        manualUndo.undo();
        ctx.check(manualStroke->strokePoints() == beforeSmooth, QStringLiteral("撤销手动平滑恢复曲线采样点"));
        manualUndo.redo();
        ctx.check(manualStroke->strokePoints() == afterSmooth, QStringLiteral("重做手动平滑恢复平滑结果"));
    }

    int textPlacementRequests = 0;
    QPointF requestedTextPosition;
    QObject::connect(&view, &CanvasView::textPlacementRequested, &view,
                     [&textPlacementRequests, &requestedTextPosition](const QPointF& scenePos) {
                         ++textPlacementRequests;
                         requestedTextPosition = scenePos;
                     });
    view.setMode(ToolMode::Text);
    const QPointF textClickScene(620.0, 330.0);
    const QPoint textClick = view.mapFromScene(textClickScene);
    sendMouseEvent(view.viewport(), QEvent::MouseButtonPress, textClick, Qt::LeftButton, Qt::LeftButton);
    sendMouseEvent(view.viewport(), QEvent::MouseButtonRelease, textClick, Qt::LeftButton, Qt::NoButton);
    QApplication::processEvents();
    ctx.check(textPlacementRequests == 1, QStringLiteral("文字模式点击画板请求放置文字"));
    if (textPlacementRequests == 1) {
        ctx.check(QLineF(requestedTextPosition, textClickScene).length() < 2.0,
                  QStringLiteral("文字放置位置对应画板坐标"),
                  QStringLiteral("requested=%1,%2").arg(requestedTextPosition.x()).arg(requestedTextPosition.y()));
    }
    ctx.check(viewScene.canvasItemCount() == 1, QStringLiteral("文字模式点击不会直接创建图元"));
    view.setMode(ToolMode::View);
    QApplication::processEvents();

    const QPointF viewPosBefore = drawnStroke ? drawnStroke->pos() : QPointF();
    const QSizeF viewSizeBefore = drawnStroke ? drawnStroke->contentSize() : QSizeF();

    view.setMode(ToolMode::View);
    QApplication::processEvents();
    const QRectF visibleScene = view.mapToScene(view.viewport()->rect()).boundingRect();
    ctx.check(visibleScene.contains(viewScene.contentRect()), QStringLiteral("查看模式自动包含全部图元"),
              QStringLiteral("visible=%1,%2 %3x%4 content=%5,%6 %7x%8")
                  .arg(visibleScene.x())
                  .arg(visibleScene.y())
                  .arg(visibleScene.width())
                  .arg(visibleScene.height())
                  .arg(viewScene.contentRect().x())
                  .arg(viewScene.contentRect().y())
                  .arg(viewScene.contentRect().width())
                  .arg(viewScene.contentRect().height()));

    const QPointF centerBefore = view.mapToScene(view.viewport()->rect().center());
    const QPoint panStart = view.viewport()->rect().center();
    const QPointF panAnchor = view.mapToScene(panStart);
    sendMouseEvent(view.viewport(), QEvent::MouseButtonPress, panStart, Qt::LeftButton, Qt::LeftButton);

    // 连续小幅拖动：按下点在场景里的位置必须始终留在鼠标下方。若平移用“按下时的
    // 场景锚点”去和随视图移动后的 mapToScene 相减，位移会被反复累加，画面抖动。
    qreal maxPanDrift = 0.0;
    for (int step = 1; step <= 6; ++step) {
        const QPoint cursor = panStart + QPoint(step * 10, step * 6);
        sendMouseEvent(view.viewport(), QEvent::MouseMove, cursor, Qt::NoButton, Qt::LeftButton);
        QApplication::processEvents();
        maxPanDrift = qMax(maxPanDrift, QLineF(view.mapToScene(cursor), panAnchor).length());
    }
    ctx.check(maxPanDrift < 3.0, QStringLiteral("拖拽平移时图元紧跟鼠标，不会来回抖动"),
              QStringLiteral("maxDrift=%1").arg(maxPanDrift));

    // 拖回起点：视图应回到原位，不残留位移。
    sendMouseEvent(view.viewport(), QEvent::MouseMove, panStart, Qt::NoButton, Qt::LeftButton);
    QApplication::processEvents();
    ctx.check(QLineF(view.mapToScene(view.viewport()->rect().center()), centerBefore).length() < 3.0,
              QStringLiteral("来回拖拽后视图回到原位，不累积漂移"));

    sendMouseEvent(view.viewport(), QEvent::MouseMove, panStart + QPoint(60, 40), Qt::NoButton, Qt::LeftButton);
    sendMouseEvent(view.viewport(), QEvent::MouseButtonRelease, panStart + QPoint(60, 40), Qt::LeftButton,
                   Qt::NoButton);
    const QPointF centerAfter = view.mapToScene(view.viewport()->rect().center());
    ctx.check(QLineF(centerBefore, centerAfter).length() > 1.0, QStringLiteral("查看模式拖拽平移整体视图"));
    if (drawnStroke) {
        ctx.check(drawnStroke->pos() == viewPosBefore && drawnStroke->contentSize() == viewSizeBefore,
                  QStringLiteral("查看模式平移不修改图元坐标"));
    }

    const qreal zoomBefore = view.zoom();
    QWheelEvent wheel(QPointF(view.viewport()->rect().center()),
                      QPointF(view.viewport()->mapToGlobal(view.viewport()->rect().center())), QPoint(), QPoint(0, 120),
                      Qt::NoButton, Qt::NoModifier, Qt::ScrollUpdate, false);
    QApplication::sendEvent(view.viewport(), &wheel);
    ctx.check(view.zoom() > zoomBefore, QStringLiteral("滚轮缩放整体视图"),
              QStringLiteral("before=%1 after=%2").arg(zoomBefore).arg(view.zoom()));
    if (drawnStroke) {
        ctx.check(drawnStroke->pos() == viewPosBefore && drawnStroke->contentSize() == viewSizeBefore,
                  QStringLiteral("查看模式缩放不修改图元坐标"));
    }

    view.setMode(ToolMode::Edit);
    view.fitAll();
    QApplication::processEvents();

    const QPointF midScene(500.0, 400.0);
    const QPoint midStroke = view.mapFromScene(midScene);
    sendMouseEvent(view.viewport(), QEvent::MouseButtonPress, midStroke, Qt::LeftButton, Qt::LeftButton);
    sendMouseEvent(view.viewport(), QEvent::MouseMove, midStroke + QPoint(55, 35), Qt::NoButton, Qt::LeftButton);
    sendMouseEvent(view.viewport(), QEvent::MouseButtonRelease, midStroke + QPoint(55, 35), Qt::LeftButton,
                   Qt::NoButton);
    QApplication::processEvents();
    ctx.check(drawnStroke && drawnStroke->isSelected(), QStringLiteral("编辑模式点选图元"));
    if (drawnStroke) {
        ctx.check(drawnStroke->pos() != viewPosBefore, QStringLiteral("编辑模式拖动移动图元"),
                  geometryText(drawnStroke->pos(), drawnStroke->contentSize()));
        viewUndo.undo();
        ctx.check(drawnStroke->pos() == viewPosBefore, QStringLiteral("撤销拖动移动图元"));
        viewUndo.redo();
        ctx.check(drawnStroke->pos() != viewPosBefore, QStringLiteral("重做拖动移动图元"));
    }

    {
        // Ctrl 在编辑模式下只负责加选/减选，不再用来拖动平移整体视图。
        const QPointF ctrlDragItemBefore = drawnStroke ? drawnStroke->pos() : QPointF();
        const QPoint ctrlDragStart =
            view.mapFromScene(drawnStroke ? drawnStroke->sceneContentRect().center() : QPointF(500.0, 400.0));
        sendMouseEvent(view.viewport(), QEvent::MouseButtonPress, ctrlDragStart, Qt::LeftButton, Qt::LeftButton,
                       Qt::ControlModifier);
        sendMouseEvent(view.viewport(), QEvent::MouseMove, ctrlDragStart + QPoint(70, 45), Qt::NoButton, Qt::LeftButton,
                       Qt::ControlModifier);
        sendMouseEvent(view.viewport(), QEvent::MouseButtonRelease, ctrlDragStart + QPoint(70, 45), Qt::LeftButton,
                       Qt::NoButton, Qt::ControlModifier);
        QApplication::processEvents();
        ctx.check(drawnStroke && drawnStroke->pos() == ctrlDragItemBefore,
                  QStringLiteral("编辑模式 Ctrl 拖动不移动图元"));
    }

    if (drawnStroke) {
        // Ctrl 只是点击（没有拖动）时仍按点选处理，保留原有的“Ctrl 点击切换选中”。
        const bool ctrlClickSelectedBefore = drawnStroke->isSelected();
        const QPoint ctrlClickPoint = view.mapFromScene(drawnStroke->sceneContentRect().center());
        sendMouseEvent(view.viewport(), QEvent::MouseButtonPress, ctrlClickPoint, Qt::LeftButton, Qt::LeftButton,
                       Qt::ControlModifier);
        sendMouseEvent(view.viewport(), QEvent::MouseButtonRelease, ctrlClickPoint, Qt::LeftButton, Qt::NoButton,
                       Qt::ControlModifier);
        QApplication::processEvents();
        ctx.check(drawnStroke->isSelected() != ctrlClickSelectedBefore,
                  QStringLiteral("编辑模式 Ctrl 点击切换图元选中"));

        // Ctrl + 滚轮缩放整体视图，且尺寸不随缩放改变。
        const qreal ctrlWheelZoomBefore = view.zoom();
        const QSizeF ctrlWheelSizeBefore = drawnStroke->contentSize();
        const QPoint wheelCenter = view.viewport()->rect().center();
        QWheelEvent ctrlWheel(QPointF(wheelCenter), QPointF(view.viewport()->mapToGlobal(wheelCenter)), QPoint(),
                              QPoint(0, 120), Qt::NoButton, Qt::ControlModifier, Qt::ScrollUpdate, false);
        QApplication::sendEvent(view.viewport(), &ctrlWheel);
        QApplication::processEvents();
        ctx.check(view.zoom() > ctrlWheelZoomBefore, QStringLiteral("编辑模式 Ctrl 滚轮缩放整体视图"),
                  QStringLiteral("before=%1 after=%2").arg(ctrlWheelZoomBefore).arg(view.zoom()));
        ctx.check(drawnStroke->contentSize() == ctrlWheelSizeBefore,
                  QStringLiteral("编辑模式 Ctrl 滚轮缩放不修改图元尺寸"));
    }

    if (drawnStroke) {
        const QSizeF sizeBeforeResize = drawnStroke->contentSize();
        const QPoint handleScreen = view.mapFromScene(drawnStroke->mapToScene(QPointF(0.0, 0.0)));
        sendMouseEvent(view.viewport(), QEvent::MouseButtonPress, handleScreen, Qt::LeftButton, Qt::LeftButton);
        sendMouseEvent(view.viewport(), QEvent::MouseMove, handleScreen + QPoint(45, 35), Qt::NoButton, Qt::LeftButton);
        sendMouseEvent(view.viewport(), QEvent::MouseButtonRelease, handleScreen + QPoint(45, 35), Qt::LeftButton,
                       Qt::NoButton);
        QApplication::processEvents();
        ctx.check(drawnStroke->contentSize().width() < sizeBeforeResize.width() &&
                      drawnStroke->contentSize().height() < sizeBeforeResize.height(),
                  QStringLiteral("编辑模式拖动控制点缩放图元"),
                  geometryText(drawnStroke->pos(), drawnStroke->contentSize()));
        viewUndo.undo();
        ctx.check(drawnStroke->contentSize() == sizeBeforeResize, QStringLiteral("撤销缩放图元（鼠标交互）"));
        viewUndo.redo();
        ctx.check(drawnStroke->contentSize() != sizeBeforeResize, QStringLiteral("重做缩放图元（鼠标交互）"));

        auto* secondStroke = new CurveItem();
        secondStroke->beginStroke(QPointF(900, 160), QColor(QStringLiteral("#0F766E")), 5.0);
        secondStroke->appendScenePoint(QPointF(1000, 220));
        secondStroke->appendScenePoint(QPointF(1100, 170));
        secondStroke->endStroke();
        viewScene.addItemsWithUndo({secondStroke}, QStringLiteral("添加曲线"));
        view.fitAll();
        QApplication::processEvents();
        viewScene.selectAllItems();
        ctx.check(viewScene.selectedItems().size() == 2, QStringLiteral("编辑模式多选图元"));

        const QPointF firstBefore = drawnStroke->pos();
        const QPointF secondBefore = secondStroke->pos();
        const QPoint multiDragStart = view.mapFromScene(QPointF(900, 160));
        sendMouseEvent(view.viewport(), QEvent::MouseButtonPress, multiDragStart, Qt::LeftButton, Qt::LeftButton);
        sendMouseEvent(view.viewport(), QEvent::MouseMove, multiDragStart + QPoint(45, 30), Qt::NoButton,
                       Qt::LeftButton);
        sendMouseEvent(view.viewport(), QEvent::MouseButtonRelease, multiDragStart + QPoint(45, 30), Qt::LeftButton,
                       Qt::NoButton);
        QApplication::processEvents();
        const QPointF firstDelta = drawnStroke->pos() - firstBefore;
        const QPointF secondDelta = secondStroke->pos() - secondBefore;
        ctx.check(QLineF(firstDelta, QPointF()).length() > 1.0, QStringLiteral("多选拖动移动图元"));
        ctx.check(QLineF(firstDelta, secondDelta).length() < 0.01, QStringLiteral("多选图元位移一致"));
        viewUndo.undo();
        ctx.check(drawnStroke->pos() == firstBefore && secondStroke->pos() == secondBefore,
                  QStringLiteral("撤销多选移动"));
        viewUndo.redo();
        ctx.check(drawnStroke->pos() != firstBefore && secondStroke->pos() != secondBefore,
                  QStringLiteral("重做多选移动"));
    }

    QFont doubleClickFont(QStringLiteral("Microsoft YaHei UI"));
    doubleClickFont.setPointSize(28);
    auto* doubleClickText = new TextItem();
    doubleClickText->setTextFormat(QStringLiteral("双击编辑"), doubleClickFont, QColor(QStringLiteral("#7C3AED")));
    doubleClickText->setPos(QPointF(180, 620));
    viewScene.addItemsWithUndo({doubleClickText}, QStringLiteral("添加文字"));
    view.fitAll();
    QApplication::processEvents();
    QObject::connect(&view, &CanvasView::itemPropertiesRequested, &view,
                     [&ctx](CanvasItem*) { ++ctx.itemPropertyRequests; });
    const QPoint doubleClickPoint = view.mapFromScene(doubleClickText->sceneContentRect().center());
    QMouseEvent doubleClickEvent(QEvent::MouseButtonDblClick, QPointF(doubleClickPoint),
                                 QPointF(view.viewport()->mapToGlobal(doubleClickPoint)), Qt::LeftButton,
                                 Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &doubleClickEvent);
    QApplication::processEvents();
    ctx.check(ctx.itemPropertyRequests == 1, QStringLiteral("双击文字请求属性弹窗"));

    // 需求 42：查看模式下双击图元也可以打开（只读）属性窗。
    view.setMode(ToolMode::View);
    view.fitAll();
    QApplication::processEvents();
    const QPoint viewDoubleClickPoint = view.mapFromScene(doubleClickText->sceneContentRect().center());
    QMouseEvent viewDoubleClickEvent(QEvent::MouseButtonDblClick, QPointF(viewDoubleClickPoint),
                                     QPointF(view.viewport()->mapToGlobal(viewDoubleClickPoint)), Qt::LeftButton,
                                     Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &viewDoubleClickEvent);
    QApplication::processEvents();
    ctx.check(ctx.itemPropertyRequests == 2, QStringLiteral("查看模式双击文字仍请求属性弹窗"));
    view.setMode(ToolMode::Edit);

    {
        ItemPropertiesDialog dialog(&viewScene, doubleClickText, &view);
        dialog.show();
        QApplication::processEvents();
        bool hasEditText = false;
        for (QPushButton* button : dialog.findChildren<QPushButton*>()) {
            if (button->text() == uiText(QStringLiteral("Edit text…"))) {
                hasEditText = true;
                break;
            }
        }
        ctx.check(!hasEditText, QStringLiteral("文字属性窗不再提供编辑文字按钮"));
        QPlainTextEdit* inlineText = dialog.findChild<QPlainTextEdit*>(QStringLiteral("ItemPropertiesTextValue"));
        ctx.check(inlineText && inlineText->isEnabled(), QStringLiteral("文字属性窗保留直接编辑正文"));
        QToolButton* textColorValue = dialog.findChild<QToolButton*>(QStringLiteral("ItemPropertiesTextColorValue"));
        QPushButton* fontValue = dialog.findChild<QPushButton*>(QStringLiteral("ItemPropertiesFontValue"));
        ctx.check(textColorValue && textColorValue->isEnabled(), QStringLiteral("文字属性窗文字颜色可编辑"));
        ctx.check(fontValue && fontValue->isEnabled(), QStringLiteral("文字属性窗字体可编辑"));
        dialog.close();
        QApplication::processEvents();
    }

    {
        ItemPropertiesDialog readOnlyDialog(&viewScene, doubleClickText, &view, false);
        readOnlyDialog.show();
        QApplication::processEvents();
        ctx.check(!readOnlyDialog.findChild<QCheckBox*>(QStringLiteral("ItemPropertiesBoldValue")) &&
                      !readOnlyDialog.findChild<QCheckBox*>(QStringLiteral("ItemPropertiesItalicValue")) &&
                      !readOnlyDialog.findChild<QCheckBox*>(QStringLiteral("ItemPropertiesUnderlineValue")) &&
                      !readOnlyDialog.findChild<QCheckBox*>(QStringLiteral("ItemPropertiesStrikeOutValue")),
                  QStringLiteral("只读文字属性窗不显示文字样式选项"));
        readOnlyDialog.close();
        QApplication::processEvents();
    }

    ctx.doubleClickText = doubleClickText;
}
