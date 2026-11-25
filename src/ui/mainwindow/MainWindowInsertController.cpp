#include "ui/mainwindow/MainWindowInsertController.h"

#include "app/Settings.h"
#include "app/TestMode.h"
#include "canvas/CanvasView.h"
#include "io/CustomShapeTemplates.h"
#include "items/CanvasItem.h"
#include "items/geometry/ShapePathBuilder.h"
#include "items/lines/AngleItem.h"
#include "items/lines/ArcItem.h"
#include "items/lines/BraceItem.h"
#include "items/lines/CurveItem.h"
#include "items/lines/SineItem.h"
#include "items/lines/StraightLineItem.h"
#include "items/polygon/PolygonItem.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "items/shapes/ShapeFactory.h"
#include "items/text/TextItem.h"
#include "scene/CanvasScene.h"
#include "ui/icons/IconFactory.h"
#include "ui/mainwindow/MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QPolygonF>
#include <QStatusBar>
#include <QTimer>
#include <QTransform>
#include <QUndoStack>
#include <QVector>
#include <QtMath>

namespace {

// 与 MainWindow 相同的报错出口：自检（离屏）时不许弹模态框。
void ReportWarning(QWidget* parent, const QString& title, const QString& text)
{
    if (TestMode::IsSelfTestMode()) {
        qWarning("self-test suppressed dialog [%s]: %s", qUtf8Printable(title), qUtf8Printable(text));
        return;
    }
    QMessageBox::warning(parent, title, text);
}

} // namespace

MainWindowInsertController::MainWindowInsertController(MainWindow* window) : m_window(window) {}

QPointF MainWindowInsertController::PlacementCenter(const QSizeF& itemSize) const
{
    const QRectF canvas = m_window->m_scene->CanvasRect();
    const QWidget* viewport = m_window->m_view->viewport();
    const QRect viewportRect = viewport->rect();
    QPointF center;
    bool atCursor = false;
    // 键盘入口才按鼠标位置落点；鼠标停在按钮或菜单上时仍落回视口中心。
    if (m_cursorPlacement) {
        const QPoint local = viewport->mapFromGlobal(QCursor::pos());
        if (viewportRect.contains(local)) {
            const QPointF scenePos = m_window->m_view->mapToScene(local);
            if (canvas.contains(scenePos)) {
                center = scenePos;
                atCursor = true;
            }
        }
    }
    if (!atCursor) {
        center = m_window->m_view->mapToScene(viewportRect.center());
        if (!canvas.contains(center)) {
            center = canvas.center();
        }
        const qreal cascade = (m_window->m_scene->CanvasItemCount() % 5) * 18.0;
        center += QPointF(cascade, cascade);
    }
    const qreal halfWidth = itemSize.width() / 2.0;
    const qreal halfHeight = itemSize.height() / 2.0;
    const qreal minX = canvas.left() + halfWidth;
    const qreal maxX = qMax(minX, canvas.right() - halfWidth);
    const qreal minY = canvas.top() + halfHeight;
    const qreal maxY = qMax(minY, canvas.bottom() - halfHeight);
    center.setX(qBound(minX, center.x(), maxX));
    center.setY(qBound(minY, center.y(), maxY));
    return center;
}

void MainWindowInsertController::InsertTextAt(const QPointF& scenePos)
{
    auto* item = new TextItem();
    item->SetTextFormat(MainWindow::tr("Text"), item->TextFont(), m_window->m_brushColor);

    const QRectF canvas = m_window->m_scene->CanvasRect();
    const QSizeF size = item->ContentSize();
    qreal x = scenePos.x() - size.width() / 2.0;
    qreal y = scenePos.y() - size.height() / 2.0;
    x = qBound(canvas.left(), x, qMax(canvas.left(), canvas.right() - size.width()));
    y = qBound(canvas.top(), y, qMax(canvas.top(), canvas.bottom() - size.height()));
    item->setPos(QPointF(x, y));

    m_window->SetToolMode(ToolMode::EDIT);
    m_window->m_scene->AddItemsWithUndo({item}, MainWindow::tr("Add text"));
    item->setSelected(true);
    // 新建文字直接在图元上编辑，光标落在正文末尾，不用再去编辑面板里点输入框。
    const bool editing = m_window->m_view->BeginTextEdit(item);
    if (!editing) {
        m_window->m_view->setFocus();
    }
    m_window->statusBar()->showMessage(MainWindow::tr("Text item added"), 4000);
}

