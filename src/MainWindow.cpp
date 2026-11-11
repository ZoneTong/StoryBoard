#include "MainWindow.h"

#include "CanvasScene.h"
#include "CanvasView.h"
#include "Commands.h"
#include "ExportDialog.h"
#include "I18n.h"
#include "IconFactory.h"
#include "ImageItem.h"
#include "ItemPropertiesDialog.h"
#include "LayerPanel.h"
#include "LineArtConverter.h"
#include "SettingsDialog.h"
#include "ShapeItem.h"
#include "TextItem.h"

#include <QAbstractButton>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPixmap>
#include <QPolygonF>
#include <QPushButton>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>
#include <QUrl>
#include <QtMath>

namespace {

QString imageFilter()
{
    return uiText(QStringLiteral("Image files (*.png *.jpg *.jpeg *.bmp *.gif);;All files (*.*)"));
}

QString canvasFilter()
{
    return uiText(QStringLiteral("StoryBoard canvases (*.sbcanvas);;All files (*.*)"));
}

QAction* makeAction(QObject* parent, IconId icon, const QString& text, const QKeySequence& shortcut = QKeySequence(),
                    const QString& tip = QString())
{
    auto* action = new QAction(makeIcon(icon), text, parent);
    if (!shortcut.isEmpty()) {
        action->setShortcut(shortcut);
    }
    action->setToolTip(tip.isEmpty() ? text : tip);
    action->setStatusTip(tip.isEmpty() ? text : tip);
    return action;
}

QColor fillIconColor(const QColor& color)
{
    QColor fill = color.isValid() ? color : QColor(96, 132, 255);
    if (fill.alpha() < 150) {
        fill.setAlpha(150);
    }
    return fill;
}

QIcon makeFillStyleIcon(FillStyle style, const QColor& color, int logicalSize)
{
    constexpr qreal scale = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * scale), static_cast<int>(logicalSize * scale));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(scale);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(logicalSize / 24.0, logicalSize / 24.0);

    const QRectF box(3.0, 4.0, 18.0, 16.0);
    const QColor ink(58, 64, 80);
    const QColor fill = fillIconColor(color);

    QPainterPath clipPath;
    clipPath.addRoundedRect(box.adjusted(1.0, 1.0, -1.0, -1.0), 2.5, 2.5);
    painter.save();
    painter.setClipPath(clipPath);

    if (style != FillStyle::None) {
        QColor background = fill;
        background.setAlpha(style == FillStyle::Solid ? 255 : 70);
        painter.fillRect(box, background);
    }

    switch (style) {
    case FillStyle::None: {
        painter.setPen(QPen(ink, 2.2, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(5.5, 18.5), QPointF(18.5, 5.5));
        break;
    }
    case FillStyle::Solid:
        break;
    case FillStyle::Hachure:
    case FillStyle::ReverseHachure:
    case FillStyle::CrossHatch: {
        painter.setPen(QPen(ink, 1.5));
        const bool forward = style != FillStyle::ReverseHachure;
        const bool backward = style != FillStyle::Hachure;
        for (qreal offset = -box.height(); offset < box.width() + box.height(); offset += 4.2) {
            if (forward) {
                painter.drawLine(QPointF(box.left() + offset, box.bottom()),
                                 QPointF(box.left() + offset + box.height(), box.top()));
            }
            if (backward) {
                painter.drawLine(QPointF(box.left() + offset, box.top()),
                                 QPointF(box.left() + offset + box.height(), box.bottom()));
            }
        }
        break;
    }
    case FillStyle::GridHatch: {
        painter.setPen(QPen(ink, 1.4));
        for (qreal x = box.left() + 4.0; x < box.right(); x += 4.5) {
            painter.drawLine(QPointF(x, box.top()), QPointF(x, box.bottom()));
        }
        for (qreal y = box.top() + 4.0; y < box.bottom(); y += 4.5) {
            painter.drawLine(QPointF(box.left(), y), QPointF(box.right(), y));
        }
        break;
    }
    case FillStyle::Honeycomb: {
        painter.setPen(QPen(ink, 1.2));
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                const QPointF center(box.left() + 4.0 + column * 5.5 + (row % 2 ? 2.75 : 0.0),
                                     box.top() + 3.5 + row * 5.0);
                QPolygonF hexagon;
                for (int i = 0; i < 6; ++i) {
                    const qreal angle = qDegreesToRadians(60.0 * i - 30.0);
                    hexagon << center + QPointF(2.8 * qCos(angle), 2.8 * qSin(angle));
                }
                painter.drawPolygon(hexagon);
            }
        }
        break;
    }
    }

    painter.restore();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(ink, 1.8));
    painter.drawRoundedRect(box, 3.0, 3.0);
    painter.end();

    return QIcon(pixmap);
}

QIcon makeFillColorIcon(const QColor& color, int logicalSize)
{
    constexpr qreal scale = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * scale), static_cast<int>(logicalSize * scale));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(scale);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(logicalSize / 24.0, logicalSize / 24.0);

    const QColor ink(58, 64, 80);
    const QColor fill = fillIconColor(color);

    QPainterPath bucket;
    bucket.moveTo(5.0, 9.0);
    bucket.lineTo(8.0, 20.0);
    bucket.lineTo(17.0, 20.0);
    bucket.lineTo(20.0, 9.0);
    bucket.closeSubpath();

    painter.setPen(QPen(ink, 1.6));
    painter.setBrush(fill);
    painter.drawPath(bucket);

    QPainterPath handle;
    handle.moveTo(7.0, 9.0);
    handle.cubicTo(7.0, 2.5, 18.0, 2.5, 18.0, 9.0);
    painter.setPen(QPen(ink, 1.6));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(handle);

    painter.setPen(QPen(ink, 1.9));
    painter.drawLine(QPointF(4.0, 9.0), QPointF(21.0, 9.0));
    painter.setPen(QPen(QColor(255, 255, 255, 180), 1.0));
    painter.drawLine(QPointF(9.5, 12.5), QPointF(11.0, 18.0));
    painter.end();

    return QIcon(pixmap);
}

QIcon makeStrokeColorIcon(const QColor& color, int logicalSize)
{
    constexpr qreal scale = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * scale), static_cast<int>(logicalSize * scale));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(scale);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(logicalSize / 24.0, logicalSize / 24.0);

    const QColor ink(58, 64, 80);
    const QColor stroke = fillIconColor(color);
    const QRectF box(4.0, 4.0, 16.0, 16.0);

    painter.setPen(QPen(ink, 1.0));
    painter.setBrush(Qt::white);
    painter.drawRoundedRect(box, 3.0, 3.0);
    painter.setPen(QPen(stroke, 3.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(box.adjusted(2.0, 2.0, -2.0, -2.0), 2.2, 2.2);

    QPainterPath nib;
    nib.moveTo(14.5, 1.0);
    nib.lineTo(22.5, 9.0);
    nib.lineTo(18.5, 10.5);
    nib.lineTo(13.0, 5.0);
    nib.closeSubpath();
    painter.setPen(QPen(ink, 0.8));
    painter.setBrush(ink);
    painter.drawPath(nib);
    painter.end();

    return QIcon(pixmap);
}

bool hasImageSuffix(const QString& path)
{
    static const QStringList suffixes = {QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
                                         QStringLiteral("bmp"), QStringLiteral("gif")};
    return suffixes.contains(QFileInfo(path).suffix().toLower());
}

bool validateCanvasRoot(const QJsonObject& root, QString* error)
{
    if (root.value(QStringLiteral("format")).toString() != QStringLiteral("StoryBoardCanvas")) {
        if (error) {
            *error = uiText(QStringLiteral("This is not a valid StoryBoard canvas file."));
        }
        return false;
    }

    const QJsonObject canvas = root.value(QStringLiteral("canvas")).toObject();
    if (canvas.value(QStringLiteral("width")).toInt() <= 0 || canvas.value(QStringLiteral("height")).toInt() <= 0) {
        if (error) {
            *error = uiText(QStringLiteral("The canvas size in the canvas file is invalid."));
        }
        return false;
    }

    const QJsonArray items = root.value(QStringLiteral("items")).toArray();
    for (const QJsonValue& value : items) {
        const QString kind = value.toObject().value(QStringLiteral("type")).toString();
        if (kind != QStringLiteral("stroke") && kind != QStringLiteral("image") && kind != QStringLiteral("text") &&
            kind != QStringLiteral("shape")) {
            if (error) {
                *error = uiText(QStringLiteral("The canvas contains an unknown item type: %1")).arg(kind);
            }
            return false;
        }
    }
    return true;
}

QLabel* makeBadge(const QString& objectName, QWidget* parent)
{
    auto* label = new QLabel(parent);
    label->setObjectName(objectName);
    label->setAlignment(Qt::AlignCenter);
    return label;
}

QList<int> brushWidthPresets()
{
    return {1, 2, 3, 4, 6, 8, 10, 12, 16, 20, 24, 32, 40, 48, 64};
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));
    setMinimumSize(1080, 700);
    setAcceptDrops(true);

    m_settings.load();
    m_brushColor = m_settings.brushColor;
    m_brushWidth = m_settings.brushWidth;

    m_scene = new CanvasScene(this);
    m_scene->applyCanvasSize(m_settings.canvasSize);
    m_scene->setShowGrid(m_settings.showGrid);

    m_undo = new QUndoStack(this);
    m_undo->setUndoLimit(300);
    m_scene->setUndoStack(m_undo);

    m_view = new CanvasView(m_scene, this);
    m_view->setBrushColor(m_brushColor);
    m_view->setBrushWidth(m_brushWidth);
    m_view->setStrokeSmoothing(m_settings.strokeSmoothing);
    m_view->setCanvasColor(m_settings.canvasColor);
    setCentralWidget(m_view);

    createActions();
    createShapeMenu();
    createLayerDock();
    createMenus();
    createToolBars();
    createStatusBar();
    connectSignals();

    if (!m_settings.windowGeometry.isEmpty()) {
        restoreGeometry(m_settings.windowGeometry);
    } else {
        resize(1440, 900);
    }

    updateModeUi();
    updateTitle();
    updateStatusBar();
    updateActions();
    updateBackgroundModeActions();

    QTimer::singleShot(0, this, [this]() {
        if (m_view) {
            m_view->fitAll();
        }
    });
}

MainWindow::~MainWindow()
{
    // QWidget 析构会先删子对象；子 CanvasScene/CanvasView 在 clear() 时仍可能发
    // 信号回来。此时 MainWindow 的派生类析构体已经跑完，槽函数里再访问成员
    // 就是未定义行为（Debug 版会报 Q_ASSERT 并直接中止）。先统一断开所有
    // 子对象到本窗口的连接，再清理撤销栈和视图持有的指针。
    const QObjectList children = findChildren<QObject*>();
    for (QObject* child : children) {
        QObject::disconnect(child, nullptr, this, nullptr);
    }

    // QObject deletes the scene child before the undo stack in some orders.
    // Clear commands while the scene is still alive, and make sure the view
    // no longer points at the scene during teardown.
    if (m_undo) {
        QObject::disconnect(m_undo, nullptr, this, nullptr);
        m_undo->clear();
    }
    if (m_view) {
        m_view->setScene(nullptr);
    }
    if (m_scene) {
        m_scene->setUndoStack(nullptr);
    }
}

