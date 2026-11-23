#include "canvas/CanvasView.h"
#include "items/CanvasItem.h"
#include "items/CanvasTypes.h"
#include "items/geometry/ShapePathBuilder.h"
#include "items/lines/ArcItem.h"
#include "items/lines/CurveItem.h"
#include "items/lines/SineItem.h"
#include "items/lines/StraightLineItem.h"
#include "items/polygon/PolygonItem.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "items/shapes/ShapeRegistry.h"
#include "ops/LineSplitOps.h"
#include "ops/MergeRegions.h"
#include "ops/PathRegions.h"
#include "scene/CanvasScene.h"
#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"
#include "selftest/core/SelftestHelpers.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/panels/properties/PropertiesPanel.h"

#include <QAction>
#include <QApplication>
#include <QPainterPath>
#include <QPointF>
#include <QToolButton>
#include <QUndoStack>
#include <QVector>
#include <QtMath>
#include <algorithm>
#include <limits>

namespace {

// 场景内所有图元的数量（切刀保留，碎片与被切图元都会被计数）。
int ItemCount(const CanvasScene& scene)
{
    return scene.CanvasItemCount();
}

} // namespace

void selftest::RunLineSplitTests(Context& ctx)
{
    // 参与者规则：≥1 个填充图元 + 恰好 1 条线类切刀。
    {
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        scene.SetMode(ToolMode::EDIT);
        QUndoStack undo;
        scene.SetUndoStack(&undo);

        auto* rect = ShapeRegistry::Create(ShapeType::RECTANGLE);
        rect->SetContentSize(QSizeF(180.0, 120.0));
        scene.AddItemsWithUndo({rect}, QStringLiteral("Add rect"));

        scene.clearSelection();
        rect->setSelected(true);
        ctx.Check(!LineSplitOps::Supports(&scene), QStringLiteral("没有切刀时分割不可用"));

        // 切刀是曲线族的直线：两端落在画布内，水平横穿矩形中部。
        auto* knife = new StraightLineItem();
        knife->SetEndpoints(QPointF(100.0, 210.0), QPointF(300.0, 210.0), QColor(QStringLiteral("#1E293B")), 2.0);
        scene.AddItemsWithUndo({knife}, QStringLiteral("Add knife"));

        scene.clearSelection();
        rect->setSelected(true);
        knife->setSelected(true);
        ctx.Check(LineSplitOps::Supports(&scene), QStringLiteral("填充图元加一条直线切刀时可用"));

        auto* secondRect = ShapeRegistry::Create(ShapeType::RECTANGLE);
        secondRect->SetContentSize(QSizeF(80.0, 80.0));
        scene.AddItemsWithUndo({secondRect}, QStringLiteral("Add second rect"));
        scene.clearSelection();
        rect->setSelected(true);
        secondRect->setSelected(true);
        ctx.Check(!LineSplitOps::Supports(&scene), QStringLiteral("只有填充图元没有切刀时不可用"));
        secondRect->setSelected(false);
    }

    // 直线切矩形：上下两块多边形，切刀保留，一次撤销还原。
    {
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        scene.SetMode(ToolMode::EDIT);
        QUndoStack undo;
        scene.SetUndoStack(&undo);

        auto* rect = ShapeRegistry::Create(ShapeType::RECTANGLE);
        rect->SetContentSize(QSizeF(180.0, 120.0));
        rect->setPos(QPointF(110.0, 150.0));
        rect->SetFill(QColor(QStringLiteral("#2563EB")), FillStyle::SOLID);
        scene.AddItemsWithUndo({rect}, QStringLiteral("Add rect"));
        auto* knife = new StraightLineItem();
        knife->SetEndpoints(QPointF(100.0, 210.0), QPointF(300.0, 210.0), QColor(QStringLiteral("#1E293B")), 2.0);
        scene.AddItemsWithUndo({knife}, QStringLiteral("Add knife"));

        scene.clearSelection();
        rect->setSelected(true);
        knife->setSelected(true);
        const int before = ItemCount(scene);
        ctx.Check(LineSplitOps::Apply(&scene, QStringLiteral("分割")), QStringLiteral("直线可以切分矩形"));
        const int after = ItemCount(scene);
        ctx.Check(after == before + 1, QStringLiteral("切分后切刀保留并多出两块碎片"),
                  QStringLiteral("before=%1 after=%2").arg(before).arg(after));

        // 碎片样式沿用被切图元，且刚好两块、互不重叠。
        QList<PolygonItem*> fragments;
        int knifeCount = 0;
        for (CanvasItem* item : scene.CanvasItems()) {
            if (auto* composite = dynamic_cast<PolygonItem*>(item)) {
                fragments.append(composite);
            } else if (dynamic_cast<StraightLineItem*>(item)) {
                ++knifeCount;
            }
        }
        ctx.Check(fragments.size() == 2 && knifeCount == 1, QStringLiteral("矩形被切成两块且直线切刀还在"),
                  QStringLiteral("fragments=%1 knives=%2").arg(fragments.size()).arg(knifeCount));
        qreal areaSum = 0.0;
        bool stylesOk = fragments.size() == 2;
        for (const PolygonItem* fragment : fragments) {
            const PolygonItem::Layer& layer = fragment->Layers().constFirst();
            stylesOk = stylesOk && layer.fill == QColor(QStringLiteral("#2563EB")) &&
                       layer.role == PolygonItem::LayerRole::FILLED_AREA && fragment->fillStyle() == FillStyle::SOLID;
            const QRectF bounds = layer.path.boundingRect();
            areaSum += bounds.width() * bounds.height();
        }
        ctx.Check(stylesOk, QStringLiteral("碎片沿用被切图元的填充色与填充样式"));
        // 包围盒面积按切刀带厚度各让出一条：容差覆盖刀带宽度 × 边长。
        ctx.Check(qAbs(areaSum - 180.0 * 120.0) < 600.0, QStringLiteral("两块碎片面积之和等于原矩形（刀带占宽）"),
                  QStringLiteral("area=%1").arg(areaSum));
        ctx.Check(fragments.at(0)->isSelected() && fragments.at(1)->isSelected(),
                  QStringLiteral("切分后碎片处于选中状态"));

        // 需求 11：碎片轮廓按闭合子路径描边，收尾边不能缺。
        bool closureOk = fragments.size() == 2;
        for (const PolygonItem* fragment : fragments) {
            const QPainterPath& piece = fragment->Layers().constFirst().path;
            const QPainterPath closed = PolygonItem::ClosedSubpaths(piece);
            closureOk = closureOk && closed.elementCount() >= piece.elementCount();
            // 闭合后的路径终点回到子路径起点：首元素与最后一个绘制元素重合。
            const QPointF first = closed.elementAt(0);
            const QPointF last = closed.elementAt(closed.elementCount() - 1);
            closureOk = closureOk && qFuzzyCompare(first.x(), last.x()) && qFuzzyCompare(first.y(), last.y());
        }
        ctx.Check(closureOk, QStringLiteral("分割碎片的每条子路径都显式闭合"));

        // 需求 12：选中填充图元 + 切刀时，编辑图元面板也展开分割按钮（6 项都可见）。
        {
            MainWindow window;
            window.show();
            QApplication::processEvents();
            window.InsertShape(ShapeType::RECTANGLE);
            window.InsertCurve(LineGlyph::STRAIGHT);
            CanvasScene* mainScene = window.findChild<CanvasScene*>();
            QList<CanvasItem*> items = mainScene->CanvasItems();
            mainScene->clearSelection();
            for (CanvasItem* item : items) {
                item->setSelected(true);
            }
            mainScene->update();
            if (auto* dockPanel = window.findChild<PropertiesPanel*>()) {
                dockPanel->Refresh();
                QApplication::processEvents();
                const QList<QToolButton*> morphButtons =
                    dockPanel->findChildren<QToolButton*>(QStringLiteral("ItemMorphButton"));
                int visibleMorph = 0;
                for (const QToolButton* button : morphButtons) {
                    if (button->isVisible()) {
                        ++visibleMorph;
                    }
                }
                // 矩形 + 直线只有分割可用：6 颗按钮里只有最后一颗（分割）可见。
                ctx.Check(morphButtons.size() == 6 && visibleMorph == 1 && morphButtons.last()->isVisible(),
                          QStringLiteral("选中填充图元与切刀时面板展开分割按钮"),
                          QStringLiteral("total=%1 visible=%2").arg(morphButtons.size()).arg(visibleMorph));
            } else {
                ctx.Check(false, QStringLiteral("编辑图元面板存在"));
            }
        }

        undo.undo();
        ctx.Check(ItemCount(scene) == before, QStringLiteral("撤销一次还原为矩形加切刀"));
        undo.redo();
        ctx.Check(ItemCount(scene) == after, QStringLiteral("重做恢复切分结果"));
    }

    // 需求 23：真机流程复现——画笔曲线 + 矩形双选时，分割动作必须使能。
    {
        MainWindow window;
        window.show();
        QApplication::processEvents();
        window.InsertShape(ShapeType::RECTANGLE);
        CanvasView* view = window.View();
        // 画一条横穿矩形本体的画笔曲线（起终点在矩形两侧之外）。
        const QRectF rectBounds = window.Scene()->CanvasItems().constFirst()->SceneContentRect();
        const QPointF crossStart = rectBounds.center() + QPointF(-rectBounds.width(), 0.0);
        view->SetMode(ToolMode::BRUSH);
        const QPoint brushStart = view->mapFromScene(crossStart);
        SendMouseEvent(view->viewport(), QEvent::MouseButtonPress, brushStart, Qt::LeftButton, Qt::LeftButton);
        for (int i = 1; i <= 8; ++i) {
            const QPoint point(brushStart.x() + i * (rectBounds.width() / 4.0),
                               brushStart.y() + (i % 2 == 0 ? 12 : -10));
            SendMouseEvent(view->viewport(), QEvent::MouseMove, point, Qt::NoButton, Qt::LeftButton);
        }
        const QPoint brushEnd(brushStart.x() + int(rectBounds.width() * 2.0), brushStart.y() + 6);
        SendMouseEvent(view->viewport(), QEvent::MouseButtonRelease, brushEnd, Qt::LeftButton, Qt::NoButton);
        QApplication::processEvents();
        view->SetMode(ToolMode::EDIT);
        QApplication::processEvents();

        CanvasScene* mainScene = window.findChild<CanvasScene*>();
        mainScene->clearSelection();
        QApplication::processEvents();
        // 按真机操作路径选择：先点选矩形（取角落，避开横穿曲线的命中容差带），
        // 再 Ctrl+点选画笔曲线的顶点，两个图元必须同时处于选中状态。
        const QPoint rectClick = view->mapFromScene(QPointF(rectBounds.left() + 12.0, rectBounds.top() + 12.0));
        SendMouseEvent(view->viewport(), QEvent::MouseButtonPress, rectClick, Qt::LeftButton, Qt::LeftButton);
        SendMouseEvent(view->viewport(), QEvent::MouseButtonRelease, rectClick, Qt::LeftButton, Qt::NoButton);
        QApplication::processEvents();
        {
            const QList<CanvasItem*> selected = mainScene->SelectedCanvasItems();
            ctx.Check(selected.size() == 1 && dynamic_cast<ShapeAbstractItem*>(selected.value(0)) != nullptr,
                      QStringLiteral("点选矩形内部仅选中矩形"), QStringLiteral("size=%1").arg(selected.size()));
        }
        const QPointF curveScenePos = QPointF(crossStart.x() + rectBounds.width() / 2.0, crossStart.y() - 10.0);
        const QPoint curveClick = view->mapFromScene(curveScenePos);
        SendMouseEvent(view->viewport(), QEvent::MouseButtonPress, curveClick, Qt::LeftButton, Qt::LeftButton,
                       Qt::ControlModifier);
        QApplication::processEvents();
        {
            const QList<CanvasItem*> pressed = mainScene->SelectedCanvasItems();
            ctx.Check(pressed.size() == 2, QStringLiteral("Ctrl 按下即同时选中矩形与曲线"),
                      QStringLiteral("pressed=%1").arg(pressed.size()));
        }
        SendMouseEvent(view->viewport(), QEvent::MouseButtonRelease, curveClick, Qt::LeftButton, Qt::NoButton,
                       Qt::ControlModifier);
        QApplication::processEvents();
        {
            const QList<CanvasItem*> selected = mainScene->SelectedCanvasItems();
            const bool hasShape = std::any_of(selected.cbegin(), selected.cend(), [](CanvasItem* item) {
                return dynamic_cast<ShapeAbstractItem*>(item) != nullptr;
            });
            const bool hasCurve = std::any_of(selected.cbegin(), selected.cend(), [](CanvasItem* item) {
                return dynamic_cast<CurveItem*>(item) != nullptr;
            });
            ctx.Check(selected.size() == 2 && hasShape && hasCurve, QStringLiteral("松开后矩形与画笔曲线同时选中"),
                      QStringLiteral("size=%1").arg(selected.size()));
        }
        ctx.Check(LineSplitOps::Supports(mainScene), QStringLiteral("矩形加画笔曲线双选时分割判定可用"),
                  QStringLiteral("items=%1").arg(mainScene->CanvasItemCount()));
        QAction* splitAction = nullptr;
        for (QAction* action : window.findChildren<QAction*>()) {
            if (action->property("lineSplitAction").toBool()) {
                splitAction = action;
            }
        }
        ctx.Check(splitAction && splitAction->isEnabled(), QStringLiteral("分割动作在双选后使能"),
                  splitAction ? QStringLiteral("enabled=%1").arg(splitAction->isEnabled()) : QStringLiteral("missing"));
        // 再验证整条链路能真正执行切分：画笔曲线必须穿过矩形本体。
        ctx.Check(LineSplitOps::Apply(mainScene, QStringLiteral("分割")), QStringLiteral("画笔曲线可以切分矩形"));
    }

    // 曲线切椭圆：中心线延伸后同样分成两块。
    {
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        scene.SetMode(ToolMode::EDIT);
        QUndoStack undo;
        scene.SetUndoStack(&undo);

        auto* ellipse = ShapeRegistry::Create(ShapeType::ELLIPSE);
        ellipse->SetContentSize(QSizeF(160.0, 120.0));
        ellipse->setPos(QPointF(120.0, 180.0));
        scene.AddItemsWithUndo({ellipse}, QStringLiteral("Add ellipse"));
        auto* curve = new CurveItem();
        curve->SetStrokePoints({QPointF(100.0, 240.0), QPointF(280.0, 240.0)}, QColor(QStringLiteral("#334155")), 3.0);
        scene.AddItemsWithUndo({curve}, QStringLiteral("Add curve"));

        scene.clearSelection();
        ellipse->setSelected(true);
        curve->setSelected(true);
        ctx.Check(LineSplitOps::Supports(&scene), QStringLiteral("曲线作为切刀时可用"));
        ctx.Check(LineSplitOps::Apply(&scene, QStringLiteral("分割")), QStringLiteral("曲线可以切分椭圆"));

        int fragments = 0;
        for (CanvasItem* item : scene.CanvasItems()) {
            if (dynamic_cast<PolygonItem*>(item)) {
                ++fragments;
            }
        }
        ctx.Check(fragments == 2, QStringLiteral("椭圆被曲线切成两块"), QStringLiteral("fragments=%1").arg(fragments));
    }

    // 需求 21：弯曲曲线按完整轨迹切分，切口贴合曲线走向而不是端点连线。
    {
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        scene.SetMode(ToolMode::EDIT);
        QUndoStack undo;
        scene.SetUndoStack(&undo);

        auto* rect = ShapeRegistry::Create(ShapeType::RECTANGLE);
        rect->SetContentSize(QSizeF(200.0, 200.0));
        rect->setPos(QPointF(110.0, 150.0));
        scene.AddItemsWithUndo({rect}, QStringLiteral("Add rect"));
        // 下凹的曲线：从矩形左缘进入、中部下探到 y≈300、右缘出去；
        // 端点连线（y≈200 的水平线）与轨迹明显不同。
        auto* curve = new CurveItem();
        curve->SetStrokePoints({QPointF(80.0, 200.0), QPointF(200.0, 300.0), QPointF(320.0, 200.0)},
                               QColor(QStringLiteral("#334155")), 4.0);
        scene.AddItemsWithUndo({curve}, QStringLiteral("Add curve"));

        scene.clearSelection();
        rect->setSelected(true);
        curve->setSelected(true);
        ctx.Check(LineSplitOps::Supports(&scene), QStringLiteral("弯曲曲线作为切刀时可用"));
        ctx.Check(LineSplitOps::Apply(&scene, QStringLiteral("分割")), QStringLiteral("弯曲曲线可以按轨迹切分矩形"));

        int fragments = 0;
        bool upperFound = false;
        bool lowerFound = false;
        for (CanvasItem* item : scene.CanvasItems()) {
            if (auto* fragment = dynamic_cast<PolygonItem*>(item)) {
                ++fragments;
                const QPainterPath painted =
                    fragment->mapToScene(fragment->MapLayerPath(fragment->Layers().constFirst().path));
                upperFound = upperFound || painted.contains(QPointF(200.0, 160.0));
                lowerFound = lowerFound || painted.contains(QPointF(200.0, 340.0));
            }
        }
        ctx.Check(fragments == 2, QStringLiteral("矩形被弯曲曲线切成两块"),
                  QStringLiteral("fragments=%1").arg(fragments));
        ctx.Check(upperFound && lowerFound, QStringLiteral("切口贴合曲线轨迹（上/下两块沿曲线分布）"),
                  QStringLiteral("upper=%1 lower=%2").arg(upperFound).arg(lowerFound));
    }

    // 需求 29：弧线由椭圆类派生几何、由曲线类派生行为，无填充区域归类为线条
    // ——与矩形双选时分割可用，并按弧线轨迹（而非端点连线）切分。
    {
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        scene.SetMode(ToolMode::EDIT);
        QUndoStack undo;
        scene.SetUndoStack(&undo);

        auto* rect = ShapeRegistry::Create(ShapeType::RECTANGLE);
        rect->SetContentSize(QSizeF(128.0, 128.0));
        rect->setPos(QPointF(500.0, 500.0));
        scene.AddItemsWithUndo({rect}, QStringLiteral("Add rect"));
        // 半椭圆弧：两端落在矩形左右两缘之外，弧底从矩形下部横穿。
        auto* arc = new ArcItem();
        arc->SetArcGeometry(QPointF(430.0, 550.0), QSizeF(268.0, 40.0), 0.0, 180.0, QColor(QStringLiteral("#1E293B")),
                            2.0);
        scene.AddItemsWithUndo({arc}, QStringLiteral("Add arc"));

        scene.clearSelection();
        rect->setSelected(true);
        arc->setSelected(true);
        ctx.Check(LineSplitOps::Supports(&scene), QStringLiteral("矩形加弧线双选时分割判定可用"));
        ctx.Check(LineSplitOps::Apply(&scene, QStringLiteral("分割")), QStringLiteral("弧线可以按轨迹切分矩形"));

        int fragments = 0;
        for (CanvasItem* item : scene.CanvasItems()) {
            if (dynamic_cast<PolygonItem*>(item)) {
                ++fragments;
            }
        }
        ctx.Check(fragments == 2, QStringLiteral("矩形被弧线切成两块"), QStringLiteral("fragments=%1").arg(fragments));
    }

    // 正弦波是曲线族里的一员：与矩形双选时同样按轨迹切分。
    {
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(1536.0, 1024.0));
        QUndoStack undo;
        scene.SetUndoStack(&undo);
        // 判定与切分都要求编辑模式（isItemEditable 只看模式与图层状态）。
        scene.SetMode(ToolMode::EDIT);
        auto* sine = new SineItem();
        sine->SetWaveGeometry(QPointF(300.0, 200.0), QSizeF(128.0, 40.0), QColor(QStringLiteral("#111827")), 4.0);
        scene.AddItemsWithUndo({sine}, QStringLiteral("Add sine"));

        auto* rect = ShapeRegistry::Create(ShapeType::RECTANGLE);
        rect->SetContentSize(QSizeF(80.0, 60.0));
        rect->setPos(QPointF(320.0, 190.0));
        scene.AddItemsWithUndo({rect}, QStringLiteral("Add rect"));
        scene.clearSelection();
        rect->setSelected(true);
        sine->setSelected(true);
        ctx.Check(LineSplitOps::Supports(&scene), QStringLiteral("正弦波曲线可作为切刀"));
    }

    // 需求 21：曲线刀线未穿过填充图形（整条落在内部）时不切分。
    {
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        scene.SetMode(ToolMode::EDIT);
        QUndoStack undo;
        scene.SetUndoStack(&undo);

        auto* rect = ShapeRegistry::Create(ShapeType::RECTANGLE);
        rect->SetContentSize(QSizeF(200.0, 200.0));
        rect->setPos(QPointF(110.0, 150.0));
        scene.AddItemsWithUndo({rect}, QStringLiteral("Add rect"));
        auto* curve = new CurveItem();
        curve->SetStrokePoints({QPointF(150.0, 200.0), QPointF(200.0, 260.0), QPointF(250.0, 200.0)},
                               QColor(QStringLiteral("#334155")), 4.0);
        scene.AddItemsWithUndo({curve}, QStringLiteral("Add curve"));

        scene.clearSelection();
        rect->setSelected(true);
        curve->setSelected(true);
        ctx.Check(!LineSplitOps::Apply(&scene, QStringLiteral("分割")), QStringLiteral("内部曲线不切分填充图形"));
        bool originalKept = false;
        for (CanvasItem* item : scene.CanvasItems()) {
            if (auto* shape = dynamic_cast<ShapeAbstractItem*>(item)) {
                originalKept = originalKept || shape == rect;
            }
        }
        ctx.Check(originalKept, QStringLiteral("未切到时原图元保留"));
    }

    // 刀线没碰到填充图形：不入栈、场景不变。
    {
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        scene.SetMode(ToolMode::EDIT);
        QUndoStack undo;
        scene.SetUndoStack(&undo);

        auto* rect = ShapeRegistry::Create(ShapeType::RECTANGLE);
        rect->SetContentSize(QSizeF(120.0, 90.0));
        rect->setPos(QPointF(110.0, 150.0));
        scene.AddItemsWithUndo({rect}, QStringLiteral("Add rect"));
        auto* knife = new StraightLineItem();
        knife->SetEndpoints(QPointF(90.0, 300.0), QPointF(250.0, 300.0), QColor(QStringLiteral("#1E293B")), 2.0);
        scene.AddItemsWithUndo({knife}, QStringLiteral("Add knife"));

        scene.clearSelection();
        rect->setSelected(true);
        knife->setSelected(true);
        const int before = ItemCount(scene);
        const int undoCount = undo.count();
        ctx.Check(!LineSplitOps::Apply(&scene, QStringLiteral("分割")),
                  QStringLiteral("切刀不与填充图形相交时返回失败"));
        ctx.Check(ItemCount(scene) == before && undo.count() == undoCount, QStringLiteral("切空时不产生图元与撤销记录"),
                  QStringLiteral("items=%1 undo=%2").arg(ItemCount(scene)).arg(undo.count()));
    }

    // 需求 25：左花括号分割波形旗帜时出现多余的奇怪色块。复现两种真机摆放：
    // A. 括号比旗面高、尖端探出旗面左缘；B. 按默认插入尺寸 42×128 与旗面
    // 128×86 中心层叠（真机默认插入即此摆放）。统计每块碎片的包围盒。
    const struct Arrangement {
        const char* label;
        QRectF flagRect;
        QRectF braceRect;
    } arrangements[] = {
        {"A-tip-outside", QRectF(300.0, 260.0, 128.0, 86.0), QRectF(280.0, 220.0, 110.0, 170.0)},
        {"B-default-cascade", QRectF(722.0, 487.0, 128.0, 86.0), QRectF(747.0, 448.0, 42.0, 128.0)},
    };
    for (const Arrangement& arrangement : arrangements) {
        CanvasScene scene;
        scene.ApplyCanvasSize(QSizeF(1536.0, 1024.0));
        scene.SetMode(ToolMode::EDIT);
        QUndoStack undo;
        scene.SetUndoStack(&undo);

        auto* flag = ShapeRegistry::Create(ShapeType::FLAG_WAVE);
        flag->SetContentSize(arrangement.flagRect.size());
        flag->setPos(arrangement.flagRect.topLeft());
        scene.AddItemsWithUndo({flag}, QStringLiteral("Add flag"));

        auto* brace = new CurveItem();
        {
            const QPainterPath braceShape = ShapePathBuilder::BracePath(
                QRectF(0.0, 0.0, arrangement.braceRect.width(), arrangement.braceRect.height()));
            QVector<QPointF> points;
            points.reserve(49);
            for (int i = 0; i <= 48; ++i) {
                points.append(arrangement.braceRect.topLeft() + braceShape.pointAtPercent(qreal(i) / qreal(48)));
            }
            brace->SetStrokePoints(points, QColor(QStringLiteral("#334155")), 4.0);
        }
        scene.AddItemsWithUndo({brace}, QStringLiteral("Add brace"));

        scene.clearSelection();
        flag->setSelected(true);
        brace->setSelected(true);
        ctx.Check(LineSplitOps::Supports(&scene), QStringLiteral("波形旗加花括号双选时分割可用"));
        ctx.Check(LineSplitOps::Apply(&scene, QStringLiteral("分割")), QStringLiteral("花括号可以切分波形旗"));

        // 需求 25：布尔减法在条带与波面近切线处会留下面积趋近 0 的退化碎屑，
        // 之前单独成图元渲染成孤立的“奇怪色块”。修复后所有碎片都是真实碎块。
        int fragments = 0;
        qreal minArea = std::numeric_limits<qreal>::max();
        QString geometry;
        for (CanvasItem* item : scene.CanvasItems()) {
            if (auto* fragment = dynamic_cast<PolygonItem*>(item)) {
                ++fragments;
                const QPainterPath painted =
                    fragment->mapToScene(fragment->MapLayerPath(fragment->Layers().constFirst().path));
                const qreal fillArea = PathRegions::Area(painted);
                minArea = qMin(minArea, fillArea);
                const QRectF bounds = painted.boundingRect().normalized();
                geometry += QStringLiteral("[area=%1 bbox=%2x%3]")
                                .arg(QString::number(fillArea, 'f', 1))
                                .arg(QString::number(bounds.width(), 'f', 1))
                                .arg(QString::number(bounds.height(), 'f', 1));
            }
        }
        ctx.Check(fragments >= 2, QStringLiteral("需求25-%1：花括号切出真实碎块").arg(arrangement.label),
                  QStringLiteral("fragments=%1 minArea=%2 %3").arg(fragments).arg(minArea, 0, 'f', 1).arg(geometry));
        ctx.Check(minArea >= 16.0, QStringLiteral("需求25-%1：不再产生退化色块碎片").arg(arrangement.label),
                  QStringLiteral("minArea=%1").arg(minArea, 0, 'f', 1));
    }

    // §5.4：线条分割以原始连续块数与刀线相交判定是否替换；切后填充不得超出原始填充区。
    {
        auto rectPath = [](qreal x, qreal y, qreal w, qreal h) {
            QPainterPath path;
            path.addRect(QRectF(x, y, w, h));
            return path;
        };
        // 原始图元本身由两块互不相连区域组成：记录原始块数，刀线未相交时不误判为被切开。
        const QPainterPath composite = rectPath(0, 0, 40, 40).united(rectPath(100, 0, 40, 40));
        const int originalBlocks = PathRegions::SplitDisjoint(composite).size();
        ctx.Check(originalBlocks == 2, QStringLiteral("多块复合图元记录原始连续块数"),
                  QStringLiteral("blocks=%1").arg(originalBlocks));

        // 切后碎片落在原始填充区内：允许掩膜校验通过。
        const QPainterPath target = rectPath(0, 0, 100, 100);
        const QPainterPath leftPiece = rectPath(0, 0, 40, 100);
        const QPainterPath rightPiece = rectPath(60, 0, 40, 100);
        const QPainterPath splitFill = leftPiece.united(rightPiece);
        ctx.Check(MergeRegions::FillWithinMask(splitFill, target),
                  QStringLiteral("线条分割结果不超出被切图元原始填充区"));
    }
}
