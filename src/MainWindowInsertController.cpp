#include "MainWindowInsertController.h"

#include "CanvasScene.h"
#include "CanvasView.h"
#include "CurveItem.h"
#include "CustomShapeTemplates.h"
#include "IconFactory.h"
#include "MainWindow.h"
#include "Settings.h"
#include "ShapeItem.h"
#include "ShapePathBuilder.h"
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

void MainWindowInsertController::insertTextAt(const QPointF& scenePos)
{
    auto* item = new TextItem();
    item->setTextFormat(MainWindow::tr("Text"), item->textFont(), m_window->m_brushColor);

    const QRectF canvas = m_window->m_scene->canvasRect();
    const QSizeF size = item->contentSize();
    qreal x = scenePos.x() - size.width() / 2.0;
    qreal y = scenePos.y() - size.height() / 2.0;
    x = qBound(canvas.left(), x, qMax(canvas.left(), canvas.right() - size.width()));
    y = qBound(canvas.top(), y, qMax(canvas.top(), canvas.bottom() - size.height()));
    item->setPos(QPointF(x, y));

    m_window->setToolMode(ToolMode::Edit);
    m_window->m_scene->addItemsWithUndo({item}, MainWindow::tr("Add text"));
    item->setSelected(true);
    m_window->m_view->setFocus();
    m_window->statusBar()->showMessage(MainWindow::tr("Text item added"), 4000);

    // 文字正文现在由属性窗直接编辑；添加后立即打开属性窗，避免再维护独立
    // 的文字输入对话框。
    m_window->showItemProperties(item);
}

void MainWindowInsertController::insertShape(ShapeType type, qreal triangleAngle1, qreal triangleAngle2,
                                             ArrowStyle startArrow, ArrowStyle endArrow, bool autoCorner)
{
    if (m_window->m_view->mode() != ToolMode::Edit) {
        m_window->setToolMode(ToolMode::Edit);
    }

    if (!m_window->m_scene->isActiveLayerWritable()) {
        m_window->statusBar()->showMessage(
            MainWindow::tr("The current layer is locked or hidden; shapes cannot be "
                           "inserted. Unlock or switch layers in the layer panel first."),
            5000);
        return;
    }

    // 需求 4/5：立方体与圆柱体插入即分组，走组合面图元的专用通道。
    if (type == ShapeType::Cube || type == ShapeType::Cylinder) {
        insertCompositeShape(type);
        return;
    }

    auto* item = new ShapeItem(type);
    // 需求 16：新图元按工具栏当前选择生成——描线颜色（画笔色未选过时与工具栏
    // 展示一致取深灰）、描线宽度不再钳制、填充颜色未选过时跟随画笔色。
    const QColor stroke =
        m_window->m_brushColor.alpha() < 8 ? QColor(QStringLiteral("#1E293B")) : m_window->m_brushColor;
    const bool filled = shapeTypeIsFilled(type) && m_window->m_fillStyle != FillStyle::None;
    const QColor fill = m_window->m_fillColor.isValid() ? m_window->m_fillColor : stroke;
    item->setStyle(stroke, filled ? fill : QColor(Qt::transparent), qMax<qreal>(0.5, m_window->m_brushWidth));
    item->setFillStyle(filled ? m_window->m_fillStyle : FillStyle::None);

    const QRectF canvas = m_window->m_scene->canvasRect();
    QSizeF size = defaultShapeSize(type);
    const qreal fitScale = qMin<qreal>(1.0, qMin(canvas.width() * 0.72 / qMax<qreal>(1.0, size.width()),
                                                 canvas.height() * 0.72 / qMax<qreal>(1.0, size.height())));
    size *= fitScale;

    QPointF center = m_window->m_view->mapToScene(m_window->m_view->viewport()->rect().center());
    if (!canvas.contains(center)) {
        center = canvas.center();
    }
    const qreal cascade = (m_window->m_scene->canvasItemCount() % 5) * 18.0;
    center += QPointF(cascade, cascade);
    center.setX(qBound(canvas.left() + size.width() / 2.0, center.x(), canvas.right() - size.width() / 2.0));
    center.setY(qBound(canvas.top() + size.height() / 2.0, center.y(), canvas.bottom() - size.height() / 2.0));

    item->setPos(center - QPointF(size.width() / 2.0, size.height() / 2.0));
    item->setContentSize(size);
    // 三角形预置项：底边保持默认 64，按预置角度重算高度（角度是真相）。
    if (type == ShapeType::Triangle && triangleAngle1 > 0.0 && triangleAngle2 > 0.0) {
        item->setTriangleAngles(triangleAngle1, triangleAngle2);
    }
    // 直线箭头预置项：复用直线，默认末端开口箭头。
    if (type == ShapeType::Line && (arrowStyleIsHead(startArrow) || arrowStyleIsHead(endArrow))) {
        item->setArrowStyles(startArrow, endArrow);
    }
    // 圆角矩形预置项：矩形按短边 16% 自动圆角（<0 自动态），尺寸变化时比例保持。
    if (type == ShapeType::Rectangle && autoCorner) {
        item->setAutoCorner(true);
    }

    m_window->m_scene->clearSelection();
    m_window->m_scene->addItemsWithUndo({item}, MainWindow::tr("Insert %1").arg(shapeTypeName(type)));
    item->setSelected(true);
    m_window->m_view->setFocus();
    if (type == ShapeType::Ellipse) {
        m_window->statusBar()->showMessage(
            MainWindow::tr("Inserted a circle; in edit mode drag a handle to stretch it into an ellipse"), 5000);
    } else {
        m_window->statusBar()->showMessage(
            MainWindow::tr("Inserted %1; drag to move, drag handles to resize, color follows the current brush")
                .arg(shapeTypeName(type)),
            5000);
    }
}