void MainWindow::createActions()
{
    m_newAct =
        makeAction(this, IconId::New, uiText(QStringLiteral("New canvas")), QKeySequence(QStringLiteral("Ctrl+N")),
                   uiText(QStringLiteral("Create an empty canvas (Ctrl+N)")));
    m_openAct =
        makeAction(this, IconId::Open, uiText(QStringLiteral("Open canvas…")), QKeySequence(QStringLiteral("Ctrl+O")),
                   uiText(QStringLiteral("Open a StoryBoard canvas (Ctrl+O)")));
    m_saveAct =
        makeAction(this, IconId::Save, uiText(QStringLiteral("Save canvas")), QKeySequence(QStringLiteral("Ctrl+S")),
                   uiText(QStringLiteral("Save the current canvas (Ctrl+S)")));
    m_saveAsAct = makeAction(this, IconId::Save, uiText(QStringLiteral("Save canvas as…")),
                             QKeySequence(QStringLiteral("Ctrl+Shift+S")),
                             uiText(QStringLiteral("Save the current canvas to a new file (Ctrl+Shift+S)")));
    m_importAct = makeAction(this, IconId::Image, uiText(QStringLiteral("Insert images…")),
                             QKeySequence(QStringLiteral("Ctrl+I")),
                             uiText(QStringLiteral("Insert images as movable, resizable items (Ctrl+I)")));
    m_lineArtAct = makeAction(
        this, IconId::LineArt, uiText(QStringLiteral("Import outline image")), QKeySequence(),
        uiText(QStringLiteral("Trace the color-block outlines of an image into stroke items (line width 1)")));
    m_exportAct = makeAction(this, IconId::Export, uiText(QStringLiteral("Export canvas…")),
                             QKeySequence(QStringLiteral("Ctrl+E")),
                             uiText(QStringLiteral("Export as PNG / JPEG / PDF (Ctrl+E)")));
    m_quitAct = new QAction(uiText(QStringLiteral("Quit")), this);
    m_quitAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Q")));
    m_quitAct->setStatusTip(uiText(QStringLiteral("Quit StoryBoard")));

    m_undoAct = makeAction(this, IconId::Undo, uiText(QStringLiteral("Undo")), QKeySequence::Undo,
                           uiText(QStringLiteral("Undo the last step (Ctrl+Z)")));
    m_undoAct->setEnabled(false);
    m_redoAct = makeAction(this, IconId::Redo, uiText(QStringLiteral("Redo")), QKeySequence(QStringLiteral("Ctrl+Y")),
                           uiText(QStringLiteral("Redo (Ctrl+Y / Ctrl+Shift+Z)")));
    m_redoAct->setShortcuts({QKeySequence(QStringLiteral("Ctrl+Y")), QKeySequence(QStringLiteral("Ctrl+Shift+Z"))});
    m_redoAct->setEnabled(false);

    m_deleteAct = makeAction(this, IconId::Delete, uiText(QStringLiteral("Delete selected items")),
                             QKeySequence::Delete, uiText(QStringLiteral("Delete the selected items (Delete)")));
    m_duplicateAct = new QAction(uiText(QStringLiteral("Duplicate selected items")), this);
    m_duplicateAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));
    m_duplicateAct->setStatusTip(uiText(QStringLiteral("Duplicate the selected items with a small offset (Ctrl+D)")));
    m_duplicateAct->setEnabled(false);
    connect(m_duplicateAct, &QAction::triggered, this, [this]() {
        if (!m_scene) {
            return;
        }
        const int duplicated = m_scene->duplicateSelectedItems();
        if (duplicated > 0) {
            statusBar()->showMessage(uiText(QStringLiteral("Duplicated %1 item(s)")).arg(duplicated), 3000);
        }
    });
    m_groupAct = new QAction(uiText(QStringLiteral("Group selected items")), this);
    m_groupAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+G")));
    m_groupAct->setStatusTip(uiText(QStringLiteral("Group the selected items so they move together (Ctrl+G)")));
    m_groupAct->setEnabled(false);
    connect(m_groupAct, &QAction::triggered, this, [this]() {
        if (!m_scene) {
            return;
        }
        const int grouped = m_scene->groupSelectedItems();
        if (grouped > 0) {
            statusBar()->showMessage(uiText(QStringLiteral("Grouped %1 item(s)")).arg(grouped), 3000);
        }
    });
    m_ungroupAct = new QAction(uiText(QStringLiteral("Ungroup selected items")), this);
    m_ungroupAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+G")));
    m_ungroupAct->setStatusTip(uiText(QStringLiteral("Ungroup the selected group(s) (Ctrl+Shift+G)")));
    m_ungroupAct->setEnabled(false);
    connect(m_ungroupAct, &QAction::triggered, this, [this]() {
        if (!m_scene) {
            return;
        }
        const int ungrouped = m_scene->ungroupSelectedItems();
        if (ungrouped > 0) {
            statusBar()->showMessage(uiText(QStringLiteral("Ungrouped %1 item(s)")).arg(ungrouped), 3000);
        }
    });
    m_smoothAct = makeAction(this, IconId::Smooth, uiText(QStringLiteral("Smooth strokes")),
                             QKeySequence(QStringLiteral("Ctrl+Shift+M")),
                             uiText(QStringLiteral("Smooth strokes once using the current tolerance (Ctrl+Shift+M)")));
    m_smoothAct->setEnabled(false);
    connect(m_smoothAct, &QAction::triggered, this, [this]() {
        if (!m_scene || !m_view) {
            return;
        }
        const int smoothed = m_scene->smoothSelectedStrokes(m_settings.strokeSmoothing);
        if (smoothed > 0) {
            statusBar()->showMessage(uiText(QStringLiteral("Smoothed %1 stroke(s)")).arg(smoothed), 3000);
        } else {
            statusBar()->showMessage(uiText(QStringLiteral("No strokes need smoothing")), 3000);
        }
    });
    m_selectAllAct = new QAction(uiText(QStringLiteral("Select all items")), this);
    m_selectAllAct->setShortcut(QKeySequence::SelectAll);
    m_selectAllAct->setStatusTip(uiText(QStringLiteral("Select all items (Ctrl+A)")));
    m_itemPropsAct = makeAction(this, IconId::Settings, uiText(QStringLiteral("Item properties…")),
                                QKeySequence(QStringLiteral("Q")),
                                uiText(QStringLiteral("Show the selected item's properties (Q)")));
    m_rotateCwAct = makeAction(this, IconId::RotateCw, uiText(QStringLiteral("Rotate 45° clockwise")),
                               QKeySequence(QStringLiteral("Ctrl+R")),
                               uiText(QStringLiteral("Rotate selected items 45° clockwise (Ctrl+R)")));
    m_rotateCcwAct = makeAction(this, IconId::RotateCcw, uiText(QStringLiteral("Rotate 45° counterclockwise")),
                                QKeySequence(QStringLiteral("Ctrl+Shift+R")),
                                uiText(QStringLiteral("Rotate selected items 45° counterclockwise (Ctrl+Shift+R)")));

    m_viewModeAct =
        makeAction(this, IconId::View, uiText(QStringLiteral("View mode")), QKeySequence(QStringLiteral("V")),
                   uiText(QStringLiteral("View mode: overview, pan and zoom (V)")));
    m_brushModeAct =
        makeAction(this, IconId::Brush, uiText(QStringLiteral("Brush mode")), QKeySequence(QStringLiteral("B")),
                   uiText(QStringLiteral("Brush mode: hold the left button to draw freely (B)")));
    m_editModeAct =
        makeAction(this, IconId::Edit, uiText(QStringLiteral("Edit mode")), QKeySequence(QStringLiteral("E")),
                   uiText(QStringLiteral("Edit mode: select, move and resize items (E)")));
    m_textModeAct =
        makeAction(this, IconId::Text, uiText(QStringLiteral("Text mode")), QKeySequence(QStringLiteral("T")),
                   uiText(QStringLiteral("Text mode: click the canvas to place a text item (T)")));
    m_viewModeAct->setCheckable(true);
    m_brushModeAct->setCheckable(true);
    m_editModeAct->setCheckable(true);
    m_textModeAct->setCheckable(true);
    m_viewModeAct->setChecked(true);

    m_modeGroup = new QActionGroup(this);
    m_modeGroup->setExclusive(true);
    m_modeGroup->addAction(m_viewModeAct);
    m_modeGroup->addAction(m_editModeAct);
    m_modeGroup->addAction(m_brushModeAct);
    m_modeGroup->addAction(m_textModeAct);
    m_viewModeAct->setData(static_cast<int>(ToolMode::View));
    m_brushModeAct->setData(static_cast<int>(ToolMode::Brush));
    m_editModeAct->setData(static_cast<int>(ToolMode::Edit));
    m_textModeAct->setData(static_cast<int>(ToolMode::Text));

    m_fitAct =
        makeAction(this, IconId::Fit, uiText(QStringLiteral("Back to overview")),
                   QKeySequence(QStringLiteral("Ctrl+0")), uiText(QStringLiteral("Fit all items (Ctrl+0 / Home)")));
    m_fitAct->setShortcuts({QKeySequence(QStringLiteral("Ctrl+0")), QKeySequence(Qt::Key_Home)});
    m_zoomInAct = makeAction(this, IconId::ZoomIn, uiText(QStringLiteral("Zoom in")),
                             QKeySequence(QStringLiteral("Ctrl+=")), uiText(QStringLiteral("Zoom in (Ctrl+=)")));
    m_zoomInAct->setShortcuts({QKeySequence(QStringLiteral("Ctrl+=")), QKeySequence(Qt::CTRL | Qt::Key_Plus)});
    m_zoomOutAct = makeAction(this, IconId::ZoomOut, uiText(QStringLiteral("Zoom out")),
                              QKeySequence(QStringLiteral("Ctrl+-")), uiText(QStringLiteral("Zoom out (Ctrl+-)")));
    m_actualSizeAct =
        makeAction(this, IconId::ZoomIn, uiText(QStringLiteral("Actual size")), QKeySequence(QStringLiteral("Ctrl+1")),
                   uiText(QStringLiteral("Show at 100% (Ctrl+1)")));

    m_replaceBgAct = makeAction(this, IconId::Background, uiText(QStringLiteral("Replace background image…")),
                                QKeySequence(), uiText(QStringLiteral("Choose an image as the canvas background")));
    m_bgColorAct = makeAction(this, IconId::Palette, uiText(QStringLiteral("Set background color…")), QKeySequence(),
                              uiText(QStringLiteral("Use a solid color as the canvas background")));
    m_clearBgAct = makeAction(this, IconId::Delete, uiText(QStringLiteral("Clear background")), QKeySequence(),
                              uiText(QStringLiteral("Restore the white background")));

    m_bgStretchAct = new QAction(uiText(QStringLiteral("Stretch to fill")), this);
    m_bgFitAct = new QAction(uiText(QStringLiteral("Fit entirely")), this);
    m_bgTileAct = new QAction(uiText(QStringLiteral("Tile")), this);
    m_bgStretchAct->setCheckable(true);
    m_bgFitAct->setCheckable(true);
    m_bgTileAct->setCheckable(true);
    m_backgroundModeGroup = new QActionGroup(this);
    m_backgroundModeGroup->setExclusive(true);
    m_backgroundModeGroup->addAction(m_bgStretchAct);
    m_backgroundModeGroup->addAction(m_bgFitAct);
    m_backgroundModeGroup->addAction(m_bgTileAct);

    m_settingsAct =
        makeAction(this, IconId::Settings, uiText(QStringLiteral("Settings…")), QKeySequence(QStringLiteral("Ctrl+,")),
                   uiText(QStringLiteral("Open board settings (Ctrl+,)")));

    m_aboutAct =
        new QAction(QIcon(QStringLiteral(":/icons/app.png")), uiText(QStringLiteral("About StoryBoard")), this);
    m_aboutQtAct = new QAction(uiText(QStringLiteral("About Qt")), this);

    connect(m_newAct, &QAction::triggered, this, &MainWindow::newCanvas);
    connect(m_openAct, &QAction::triggered, this, &MainWindow::openCanvasDialog);
    connect(m_saveAct, &QAction::triggered, this, [this]() { saveCanvas(); });
    connect(m_saveAsAct, &QAction::triggered, this, [this]() { saveCanvasAs(); });
    connect(m_importAct, &QAction::triggered, this, &MainWindow::importImagesDialog);
    connect(m_lineArtAct, &QAction::triggered, this, &MainWindow::importOutlineImageDialog);
    connect(m_exportAct, &QAction::triggered, this, &MainWindow::exportCanvas);
    connect(m_quitAct, &QAction::triggered, this, &QWidget::close);

    connect(m_undoAct, &QAction::triggered, m_undo, &QUndoStack::undo);
    connect(m_redoAct, &QAction::triggered, m_undo, &QUndoStack::redo);
    connect(m_deleteAct, &QAction::triggered, m_scene, &CanvasScene::deleteSelected);
    connect(m_selectAllAct, &QAction::triggered, m_scene, &CanvasScene::selectAllItems);
    connect(m_itemPropsAct, &QAction::triggered, this, &MainWindow::showSelectedItemProperties);
    connect(m_rotateCwAct, &QAction::triggered, this, [this]() { rotateSelected(45.0); });
    connect(m_rotateCcwAct, &QAction::triggered, this, [this]() { rotateSelected(-45.0); });

    connect(m_modeGroup, &QActionGroup::triggered, this,
            [this](QAction* action) { setToolMode(static_cast<ToolMode>(action->data().toInt())); });
    connect(m_fitAct, &QAction::triggered, m_view, &CanvasView::fitAll);
    connect(m_zoomInAct, &QAction::triggered, m_view, &CanvasView::zoomIn);
    connect(m_zoomOutAct, &QAction::triggered, m_view, &CanvasView::zoomOut);
    connect(m_actualSizeAct, &QAction::triggered, m_view, &CanvasView::actualSize);

    connect(m_replaceBgAct, &QAction::triggered, this, &MainWindow::replaceBackgroundImage);
    connect(m_bgColorAct, &QAction::triggered, this, &MainWindow::setBackgroundColor);
    connect(m_clearBgAct, &QAction::triggered, this, &MainWindow::clearBackground);
    connect(m_settingsAct, &QAction::triggered, this, &MainWindow::showSettings);

    m_fillNoneAct = new QAction(uiText(QStringLiteral("No fill")), this);
    m_fillSolidAct = new QAction(uiText(QStringLiteral("Solid fill")), this);
    m_fillHachureAct = new QAction(uiText(QStringLiteral("Hatch fill")), this);
    m_fillReverseAct = new QAction(uiText(QStringLiteral("Reverse hatch fill")), this);
    m_fillCrossAct = new QAction(uiText(QStringLiteral("Cross hatch fill")), this);
    m_fillGridAct = new QAction(uiText(QStringLiteral("Grid hatch fill")), this);
    m_fillHoneycombAct = new QAction(uiText(QStringLiteral("Hexagon fill")), this);
    for (QAction* action : {m_fillNoneAct, m_fillSolidAct, m_fillHachureAct, m_fillReverseAct, m_fillCrossAct,
                            m_fillGridAct, m_fillHoneycombAct}) {
        action->setCheckable(true);
    }
    m_fillStyleGroup = new QActionGroup(this);
    m_fillStyleGroup->setExclusive(true);
    m_fillStyleGroup->addAction(m_fillNoneAct);
    m_fillStyleGroup->addAction(m_fillSolidAct);
    m_fillStyleGroup->addAction(m_fillHachureAct);
    m_fillStyleGroup->addAction(m_fillReverseAct);
    m_fillStyleGroup->addAction(m_fillCrossAct);
    m_fillStyleGroup->addAction(m_fillGridAct);
    m_fillStyleGroup->addAction(m_fillHoneycombAct);
    m_fillSolidAct->setChecked(true);
    connect(m_fillNoneAct, &QAction::triggered, this, [this]() { setFillStyle(FillStyle::None); });
    connect(m_fillSolidAct, &QAction::triggered, this, [this]() { setFillStyle(FillStyle::Solid); });
    connect(m_fillHachureAct, &QAction::triggered, this, [this]() { setFillStyle(FillStyle::Hachure); });
    connect(m_fillReverseAct, &QAction::triggered, this, [this]() { setFillStyle(FillStyle::ReverseHachure); });
    connect(m_fillCrossAct, &QAction::triggered, this, [this]() { setFillStyle(FillStyle::CrossHatch); });
    connect(m_fillGridAct, &QAction::triggered, this, [this]() { setFillStyle(FillStyle::GridHatch); });
    connect(m_fillHoneycombAct, &QAction::triggered, this, [this]() { setFillStyle(FillStyle::Honeycomb); });
    m_fillColorAct = new QAction(uiText(QStringLiteral("Fill color…")), this);
    m_fillColorAct->setObjectName(QStringLiteral("FillColorAction"));
    m_fillColorAct->setIcon(makeFillColorIcon(m_fillColor.isValid() ? m_fillColor : QColor(30, 41, 59, 36), 16));
    connect(m_fillColorAct, &QAction::triggered, this, &MainWindow::chooseFillColor);
    m_strokeColorAct = new QAction(uiText(QStringLiteral("Outline color…")), this);
    m_strokeColorAct->setObjectName(QStringLiteral("OutlineColorAction"));
    m_strokeColorAct->setIcon(makeStrokeColorIcon(m_brushColor, 16));
    connect(m_strokeColorAct, &QAction::triggered, this, &MainWindow::chooseStrokeColor);
    m_brushColorAct = new QAction(uiText(QStringLiteral("Brush color…")), this);
    m_brushColorAct->setObjectName(QStringLiteral("BrushColorAction"));
    m_brushColorAct->setIcon(makeColorIcon(m_brushColor, 16));
    connect(m_brushColorAct, &QAction::triggered, this, &MainWindow::chooseBrushColor);

    // 需求 31：画笔样式菜单下提供粗细选项，并与工具栏下拉共用同一个设置。
    m_brushWidthGroup = new QActionGroup(this);
    m_brushWidthGroup->setExclusive(true);
    for (int width : brushWidthPresets()) {
        auto* action = new QAction(QStringLiteral("%1 px").arg(width), this);
        action->setObjectName(QStringLiteral("BrushWidthAction%1").arg(width));
        action->setData(width);
        action->setCheckable(true);
        m_brushWidthGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, width]() {
            setBrushWidthUi(width);
            m_settings.brushWidth = width;
            m_settings.save();
        });
    }

    auto applyBackgroundMode = [this](BackgroundMode mode) {
        BackgroundState background = m_scene->background();
        if (background.type != BackgroundType::Image) {
            return;
        }
        background.mode = mode;
        m_scene->replaceBackground(background, uiText(QStringLiteral("Adjust background fill mode")));
    };
    connect(m_bgStretchAct, &QAction::triggered, this,
            [applyBackgroundMode]() { applyBackgroundMode(BackgroundMode::Stretch); });
    connect(m_bgFitAct, &QAction::triggered, this,
            [applyBackgroundMode]() { applyBackgroundMode(BackgroundMode::Fit); });
    connect(m_bgTileAct, &QAction::triggered, this,
            [applyBackgroundMode]() { applyBackgroundMode(BackgroundMode::Tile); });

    connect(m_aboutAct, &QAction::triggered, this, &MainWindow::showAbout);
    connect(m_aboutQtAct, &QAction::triggered, qApp, &QApplication::aboutQt);
}

