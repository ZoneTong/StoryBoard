#include "MainWindowInsertController.h"

#include "ArcItem.h"
#include "BraceItem.h"
#include "CanvasItem.h"
#include "CanvasScene.h"
#include "CanvasView.h"
#include "CurveItem.h"
#include "CustomShapeTemplates.h"
#include "IconFactory.h"
#include "MainWindow.h"
#include "PolygonItem.h"
#include "Settings.h"
#include "ShapeItem.h"
#include "ShapePathBuilder.h"
#include "SineItem.h"
#include "StraightLineItem.h"
#include "TextItem.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QStatusBar>
#include <QTimer>
#include <QVector>
#include <QtMath>

namespace {

bool selfTestMode()
{
    return qApp && qApp->property("storyboardSelfTest").toBool();
}

// 与 MainWindow 相同的报错出口：自检（离屏）时不许弹模态框。
void reportWarning(QWidget* parent, const QString& title, const QString& text)
{
    if (selfTestMode()) {
        qWarning("self-test suppressed dialog [%s]: %s", qUtf8Printable(title), qUtf8Printable(text));
        return;
    }
    QMessageBox::warning(parent, title, text);
}

} // namespace

MainWindowInsertController::MainWindowInsertController(MainWindow* window) : m_window(window) {}

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
    m_window->m_view->setFocus();
    m_window->statusBar()->showMessage(MainWindow::tr("Text item added"), 4000);

    // 文字正文现在由属性窗直接编辑；添加后立即打开属性窗，避免再维护独立
    // 的文字输入对话框。
    m_window->ShowItemProperties(item);
}

void MainWindowInsertController::InsertShape(ShapeType type, qreal triangleAngle1, qreal triangleAngle2,
                                             qreal cornerRatio, qreal parallelogramAngle)
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
    // 菱形预置：90° 平行四边形旋转 45° 后的外接对角线仍是基准边长，
    // 内容框按边长 = 对角线 / √2 取正方形（平行四边形默认框对菱形无意义）。
    if (type == ShapeType::PARALLELOGRAM && parallelogramAngle > 0.0) {
        const qreal diag = qMin<qreal>(128.0, qMin(canvas.width(), canvas.height()) * 0.72);
        size = QSizeF(diag / qSqrt(2.0), diag / qSqrt(2.0));
    }

    QPointF center = m_window->m_view->mapToScene(m_window->m_view->viewport()->rect().center());
    if (!canvas.contains(center)) {
        center = canvas.center();
    }
    const qreal cascade = (m_window->m_scene->CanvasItemCount() % 5) * 18.0;
    center += QPointF(cascade, cascade);
    center.setX(qBound(canvas.left() + size.width() / 2.0, center.x(), canvas.right() - size.width() / 2.0));
    center.setY(qBound(canvas.top() + size.height() / 2.0, center.y(), canvas.bottom() - size.height() / 2.0));
    const QPointF topLeft = center - QPointF(size.width() / 2.0, size.height() / 2.0);

    CanvasItem* item = nullptr;
    {
        auto* shape = CreateShapeItem(type);
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
        // 菱形预置项：锁内角并旋转 45°（数值只有 90° 一种，公式化以便扩展）。
        if (type == ShapeType::PARALLELOGRAM && parallelogramAngle > 0.0) {
            shape->SetParallelogramAngle(parallelogramAngle);
            shape->SetRotationAngle(45.0);
        }
        item = shape;
    }

    m_window->m_scene->clearSelection();
    m_window->m_scene->AddItemsWithUndo({item}, MainWindow::tr("Insert %1").arg(ShapeTypeName(type)));
    item->setSelected(true);
    m_window->m_view->setFocus();
    if (type == ShapeType::ELLIPSE) {
        m_window->statusBar()->showMessage(
            MainWindow::tr("Inserted a circle; in edit mode drag a handle to stretch it into an ellipse"), 5000);
    } else {
        m_window->statusBar()->showMessage(
            MainWindow::tr("Inserted %1; drag to move, drag handles to resize, color follows the current brush")
                .arg(ShapeTypeName(type)),
            5000);
    }
}