void MainWindowInsertController::InsertShape(ShapeType type, qreal triangleAngle1, qreal triangleAngle2,
                                             qreal cornerRatio)
{
    if (!m_window->m_scene->IsActiveLayerWritable()) {
        m_window->statusBar()->showMessage(
            MainWindow::tr("The current layer is locked or hidden; shapes cannot be "
                           "inserted. Unlock or switch layers in the layer panel first."),
            5000);
        return;
    }

    // 插入成功才切到编辑模式：图层不可写时不白白把用户从查看模式拽出来。
    if (m_window->m_view->mode() != ToolMode::EDIT) {
        m_window->SetToolMode(ToolMode::EDIT);
    }

    // 需求 4/5：立方体与圆柱体插入即分组，走组合面图元的专用通道。
    if (type == ShapeType::CUBE || type == ShapeType::CYLINDER) {
        InsertCompositeShape(type);
        return;
    }

    // 需求 16：新图元按工具栏当前选择生成——描线颜色（画笔色未选过时与工具栏
    // 展示一致取深灰）、描线宽度不再钳制、填充颜色未选过时跟随画笔色。
    const QColor stroke =
        m_window->m_brushColor.alpha() < 8 ? QColor(QStringLiteral("#1E293B")) : m_window->m_brushColor;
    const qreal strokeWidth = qMax<qreal>(0.5, m_window->m_brushWidth);
    const bool filled = ShapeTypeIsFilled(type) && m_window->m_fillStyle != FillStyle::NONE;
    const QColor fill = m_window->m_fillColor.isValid() ? m_window->m_fillColor : stroke;
    const FillStyle fillStyle = filled ? m_window->m_fillStyle : FillStyle::NONE;

    // 图形族已按 ShapeType 拆成具体类：十字星 / 六芒星 / 心形 / 旗帜 / 卷轴
    // 与其余有面积图形一样直接建类，不再经多边形包装。
    const QRectF canvas = m_window->m_scene->CanvasRect();
    QSizeF size = DefaultShapeSize(type);
    const qreal fitScale = qMin<qreal>(1.0, qMin(canvas.width() * 0.72 / qMax<qreal>(1.0, size.width()),
                                                 canvas.height() * 0.72 / qMax<qreal>(1.0, size.height())));
    size *= fitScale;

    const QPointF center = PlacementCenter(size);
    const QPointF topLeft = center - QPointF(size.width() / 2.0, size.height() / 2.0);

    CanvasItem* item = nullptr;
    {
        auto* shape = ShapeFactory::Create(type);
        shape->SetStyle(stroke, filled ? fill : QColor(Qt::transparent), strokeWidth);
        shape->SetFillStyle(fillStyle);
        // 需求 1：新图元沿用工具栏当前描线样式。
        shape->SetStrokeStyle(m_window->m_strokeStyle);
        shape->setPos(topLeft);
        shape->SetContentSize(size);
        // 三角形预置项：底边保持默认 64，按预置角度重算高度（角度是真相）。
        if (type == ShapeType::TRIANGLE && triangleAngle1 > 0.0 && triangleAngle2 > 0.0) {
            shape->SetTriangleAngles(triangleAngle1, triangleAngle2);
        }
        // 直线箭头预置项由上面的 LineItem 分支处理，这里只剩有面积图形。
        // 圆角矩形预置项：按短边比例折算成显式 px 圆角。
        if (type == ShapeType::RECTANGLE && cornerRatio > 0.0) {
            shape->SetCornerRadius(qMin(size.width(), size.height()) * cornerRatio);
        }
        item = shape;
    }

    m_window->m_scene->clearSelection();
    m_window->m_scene->AddItemsWithUndo({item}, MainWindow::tr("Insert %1").arg(ShapeTypes::Name(type)));
    item->setSelected(true);
    m_window->m_view->setFocus();
    if (type == ShapeType::ELLIPSE) {
        m_window->statusBar()->showMessage(
            MainWindow::tr("Inserted a circle; in edit mode drag a handle to stretch it into an ellipse"), 5000);
    } else {
        m_window->statusBar()->showMessage(
            MainWindow::tr("Inserted %1; drag to move, drag handles to resize, color follows the current brush")
                .arg(ShapeTypes::Name(type)),
            5000);
    }
}