void MainWindow::createShapeMenu()
{
    m_shapeMenu = new QMenu(uiText(QStringLiteral("Common shapes")), this);
    m_shapeMenu->setObjectName(QStringLiteral("ShapeMenu"));
    m_shapeMenu->setToolTipsVisible(true);

    for (ShapeType type : allShapeTypes()) {
        QAction* action = m_shapeMenu->addAction(makeShapeIcon(type, QColor(58, 64, 80), 18), shapeTypeName(type));
        action->setObjectName(QStringLiteral("ShapeAction_%1").arg(shapeTypeToString(type)));
        action->setData(static_cast<int>(type));
        action->setToolTip(uiText(QStringLiteral("Insert %1 (uses the current brush color)")).arg(shapeTypeName(type)));
        connect(action, &QAction::triggered, this, [this, type]() { insertShape(type); });
    }
}

void MainWindow::createLayerDock()
{
    m_layerDock = new QDockWidget(uiText(QStringLiteral("Layers")), this);
    m_layerDock->setObjectName(QStringLiteral("LayerDock"));
    m_layerDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_layerDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable |
                             QDockWidget::DockWidgetClosable);

    m_layerPanel = new LayerPanel(m_scene, m_layerDock);
    m_layerDock->setWidget(m_layerPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_layerDock);

    connect(m_layerPanel, &LayerPanel::statusMessage, this,
            [this](const QString& message) { statusBar()->showMessage(message, 3000); });
}

void MainWindow::createMenus()
{
    QMenu* fileMenu = menuBar()->addMenu(uiText(QStringLiteral("&File")));
    fileMenu->addAction(m_newAct);
    fileMenu->addAction(m_openAct);
    // 需求：在“打开画布”后面提供图片线稿化入口，把色块轮廓变成笔画图元。
    fileMenu->addAction(m_lineArtAct);
    fileMenu->addSeparator();
    fileMenu->addAction(m_saveAct);
    fileMenu->addAction(m_saveAsAct);
    fileMenu->addSeparator();
    fileMenu->addAction(m_importAct);
    fileMenu->addAction(m_exportAct);
    fileMenu->addSeparator();
    fileMenu->addAction(m_quitAct);

    QMenu* editMenu = menuBar()->addMenu(uiText(QStringLiteral("&Edit")));
    editMenu->addAction(m_undoAct);
    editMenu->addAction(m_redoAct);
    editMenu->addSeparator();
    editMenu->addAction(m_deleteAct);
    editMenu->addAction(m_duplicateAct);
    editMenu->addAction(m_groupAct);
    editMenu->addAction(m_ungroupAct);
    editMenu->addAction(m_smoothAct);
    editMenu->addAction(m_selectAllAct);
    editMenu->addSeparator();
    editMenu->addAction(m_itemPropsAct);
    editMenu->addSeparator();

    auto applySelectedHeight = [this](int delta) {
        if (m_view->mode() != ToolMode::Edit) {
            statusBar()->showMessage(uiText(QStringLiteral("Height adjustment is available in edit mode only")), 3000);
            return;
        }
        const int changed = m_scene->adjustSelectedHeights(delta);
        if (changed == 0) {
            statusBar()->showMessage(
                uiText(QStringLiteral("No selected items can change height (or the height limit was reached)")), 3000);
        } else {
            statusBar()->showMessage(uiText(QStringLiteral("Changed the height of %1 item(s) by %2"))
                                         .arg(changed)
                                         .arg(delta > 0 ? QStringLiteral("+1") : QStringLiteral("-1")),
                                     3000);
        }
    };

    QAction* heightUpAct = editMenu->addAction(uiText(QStringLiteral("Height +1 (stack up)    ]")));
    heightUpAct->setShortcut(QKeySequence(Qt::Key_BracketRight));
    heightUpAct->setStatusTip(uiText(QStringLiteral("Raise the stacking height of selected items (shortcut ])")));
    connect(heightUpAct, &QAction::triggered, this, [applySelectedHeight]() { applySelectedHeight(1); });

    QAction* heightDownAct = editMenu->addAction(uiText(QStringLiteral("Height -1 (stack down)    [")));
    heightDownAct->setShortcut(QKeySequence(Qt::Key_BracketLeft));
    heightDownAct->setStatusTip(uiText(QStringLiteral("Lower the stacking height of selected items (shortcut [)")));
    connect(heightDownAct, &QAction::triggered, this, [applySelectedHeight]() { applySelectedHeight(-1); });

    editMenu->addSeparator();
    editMenu->addAction(m_rotateCwAct);
    editMenu->addAction(m_rotateCcwAct);

    QMenu* insertMenu = menuBar()->addMenu(uiText(QStringLiteral("&Insert")));
    insertMenu->addMenu(m_shapeMenu);
    insertMenu->addSeparator();
    insertMenu->addAction(m_importAct);

    QMenu* viewMenu = menuBar()->addMenu(uiText(QStringLiteral("&View")));
    viewMenu->addAction(m_viewModeAct);
    viewMenu->addAction(m_editModeAct);
    viewMenu->addAction(m_brushModeAct);
    viewMenu->addAction(m_textModeAct);
    viewMenu->addSeparator();
    viewMenu->addAction(m_fitAct);
    viewMenu->addAction(m_zoomInAct);
    viewMenu->addAction(m_zoomOutAct);
    viewMenu->addAction(m_actualSizeAct);
    viewMenu->addSeparator();
    QAction* layerDockAction = m_layerDock->toggleViewAction();
    layerDockAction->setText(uiText(QStringLiteral("Layer panel")));
    layerDockAction->setStatusTip(uiText(QStringLiteral("Show or hide the layer panel")));
    viewMenu->addAction(layerDockAction);

    QMenu* canvasMenu = menuBar()->addMenu(uiText(QStringLiteral("&Canvas")));
    canvasMenu->addAction(m_importAct);
    canvasMenu->addAction(m_lineArtAct);
    canvasMenu->addSeparator();
    canvasMenu->addAction(m_replaceBgAct);
    canvasMenu->addAction(m_bgColorAct);
    canvasMenu->addAction(m_clearBgAct);
    QMenu* fillMenu = canvasMenu->addMenu(uiText(QStringLiteral("Background fill mode")));
    fillMenu->addAction(m_bgStretchAct);
    fillMenu->addAction(m_bgFitAct);
    fillMenu->addAction(m_bgTileAct);
    canvasMenu->addSeparator();

    QMenu* styleMenu = menuBar()->addMenu(uiText(QStringLiteral("&Style")));
    styleMenu->setObjectName(QStringLiteral("StyleMenu"));

    QMenu* brushStyleMenu = styleMenu->addMenu(uiText(QStringLiteral("Brush style")));
    brushStyleMenu->setObjectName(QStringLiteral("BrushStyleMenu"));
    brushStyleMenu->addAction(m_brushColorAct);
    brushStyleMenu->addSeparator();
    QMenu* brushWidthMenu = brushStyleMenu->addMenu(uiText(QStringLiteral("Brush width")));
    brushWidthMenu->setObjectName(QStringLiteral("BrushWidthMenu"));
    for (QAction* action : m_brushWidthGroup->actions()) {
        brushWidthMenu->addAction(action);
    }

    QMenu* fillStyleMenu = styleMenu->addMenu(uiText(QStringLiteral("Fill style")));
    fillStyleMenu->setObjectName(QStringLiteral("FillStyleMenu"));
    fillStyleMenu->addAction(m_fillNoneAct);
    fillStyleMenu->addAction(m_fillSolidAct);
    fillStyleMenu->addAction(m_fillHachureAct);
    fillStyleMenu->addAction(m_fillReverseAct);
    fillStyleMenu->addAction(m_fillCrossAct);
    fillStyleMenu->addAction(m_fillGridAct);
    fillStyleMenu->addAction(m_fillHoneycombAct);

    // 需求 29：颜色从填充样式里分离出来，放在填充样式之后。
    styleMenu->addAction(m_fillColorAct);
    styleMenu->addAction(m_strokeColorAct);

    m_canvasesMenu = menuBar()->addMenu(uiText(QStringLiteral("&Board")));
    connect(m_canvasesMenu, &QMenu::aboutToShow, this, &MainWindow::updateCanvasesMenu);
    updateCanvasesMenu();

    QMenu* helpMenu = menuBar()->addMenu(uiText(QStringLiteral("&Help")));
    helpMenu->addAction(m_aboutAct);
    helpMenu->addAction(m_aboutQtAct);
}