void MainWindowInsertController::insertCompositeShape(ShapeType type)
{
    // 需求 16：与常见图形同一套工具栏取值规则（描线宽度不再钳制、填充跟随画笔色）。
    const QColor stroke =
        m_window->m_brushColor.alpha() < 8 ? QColor(QStringLiteral("#1E293B")) : m_window->m_brushColor;
    const qreal width = qMax<qreal>(0.5, m_window->m_brushWidth);
    const bool filled = m_window->m_fillStyle != FillStyle::None;
    const QColor fill = m_window->m_fillColor.isValid() ? m_window->m_fillColor : stroke;
    const FillStyle fillStyle = filled ? m_window->m_fillStyle : FillStyle::None;

    auto makeFace = [&stroke, &fill, fillStyle, width](const QString& name, const QPainterPath& unitPath,
                                                       const QPointF& topLeft, const QSizeF& size) {
        auto* face = new ShapeItem(ShapeType::Rectangle);
        face->setCustomShape(name, unitPath);
        face->setStyle(stroke, fill, width);
        face->setFillStyle(fillStyle);
        face->setPos(topLeft);
        face->setContentSize(size);
        return face;
    };

    const QRectF canvas = m_window->m_scene->canvasRect();
    QSizeF size = defaultShapeSize(type);
    const qreal fitScale = qMin<qreal>(1.0, qMin(canvas.width() * 0.72 / qMax<qreal>(1.0, size.width()),
                                                 canvas.height() * 0.72 / qMax<qreal>(1.0, size.height())));
    size *= fitScale;

    QPointF center = m_window->m_view->mapToScene(m_window->m_view->viewport()->rect().center());
    if (!canvas.contains(center)) {
        center = canvas.center();
    }
    const qreal cascade = (m_window->m_scene->canvasItemCount() % 5) * 18.0;
    center += QPointF(cascade, cascade);
    center.setX(qBound(canvas.left() + size.width() / 2.0, center.x(), canvas.right() - size.width() / 2.0));
    center.setY(qBound(canvas.top() + size.height() / 2.0, center.y(), canvas.bottom() - size.height() / 2.0));
    const QPointF topLeft = center - QPointF(size.width() / 2.0, size.height() / 2.0);

    QList<CanvasItem*> faces;
    if (type == ShapeType::Cube) {
        // 等轴测立方体拆成三个菱形面：顶面、左面、右面，轮廓沿用原 cubePath 的六边形切分。
        const qreal w = size.width();
        const qreal h = size.height();
        QPainterPath top;
        top.moveTo(0.5, 0.0);
        top.lineTo(1.0, 0.5);
        top.lineTo(0.5, 1.0);
        top.lineTo(0.0, 0.5);
        top.closeSubpath();
        QPainterPath leftFace;
        leftFace.moveTo(0.0, 0.0);
        leftFace.lineTo(1.0, 1.0 / 3.0);
        leftFace.lineTo(1.0, 1.0);
        leftFace.lineTo(0.0, 2.0 / 3.0);
        leftFace.closeSubpath();
        QPainterPath rightFace;
        rightFace.moveTo(0.0, 1.0 / 3.0);
        rightFace.lineTo(1.0, 0.0);
        rightFace.lineTo(1.0, 2.0 / 3.0);
        rightFace.lineTo(0.0, 1.0);
        rightFace.closeSubpath();
        faces.append(makeFace(MainWindow::tr("Cube top"), top, topLeft, QSizeF(w, h * 0.5)));
        faces.append(makeFace(MainWindow::tr("Cube left"), leftFace, topLeft + QPointF(0.0, h * 0.25),
                              QSizeF(w * 0.5, h * 0.75)));
        faces.append(makeFace(MainWindow::tr("Cube right"), rightFace, topLeft + QPointF(w * 0.5, h * 0.25),
                              QSizeF(w * 0.5, h * 0.75)));
    } else {
        // 需求 18：圆柱体 = 柱身（底缘带下半椭圆弧）+ 顶椭圆，顶椭圆叠在柱身上层。
        const qreal w = size.width();
        const qreal h = size.height();
        const qreal ry = qMin(h * 0.16, w * 0.30);

        const qreal bodyH = h - ry;
        QPainterPath body;
        const qreal cy = (h - 2.0 * ry) / bodyH;
        const qreal ryUnit = ry / bodyH;
        body.moveTo(0.0, 0.0);
        body.lineTo(1.0, 0.0);
        body.lineTo(1.0, cy);
        body.arcTo(QRectF(0.0, cy - ryUnit, 1.0, ryUnit * 2.0), 0.0, -180.0);
        body.closeSubpath();
        faces.append(makeFace(MainWindow::tr("Cylinder body"), body, topLeft + QPointF(0.0, ry), QSizeF(w, bodyH)));

        auto* top = new ShapeItem(ShapeType::Ellipse);
        top->setStyle(stroke, fill, width);
        top->setFillStyle(fillStyle);
        top->setPos(topLeft);
        top->setContentSize(QSizeF(w, ry * 2.0));
        faces.append(top);
    }

    m_window->m_scene->clearSelection();
    m_window->m_scene->addItemsWithUndo(faces, MainWindow::tr("Insert %1").arg(shapeTypeName(type)));
    m_window->m_scene->assignNewGroup(faces);
    for (CanvasItem* face : faces) {
        face->setSelected(true);
    }
    m_window->m_view->setFocus();
    m_window->statusBar()->showMessage(
        MainWindow::tr("Inserted %1 as a grouped set of faces; drag to move the whole shape").arg(shapeTypeName(type)),
        5000);
}