void MainWindow::InsertCurve(LineGlyph glyph, ArrowStyle startArrow, ArrowStyle endArrow)
{
    if (!m_view || !m_scene) {
        return;
    }
    // 线条族同样按“插入即进编辑模式”处理，插入后可以直接拖动/改手柄。
    if (m_view->mode() != ToolMode::EDIT) {
        SetToolMode(ToolMode::EDIT);
    }
    // 弧线默认外接框与椭圆 / 扇形一致（128×128，半轴 64/64）；直线 / 正弦保持扁长比例。
    const QSizeF size = glyph == LineGlyph::STRAIGHT                             ? QSizeF(128.0, 64.0)
                        : (glyph == LineGlyph::ARC || glyph == LineGlyph::ANGLE) ? QSizeF(128.0, 128.0)
                                                                                 : QSizeF(128.0, 40.0);
    const QPointF center = m_insert->PlacementCenter(size);
    const QPointF topLeft = center - QPointF(size.width() / 2.0, size.height() / 2.0);

    const QColor stroke = m_brushColor.alpha() < 8 ? QColor(QStringLiteral("#1E293B")) : m_brushColor;
    const qreal width = qMax<qreal>(0.5, m_brushWidth);

    LineAbstractItem* curve = nullptr;
    if (glyph == LineGlyph::STRAIGHT) {
        auto* line = new StraightLineItem();
        const QPointF start(topLeft.x(), topLeft.y() + size.height() / 2.0);
        const QPointF end(topLeft.x() + size.width(), start.y());
        line->SetEndpoints(start, end, stroke, width);
        curve = line;
    } else if (glyph == LineGlyph::ARC) {
        auto* arc = new ArcItem();
        arc->SetArcGeometry(topLeft, size, 30.0, 120.0, stroke, width);
        curve = arc;
    } else if (glyph == LineGlyph::ANGLE) {
        auto* angle = new AngleItem();
        angle->SetAngleGeometry(topLeft, size, 30.0, 120.0, stroke, width);
        curve = angle;
    } else if (glyph == LineGlyph::SINE) {
        auto* sine = new SineItem();
        sine->SetWaveGeometry(topLeft, size, stroke, width);
        curve = sine;
    } else {
        return;
    }
    curve->SetStrokePattern(m_strokeStyle);
    curve->SetArrowStyles(startArrow, endArrow);

    m_scene->clearSelection();
    m_scene->AddItemsWithUndo({curve}, tr("Insert %1").arg(curve->DisplayName()));
    curve->setSelected(true);
    m_view->setFocus();
    statusBar()->showMessage(tr("Inserted %1; drag to move, drag handles to resize, color follows the current brush")
                                 .arg(curve->DisplayName()),
                             5000);
}

