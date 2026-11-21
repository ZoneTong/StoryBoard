#include "MainWindow.h"

#include "CanvasBackgroundController.h"
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
#include <QLineEdit>
#include <QSpinBox>
#include <QStatusBar>
#include <QTimer>
#include <QUndoStack>
#include <QtMath>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));
    setMinimumSize(1080, 700);
    setAcceptDrops(true);

    m_settings.Load();
    m_lineArtSettings = m_settings;
    m_brushColor = m_settings.brushColor;
    m_brushWidth = m_settings.brushWidth;
    // 需求 16：填充色默认取中灰（填充样式默认“无填充”，切到纯色时才可见）。
    m_fillColor = QColor(0x9C, 0xA3, 0xAF);

    m_scene = new CanvasScene(this);
    m_scene->ApplyCanvasSize(m_settings.canvasSize);
    m_scene->SetShowGrid(m_settings.showGrid);

    m_undo = new QUndoStack(this);
    m_undo->setUndoLimit(300);
    m_scene->SetUndoStack(m_undo);

    m_view = new CanvasView(m_scene, this);
    m_view->SetBrushColor(m_brushColor);
    m_view->SetBrushWidth(m_brushWidth);
    m_view->SetStrokeSmoothing(m_settings.strokeSmoothing);
    m_view->SetCanvasColor(m_settings.canvasColor);
    setCentralWidget(m_view);

    m_background = std::make_unique<CanvasBackgroundController>(this);
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

    CreateActions();
    CreateShapeMenu();
    m_docks->CreateLayerDock();
    m_docks->CreatePropertiesPanelDock();
    m_docks->CreateViewModeDock();
    CreateMenus();
    CreateToolBars();
    m_docks->AttachViewModeToolBar();
    m_docks->ApplyDockLayout();
    CreateStatusBar();
    ConnectSignals();
    m_shortcuts->ApplyShortcutOverrides();

    if (!m_settings.windowGeometry.isEmpty()) {
        restoreGeometry(m_settings.windowGeometry);
    } else {
        resize(1440, 900);
    }

    UpdateModeUi();
    UpdateTitle();
    UpdateStatusBar();
    UpdateActions();
    UpdateBackgroundModeActions();

    QTimer::singleShot(0, this, [this]() {
        if (m_view) {
            m_view->FitAll();
            // 需求 1：启动保持 fitAll 的缩放不变，只把视口居中到画布中心，
            // 让首屏优先落在画布最中间的区域。
            if (m_scene) {
                m_view->centerOn(m_scene->CanvasRect().center());
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
        m_scene->SetUndoStack(nullptr);
    }
}

void MainWindow::CreateActions()
{
    m_ui->BuildActions();
}

void MainWindow::CreateShapeMenu()
{
    m_ui->BuildShapeMenu();
}

void MainWindow::UpdateCustomShapeMenu()
{
    m_insert->UpdateCustomShapeMenu();
}

// 需求 7：自定义图形面板是独立浮动窗口，按钮/菜单只负责把它展开。
void MainWindow::OpenCustomShapePanel(QWidget* anchor)
{
    if (!m_customShapePanel) {
        m_customShapePanel = new CustomShapePanel(this);
    }
    m_customShapePanel->ShowAt(anchor);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (m_docks) {
        m_docks->HandleMoveEvent(watched, event);
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::CreateMenus()
{
    m_ui->BuildMenus();
}

void MainWindow::CreateToolBars()
{
    m_ui->BuildToolBars();
}

void MainWindow::CreateStatusBar()
{
    m_ui->BuildStatusBar();
}

void MainWindow::ConnectSignals()
{
    connect(m_undo, &QUndoStack::canUndoChanged, this, &MainWindow::UpdateActions);
    connect(m_undo, &QUndoStack::canRedoChanged, this, &MainWindow::UpdateActions);
    connect(m_undo, &QUndoStack::cleanChanged, this, &MainWindow::UpdateTitle);
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
    connect(m_view, &CanvasView::modeChanged, this, [this](ToolMode) { UpdateModeUi(); });
    connect(m_view, &CanvasView::statusMessage, this,
            [this](const QString& message) { statusBar()->showMessage(message, 4000); });
    connect(m_view, &CanvasView::textPlacementRequested, this, &MainWindow::InsertTextAt);
    connect(m_view, &CanvasView::itemPropertiesRequested, this, &MainWindow::ShowItemProperties);

    connect(m_scene, &CanvasScene::contentChanged, this, [this]() {
        if (m_itemPropertiesDialog &&
            (!m_itemPropertiesDialog->Item() || m_itemPropertiesDialog->Item()->scene() != m_scene)) {
            CloseItemProperties();
        }
        UpdateActions();
        UpdateStatusBar();
    });
    connect(m_scene, &CanvasScene::layersChanged, this, &MainWindow::UpdateStatusBar);
    connect(m_scene, &CanvasScene::activeLayerChanged, this, [this](int) { UpdateStatusBar(); });
    connect(m_scene, &CanvasScene::selectionChanged, this, &MainWindow::UpdateActions);
    connect(m_scene, &CanvasScene::selectionChanged, this, &MainWindow::UpdatePropertiesPanelDockVisibility);
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &MainWindow::UpdateActions);
    connect(m_scene, &CanvasScene::backgroundChanged, this, [this]() {
        UpdateBackgroundModeActions();
        UpdateActions();
    });
    connect(m_scene, &CanvasScene::canvasSizeChanged, this, [this](const QSizeF&) {
        if (m_view->mode() == ToolMode::VIEW) {
            m_view->FitAll();
        }
    });
}

void MainWindow::SetToolMode(ToolMode mode)
{
    m_viewStatus->SetToolMode(mode);
}

void MainWindow::UpdateModeUi()
{
    m_viewStatus->UpdateModeUi();
}

void MainWindow::UpdateActions()
{
    m_viewStatus->UpdateActions();
}

void MainWindow::UpdateBackgroundModeActions()
{
    m_viewStatus->UpdateBackgroundModeActions();
}

void MainWindow::UpdateStatusBar()
{
    m_viewStatus->UpdateStatusBar();
}

void MainWindow::UpdateTitle()
{
    m_document->UpdateTitle();
}

void MainWindow::SetBrushColor(const QColor& color, bool remember)
{
    m_style->SetBrushColor(color, remember);
}

void MainWindow::SetBrushWidthUi(int width)
{
    m_style->SetBrushWidthUi(width);
}

void MainWindow::ApplyBrushWidth(int width)
{
    SetBrushWidthUi(width);
    m_settings.brushWidth = m_brushWidth;
    m_settings.Save();
    m_style->ApplyStrokeWidthToSelection(m_brushWidth);
}

void MainWindow::SetStrokeSmoothingUi(qreal tolerance)
{
    m_style->SetStrokeSmoothingUi(tolerance);
}

void MainWindow::SetStrokeStyle(Qt::PenStyle style)
{
    m_style->SetStrokeStyle(style);
}

void MainWindow::FocusStrokeWidthSpin()
{
    if (!m_widthSpin) {
        return;
    }
    // 需求 1：菜单“描线宽度”是普通菜单项，触发后把焦点放到工具栏宽度输入框末尾。
    // 窗口已激活时 activateWindow() 无副作用；未激活时先激活再 setFocus，避免焦点丢失。
    activateWindow();
    m_widthSpin->setFocus(Qt::ShortcutFocusReason);
    // lineEdit() 是保护接口，直接取 SpinBox 内部的编辑器来移动光标。
    if (auto* edit = m_widthSpin->findChild<QLineEdit*>()) {
        edit->end(false);
    }
}

void MainWindow::ChooseBrushColor()
{
    m_style->ChooseBrushColor();
}

void MainWindow::NewCanvas()
{
    m_document->NewCanvas();
}

void MainWindow::OpenCanvasDialog()
{
    m_document->OpenCanvasDialog();
}

bool MainWindow::OpenCanvasFile(const QString& path)
{
    return m_document->OpenCanvasFile(path);
}

bool MainWindow::SaveCanvas()
{
    return m_document->SaveCanvas();
}

bool MainWindow::SaveCanvasAs()
{
    return m_document->SaveCanvasAs();
}

void MainWindow::ExportCanvas()
{
    m_io->ExportCanvas();
}

void MainWindow::ExportExcalidraw()
{
    m_document->ExportExcalidraw();
}

void MainWindow::ExportCustomShape()
{
    m_document->ExportCustomShape();
}

void MainWindow::ImportImagesDialog()
{
    m_io->ImportImagesDialog();
}

void MainWindow::ImportOutlineImageDialog()
{
    m_io->ImportOutlineImageDialog();
}

void MainWindow::InsertBrace(bool left)
{
    m_insert->InsertBrace(left);
}

void MainWindow::InsertShape(ShapeType type, qreal triangleAngle1, qreal triangleAngle2, qreal cornerRatio)
{
    m_insert->InsertShape(type, triangleAngle1, triangleAngle2, cornerRatio);
}

void MainWindow::InsertCustomShape(const QString& filePath)
{
    m_insert->InsertCustomShape(filePath);
}

void MainWindow::OpenShapesFolder()
{
    m_insert->OpenShapesFolder();
}

void MainWindow::UpdatePropertiesPanelDockVisibility()
{
    m_viewStatus->UpdatePropertiesPanelDockVisibility();
}

void MainWindow::SetFillStyle(FillStyle style)
{
    m_style->SetFillStyle(style);
}

void MainWindow::ChooseFillColor()
{
    m_style->ChooseFillColor();
}

void MainWindow::ChooseStrokeColor()
{
    m_style->ChooseStrokeColor();
}

void MainWindow::UpdateFillActions()
{
    m_style->UpdateFillActions();
}

void MainWindow::InsertTextAt(const QPointF& scenePos)
{
    m_insert->InsertTextAt(scenePos);
}

void MainWindow::ShowSelectedItemProperties()
{
    m_editActions->ShowSelectedItemProperties();
}

void MainWindow::ShowItemProperties(CanvasItem* item)
{
    m_editActions->ShowItemProperties(item);
}

void MainWindow::CloseItemProperties()
{
    m_editActions->CloseItemProperties();
}

void MainWindow::RotateSelected(qreal degrees)
{
    m_editActions->RotateSelected(degrees);
}

void MainWindow::ApplyAlignment(Alignment::Mode mode)
{
    m_editActions->ApplyAlignment(mode);
}

void MainWindow::ApplyBoolOperation(BoolOps::Operation operation)
{
    m_editActions->ApplyBoolOperation(operation);
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    // 合并图元和弦：Ctrl+M 之后 1 秒内按 U / X / I / S / F / L 直接执行对应子命令。
    if (m_boolChordPending) {
        m_boolChordPending = false;
        switch (event->key()) {
        case Qt::Key_U:
            ApplyBoolOperation(BoolOps::Operation::UNION);
            return;
        case Qt::Key_X:
            ApplyBoolOperation(BoolOps::Operation::XOR);
            return;
        case Qt::Key_I:
            ApplyBoolOperation(BoolOps::Operation::INTERSECT);
            return;
        case Qt::Key_S:
            ApplyBoolOperation(BoolOps::Operation::SUBTRACT);
            return;
        case Qt::Key_F:
            ApplyBoolOperation(BoolOps::Operation::FRAGMENT);
            return;
        case Qt::Key_L:
            ApplyBoolOperation(BoolOps::Operation::LINE_SPLIT);
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

void MainWindow::ReplaceCanvasBackgroundImage()
{
    m_background->ReplaceCanvasBackgroundImage();
}

void MainWindow::SetBackgroundColor()
{
    m_background->SetBackgroundColor();
}

void MainWindow::ClearBackground()
{
    m_background->ClearBackground();
}

void MainWindow::ApplyUiLanguage()
{
    m_app->ApplyUiLanguage();
}

void MainWindow::ShowSettings()
{
    m_app->ShowSettings();
}

void MainWindow::UpdateCanvasesMenu()
{
    m_document->UpdateCanvasesMenu();
}

void MainWindow::OpenCanvasesFolder()
{
    m_document->OpenCanvasesFolder();
}

void MainWindow::ShowShortcuts()
{
    m_shortcuts->ShowShortcuts();
}

void MainWindow::ShowAbout()
{
    m_app->ShowAbout();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    m_view->FinishCurrentStroke();
    if (!m_document->MaybeSave()) {
        event->ignore();
        return;
    }

    m_app->PersistBeforeExit();
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (m_io->AcceptsDropUrls(event->mimeData())) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    m_io->HandleDrop(event);
}