void MainWindowInsertController::insertBrace(bool left)
{
    if (m_window->m_view->mode() != ToolMode::Edit) {
        m_window->setToolMode(ToolMode::Edit);
    }

    if (!m_window->m_scene->isActiveLayerWritable()) {
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
    const QRectF canvas = m_window->m_scene->canvasRect();
    QSizeF size(42.0, 128.0);
    const qreal fitScale = qMin<qreal>(1.0, qMin(canvas.width() * 0.72 / qMax<qreal>(1.0, size.width()),
                                                 canvas.height() * 0.72 / qMax<qreal>(1.0, size.height())));
    size *= fitScale;

    QPointF center = m_window->m_view->mapToScene(m_window->m_view->viewport()->rect().center());
    if (!canvas.contains(center)) {
        center = canvas.center();
    }
    const qreal cascade = (m_window->m_scene->canvasItemCount() % 5) * 18.0;
    center += QPointF(cascade, cascade);
    center.setX(qBound(canvas.left() + size.width() / 2.0, center.x(), canvas.right() - size.width() / 2.0));
    center.setY(qBound(canvas.top() + size.height() / 2.0, center.y(), canvas.bottom() - size.height() / 2.0));

    const QPointF topLeft = center - QPointF(size.width() / 2.0, size.height() / 2.0);
    const QPainterPath braceShape = ShapePathBuilder::bracePath(QRectF(QPointF(0.0, 0.0), size));
    constexpr int sampleCount = 64;
    QVector<QPointF> points;
    points.reserve(sampleCount + 1);
    for (int i = 0; i <= sampleCount; ++i) {
        points.append(topLeft + braceShape.pointAtPercent(qreal(i) / qreal(sampleCount)));
    }

    auto* item = new CurveItem();
    item->setStrokePoints(points, stroke, width, 0.0, true);
    item->setSource(CurveSource::Brace);
    // 右括号 = 左括号旋转 180°（旋转绕内容中心，几何不复制第二套）。
    if (!left) {
        item->setRotationAngle(180.0);
    }

    m_window->m_scene->clearSelection();
    m_window->m_scene->addItemsWithUndo(
        {item}, MainWindow::tr("Insert %1").arg(left ? MainWindow::tr("Left brace") : MainWindow::tr("Right brace")));
    item->setSelected(true);
    m_window->m_view->setFocus();
    m_window->statusBar()->showMessage(MainWindow::tr("Inserted %1 as a curve; drag to move, drag handles to resize")
                                           .arg(left ? MainWindow::tr("Left brace") : MainWindow::tr("Right brace")),
                                       5000);
}

void MainWindowInsertController::insertCustomShape(const QString& filePath)
{
    if (m_window->m_view->mode() != ToolMode::Edit) {
        m_window->setToolMode(ToolMode::Edit);
    }

    if (!m_window->m_scene->isActiveLayerWritable()) {
        m_window->statusBar()->showMessage(
            MainWindow::tr("The current layer is locked or hidden; shapes cannot be "
                           "inserted. Unlock or switch layers in the layer panel first."),
            5000);
        return;
    }

    CustomShapeTemplate customShape;
    QString error;
    if (!loadCustomShapeTemplate(filePath, &customShape, &error)) {
        reportWarning(m_window, MainWindow::tr("Custom shape"),
                      MainWindow::tr("Could not read %1:\n%2").arg(QDir::toNativeSeparators(filePath), error));
        return;
    }

    auto* item = new ShapeItem();
    item->setCustomShape(customShape.name, customShape.unitPath);
    // 需求 16：与常见图形一致，按工具栏当前选择取样式（填充未选色跟随画笔色）。
    const QColor stroke =
        m_window->m_brushColor.alpha() < 8 ? QColor(QStringLiteral("#1E293B")) : m_window->m_brushColor;
    const QColor fill = m_window->m_fillColor.isValid() ? m_window->m_fillColor : stroke;
    const bool filled = m_window->m_fillStyle != FillStyle::None;
    item->setStyle(stroke, filled ? fill : QColor(Qt::transparent), qMax<qreal>(0.5, m_window->m_brushWidth));
    item->setFillStyle(filled ? m_window->m_fillStyle : FillStyle::None);

    // 需求 7：模板按原始宽高比插入，长边固定 64（2 格）；画布装不下时才等比缩到 72%。
    const QRectF canvas = m_window->m_scene->canvasRect();
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
    const qreal cascade = (m_window->m_scene->canvasItemCount() % 5) * 18.0;
    center += QPointF(cascade, cascade);
    center.setX(qBound(canvas.left() + size.width() / 2.0, center.x(), canvas.right() - size.width() / 2.0));
    center.setY(qBound(canvas.top() + size.height() / 2.0, center.y(), canvas.bottom() - size.height() / 2.0));

    item->setPos(center - QPointF(size.width() / 2.0, size.height() / 2.0));
    item->setContentSize(size);

    m_window->m_scene->clearSelection();
    m_window->m_scene->addItemsWithUndo({item}, MainWindow::tr("Insert custom shape “%1”").arg(customShape.name));
    item->setSelected(true);
    m_window->m_view->setFocus();
    m_window->statusBar()->showMessage(
        MainWindow::tr("Inserted custom shape “%1” from the shapes folder").arg(customShape.name), 4000);
}

void MainWindowInsertController::openShapesFolder()
{
    const AppSettings::ShapesFolderOpenResult result = AppSettings::openShapesFolder();
    if (!result.opened) {
        m_window->statusBar()->showMessage(AppSettings::shapesFolderFailureText(result), 6000);
    }
}

void MainWindowInsertController::updateCustomShapeMenu()
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
        QTimer::singleShot(0, m_window, [this]() { m_window->openCustomShapePanel(nullptr); });
    });
    m_customShapeTrigger = trigger;
    m_window->m_customShapeMenu->addAction(trigger);
    if (m_window->m_shapeMenu) {
        m_window->m_shapeMenu->addAction(trigger);
    }
}