void MainWindowInsertController::InsertCompositeShape(ShapeType type)
{
    // 需求 16：与常见图形同一套工具栏取值规则（描线宽度不再钳制、填充跟随画笔色）。
    const QColor stroke =
        m_window->m_brushColor.alpha() < 8 ? QColor(QStringLiteral("#1E293B")) : m_window->m_brushColor;
    const qreal width = qMax<qreal>(0.5, m_window->m_brushWidth);
    const bool filled = m_window->m_fillStyle != FillStyle::NONE;
    const QColor fill = m_window->m_fillColor.isValid() ? m_window->m_fillColor : stroke;
    const FillStyle fillStyle = filled ? m_window->m_fillStyle : FillStyle::NONE;

    // 需求 9：立方体的三个面都是平行四边形图元本体（默认 60° 内角），
    // 靠各自的旋转角摆放成等轴测，不再用自定义轮廓冒充斜置的菱形。
    auto makeFace = [&stroke, &fill, fillStyle, width, this](const QString& name, const QPointF& center,
                                                             const QSizeF& faceSize, qreal rotation) {
        auto* face = ShapeFactory::Create(ShapeType::PARALLELOGRAM);
        // 名字走图元名：customName 只留给真正的自定义轮廓，类型仍显示「平行四边形」。
        face->SetName(name);
        face->SetStyle(stroke, fill, width);
        face->SetFillStyle(fillStyle);
        // 需求 1：组内各面同样沿用工具栏描线样式。
        face->SetStrokeStyle(m_window->m_strokeStyle);
        face->SetContentSize(faceSize);
        face->setPos(center - QPointF(faceSize.width() / 2.0, faceSize.height() / 2.0));
        face->SetRotationAngle(rotation);
        return face;
    };

    const QRectF canvas = m_window->m_scene->CanvasRect();
    QSizeF size = DefaultShapeSize(type);
    const qreal fitScale = qMin<qreal>(1.0, qMin(canvas.width() * 0.72 / qMax<qreal>(1.0, size.width()),
                                                 canvas.height() * 0.72 / qMax<qreal>(1.0, size.height())));
    size *= fitScale;

    const QPointF center = PlacementCenter(size);
    const QPointF topLeft = center - QPointF(size.width() / 2.0, size.height() / 2.0);

    QList<CanvasItem*> faces;
    if (type == ShapeType::CUBE) {
        // 需求 9：等轴测立方体 = 三个平行四边形图元的组。每个面都是 60° 平行四边形本体，
        // 顶面旋转 30°、左面 90°、右面 -30°，三个面的中心落在外轮廓的 0.25/0.5/0.75 处。
        // 单一边长 s 生成共享顶点：内容框 W = 1.732s、H = 2s（默认 128 框 → s = 64），
        // 三个面是与中心 O 相连的三个菱形，尺寸 (1.5s, 0.866s)、内角 60°，拉伸后仍无缝。
        const qreal s = size.height() / 2.0;
        const qreal boxW = 1.7320508 * s;
        const qreal boxH = 2.0 * s;
        const QSizeF faceSize(boxW * 0.8660254, boxH * 0.4330127);
        const QPointF origin = center - QPointF(boxW / 2.0, boxH / 2.0);
        const qreal cx = origin.x() + boxW / 2.0;
        const qreal cy = origin.y() + boxH / 2.0;
        faces.append(makeFace(MainWindow::tr("Cube top"), QPointF(cx, cy - 0.5 * s), faceSize, 30.0));
        faces.append(makeFace(MainWindow::tr("Cube left"), QPointF(cx - 0.4330127 * s, cy + 0.25 * s), faceSize, 90.0));
        faces.append(
            makeFace(MainWindow::tr("Cube right"), QPointF(cx + 0.4330127 * s, cy + 0.25 * s), faceSize, -30.0));
    } else {
        // 需求 30：圆柱体 = 一个有面积的多边形柱身 + 顶椭圆；先加柱身再加椭圆，
        // 层内高度递增，椭圆因此叠在多边形之上。
        const qreal w = size.width();
        const qreal h = size.height();
        const qreal ry = qMin(h * 0.16, w * 0.30);

        const qreal bodyH = h - ry;
        const QRectF bodyRect(topLeft + QPointF(0.0, ry), QSizeF(w, bodyH));
        QPainterPath body;
        const qreal cy = (h - 2.0 * ry) / bodyH;
        const qreal ryUnit = ry / bodyH;
        // 柱身顶边取顶椭圆的下半弧，不再画横穿椭圆中心的弦。
        const QRectF topUnit(0.0, -ryUnit, 1.0, ryUnit * 2.0);
        body.arcMoveTo(topUnit, 180.0);
        body.arcTo(topUnit, 180.0, -180.0);
        body.lineTo(1.0, cy);
        body.arcTo(QRectF(0.0, cy - ryUnit, 1.0, ryUnit * 2.0), 0.0, -180.0);
        body.closeSubpath();
        QTransform toScene;
        toScene.translate(bodyRect.left(), bodyRect.top());
        toScene.scale(bodyRect.width(), bodyRect.height());

        PolygonItem::Layer bodyLayer;
        bodyLayer.path = toScene.map(body);
        bodyLayer.fill = filled ? fill : QColor(Qt::transparent);
        bodyLayer.stroke = stroke;
        bodyLayer.strokeWidth = width;
        bodyLayer.filled = true;
        bodyLayer.role = PolygonItem::LayerRole::FILLED_AREA;
        auto* bodyItem = new PolygonItem(QList<PolygonItem::Layer>{bodyLayer}, fillStyle, m_window->m_strokeStyle);
        bodyItem->SetName(MainWindow::tr("Cylinder body"));
        faces.append(bodyItem);

        auto* top = ShapeFactory::Create(ShapeType::ELLIPSE);
        top->SetStyle(stroke, fill, width);
        top->SetFillStyle(fillStyle);
        top->SetStrokeStyle(m_window->m_strokeStyle);
        top->setPos(topLeft);
        top->SetContentSize(QSizeF(w, ry * 2.0));
        faces.append(top);
    }

    m_window->m_scene->clearSelection();
    m_window->m_scene->AddItemsWithUndo(faces, MainWindow::tr("Insert %1").arg(ShapeTypes::Name(type)));
    m_window->m_scene->AssignNewGroup(faces);
    for (CanvasItem* face : faces) {
        face->setSelected(true);
    }
    m_window->m_view->setFocus();
    m_window->statusBar()->showMessage(
        MainWindow::tr("Inserted %1 as a grouped set of faces; drag to move the whole shape")
            .arg(ShapeTypes::Name(type)),
        5000);
}