void MainWindow::createToolBars()
{
    auto* mainBar = new QToolBar(uiText(QStringLiteral("Main toolbar")), this);
    mainBar->setObjectName(QStringLiteral("MainToolBar"));
    mainBar->setMovable(false);
    mainBar->setFloatable(false);
    mainBar->setIconSize(QSize(21, 21));
    mainBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    mainBar->setContextMenuPolicy(Qt::PreventContextMenu);
    addToolBar(Qt::TopToolBarArea, mainBar);

    mainBar->addAction(m_newAct);
    mainBar->addAction(m_openAct);
    mainBar->addAction(m_saveAct);
    mainBar->addAction(m_exportAct);
    mainBar->addSeparator();
    mainBar->addAction(m_undoAct);
    mainBar->addAction(m_redoAct);
    // 需求 40：顺/逆时针旋转 45° 除了菜单快捷键，也必须出现在工具栏。
    mainBar->addAction(m_rotateCwAct);
    mainBar->addAction(m_rotateCcwAct);
    mainBar->addSeparator();

    CreateFillToolbar(mainBar);
    mainBar->addSeparator();
    CreateBrushToolbar(mainBar);
    mainBar->addSeparator();

    mainBar->addAction(m_fitAct);
    mainBar->addAction(m_zoomInAct);
    mainBar->addAction(m_zoomOutAct);
    mainBar->addSeparator();
    mainBar->addAction(m_importAct);
    mainBar->addAction(m_replaceBgAct);
    mainBar->addAction(m_settingsAct);

    auto* modeBar = new QToolBar(uiText(QStringLiteral("Mode")), this);
    modeBar->setObjectName(QStringLiteral("ModeToolBar"));
    modeBar->setMovable(false);
    modeBar->setFloatable(false);
    modeBar->setIconSize(QSize(23, 23));
    modeBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    modeBar->setContextMenuPolicy(Qt::PreventContextMenu);
    modeBar->setMinimumWidth(80);
    addToolBar(Qt::LeftToolBarArea, modeBar);
    modeBar->addAction(m_viewModeAct);
    modeBar->addAction(m_editModeAct);
    modeBar->addAction(m_brushModeAct);
    modeBar->addAction(m_textModeAct);
}

void MainWindow::CreateBrushToolbar(QToolBar* mainBar)
{
    auto* brushLabel = new QLabel(uiText(QStringLiteral("Brush")), mainBar);
    brushLabel->setObjectName(QStringLiteral("ToolBarLabel"));
    mainBar->addWidget(brushLabel);
    m_colorButton = new QToolButton(mainBar);
    m_colorButton->setObjectName(QStringLiteral("ColorButton"));
    m_colorButton->setIcon(makeColorIcon(m_brushColor, 20));
    m_colorButton->setIconSize(QSize(20, 20));
    m_colorButton->setToolTip(
        uiText(QStringLiteral("Brush color: %1 (click to choose)")).arg(m_brushColor.name(QColor::HexArgb).toUpper()));
    m_colorButton->setPopupMode(QToolButton::MenuButtonPopup);

    auto* colorMenu = new QMenu(m_colorButton);
    const QList<QPair<QString, QColor>> presets = {
        {uiText(QStringLiteral("Ink black")), QColor(QStringLiteral("#111827"))},
        {uiText(QStringLiteral("Red")), QColor(QStringLiteral("#EF4444"))},
        {uiText(QStringLiteral("Orange")), QColor(QStringLiteral("#F59E0B"))},
        {uiText(QStringLiteral("Green")), QColor(QStringLiteral("#10B981"))},
        {uiText(QStringLiteral("Blue")), QColor(QStringLiteral("#3B82F6"))},
        {uiText(QStringLiteral("Purple")), QColor(QStringLiteral("#8B5CF6"))},
        {uiText(QStringLiteral("Pink")), QColor(QStringLiteral("#EC4899"))},
        {uiText(QStringLiteral("White")), QColor(QStringLiteral("#FFFFFF"))}};
    for (const auto& preset : presets) {
        QAction* action = colorMenu->addAction(makeColorIcon(preset.second, 16), preset.first);
        connect(action, &QAction::triggered, this, [this, color = preset.second]() { setBrushColor(color, true); });
    }
    colorMenu->addSeparator();
    QAction* customColor = colorMenu->addAction(uiText(QStringLiteral("Custom color…")));
    connect(customColor, &QAction::triggered, this, &MainWindow::chooseBrushColor);
    m_colorButton->setMenu(colorMenu);
    connect(m_colorButton, &QToolButton::clicked, this, &MainWindow::chooseBrushColor);
    mainBar->addWidget(m_colorButton);

    m_widthCombo = new QComboBox(mainBar);
    m_widthCombo->setObjectName(QStringLiteral("BrushWidthCombo"));
    m_widthCombo->setFixedWidth(96);
    m_widthCombo->setFixedHeight(28);
    m_widthCombo->setToolTip(uiText(QStringLiteral("Brush width")));
    for (int width : brushWidthPresets()) {
        m_widthCombo->addItem(QStringLiteral("%1 px").arg(width), width);
    }
    connect(m_widthCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        const int value = m_widthCombo->itemData(index).toInt();
        if (value <= 0) {
            return;
        }
        m_brushWidth = value;
        m_view->setBrushWidth(value);
        m_settings.brushWidth = value;
        m_settings.save();
        if (m_brushWidthGroup) {
            for (QAction* action : m_brushWidthGroup->actions()) {
                if (action->data().toInt() == value) {
                    const QSignalBlocker blocker(action);
                    action->setChecked(true);
                }
            }
        }
    });
    setBrushWidthUi(m_brushWidth);
    auto* widthLabel = new QLabel(uiText(QStringLiteral("Width")), mainBar);
    widthLabel->setObjectName(QStringLiteral("ToolBarLabel"));
    mainBar->addWidget(widthLabel);
    mainBar->addWidget(m_widthCombo);

    auto* smoothButton = new QToolButton(mainBar);
    smoothButton->setObjectName(QStringLiteral("SmoothStrokeButton"));
    smoothButton->setDefaultAction(m_smoothAct);
    smoothButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    smoothButton->setToolTip(uiText(QStringLiteral("Smooth strokes once")));
    mainBar->addWidget(smoothButton);

    auto* smoothingLabel = new QLabel(uiText(QStringLiteral("Tolerance")), mainBar);
    smoothingLabel->setObjectName(QStringLiteral("ToolBarLabel"));
    mainBar->addWidget(smoothingLabel);

    m_smoothingSpin = new QSpinBox(mainBar);
    m_smoothingSpin->setObjectName(QStringLiteral("BrushSmoothingSpin"));
    m_smoothingSpin->setRange(0, 30);
    m_smoothingSpin->setSuffix(QStringLiteral(" px"));
    m_smoothingSpin->setFixedWidth(78);
    m_smoothingSpin->setFixedHeight(28);
    m_smoothingSpin->setValue(m_settings.strokeSmoothing);
    m_smoothingSpin->setToolTip(
        uiText(QStringLiteral("Auto-smoothing tolerance used when a stroke ends; 0 disables automatic smoothing")));
    connect(m_smoothingSpin, &QSpinBox::valueChanged, this, [this](int value) {
        m_settings.strokeSmoothing = value;
        m_view->setStrokeSmoothing(value);
        m_settings.save();
    });
    mainBar->addWidget(m_smoothingSpin);
}

void MainWindow::CreateFillToolbar(QToolBar* mainBar)
{
    // 需求 41：填充样式按钮前显示“图形”标签，与画笔区形成明确分组。
    auto* fillLabel = new QLabel(uiText(QStringLiteral("Shape")), mainBar);
    fillLabel->setObjectName(QStringLiteral("ToolBarLabel"));
    mainBar->addWidget(fillLabel);
    auto* shapeButton = new QToolButton(mainBar);
    shapeButton->setObjectName(QStringLiteral("ShapeButton"));
    shapeButton->setIcon(makeIcon(IconId::Shapes, QColor(58, 64, 80), 21));
    shapeButton->setIconSize(QSize(21, 21));
    shapeButton->setPopupMode(QToolButton::InstantPopup);
    shapeButton->setMenu(m_shapeMenu);
    shapeButton->setToolTip(uiText(QStringLiteral("Quickly insert circles, rectangles, arrows, braces and more")));
    shapeButton->setStatusTip(uiText(QStringLiteral("Quickly insert common shapes (uses the current brush color)")));
    mainBar->addWidget(shapeButton);

    m_fillButton = new QToolButton(mainBar);
    m_fillButton->setObjectName(QStringLiteral("FillButton"));
    m_fillButton->setIconSize(QSize(20, 20));
    m_fillButton->setPopupMode(QToolButton::InstantPopup);
    auto* fillMenu = new QMenu(m_fillButton);
    fillMenu->addAction(m_fillNoneAct);
    fillMenu->addAction(m_fillSolidAct);
    fillMenu->addAction(m_fillHachureAct);
    fillMenu->addAction(m_fillReverseAct);
    fillMenu->addAction(m_fillCrossAct);
    fillMenu->addAction(m_fillGridAct);
    fillMenu->addAction(m_fillHoneycombAct);
    m_fillButton->setMenu(fillMenu);
    updateFillActions();
    mainBar->addWidget(m_fillButton);

    // 需求 29：填充颜色、描边颜色作为独立按钮，排在填充样式按钮之后。
    m_fillColorButton = new QToolButton(mainBar);
    m_fillColorButton->setObjectName(QStringLiteral("FillColorButton"));
    m_fillColorButton->setIconSize(QSize(20, 20));
    m_fillColorButton->setDefaultAction(m_fillColorAct);
    m_fillColorButton->setToolTip(uiText(QStringLiteral("Fill color: %1 (click to choose)"))
                                      .arg(m_fillColor.isValid() ? m_fillColor.name(QColor::HexArgb).toUpper()
                                                                 : uiText(QStringLiteral("Follow brush color"))));
    mainBar->addWidget(m_fillColorButton);

    m_strokeColorButton = new QToolButton(mainBar);
    m_strokeColorButton->setObjectName(QStringLiteral("StrokeColorButton"));
    m_strokeColorButton->setIconSize(QSize(20, 20));
    m_strokeColorButton->setDefaultAction(m_strokeColorAct);
    m_strokeColorButton->setToolTip(uiText(QStringLiteral("Outline color: %1 (click to choose)"))
                                        .arg(m_brushColor.name(QColor::HexArgb).toUpper()));
    mainBar->addWidget(m_strokeColorButton);
}

void MainWindow::createStatusBar()
{
    statusBar()->setSizeGripEnabled(true);
    m_hintLabel = new QLabel(this);
    m_hintLabel->setObjectName(QStringLiteral("StatusHint"));
    m_modeBadge = makeBadge(QStringLiteral("Badge"), this);
    m_zoomBadge = makeBadge(QStringLiteral("Badge"), this);
    m_countBadge = makeBadge(QStringLiteral("Badge"), this);

    statusBar()->addWidget(m_hintLabel, 1);
    statusBar()->addPermanentWidget(m_modeBadge);
    statusBar()->addPermanentWidget(m_zoomBadge);
    statusBar()->addPermanentWidget(m_countBadge);
}

