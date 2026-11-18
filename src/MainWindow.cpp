#include "MainWindow.h"

#include "BackgroundController.h"
#include "CanvasScene.h"
#include "CanvasView.h"
#include "CustomShapePanel.h"
#include "DockPanelController.h"
#include "DocumentController.h"
#include "ImportExportController.h"
#include "ItemPropertiesDialog.h"
#include "MainWindowAppController.h"
#include "MainWindowEditActionsController.h"
#include "MainWindowInsertController.h"
#include "ShortcutController.h"
#include "StyleController.h"
#include "UiBuilderController.h"
#include "ViewStatusController.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QStatusBar>
#include <QTimer>
#include <QUndoStack>
#include <QtMath>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));
    setMinimumSize(1080, 700);
    setAcceptDrops(true);

    m_settings.load();
    m_lineArtSettings = m_settings;
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

    m_background = std::make_unique<BackgroundController>(this);
    m_docks = std::make_unique<DockPanelController>(this);
    m_shortcuts = std::make_unique<ShortcutController>(this);
    m_document = std::make_unique<DocumentController>(this);
    m_io = std::make_unique<ImportExportController>(this);
    m_style = std::make_unique<StyleController>(this);
    m_viewStatus = std::make_unique<ViewStatusController>(this);
    m_ui = std::make_unique<UiBuilderController>(this);
    m_editActions = std::make_unique<MainWindowEditActionsController>(this);
    m_insert = std::make_unique<MainWindowInsertController>(this);
    m_app = std::make_unique<MainWindowAppController>(this);

    createActions();
    createShapeMenu();
    m_docks->createLayerDock();
    m_docks->createItemDock();
    m_docks->createViewModeDock();
    createMenus();
    createToolBars();
    m_docks->attachViewModeToolBar();
    m_docks->applyDockLayout();
    createStatusBar();
    connectSignals();
    m_shortcuts->applyShortcutOverrides();

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
            // 需求 1：启动保持 fitAll 的缩放不变，只把视口居中到画布中心，
            // 让首屏优先落在画布最中间的区域。
            if (m_scene) {
                m_view->centerOn(m_scene->canvasRect().center());
            }
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
    m_ui->buildActions();
}

void MainWindow::createShapeMenu()
{
    m_ui->buildShapeMenu();
}

void MainWindow::updateCustomShapeMenu()
{
    m_insert->updateCustomShapeMenu();
}