void MainWindowInsertController::InsertBrace(bool left)
{
    if (m_window->m_view->mode() != ToolMode::EDIT) {
        m_window->SetToolMode(ToolMode::EDIT);
    }

    if (!m_window->m_scene->IsActiveLayerWritable()) {
        m_window->statusBar()->showMessage(
            MainWindow::tr("The current layer is locked or hidden; shapes cannot be "
                           "inserted. Unlock or switch layers in the layer panel first."),
            5000);
        return;
    }

    const QColor stroke =
        m_window->m_brushColor.alpha() < 8 ? QColor(QStringLiteral("#1E293B")) : m_window->m_brushColor;
    const qreal width = qMax<qreal>(0.5, m_window->m_brushWidth);

    // 花括号沿用原默认尺寸（21×64，2 格基准）在 64px 格下放大一倍：42×128。
    const QRectF canvas = m_window->m_scene->CanvasRect();
    QSizeF size(42.0, 128.0);
    const qreal fitScale = qMin<qreal>(1.0, qMin(canvas.width() * 0.72 / qMax<qreal>(1.0, size.width()),
                                                 canvas.height() * 0.72 / qMax<qreal>(1.0, size.height())));
    size *= fitScale;

    QPointF center = PlacementCenter(size);

    const QPointF topLeft = center - QPointF(size.width() / 2.0, size.height() / 2.0);
    auto* item = new BraceItem();
    item->SetBraceRect(QRectF(topLeft, size), stroke, width);
    item->SetStrokePattern(m_window->m_strokeStyle);
    // 右括号 = 左括号旋转 180°（旋转绕内容中心，几何不复制第二套）。
    if (!left) {
        item->SetRotationAngle(180.0);
    }

    m_window->m_scene->clearSelection();
    m_window->m_scene->AddItemsWithUndo(
        {item}, MainWindow::tr("Insert %1").arg(left ? MainWindow::tr("Left brace") : MainWindow::tr("Right brace")));
    item->setSelected(true);
    m_window->m_view->setFocus();
    m_window->statusBar()->showMessage(MainWindow::tr("Inserted %1 as a brace; drag to move, drag handles to resize")
                                           .arg(left ? MainWindow::tr("Left brace") : MainWindow::tr("Right brace")),
                                       5000);
}