void MainWindow::connectSignals()
{
    connect(m_undo, &QUndoStack::canUndoChanged, this, &MainWindow::updateActions);
    connect(m_undo, &QUndoStack::canRedoChanged, this, &MainWindow::updateActions);
    connect(m_undo, &QUndoStack::cleanChanged, this, &MainWindow::updateTitle);
    connect(m_undo, &QUndoStack::undoTextChanged, this, [this](const QString& text) {
        m_undoAct->setToolTip(text.isEmpty() ? uiText(QStringLiteral("Undo"))
                                             : uiText(QStringLiteral("Undo: %1")).arg(text));
    });
    connect(m_undo, &QUndoStack::redoTextChanged, this, [this](const QString& text) {
        m_redoAct->setToolTip(text.isEmpty() ? uiText(QStringLiteral("Redo"))
                                             : uiText(QStringLiteral("Redo: %1")).arg(text));
    });

    connect(m_view, &CanvasView::zoomChanged, this, [this](qreal zoom) {
        if (m_zoomBadge) {
            m_zoomBadge->setText(QStringLiteral("%1%").arg(qRound(zoom * 100.0)));
        }
    });
    connect(m_view, &CanvasView::modeChanged, this, [this](ToolMode) { updateModeUi(); });
    connect(m_view, &CanvasView::statusMessage, this,
            [this](const QString& message) { statusBar()->showMessage(message, 4000); });
    connect(m_view, &CanvasView::textPlacementRequested, this, &MainWindow::insertTextAt);
    connect(m_view, &CanvasView::itemPropertiesRequested, this, &MainWindow::showItemProperties);

    connect(m_scene, &CanvasScene::contentChanged, this, [this]() {
        if (m_itemPropertiesDialog &&
            (!m_itemPropertiesDialog->item() || m_itemPropertiesDialog->item()->scene() != m_scene)) {
            closeItemProperties();
        }
        updateActions();
        updateStatusBar();
    });
    connect(m_scene, &CanvasScene::layersChanged, this, &MainWindow::updateStatusBar);
    connect(m_scene, &CanvasScene::activeLayerChanged, this, [this](int) { updateStatusBar(); });
    connect(m_scene, &CanvasScene::selectionChanged, this, &MainWindow::updateActions);
    connect(m_scene, &CanvasScene::backgroundChanged, this, [this]() {
        updateBackgroundModeActions();
        updateActions();
    });
    connect(m_scene, &CanvasScene::canvasSizeChanged, this, [this](const QSizeF&) {
        if (m_view->mode() == ToolMode::View) {
            m_view->fitAll();
        }
    });
}

void MainWindow::setToolMode(ToolMode mode)
{
    m_view->setMode(mode);
    updateModeUi();
}

void MainWindow::updateModeUi()
{
    if (!m_view) {
        return;
    }

    const ToolMode mode = m_view->mode();
    m_viewModeAct->setChecked(mode == ToolMode::View);
    m_brushModeAct->setChecked(mode == ToolMode::Brush);
    m_editModeAct->setChecked(mode == ToolMode::Edit);
    m_textModeAct->setChecked(mode == ToolMode::Text);

    if (m_modeBadge) {
        m_modeBadge->setText(toolModeName(mode));
    }

    if (m_hintLabel) {
        switch (mode) {
        case ToolMode::View:
            m_hintLabel->setText(uiText(
                QStringLiteral("View mode: drag to pan · wheel to zoom · switching modes returns to the overview")));
            break;
        case ToolMode::Brush:
            m_hintLabel->setText(uiText(
                QStringLiteral("Brush mode: left button to draw · wheel to zoom · Space / middle button to pan")));
            break;
        case ToolMode::Edit:
            m_hintLabel->setText(uiText(
                QStringLiteral("Edit mode: items on visible, unlocked layers can be edited · drag or box-select · "
                               "resize with handles · drag canvas edges to resize the canvas · Delete to remove")));
            break;
        case ToolMode::Text:
            m_hintLabel->setText(uiText(
                QStringLiteral("Text mode: click the canvas to place text · set font, size, color and bold/italic")));
            break;
        }
    }

    updateStatusBar();
    updateActions();
}

void MainWindow::updateActions()
{
    if (!m_undo || !m_scene || !m_view) {
        return;
    }

    m_undoAct->setEnabled(m_undo->canUndo());
    m_redoAct->setEnabled(m_undo->canRedo());

    const bool editing = m_view->mode() == ToolMode::Edit;
    m_deleteAct->setEnabled(editing && !m_scene->selectedItems().isEmpty());
    m_duplicateAct->setEnabled(editing && !m_scene->selectedCanvasItems().isEmpty());
    m_groupAct->setEnabled(editing && m_scene->canGroupSelectedItems());
    m_ungroupAct->setEnabled(editing && m_scene->canUngroupSelectedItems());
    m_smoothAct->setEnabled(editing && m_scene->canSmoothSelectedStrokes());
    m_selectAllAct->setEnabled(editing && m_scene->canvasItemCount() > 0);
    m_itemPropsAct->setEnabled(m_scene->selectedCanvasItems().size() == 1);
    m_rotateCwAct->setEnabled(editing && !m_scene->selectedCanvasItems().isEmpty());
    m_rotateCcwAct->setEnabled(editing && !m_scene->selectedCanvasItems().isEmpty());

    updateBackgroundModeActions();
}

void MainWindow::updateBackgroundModeActions()
{
    if (!m_scene || !m_bgStretchAct) {
        return;
    }

    const BackgroundState background = m_scene->background();
    const bool hasImage = background.type == BackgroundType::Image;
    m_bgStretchAct->setEnabled(hasImage);
    m_bgFitAct->setEnabled(hasImage);
    m_bgTileAct->setEnabled(hasImage);
    m_bgStretchAct->setChecked(hasImage && background.mode == BackgroundMode::Stretch);
    m_bgFitAct->setChecked(hasImage && background.mode == BackgroundMode::Fit);
    m_bgTileAct->setChecked(hasImage && background.mode == BackgroundMode::Tile);
    m_clearBgAct->setEnabled(background.type != BackgroundType::None);
}

void MainWindow::updateStatusBar()
{
    if (!m_view || !m_scene) {
        return;
    }

    if (m_modeBadge) {
        m_modeBadge->setText(toolModeName(m_view->mode()));
    }
    if (m_zoomBadge) {
        m_zoomBadge->setText(QStringLiteral("%1%").arg(qRound(m_view->zoom() * 100.0)));
    }
    if (m_countBadge) {
        m_countBadge->setText(uiText(QStringLiteral("%1 items · %2 layers"))
                                  .arg(m_scene->canvasItemCount())
                                  .arg(m_scene->userLayerCount()));
    }
}

QString MainWindow::documentName() const
{
    if (m_filePath.isEmpty()) {
        return uiText(QStringLiteral("Untitled canvas"));
    }
    const QString name = QFileInfo(m_filePath).completeBaseName();
    return name.isEmpty() ? uiText(QStringLiteral("Untitled canvas")) : name;
}

void MainWindow::updateTitle()
{
    setWindowTitle(uiText(QStringLiteral("%1[*] - StoryBoard")).arg(documentName()));
    setWindowModified(m_undo && !m_undo->isClean());
    if (!m_filePath.isEmpty()) {
        setWindowFilePath(m_filePath);
    }
}

void MainWindow::setBrushColor(const QColor& color, bool remember)
{
    if (!color.isValid()) {
        return;
    }

    m_brushColor = color;
    m_view->setBrushColor(color);
    if (m_colorButton) {
        m_colorButton->setIcon(makeColorIcon(color, 20));
        m_colorButton->setToolTip(
            uiText(QStringLiteral("Brush color: %1 (click to choose)")).arg(color.name(QColor::HexArgb).toUpper()));
    }
    if (m_brushColorAct) {
        m_brushColorAct->setIcon(makeColorIcon(color, 16));
    }
    if (m_strokeColorAct) {
        m_strokeColorAct->setIcon(makeStrokeColorIcon(color, 16));
    }
    if (m_strokeColorButton) {
        m_strokeColorButton->setToolTip(
            uiText(QStringLiteral("Outline color: %1 (click to choose)")).arg(color.name(QColor::HexArgb).toUpper()));
    }
    if (m_fillButton) {
        updateFillActions();
    }
    if (remember) {
        m_settings.brushColor = color;
        m_settings.save();
    }
}

void MainWindow::setBrushWidthUi(int width)
{
    width = qBound(1, width, 64);
    m_brushWidth = width;
    if (m_view) {
        m_view->setBrushWidth(width);
    }
    if (m_widthCombo) {
        int index = m_widthCombo->findData(width);
        if (index < 0) {
            m_widthCombo->addItem(QStringLiteral("%1 px").arg(width), width);
            index = m_widthCombo->count() - 1;
        }
        const QSignalBlocker blocker(m_widthCombo);
        m_widthCombo->setCurrentIndex(index);
    }
    if (m_brushWidthGroup) {
        for (QAction* action : m_brushWidthGroup->actions()) {
            if (action->data().toInt() == width) {
                const QSignalBlocker blocker(action);
                action->setChecked(true);
                break;
            }
        }
    }
}

void MainWindow::setStrokeSmoothingUi(int tolerance)
{
    tolerance = qBound(0, tolerance, 30);
    m_settings.strokeSmoothing = tolerance;
    if (m_view) {
        m_view->setStrokeSmoothing(tolerance);
    }
    if (m_smoothingSpin) {
        const QSignalBlocker blocker(m_smoothingSpin);
        m_smoothingSpin->setValue(tolerance);
    }
}

void MainWindow::chooseBrushColor()
{
    const QColor color = QColorDialog::getColor(m_brushColor, this, uiText(QStringLiteral("Choose brush color")),
                                                QColorDialog::ShowAlphaChannel);
    if (color.isValid()) {
        setBrushColor(color, true);
    }
}

void MainWindow::newCanvas()
{
    if (!maybeSave()) {
        return;
    }

    m_view->cancelCurrentStroke();
    m_scene->resetDocument();
    m_scene->applyCanvasSize(m_settings.canvasSize);
    m_scene->setShowGrid(m_settings.showGrid);
    m_scene->applyBackground(BackgroundState());
    m_filePath.clear();
    m_undo->clear();
    m_undo->setClean();

    if (m_view->mode() != ToolMode::View) {
        m_view->setMode(ToolMode::View);
    } else {
        m_view->fitAll();
    }

    updateModeUi();
    updateTitle();
    updateStatusBar();
    updateActions();
    statusBar()->showMessage(uiText(QStringLiteral("New empty canvas created")), 3000);
}

void MainWindow::openCanvasDialog()
{
    const QString path = QFileDialog::getOpenFileName(this, uiText(QStringLiteral("Open canvas")),
                                                      AppSettings::canvasesDir(), canvasFilter());
    if (!path.isEmpty()) {
        loadCanvasFile(path, true);
    }
}

bool MainWindow::openCanvasFile(const QString& path)
{
    return loadCanvasFile(path, true);
}

bool MainWindow::loadCanvasFile(const QString& path, bool showErrors)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (showErrors) {
            QMessageBox::warning(
                this, uiText(QStringLiteral("Open canvas")),
                uiText(QStringLiteral("Could not read the file:\n%1")).arg(QDir::toNativeSeparators(path)));
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (showErrors) {
            QMessageBox::warning(
                this, uiText(QStringLiteral("Open canvas")),
                uiText(QStringLiteral("Failed to parse the canvas file:\n%1")).arg(parseError.errorString()));
        }
        return false;
    }

    const QJsonObject root = document.object();
    QString validationError;
    if (!validateCanvasRoot(root, &validationError)) {
        if (showErrors) {
            QMessageBox::warning(this, uiText(QStringLiteral("Open canvas")), validationError);
        }
        return false;
    }

    if (!maybeSave()) {
        return false;
    }

    m_view->finishCurrentStroke();
    QString loadError;
    if (!m_scene->fromJson(root, &loadError)) {
        if (showErrors) {
            QMessageBox::warning(this, uiText(QStringLiteral("Open canvas")), loadError);
        }
        return false;
    }

    m_filePath = QFileInfo(path).absoluteFilePath();
    m_undo->setClean();
    if (m_view->mode() != ToolMode::View) {
        m_view->setMode(ToolMode::View);
    } else {
        m_view->fitAll();
    }

    m_settings.lastCanvasPath = m_filePath;
    updateModeUi();
    updateTitle();
    updateStatusBar();
    updateActions();
    statusBar()->showMessage(uiText(QStringLiteral("Canvas opened: %1")).arg(QFileInfo(m_filePath).fileName()), 4000);
    return true;
}

bool MainWindow::saveCanvas()
{
    if (m_filePath.isEmpty()) {
        return saveCanvasAs();
    }
    return saveToFile(m_filePath);
}