// 需求 7：自定义图形面板是独立浮动窗口，按钮/菜单只负责把它展开。
void MainWindow::openCustomShapePanel(QWidget* anchor)
{
    if (!m_customShapePanel) {
        m_customShapePanel = new CustomShapePanel(this);
    }
    m_customShapePanel->showAt(anchor);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (m_docks) {
        m_docks->handleMoveEvent(watched, event);
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::createMenus()
{
    m_ui->buildMenus();
}

void MainWindow::createToolBars()
{
    m_ui->buildToolBars();
}

void MainWindow::createStatusBar()
{
    m_ui->buildStatusBar();
}

void MainWindow::connectSignals()
{
    connect(m_undo, &QUndoStack::canUndoChanged, this, &MainWindow::updateActions);
    connect(m_undo, &QUndoStack::canRedoChanged, this, &MainWindow::updateActions);
    connect(m_undo, &QUndoStack::cleanChanged, this, &MainWindow::updateTitle);
    connect(m_undo, &QUndoStack::undoTextChanged, this, [this](const QString& text) {
        m_undoAct->setToolTip(text.isEmpty() ? tr("Undo") : tr("Undo: %1").arg(text));
    });
    connect(m_undo, &QUndoStack::redoTextChanged, this, [this](const QString& text) {
        m_redoAct->setToolTip(text.isEmpty() ? tr("Redo") : tr("Redo: %1").arg(text));
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
    connect(m_scene, &CanvasScene::selectionChanged, this, &MainWindow::updateItemDockVisibility);
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &MainWindow::updateActions);
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
    m_viewStatus->setToolMode(mode);
}

void MainWindow::updateModeUi()
{
    m_viewStatus->updateModeUi();
}

void MainWindow::updateActions()
{
    m_viewStatus->updateActions();
}

void MainWindow::updateBackgroundModeActions()
{
    m_viewStatus->updateBackgroundModeActions();
}

void MainWindow::updateStatusBar()
{
    m_viewStatus->updateStatusBar();
}

void MainWindow::updateTitle()
{
    m_document->updateTitle();
}

void MainWindow::setBrushColor(const QColor& color, bool remember)
{
    m_style->setBrushColor(color, remember);
}

void MainWindow::setBrushWidthUi(int width)
{
    m_style->setBrushWidthUi(width);
}

void MainWindow::applyBrushWidth(int width)
{
    setBrushWidthUi(width);
    m_settings.brushWidth = m_brushWidth;
    m_settings.save();
    m_style->applyStrokeWidthToSelection(m_brushWidth);
}

void MainWindow::setStrokeSmoothingUi(qreal tolerance)
{
    m_style->setStrokeSmoothingUi(tolerance);
}

void MainWindow::chooseBrushColor()
{
    m_style->chooseBrushColor();
}

void MainWindow::newCanvas()
{
    m_document->newCanvas();
}

void MainWindow::openCanvasDialog()
{
    m_document->openCanvasDialog();
}

bool MainWindow::openCanvasFile(const QString& path)
{
    return m_document->openCanvasFile(path);
}

bool MainWindow::saveCanvas()
{
    return m_document->saveCanvas();
}

bool MainWindow::saveCanvasAs()
{
    return m_document->saveCanvasAs();
}

void MainWindow::exportCanvas()
{
    m_io->exportCanvas();
}

void MainWindow::exportExcalidraw()
{
    m_document->exportExcalidraw();
}

void MainWindow::exportCustomShape()
{
    m_document->exportCustomShape();
}

void MainWindow::importImagesDialog()
{
    m_io->importImagesDialog();
}

void MainWindow::importOutlineImageDialog()
{
    m_io->importOutlineImageDialog();
}

void MainWindow::insertBrace(bool left)
{
    m_insert->insertBrace(left);
}

void MainWindow::insertShape(ShapeType type, qreal triangleAngle1, qreal triangleAngle2, ArrowStyle startArrow,
                             ArrowStyle endArrow, bool autoCorner)
{
    m_insert->insertShape(type, triangleAngle1, triangleAngle2, startArrow, endArrow, autoCorner);
}

void MainWindow::insertCustomShape(const QString& filePath)
{
    m_insert->insertCustomShape(filePath);
}

void MainWindow::openShapesFolder()
{
    m_insert->openShapesFolder();
}

void MainWindow::updateItemDockVisibility()
{
    m_viewStatus->updateItemDockVisibility();
}

void MainWindow::setFillStyle(FillStyle style)
{
    m_style->setFillStyle(style);
}

void MainWindow::chooseFillColor()
{
    m_style->chooseFillColor();
}

void MainWindow::chooseStrokeColor()
{
    m_style->chooseStrokeColor();
}

void MainWindow::updateFillActions()
{
    m_style->updateFillActions();
}

void MainWindow::insertTextAt(const QPointF& scenePos)
{
    m_insert->insertTextAt(scenePos);
}

void MainWindow::showSelectedItemProperties()
{
    m_editActions->showSelectedItemProperties();
}

void MainWindow::showItemProperties(CanvasItem* item)
{
    m_editActions->showItemProperties(item);
}

void MainWindow::closeItemProperties()
{
    m_editActions->closeItemProperties();
}

void MainWindow::rotateSelected(qreal degrees)
{
    m_editActions->rotateSelected(degrees);
}

void MainWindow::applyAlignment(Alignment::Mode mode)
{
    m_editActions->applyAlignment(mode);
}

void MainWindow::applyBoolOperation(BoolOps::Operation operation)
{
    m_editActions->applyBoolOperation(operation);
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    // 合并图元和弦：Ctrl+M 之后 1 秒内按 U / X / I / S / F / L 直接执行对应子命令。
    if (m_boolChordPending) {
        m_boolChordPending = false;
        switch (event->key()) {
        case Qt::Key_U:
            applyBoolOperation(BoolOps::Operation::Union);
            return;
        case Qt::Key_X:
            applyBoolOperation(BoolOps::Operation::Xor);
            return;
        case Qt::Key_I:
            applyBoolOperation(BoolOps::Operation::Intersect);
            return;
        case Qt::Key_S:
            applyBoolOperation(BoolOps::Operation::Subtract);
            return;
        case Qt::Key_F:
            applyBoolOperation(BoolOps::Operation::Fragment);
            return;
        case Qt::Key_L:
            applyBoolOperation(BoolOps::Operation::LineSplit);
            return;
        default:
            break;
        }
    }
    if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_M) {
        m_boolChordPending = true;
        statusBar()->showMessage(tr("Merge items: press U / X / I / S / F / L within 1 second"), 1000);
        QTimer::singleShot(1000, this, [this]() { m_boolChordPending = false; });
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::replaceBackgroundImage()
{
    m_background->replaceBackgroundImage();
}

void MainWindow::setBackgroundColor()
{
    m_background->setBackgroundColor();
}

void MainWindow::clearBackground()
{
    m_background->clearBackground();
}

void MainWindow::applyUiLanguage()
{
    m_app->applyUiLanguage();
}

void MainWindow::showSettings()
{
    m_app->showSettings();
}

void MainWindow::updateCanvasesMenu()
{
    m_document->updateCanvasesMenu();
}

void MainWindow::openCanvasesFolder()
{
    m_document->openCanvasesFolder();
}

void MainWindow::showShortcuts()
{
    m_shortcuts->showShortcuts();
}

void MainWindow::showAbout()
{
    m_app->showAbout();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    m_view->finishCurrentStroke();
    if (!m_document->maybeSave()) {
        event->ignore();
        return;
    }

    m_app->persistBeforeExit();
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (m_io->acceptsDropUrls(event->mimeData())) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    m_io->handleDrop(event);
}