void MainWindowInsertController::InsertCustomShape(const QString& filePath)
{
    if (m_window->m_view->mode() != ToolMode::EDIT) {
        m_window->SetToolMode(ToolMode::EDIT);
    }

    if (!m_window->m_scene->IsActiveLayerWritable()) {
        m_window->statusBar()->showMessage(
            MainWindow::tr("The current layer is locked or hidden; shapes cannot be "
                           "inserted. Unlock or switch layers in the layer panel first."),
            5000);
        return;
    }

    CustomShapeTemplate customShape;
    QString error;
    if (!LoadCustomShapeTemplate(filePath, &customShape, &error)) {
        ReportWarning(m_window, MainWindow::tr("Custom shape"),
                      MainWindow::tr("Could not read %1:\n%2").arg(QDir::toNativeSeparators(filePath), error));
        return;
    }

    // 需求 16：与常见图形一致，按工具栏当前选择取样式（填充未选色跟随画笔色）。
    const QColor stroke =
        m_window->m_brushColor.alpha() < 8 ? QColor(QStringLiteral("#1E293B")) : m_window->m_brushColor;
    const QColor fill = m_window->m_fillColor.isValid() ? m_window->m_fillColor : stroke;
    const bool filled = m_window->m_fillStyle != FillStyle::NONE;
    const FillStyle fillStyle = filled ? m_window->m_fillStyle : FillStyle::NONE;
    const qreal strokeWidth = qMax<qreal>(0.5, m_window->m_brushWidth);

    // 需求 7：模板按原始宽高比插入，长边固定 128（2 格）；画布装不下时才等比缩到 72%。
    const QRectF canvas = m_window->m_scene->CanvasRect();
    const qreal aspect = qMax<qreal>(0.05, customShape.aspectRatio);
    const qreal longSide = 128.0;
    QSizeF size = aspect >= 1.0 ? QSizeF(longSide, longSide / aspect) : QSizeF(longSide * aspect, longSide);
    const qreal fitScale = qMin<qreal>(1.0, qMin(canvas.width() * 0.72 / qMax<qreal>(1.0, size.width()),
                                                 canvas.height() * 0.72 / qMax<qreal>(1.0, size.height())));
    size *= fitScale;
    size.setWidth(qMax<qreal>(16.0, size.width()));
    size.setHeight(qMax<qreal>(16.0, size.height()));

    const QPointF center = PlacementCenter(size);
    const QRectF targetRect(center - QPointF(size.width() / 2.0, size.height() / 2.0), size);

    // §6：模板按子路径拆分——闭合部分成多边形（单层 FILLED_AREA），开放部分成曲线。
    QList<CanvasItem*> items;
    if (customShape.parts.isEmpty()) {
        // 旧解析或退化轮廓：退回单图元自定义轮廓，保持兼容。
        auto* item = ShapeFactory::Create(ShapeType::RECTANGLE);
        item->SetCustomShape(customShape.name, customShape.unitPath);
        item->SetStyle(stroke, filled ? fill : QColor(Qt::transparent), strokeWidth);
        item->SetFillStyle(fillStyle);
        item->SetStrokeStyle(m_window->m_strokeStyle);
        item->setPos(targetRect.topLeft());
        item->SetContentSize(size);
        items.append(item);
    } else {
        QTransform unitToTarget;
        unitToTarget.translate(targetRect.left(), targetRect.top());
        unitToTarget.scale(targetRect.width(), targetRect.height());
        for (const CustomShapePart& part : std::as_const(customShape.parts)) {
            if (part.unitPath.isEmpty()) {
                continue;
            }
            if (part.closed) {
                PolygonItem::Layer layer;
                layer.path = unitToTarget.map(part.unitPath);
                layer.fill = filled ? fill : QColor(Qt::transparent);
                layer.stroke = stroke;
                layer.strokeWidth = strokeWidth;
                layer.filled = true;
                layer.role = PolygonItem::LayerRole::FILLED_AREA;
                items.append(new PolygonItem(QList<PolygonItem::Layer>{layer}, fillStyle, m_window->m_strokeStyle));
            } else {
                const QList<QPolygonF> polygons = part.unitPath.toSubpathPolygons();
                if (polygons.isEmpty()) {
                    continue;
                }
                QVector<QPointF> points;
                for (const QPointF& point : polygons.constFirst()) {
                    points.append(unitToTarget.map(point));
                }
                auto* curve = new CurveItem();
                curve->SetStrokePoints(points, stroke, strokeWidth, 0.0, true);
                items.append(curve);
            }
        }
    }

    if (items.isEmpty()) {
        return;
    }

    m_window->m_scene->clearSelection();
    const QString insertText = MainWindow::tr("Insert custom shape “%1”").arg(customShape.name);
    // 导入（插入）自定义图形即一次操作：多图元自动成组与插入合并成一条撤销记录，
    // 撤销时组名 / 父子记录随图元一起回退，重做也不丢组；单图元不建组（§6）。
    QUndoStack* undoStack = m_window->m_scene->UndoStack();
    const bool groupable = items.size() >= 2;
    if (groupable && undoStack) {
        undoStack->beginMacro(insertText);
    }
    m_window->m_scene->AddItemsWithUndo(items, insertText);
    for (CanvasItem* item : std::as_const(items)) {
        item->setSelected(true);
    }

    // §6：≥2 部分整组成组，单部分不落组；分组失败时图元保留并给出状态栏提示（§7.8）。
    if (groupable) {
        GroupFailureReason reason = GroupFailureReason::NONE;
        const int groupId = m_window->m_scene->AssignNewGroupWithUndo(items, &reason, customShape.name, insertText);
        if (groupId == 0) {
            m_window->statusBar()->showMessage(
                MainWindow::tr("Custom shape inserted, but grouping failed: %1").arg(GroupFailureReasonText(reason)),
                4000);
        }
        if (undoStack) {
            undoStack->endMacro();
        }
    }
    m_window->m_view->setFocus();
    m_window->statusBar()->showMessage(
        MainWindow::tr("Inserted custom shape “%1” from the shapes folder").arg(customShape.name), 4000);
}