bool MainWindow::saveCanvasAs()
{
    QString suggestion = m_filePath;
    if (suggestion.isEmpty()) {
        const QString name = documentName();
        suggestion = QDir(AppSettings::canvasesDir()).filePath(name + QStringLiteral(".sbcanvas"));
    }

    QString path = QFileDialog::getSaveFileName(this, uiText(QStringLiteral("Save canvas as")), suggestion,
                                                uiText(QStringLiteral("StoryBoard canvases (*.sbcanvas)")));
    if (path.isEmpty()) {
        return false;
    }
    if (!path.endsWith(QStringLiteral(".sbcanvas"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".sbcanvas");
    }
    return saveToFile(path);
}

bool MainWindow::saveToFile(const QString& path)
{
    m_view->finishCurrentStroke();

    const QJsonDocument document(m_scene->toJson());
    const QByteArray data = document.toJson(QJsonDocument::Indented);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(
            this, uiText(QStringLiteral("Save canvas")),
            uiText(QStringLiteral("Could not write the file:\n%1")).arg(QDir::toNativeSeparators(path)));
        return false;
    }
    file.write(data);
    if (!file.commit()) {
        QMessageBox::warning(
            this, uiText(QStringLiteral("Save canvas")),
            uiText(QStringLiteral("Failed to save the canvas:\n%1")).arg(QDir::toNativeSeparators(path)));
        return false;
    }

    m_filePath = QFileInfo(path).absoluteFilePath();
    m_settings.lastCanvasPath = m_filePath;
    m_settings.save();
    m_undo->setClean();
    updateTitle();
    updateActions();
    statusBar()->showMessage(uiText(QStringLiteral("Canvas saved: %1")).arg(QFileInfo(m_filePath).fileName()), 4000);
    return true;
}

bool MainWindow::maybeSave()
{
    if (!m_undo || m_undo->isClean()) {
        return true;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(uiText(QStringLiteral("Unsaved changes")));
    box.setText(uiText(QStringLiteral("The current canvas has unsaved changes.")));
    box.setInformativeText(uiText(QStringLiteral("Save this canvas first?")));
    QPushButton* saveButton = box.addButton(uiText(QStringLiteral("Save canvas")), QMessageBox::AcceptRole);
    QPushButton* discardButton = box.addButton(uiText(QStringLiteral("Don't save")), QMessageBox::DestructiveRole);
    box.addButton(uiText(QStringLiteral("Cancel")), QMessageBox::RejectRole);
    box.setDefaultButton(saveButton);
    box.exec();

    if (box.clickedButton() == saveButton) {
        return saveCanvas();
    }
    if (box.clickedButton() == discardButton) {
        return true;
    }
    return false;
}

void MainWindow::exportCanvas()
{
    m_view->finishCurrentStroke();
    QString startDirectory = m_settings.lastExportDir;
    if (startDirectory.isEmpty()) {
        startDirectory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }

    ExportDialog dialog(m_scene, startDirectory, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const ExportDialog::Options options = dialog.options();
    m_settings.lastExportDir = QFileInfo(options.filePath).absolutePath();
    m_settings.save();

    if (options.separateLayers) {
        const QStringList paths = options.filePaths;
        if (paths.isEmpty()) {
            QMessageBox::warning(this, uiText(QStringLiteral("Export canvas")),
                                 uiText(QStringLiteral("No visible layers to export.")));
            return;
        }

        QStringList exported;
        for (int i = 0; i < paths.size(); ++i) {
            const int layerId = i < options.layerIds.size() ? options.layerIds.at(i) : -1;
            if (!exportToFile(paths.at(i), options.area, options.scale, options.format, false, layerId)) {
                return;
            }
            exported.append(paths.at(i));
        }

        const QString folder = QFileInfo(paths.constFirst()).absolutePath();
        statusBar()->showMessage(uiText(QStringLiteral("Exported %1 visible layer file(s)")).arg(exported.size()),
                                 5000);
        QMessageBox box(this);
        box.setIcon(QMessageBox::Information);
        box.setWindowTitle(uiText(QStringLiteral("Export complete")));
        box.setText(uiText(QStringLiteral("The visible layers were exported successfully.")));
        box.setInformativeText(
            uiText(QStringLiteral("%1 file(s) in\n%2")).arg(exported.size()).arg(QDir::toNativeSeparators(folder)));
        QPushButton* openFolderButton = box.addButton(uiText(QStringLiteral("Open folder")), QMessageBox::ActionRole);
        box.addButton(uiText(QStringLiteral("Done")), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == openFolderButton) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
        }
        return;
    }

    exportToFile(options.filePath, options.area, options.scale, options.format);
}

bool MainWindow::exportToFile(const QString& path, const QRectF& area, qreal scale, const QString& format,
                              bool showCompletion, int layerId)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QImage image =
        layerId >= 0 ? m_scene->renderLayerImage(area, scale, layerId) : m_scene->renderImage(area, scale);
    QApplication::restoreOverrideCursor();

    if (image.isNull()) {
        QMessageBox::warning(this, uiText(QStringLiteral("Export canvas")),
                             uiText(QStringLiteral("Rendering the canvas failed: nothing to export.")));
        return false;
    }

    bool success = false;
    QString errorMessage;
    if (format == QStringLiteral("PDF")) {
        {
            QPdfWriter writer(path);
            writer.setResolution(96);
            writer.setTitle(uiText(QStringLiteral("StoryBoard Canvas")));
            writer.setCreator(QStringLiteral("StoryBoard"));
            writer.setPageMargins(QMarginsF(0.0, 0.0, 0.0, 0.0), QPageLayout::Point);
            const qreal widthPoints = image.width() * 72.0 / 96.0;
            const qreal heightPoints = image.height() * 72.0 / 96.0;
            writer.setPageSize(QPageSize(QSizeF(widthPoints, heightPoints), QPageSize::Point,
                                         QStringLiteral("StoryBoard"), QPageSize::ExactMatch));
            QPainter painter(&writer);
            if (painter.isActive()) {
                painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
                painter.drawImage(QRectF(0.0, 0.0, writer.width(), writer.height()), image);
                painter.end();
                success = true;
            } else {
                errorMessage = uiText(QStringLiteral("Could not create the PDF output."));
            }
        }
        if (success && QFileInfo(path).size() <= 0) {
            success = false;
            errorMessage = uiText(QStringLiteral("Failed to write the PDF file."));
        }
    } else {
        QImageWriter writer(path, format == QStringLiteral("JPEG") ? "JPEG" : "PNG");
        if (format == QStringLiteral("JPEG")) {
            writer.setQuality(92);
        }
        success = writer.write(image);
        if (!success) {
            errorMessage = writer.errorString();
        }
    }

    if (!success) {
        QMessageBox::critical(this, uiText(QStringLiteral("Export failed")),
                              uiText(QStringLiteral("Could not export the canvas:\n%1\n\n%2"))
                                  .arg(QDir::toNativeSeparators(path), errorMessage));
        return false;
    }

    m_settings.lastExportDir = QFileInfo(path).absolutePath();
    m_settings.save();
    statusBar()->showMessage(uiText(QStringLiteral("Canvas exported: %1")).arg(QFileInfo(path).fileName()), 5000);

    if (!showCompletion) {
        return true;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(uiText(QStringLiteral("Export complete")));
    box.setText(uiText(QStringLiteral("The canvas was exported successfully.")));
    box.setInformativeText(QDir::toNativeSeparators(path));
    QPushButton* openFileButton = box.addButton(uiText(QStringLiteral("Open file")), QMessageBox::AcceptRole);
    QPushButton* openFolderButton = box.addButton(uiText(QStringLiteral("Open folder")), QMessageBox::ActionRole);
    box.addButton(uiText(QStringLiteral("Done")), QMessageBox::RejectRole);
    box.exec();

    if (box.clickedButton() == openFileButton) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    } else if (box.clickedButton() == openFolderButton) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
    }

    return true;
}

void MainWindow::importImagesDialog()
{
    QString startDirectory = m_settings.lastImageDir;
    if (startDirectory.isEmpty()) {
        startDirectory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }

    const QStringList files =
        QFileDialog::getOpenFileNames(this, uiText(QStringLiteral("Insert images")), startDirectory, imageFilter());
    if (!files.isEmpty()) {
        importImageFiles(files);
    }
}

void MainWindow::importOutlineImageDialog()
{
    QString startDirectory = m_settings.lastImageDir;
    if (startDirectory.isEmpty()) {
        startDirectory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }

    const QString file = QFileDialog::getOpenFileName(this, uiText(QStringLiteral("Import outline image")),
                                                      startDirectory, imageFilter());
    if (!file.isEmpty()) {
        importOutlineImageFile(file);
    }
}

void MainWindow::importOutlineImageFile(const QString& file)
{
    if (file.isEmpty()) {
        return;
    }

    if (m_view->mode() != ToolMode::Edit) {
        setToolMode(ToolMode::Edit);
    }

    if (!m_scene->isActiveLayerWritable()) {
        statusBar()->showMessage(
            uiText(QStringLiteral("The current layer is locked or hidden; the image outline cannot be imported. Unlock "
                                  "or switch layers in the layer panel first.")),
            5000);
        return;
    }

    QImageReader reader(file);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        QMessageBox::warning(
            this, uiText(QStringLiteral("Import outline image")),
            uiText(QStringLiteral("Could not read the image:\n%1")).arg(QDir::toNativeSeparators(file)));
        return;
    }

    // 参数来自设置里的“图片线稿化”分组；线宽按需求固定为 1，不受设置影响。
    LineArtConverter::Options options;
    options.colorLevels = m_settings.lineArtColorLevels;
    options.paletteMergeDistance = m_settings.lineArtPaletteMerge;
    options.adjacentMergeDistance = m_settings.lineArtAdjacentMerge;
    options.minRegionArea = m_settings.lineArtMinRegionArea;
    const LineArtConverter::Result traced = LineArtConverter::extract(image, options);
    if (!traced.ok || traced.outlines.isEmpty()) {
        QMessageBox::warning(this, uiText(QStringLiteral("Import outline image")),
                             traced.error.isEmpty() ? uiText(QStringLiteral("No color blocks were found in the image."))
                                                    : uiText(traced.error));
        return;
    }

    // 和插入图片一致：等比缩到画布的 60% 以内，居中放到当前视图中心。
    const QSizeF processed(traced.imageSize.width(), traced.imageSize.height());
    qreal scale = qMin(1.0, qMin(m_scene->canvasSize().width() * 0.6 / processed.width(),
                                 m_scene->canvasSize().height() * 0.6 / processed.height()));
    if (!(scale > 0.0)) {
        scale = 1.0;
    }

    const QPointF viewCenter = m_view->mapToScene(m_view->viewport()->rect().center());
    const QSizeF placed(processed.width() * scale, processed.height() * scale);
    const QPointF topLeft = viewCenter - QPointF(placed.width() / 2.0, placed.height() / 2.0);

    QList<CanvasItem*> items;
    items.reserve(traced.outlines.size());
    for (const LineArtConverter::Outline& outline : traced.outlines) {
        if (outline.points.size() < 4) {
            continue;
        }

        QVector<QPointF> scenePoints;
        scenePoints.reserve(outline.points.size());
        for (const QPointF& point : outline.points) {
            scenePoints.append(topLeft + QPointF(point.x() * scale, point.y() * scale));
        }

        // 不再做额外平滑：轮廓已经按像素级 RDP 简化过，再平滑会把方角磨圆；
        // 折线模式让路径按直线段绘制，方角保持方正。
        auto* stroke = new StrokeItem();
        stroke->setStrokePoints(scenePoints, outline.color, options.strokeWidth, 0.0, true);
        items.append(stroke);
    }

    if (items.isEmpty()) {
        QMessageBox::warning(this, uiText(QStringLiteral("Import outline image")),
                             uiText(QStringLiteral("No color blocks were found in the image.")));
        return;
    }

    m_scene->addItemsWithUndo(items, uiText(QStringLiteral("Import outline image")));

    m_settings.lastImageDir = QFileInfo(file).absolutePath();
    m_settings.save();

    statusBar()->showMessage(uiText(QStringLiteral("%1 stroke(s) created from the image outlines")).arg(items.size()),
                             5000);
    if (traced.truncated) {
        QMessageBox::information(
            this, uiText(QStringLiteral("Import outline image")),
            uiText(QStringLiteral("The image has too many color blocks; only the first %1 outlines were converted."))
                .arg(items.size()));
    }
}