void MainWindow::InsertCurve(LineGlyph glyph, ArrowStyle startArrow, ArrowStyle endArrow)
{
    if (!m_view || !m_scene) {
        return;
    }
    const QSizeF size = glyph == LineGlyph::STRAIGHT ? QSizeF(128.0, 64.0)
                        : glyph == LineGlyph::ARC    ? QSizeF(128.0, 94.0)
                                                     : QSizeF(128.0, 40.0);
    const QRectF canvas = m_scene->CanvasRect();
    QPointF center = m_view->mapToScene(m_view->viewport()->rect().center());
    if (!canvas.contains(center)) {
        center = canvas.center();
    }
    const qreal cascade = (m_scene->CanvasItemCount() % 5) * 18.0;
    center += QPointF(cascade, cascade);
    center.setX(qBound(canvas.left() + size.width() / 2.0, center.x(), canvas.right() - size.width() / 2.0));
    center.setY(qBound(canvas.top() + size.height() / 2.0, center.y(), canvas.bottom() - size.height() / 2.0));
    const QPointF topLeft = center - QPointF(size.width() / 2.0, size.height() / 2.0);

    const QColor stroke = m_brushColor.alpha() < 8 ? QColor(QStringLiteral("#1E293B")) : m_brushColor;
    const qreal width = qMax<qreal>(0.5, m_brushWidth);

    LineItemAbstract* curve = nullptr;
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
        auto* face = CreateShapeItem(ShapeType::PARALLELOGRAM);
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

    QPointF center = m_window->m_view->mapToScene(m_window->m_view->viewport()->rect().center());
    if (!canvas.contains(center)) {
        center = canvas.center();
    }
    const qreal cascade = (m_window->m_scene->CanvasItemCount() % 5) * 18.0;
    center += QPointF(cascade, cascade);
    center.setX(qBound(canvas.left() + size.width() / 2.0, center.x(), canvas.right() - size.width() / 2.0));
    center.setY(qBound(canvas.top() + size.height() / 2.0, center.y(), canvas.bottom() - size.height() / 2.0));
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

        auto* top = CreateShapeItem(ShapeType::ELLIPSE);
        top->SetStyle(stroke, fill, width);
        top->SetFillStyle(fillStyle);
        top->SetStrokeStyle(m_window->m_strokeStyle);
        top->setPos(topLeft);
        top->SetContentSize(QSizeF(w, ry * 2.0));
        faces.append(top);
    }

    m_window->m_scene->clearSelection();
    m_window->m_scene->AddItemsWithUndo(faces, MainWindow::tr("Insert %1").arg(ShapeTypeName(type)));
    m_window->m_scene->AssignNewGroup(faces);
    for (CanvasItem* face : faces) {
        face->setSelected(true);
    }
    m_window->m_view->setFocus();
    m_window->statusBar()->showMessage(
        MainWindow::tr("Inserted %1 as a grouped set of faces; drag to move the whole shape").arg(ShapeTypeName(type)),
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

    QPointF center = m_window->m_view->mapToScene(m_window->m_view->viewport()->rect().center());
    if (!canvas.contains(center)) {
        center = canvas.center();
    }
    const qreal cascade = (m_window->m_scene->CanvasItemCount() % 5) * 18.0;
    center += QPointF(cascade, cascade);
    center.setX(qBound(canvas.left() + size.width() / 2.0, center.x(), canvas.right() - size.width() / 2.0));
    center.setY(qBound(canvas.top() + size.height() / 2.0, center.y(), canvas.bottom() - size.height() / 2.0));

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
        reportWarning(m_window, MainWindow::tr("Custom shape"),
                      MainWindow::tr("Could not read %1:\n%2").arg(QDir::toNativeSeparators(filePath), error));
        return;
    }

    auto* item = CreateShapeItem(ShapeType::RECTANGLE);
    item->SetCustomShape(customShape.name, customShape.unitPath);
    // 需求 16：与常见图形一致，按工具栏当前选择取样式（填充未选色跟随画笔色）。
    const QColor stroke =
        m_window->m_brushColor.alpha() < 8 ? QColor(QStringLiteral("#1E293B")) : m_window->m_brushColor;
    const QColor fill = m_window->m_fillColor.isValid() ? m_window->m_fillColor : stroke;
    const bool filled = m_window->m_fillStyle != FillStyle::NONE;
    item->SetStyle(stroke, filled ? fill : QColor(Qt::transparent), qMax<qreal>(0.5, m_window->m_brushWidth));
    item->SetFillStyle(filled ? m_window->m_fillStyle : FillStyle::NONE);
    item->SetStrokeStyle(m_window->m_strokeStyle);

    // 需求 7：模板按原始宽高比插入，长边固定 64（2 格）；画布装不下时才等比缩到 72%。
    const QRectF canvas = m_window->m_scene->CanvasRect();
    const qreal aspect = qMax<qreal>(0.05, customShape.aspectRatio);
    const qreal longSide = 64.0;
    QSizeF size = aspect >= 1.0 ? QSizeF(longSide, longSide / aspect) : QSizeF(longSide * aspect, longSide);
    const qreal fitScale = qMin<qreal>(1.0, qMin(canvas.width() * 0.72 / qMax<qreal>(1.0, size.width()),
                                                 canvas.height() * 0.72 / qMax<qreal>(1.0, size.height())));
    size *= fitScale;
    size.setWidth(qMax<qreal>(16.0, size.width()));
    size.setHeight(qMax<qreal>(16.0, size.height()));

    QPointF center = m_window->m_view->mapToScene(m_window->m_view->viewport()->rect().center());
    if (!canvas.contains(center)) {
        center = canvas.center();
    }
    const qreal cascade = (m_window->m_scene->CanvasItemCount() % 5) * 18.0;
    center += QPointF(cascade, cascade);
    center.setX(qBound(canvas.left() + size.width() / 2.0, center.x(), canvas.right() - size.width() / 2.0));
    center.setY(qBound(canvas.top() + size.height() / 2.0, center.y(), canvas.bottom() - size.height() / 2.0));

    item->setPos(center - QPointF(size.width() / 2.0, size.height() / 2.0));
    item->SetContentSize(size);

    m_window->m_scene->clearSelection();
    m_window->m_scene->AddItemsWithUndo({item}, MainWindow::tr("Insert custom shape “%1”").arg(customShape.name));
    item->setSelected(true);
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