void MainWindowInsertController::OpenShapesFolder()
{
    const AppSettings::ShapesFolderOpenResult result = AppSettings::OpenShapesFolder();
    if (!result.opened) {
        m_window->statusBar()->showMessage(AppSettings::ShapesFolderFailureText(result), 6000);
    }
}

void MainWindowInsertController::UpdateCustomShapeMenu()
{
    if (!m_window->m_customShapeMenu) {
        return;
    }
    m_window->m_customShapeMenu->clear();
    if (m_customShapeTrigger) {
        // 上一轮挂在「常见图形」末尾的同一项先摘掉，再加入新的。
        m_window->m_shapeMenu->removeAction(m_customShapeTrigger);
        delete m_customShapeTrigger;
        m_customShapeTrigger = nullptr;
    }

    // 需求：自定义图形不再是「常见图形」下的二级子菜单，菜单末尾直接给出这一项；
    // 工具栏下拉（m_customShapeMenu）复用同一个动作，两边看到的是同一入口。
    // 菜单项被点击时菜单马上关闭，用 singleShot(0) 延迟一拍再展开面板，否则面板拿不到焦点。
    QAction* trigger = new QAction(MainWindow::tr("Custom shapes…"), m_window);
    trigger->setObjectName(QStringLiteral("CustomShapePanelTrigger"));
    trigger->setToolTip(MainWindow::tr("Open the custom shape panel, one tab per folder in the shapes directory"));
    QObject::connect(trigger, &QAction::triggered, m_window, [this]() {
        QTimer::singleShot(0, m_window, [this]() { m_window->OpenCustomShapePanel(nullptr); });
    });
    m_customShapeTrigger = trigger;
    m_window->m_customShapeMenu->addAction(trigger);
    if (m_window->m_shapeMenu) {
        m_window->m_shapeMenu->addAction(trigger);
    }
}