void MainWindow::importImageFiles(const QStringList& files)
{
    if (files.isEmpty()) {
        return;
    }

    if (m_view->mode() != ToolMode::Edit) {
        setToolMode(ToolMode::Edit);
    }

    if (!m_scene->isActiveLayerWritable()) {
        statusBar()->showMessage(uiText(QStringLiteral("The current layer is locked or hidden; images cannot be "
                                                       "inserted. Unlock or switch layers in the layer panel first.")),
                                 5000);
        return;
    }

    const QPointF viewCenter = m_view->mapToScene(m_view->viewport()->rect().center());
    const QSizeF maxSize(m_scene->canvasSize().width() * 0.6, m_scene->canvasSize().height() * 0.6);

    QList<CanvasItem*> items;
    QStringList failed;
    int offsetIndex = 0;

    for (const QString& file : files) {
        QImageReader reader(file);
        reader.setAutoTransform(true);
        const QImage image = reader.read();
        if (image.isNull()) {
            failed.append(file);
            continue;
        }

        const QPixmap pixmap = QPixmap::fromImage(image);
        if (pixmap.isNull()) {
            failed.append(file);
            continue;
        }

        qreal scaleFactor = 1.0;
        if (pixmap.width() > 0 && pixmap.height() > 0) {
            scaleFactor = qMin(1.0, qMin(maxSize.width() / pixmap.width(), maxSize.height() / pixmap.height()));
        }
        QSizeF size(pixmap.width() * scaleFactor, pixmap.height() * scaleFactor);
        size.setWidth(qMax<qreal>(16.0, size.width()));
        size.setHeight(qMax<qreal>(16.0, size.height()));

        auto* item = new ImageItem();
        item->setImage(pixmap, QFileInfo(file).fileName());
        item->setContentSize(size);
        item->setPos(viewCenter - QPointF(size.width() / 2.0, size.height() / 2.0) +
                     QPointF(offsetIndex * 26.0, offsetIndex * 26.0));
        items.append(item);
        ++offsetIndex;
    }

    if (items.isEmpty()) {
        QMessageBox::warning(this, uiText(QStringLiteral("Insert images")),
                             uiText(QStringLiteral("No usable image files.")));
        return;
    }

    m_scene->addItemsWithUndo(items, uiText(QStringLiteral("Insert images")));
    for (CanvasItem* item : items) {
        item->setSelected(true);
    }

    m_settings.lastImageDir = QFileInfo(files.constFirst()).absolutePath();
    m_settings.save();

    statusBar()->showMessage(uiText(QStringLiteral("%1 image(s) inserted")).arg(items.size()), 4000);
    if (!failed.isEmpty()) {
        QMessageBox::warning(this, uiText(QStringLiteral("Some images were not inserted")),
                             uiText(QStringLiteral("The following files could not be read:\n%1"))
                                 .arg(QDir::toNativeSeparators(failed.join(QLatin1Char('\n')))));
    }
}

void MainWindow::insertShape(ShapeType type)
{
    if (m_view->mode() != ToolMode::Edit) {
        setToolMode(ToolMode::Edit);
    }

    if (!m_scene->isActiveLayerWritable()) {
        statusBar()->showMessage(uiText(QStringLiteral("The current layer is locked or hidden; shapes cannot be "
                                                       "inserted. Unlock or switch layers in the layer panel first.")),
                                 5000);
        return;
    }

    auto* item = new ShapeItem(type);
    const QColor stroke = m_brushColor.alpha() < 8 ? QColor(QStringLiteral("#1E293B")) : m_brushColor;
    QColor fill = stroke;
    fill.setAlpha(36);
    if (m_fillColor.isValid()) {
        fill = m_fillColor;
    }
    const bool hollowByDefault = type == ShapeType::Arrow || type == ShapeType::DoubleArrow;
    const bool filled = shapeTypeIsFilled(type) && m_fillStyle != FillStyle::None && !hollowByDefault;
    item->setStyle(stroke, filled ? fill : QColor(Qt::transparent), qBound<qreal>(2.5, m_brushWidth, 10.0));
    item->setFillStyle(filled ? m_fillStyle : FillStyle::None);

    const QRectF canvas = m_scene->canvasRect();
    QSizeF size = defaultShapeSize(type);
    const qreal fitScale = qMin<qreal>(1.0, qMin(canvas.width() * 0.72 / qMax<qreal>(1.0, size.width()),
                                                 canvas.height() * 0.72 / qMax<qreal>(1.0, size.height())));
    size *= fitScale;

    QPointF center = m_view->mapToScene(m_view->viewport()->rect().center());
    if (!canvas.contains(center)) {
        center = canvas.center();
    }
    const qreal cascade = (m_scene->canvasItemCount() % 5) * 18.0;
    center += QPointF(cascade, cascade);
    center.setX(qBound(canvas.left() + size.width() / 2.0, center.x(), canvas.right() - size.width() / 2.0));
    center.setY(qBound(canvas.top() + size.height() / 2.0, center.y(), canvas.bottom() - size.height() / 2.0));

    item->setPos(center - QPointF(size.width() / 2.0, size.height() / 2.0));
    item->setContentSize(size);

    m_scene->clearSelection();
    m_scene->addItemsWithUndo({item}, uiText(QStringLiteral("Insert %1")).arg(shapeTypeName(type)));
    item->setSelected(true);
    m_view->setFocus();
    if (type == ShapeType::Ellipse) {
        statusBar()->showMessage(
            uiText(QStringLiteral("Inserted a circle; in edit mode drag a handle to stretch it into an ellipse")),
            5000);
    } else {
        statusBar()->showMessage(
            uiText(QStringLiteral("Inserted %1; drag to move, drag handles to resize, color follows the current brush"))
                .arg(shapeTypeName(type)),
            5000);
    }
}

void MainWindow::setFillStyle(FillStyle style)
{
    m_fillStyle = style;
    updateFillActions();
    applyFillToSelection();
}

void MainWindow::chooseFillColor()
{
    const QColor initial = m_fillColor.isValid() ? m_fillColor : QColor(59, 130, 246, 96);
    const QColor color = QColorDialog::getColor(initial, this, uiText(QStringLiteral("Choose fill color")),
                                                QColorDialog::ShowAlphaChannel);
    if (!color.isValid()) {
        return;
    }
    m_fillColor = color;
    updateFillActions();
    applyFillToSelection();
}

void MainWindow::chooseStrokeColor()
{
    const QColor initial = m_brushColor.alpha() < 8 ? QColor(QStringLiteral("#1E293B")) : m_brushColor;
    const QColor color = QColorDialog::getColor(initial, this, uiText(QStringLiteral("Choose outline color")),
                                                QColorDialog::ShowAlphaChannel);
    if (!color.isValid()) {
        return;
    }

    setBrushColor(color, true);

    QList<ShapeItem*> shapes;
    const QList<CanvasItem*> selected = m_scene->selectedCanvasItems();
    for (CanvasItem* item : selected) {
        if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
            shapes.append(shape);
        }
    }

    if (shapes.isEmpty()) {
        statusBar()->showMessage(uiText(QStringLiteral("Outline color set; it will be used for newly inserted shapes")),
                                 4000);
        return;
    }

    QList<ShapeItemState> before;
    QList<ShapeItemState> after;
    before.reserve(shapes.size());
    after.reserve(shapes.size());
    for (ShapeItem* shape : shapes) {
        before.append(shape->state());
        shape->setStyle(color, shape->fillColor(), shape->strokeWidth());
        after.append(shape->state());
    }

    if (before == after) {
        statusBar()->showMessage(uiText(QStringLiteral("The outline color did not change")), 3000);
        return;
    }

    m_undo->push(new ShapeStyleCommand(m_scene, shapes, before, after, uiText(QStringLiteral("Change outline color"))));
    statusBar()->showMessage(uiText(QStringLiteral("Applied outline color to %1 shape(s)")).arg(shapes.size()), 4000);
}

void MainWindow::applyFillToSelection()
{
    if (!m_scene || !m_undo) {
        return;
    }

    QList<ShapeItem*> shapes;
    int outlineOnlyCount = 0;
    const QList<CanvasItem*> selected = m_scene->selectedCanvasItems();
    for (CanvasItem* item : selected) {
        auto* shape = dynamic_cast<ShapeItem*>(item);
        if (!shape) {
            continue;
        }
        if (shapeTypeIsFilled(shape->shapeType())) {
            shapes.append(shape);
        } else {
            ++outlineOnlyCount;
        }
    }

    if (shapes.isEmpty()) {
        if (outlineOnlyCount > 0) {
            statusBar()->showMessage(
                uiText(QStringLiteral("The selected shape is stroke-only (line, brace, etc.); fill is not supported")),
                4000);
        } else {
            statusBar()->showMessage(
                uiText(QStringLiteral("Fill set to \"%1\"; it will be used for newly inserted shapes"))
                    .arg(fillStyleName(m_fillStyle)),
                4000);
        }
        return;
    }

    QList<ShapeItemState> before;
    QList<ShapeItemState> after;
    before.reserve(shapes.size());
    after.reserve(shapes.size());
    for (ShapeItem* shape : shapes) {
        before.append(shape->state());
        QColor fill = m_fillColor.isValid() ? m_fillColor : shape->fillColor();
        if (fill.alpha() == 0) {
            fill = shape->strokeColor();
        }
        shape->setFill(fill, m_fillStyle);
        after.append(shape->state());
    }

    if (before == after) {
        statusBar()->showMessage(uiText(QStringLiteral("The fill style did not change")), 3000);
        return;
    }

    m_undo->push(new ShapeStyleCommand(m_scene, shapes, before, after, uiText(QStringLiteral("Change fill style"))));
    statusBar()->showMessage(
        uiText(QStringLiteral("Applied \"%2\" to %1 shape(s)")).arg(shapes.size()).arg(fillStyleName(m_fillStyle)),
        4000);
}

void MainWindow::updateFillActions()
{
    if (!m_fillButton) {
        return;
    }

    if (m_fillNoneAct) {
        m_fillNoneAct->setChecked(m_fillStyle == FillStyle::None);
    }
    if (m_fillSolidAct) {
        m_fillSolidAct->setChecked(m_fillStyle == FillStyle::Solid);
    }
    if (m_fillHachureAct) {
        m_fillHachureAct->setChecked(m_fillStyle == FillStyle::Hachure);
    }
    if (m_fillReverseAct) {
        m_fillReverseAct->setChecked(m_fillStyle == FillStyle::ReverseHachure);
    }
    if (m_fillCrossAct) {
        m_fillCrossAct->setChecked(m_fillStyle == FillStyle::CrossHatch);
    }
    if (m_fillGridAct) {
        m_fillGridAct->setChecked(m_fillStyle == FillStyle::GridHatch);
    }
    if (m_fillHoneycombAct) {
        m_fillHoneycombAct->setChecked(m_fillStyle == FillStyle::Honeycomb);
    }

    const QColor swatch = m_fillColor.isValid() ? m_fillColor : m_brushColor;
    m_fillButton->setIcon(makeFillStyleIcon(m_fillStyle, swatch, 20));
    if (m_fillNoneAct) {
        m_fillNoneAct->setIcon(makeFillStyleIcon(FillStyle::None, swatch, 16));
    }
    if (m_fillSolidAct) {
        m_fillSolidAct->setIcon(makeFillStyleIcon(FillStyle::Solid, swatch, 16));
    }
    if (m_fillHachureAct) {
        m_fillHachureAct->setIcon(makeFillStyleIcon(FillStyle::Hachure, swatch, 16));
    }
    if (m_fillReverseAct) {
        m_fillReverseAct->setIcon(makeFillStyleIcon(FillStyle::ReverseHachure, swatch, 16));
    }
    if (m_fillCrossAct) {
        m_fillCrossAct->setIcon(makeFillStyleIcon(FillStyle::CrossHatch, swatch, 16));
    }
    if (m_fillGridAct) {
        m_fillGridAct->setIcon(makeFillStyleIcon(FillStyle::GridHatch, swatch, 16));
    }
    if (m_fillHoneycombAct) {
        m_fillHoneycombAct->setIcon(makeFillStyleIcon(FillStyle::Honeycomb, swatch, 16));
    }
    const QString colorText = m_fillColor.isValid() ? m_fillColor.name(QColor::HexArgb).toUpper()
                                                    : uiText(QStringLiteral("Follow brush color"));
    m_fillButton->setToolTip(
        uiText(QStringLiteral("Fill: %1 · %2 (click to choose)")).arg(fillStyleName(m_fillStyle), colorText));
    if (m_fillColorAct) {
        m_fillColorAct->setIcon(makeFillColorIcon(swatch, 16));
    }
    if (m_fillColorButton) {
        m_fillColorButton->setToolTip(uiText(QStringLiteral("Fill color: %1 (click to choose)")).arg(colorText));
    }
}

void MainWindow::insertTextAt(const QPointF& scenePos)
{
    auto* item = new TextItem();
    item->setTextFormat(uiText(QStringLiteral("Text")), item->textFont(), m_brushColor);

    const QRectF canvas = m_scene->canvasRect();
    const QSizeF size = item->contentSize();
    qreal x = scenePos.x() - size.width() / 2.0;
    qreal y = scenePos.y() - size.height() / 2.0;
    x = qBound(canvas.left(), x, qMax(canvas.left(), canvas.right() - size.width()));
    y = qBound(canvas.top(), y, qMax(canvas.top(), canvas.bottom() - size.height()));
    item->setPos(QPointF(x, y));

    setToolMode(ToolMode::Edit);
    m_scene->addItemsWithUndo({item}, uiText(QStringLiteral("Add text")));
    item->setSelected(true);
    m_view->setFocus();
    statusBar()->showMessage(uiText(QStringLiteral("Text item added")), 4000);

    // 文字正文现在由属性窗直接编辑；添加后立即打开属性窗，避免再维护独立
    // 的文字输入对话框。
    showItemProperties(item);
}

void MainWindow::showSelectedItemProperties()
{
    const QList<CanvasItem*> items = m_scene->selectedCanvasItems();
    if (items.size() != 1) {
        statusBar()->showMessage(uiText(QStringLiteral("Select a single item to show its properties")), 3000);
        return;
    }
    showItemProperties(items.constFirst());
}

void MainWindow::showItemProperties(CanvasItem* item)
{
    if (!item || item->scene() != m_scene) {
        return;
    }

    // 需求 42：属性窗在任何模式都能打开，但只有编辑模式下且图层可见未锁定
    // 时才允许修改；查看/画笔/文字模式一律以只读方式展示。
    const bool editable = m_view->mode() == ToolMode::Edit && m_scene->isItemEditable(item);

    closeItemProperties();
    ItemPropertiesDialog dialog(m_scene, item, this, editable);
    m_itemPropertiesDialog = &dialog;
    dialog.exec();
    m_itemPropertiesDialog = nullptr;
}

void MainWindow::closeItemProperties()
{
    if (!m_itemPropertiesDialog) {
        return;
    }
    QPointer<ItemPropertiesDialog> dialog = m_itemPropertiesDialog;
    m_itemPropertiesDialog = nullptr;
    if (dialog) {
        dialog->close();
    }
}

void MainWindow::rotateSelected(qreal degrees)
{
    if (m_view->mode() != ToolMode::Edit) {
        statusBar()->showMessage(uiText(QStringLiteral("Rotation is available in edit mode only")), 3000);
        return;
    }
    const int changed =
        m_scene->rotateSelected(degrees, degrees > 0 ? uiText(QStringLiteral("Rotate items clockwise"))
                                                     : uiText(QStringLiteral("Rotate items counterclockwise")));
    if (changed == 0) {
        statusBar()->showMessage(uiText(QStringLiteral("No selected items can be rotated")), 3000);
        return;
    }
    statusBar()->showMessage(uiText(QStringLiteral("Rotated %1 item(s) by %2°")).arg(changed).arg(qAbs(degrees)), 3000);
}

void MainWindow::replaceBackgroundImage()
{
    QString startDirectory = m_settings.lastImageDir;
    if (startDirectory.isEmpty()) {
        startDirectory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }

    const QString path = QFileDialog::getOpenFileName(this, uiText(QStringLiteral("Replace background image")),
                                                      startDirectory, imageFilter());
    if (path.isEmpty()) {
        return;
    }

    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        QMessageBox::warning(this, uiText(QStringLiteral("Replace background")),
                             uiText(QStringLiteral("Could not read the image:\n%1")).arg(reader.errorString()));
        return;
    }

    BackgroundState background;
    background.type = BackgroundType::Image;
    background.image = QPixmap::fromImage(image);
    background.mode =
        m_scene->background().type == BackgroundType::Image ? m_scene->background().mode : BackgroundMode::Stretch;
    m_scene->replaceBackground(background, uiText(QStringLiteral("Replace background image")));

    m_settings.lastImageDir = QFileInfo(path).absolutePath();
    m_settings.save();
    statusBar()->showMessage(uiText(QStringLiteral("Background replaced")), 3000);
}

void MainWindow::setBackgroundColor()
{
    QColor initial(255, 255, 255);
    if (m_scene->background().type == BackgroundType::Color) {
        initial = m_scene->background().color;
    }

    const QColor color = QColorDialog::getColor(initial, this, uiText(QStringLiteral("Choose background color")));
    if (!color.isValid()) {
        return;
    }

    BackgroundState background;
    background.type = BackgroundType::Color;
    background.color = color;
    m_scene->replaceBackground(background, uiText(QStringLiteral("Set background color")));
    statusBar()->showMessage(uiText(QStringLiteral("Background color updated")), 3000);
}

void MainWindow::clearBackground()
{
    m_scene->replaceBackground(BackgroundState(), uiText(QStringLiteral("Clear background")));
    statusBar()->showMessage(uiText(QStringLiteral("Background cleared")), 3000);
}

void MainWindow::applyUiLanguage()
{
    // 先按字典把已经创建出来的文字整体切到目标语言。
    uiRetranslateWidgetTree(this);

    // 形状菜单的文字与提示是拼出来的，字典无法整体命中，按当前语言重新生成。
    if (m_shapeMenu) {
        for (QAction* action : m_shapeMenu->actions()) {
            const ShapeType type = static_cast<ShapeType>(action->data().toInt());
            action->setText(shapeTypeName(type));
            action->setToolTip(
                uiText(QStringLiteral("Insert %1 (uses the current brush color)")).arg(shapeTypeName(type)));
        }
    }

    // 图层面板每行文字（图层名、图元数、锁定/隐藏后缀）是动态拼的，重建一次。
    if (m_layerPanel) {
        m_layerPanel->refresh();
    }

    if (m_view) {
        setBrushColor(m_brushColor, false);
    }
    updateFillActions();
    updateModeUi();
    updateStatusBar();
    updateTitle();
    updateCanvasesMenu();
}

void MainWindow::showSettings()
{
    AppSettings updated;
    UiLanguage requestedLanguage = uiLanguageSetting();
    {
        SettingsDialog dialog(m_settings, this);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        updated = dialog.settings();
        requestedLanguage = dialog.language();
    }

    // 对话框对象已经析构，再切换语言/重建动态文案，避免在已关闭的模态窗上重译。
    if (requestedLanguage != uiLanguageSetting()) {
        setUiLanguageSetting(requestedLanguage);
        applyUiLanguage();
    }
    // 对话框里的设置是主窗口设置的一份完整副本：语言另存，其余整份拷回，
    // 避免新增参数（例如线稿化档位）漏在“改完却记不住”的队伍里。
    m_settings = updated;

    setBrushColor(m_settings.brushColor, false);
    setBrushWidthUi(m_settings.brushWidth);
    setStrokeSmoothingUi(m_settings.strokeSmoothing);
    m_view->setCanvasColor(m_settings.canvasColor);
    m_scene->setShowGrid(m_settings.showGrid);
    m_scene->setCanvasSizeWithUndo(m_settings.canvasSize);

    if (m_view->mode() == ToolMode::View) {
        m_view->fitAll();
    }

    m_settings.save();
    updateStatusBar();
    statusBar()->showMessage(uiText(QStringLiteral("Settings saved")), 3000);
}

void MainWindow::fitAll()
{
    m_view->fitAll();
}

void MainWindow::updateCanvasesMenu()
{
    if (!m_canvasesMenu) {
        return;
    }

    m_canvasesMenu->clear();
    // 需求 37：设置从“画布”菜单移到这里；最近画布列表仍动态更新。
    m_canvasesMenu->addAction(m_settingsAct);
    m_canvasesMenu->addSeparator();
    m_canvasesMenu->addAction(m_saveAct);
    m_canvasesMenu->addAction(m_saveAsAct);
    m_canvasesMenu->addAction(m_openAct);
    // 画板菜单里的“打开画布”后面也放同一个线稿化入口，跟文件菜单保持一致。
    m_canvasesMenu->addAction(m_lineArtAct);
    m_canvasesMenu->addSeparator();

    QDir directory(AppSettings::canvasesDir());
    const QFileInfoList files = directory.entryInfoList({QStringLiteral("*.sbcanvas")}, QDir::Files, QDir::Time);
    if (files.isEmpty()) {
        QAction* emptyAction = m_canvasesMenu->addAction(uiText(QStringLiteral("(Canvas folder is empty)")));
        emptyAction->setEnabled(false);
    } else {
        const int maximum = qMin(15, static_cast<int>(files.size()));
        for (int i = 0; i < maximum; ++i) {
            const QFileInfo info = files.at(i);
            QAction* action =
                m_canvasesMenu->addAction(makeIcon(IconId::Save, QColor(90, 98, 112), 16), info.completeBaseName());
            action->setCheckable(true);
            action->setChecked(info.absoluteFilePath() == m_filePath);
            action->setToolTip(QDir::toNativeSeparators(info.absoluteFilePath()));
            const QString filePath = info.absoluteFilePath();
            connect(action, &QAction::triggered, this, [this, filePath]() { loadCanvasFile(filePath, true); });
        }
        if (files.size() > maximum) {
            QAction* moreAction =
                m_canvasesMenu->addAction(uiText(QStringLiteral("… %1 canvases in total")).arg(files.size()));
            moreAction->setEnabled(false);
        }
    }

    m_canvasesMenu->addSeparator();
    QAction* openFolderAction = m_canvasesMenu->addAction(uiText(QStringLiteral("Open canvases folder")));
    connect(openFolderAction, &QAction::triggered, this, &MainWindow::openCanvasesFolder);
}

void MainWindow::openCanvasesFolder()
{
    const QString directory = AppSettings::canvasesDir();
    QDir().mkpath(directory);
    QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
}

void MainWindow::showAbout()
{
    QString html;
    html += QStringLiteral("<h3>%1 %2</h3>")
                .arg(uiText(QStringLiteral("StoryBoard Canvas")), QCoreApplication::applicationVersion());
    html += QStringLiteral("<p>%1</p>")
                .arg(uiText(QStringLiteral("A lightweight canvas app for freehand strokes, image items, overview "
                                           "navigation, canvas files and export.")));
    html += QStringLiteral("<p><b>%1</b></p><ul>").arg(uiText(QStringLiteral("Key operations")));
    const QStringList operations = {
        QStringLiteral("V View mode: pan / zoom the whole canvas, automatic overview"),
        QStringLiteral("E Edit mode: items on visible, unlocked layers can be edited; locked layers cannot; ] / [ "
                       "adjust the height of the selection; right-click adjusts layer / height"),
        QStringLiteral("B Brush mode: freehand strokes, with color and width options"),
        QStringLiteral("T Text mode: click the canvas to add text, double-click text to edit it again"),
        QStringLiteral("Layers: the checkbox in the right panel toggles visibility, the lock button locks a layer; "
                       "layers and items both support multi-select"),
        QStringLiteral("Insert → Shapes: quickly insert rectangles, ellipses, arrows, braces, speech bubbles and more"),
        QStringLiteral("Ctrl+0 returns to the overview, Ctrl+Z / Ctrl+Y undo / redo"),
    };
    for (const QString& operation : operations) {
        html += QStringLiteral("<li>%1</li>").arg(uiText(operation));
    }
    html += QStringLiteral("</ul><p>%1</p>")
                .arg(uiText(QStringLiteral("Built with Qt %1.")).arg(QString::fromLatin1(qVersion())));
    QMessageBox box(QMessageBox::Information, uiText(QStringLiteral("About StoryBoard Canvas")), html,
                    QMessageBox::NoButton, this);
    box.addButton(uiText(QStringLiteral("OK")), QMessageBox::AcceptRole);
    box.exec();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    m_view->finishCurrentStroke();
    if (!maybeSave()) {
        event->ignore();
        return;
    }

    m_settings.windowGeometry = saveGeometry();
    m_settings.brushColor = m_brushColor;
    m_settings.brushWidth = m_brushWidth;
    if (m_smoothingSpin) {
        m_settings.strokeSmoothing = m_smoothingSpin->value();
    }
    if (m_scene) {
        m_settings.canvasSize = m_scene->canvasSize();
        m_settings.showGrid = m_scene->showGrid();
    }
    m_settings.save();
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }

    for (const QUrl& url : event->mimeData()->urls()) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QString path = url.toLocalFile();
        if (hasImageSuffix(path) || path.endsWith(QStringLiteral(".sbcanvas"), Qt::CaseInsensitive)) {
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    QStringList images;
    QString canvasPath;

    for (const QUrl& url : event->mimeData()->urls()) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QString path = url.toLocalFile();
        if (path.endsWith(QStringLiteral(".sbcanvas"), Qt::CaseInsensitive)) {
            canvasPath = path;
        } else if (hasImageSuffix(path)) {
            images.append(path);
        }
    }

    if (!canvasPath.isEmpty() && images.isEmpty()) {
        if (loadCanvasFile(canvasPath, true)) {
            event->acceptProposedAction();
        }
        return;
    }

    if (!images.isEmpty()) {
        importImageFiles(images);
        event->acceptProposedAction();
    }
}
