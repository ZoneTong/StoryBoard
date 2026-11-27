#include "ui/mainwindow/MainWindow.h"

#include "app/AutoSaveService.h"
#include "app/CanvasSession.h"
#include "app/CanvasTabsController.h"
#include "app/DockPanelController.h"
#include "app/DocumentController.h"
#include "app/FullScreenController.h"
#include "app/ShortcutController.h"
#include "canvas/CanvasView.h"
#include "canvas/controllers/CanvasBackgroundController.h"
#include "io/ImportExportController.h"
#include "scene/CanvasScene.h"
#include "ui/builder/UiBuilderController.h"
#include "ui/mainwindow/MainWindowAppController.h"
#include "ui/mainwindow/MainWindowEditActionsController.h"
#include "ui/mainwindow/MainWindowInsertController.h"
#include "ui/panels/customshape/CustomShapePanel.h"
#include "ui/widgets/StyleController.h"
#include "ui/widgets/ViewStatusController.h"

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

    // 首份文档由 CanvasSession 持有（场景 / 视图 / 撤销栈都不挂窗口 parent），
    // 中央区换成一个 QTabWidget，第一页就是这份文档的视图。
    m_scene = new CanvasScene;
    m_scene->ApplyCanvasSize(m_settings.canvasSize);
    m_scene->SetShowGrid(m_settings.showGrid);

    m_undo = new QUndoStack;
    m_undo->setUndoLimit(300);
    m_scene->SetUndoStack(m_undo);

    m_view = new CanvasView(m_scene);
    m_view->SetBrushColor(m_brushColor);
    m_view->SetBrushWidth(m_brushWidth);
    m_view->SetStrokeSmoothing(m_settings.strokeSmoothing);
    m_view->SetCanvasColor(m_settings.canvasColor);

    m_tabs = std::make_unique<CanvasTabsController>(this);
    m_autoSave = std::make_unique<AutoSaveService>(this);
    m_tabs->AdoptSession(new CanvasSession(this, m_scene, m_view, m_undo, 1, 1));
    setCentralWidget(m_tabs->Tabs());

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
    // 全屏控制器最后建：它要在菜单、工具栏、停靠区都存在之后才能快照界面状态。
    m_fullScreen = std::make_unique<FullScreenController>(this);
    m_shortcuts->ApplyShortcutOverrides();
    // 键盘事件在焦点控件未消费时会上冒到窗口，这里挂自己才能在冒泡阶段收到数字键。
    installEventFilter(this);

    if (!m_settings.windowGeometry.isEmpty()) {
        restoreGeometry(m_settings.windowGeometry);
    } else {
        resize(1440, 900);
    }

    if (m_tabs && m_tabs->Current()) {
        ActivateSession(m_tabs->Current());
    }
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
    // 场景析构还要 clear()，期间仍会发 selectionChanged / layersChanged 等信号；
    // 面板类子对象若还连着这些信号，就会在窗口析构体跑完后操作正在销毁的行控件。
    // 统一掐掉场景作为发送方的所有连接，让销毁期不再回调任何界面对象。
    if (m_scene) {
        QObject::disconnect(m_scene, nullptr, nullptr, nullptr);
    }

    // 逐份会话做同样的清理（画布 / 撤销栈都在会话里，由它负责按顺序析构）。
    if (m_tabs) {
        for (CanvasSession* session : m_tabs->Sessions()) {
            if (session->Scene()) {
                QObject::disconnect(session->Scene(), nullptr, nullptr, nullptr);
            }
            if (session->Undo()) {
                QObject::disconnect(session->Undo(), nullptr, this, nullptr);
            }
        }
    }
    // 先拆掉缓存了场景 / 视图指针的控制器，再删会话。面板、文档控制器、
    // 全屏栏都持有当前场景或视图指针，倒序析构时会回调到已释放的场景上。
    if (m_docks) {
        m_docks->SetScene(nullptr);
    }
    m_viewStatus.reset();
    m_style.reset();
    m_editActions.reset();
    m_insert.reset();
    m_app.reset();
    m_fullScreen.reset();
    m_document.reset();
    m_shortcuts.reset();
    m_io.reset();
    m_ui.reset();
    m_docks.reset();
    m_background.reset();
    if (m_autoSave) {
        m_autoSave->ForgetAll();
        m_autoSave.reset();
    }
    if (m_tabs) {
        m_tabs->ClearAll();
    }
    // QMainWindow 的布局仍记着中央控件；直接删会留下悬空项，先摘下来。
    if (QWidget* central = takeCentralWidget()) {
        central->setParent(nullptr);
    }
    m_tabs.reset();
    m_scene = nullptr;
    m_view = nullptr;
    m_undo = nullptr;
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
    // 全屏期窗口尺寸变化时把悬浮工具栏重新居中。
    if (m_fullScreen) {
        m_fullScreen->HandleWindowEvent(watched, event);
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
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &MainWindow::UpdateActions);
}

void MainWindow::AttachSession(CanvasSession* session)
{
    if (!session) {
        return;
    }
    CanvasScene* scene = session->Scene();
    CanvasView* view = session->View();
    QUndoStack* undo = session->Undo();
    auto isCurrent = [this, session]() { return m_tabs && m_tabs->Current() == session; };

    if (undo) {
        connect(undo, &QUndoStack::canUndoChanged, this, [this, isCurrent]() {
            if (isCurrent()) {
                UpdateActions();
            }
        });
        connect(undo, &QUndoStack::canRedoChanged, this, [this, isCurrent]() {
            if (isCurrent()) {
                UpdateActions();
            }
        });
        connect(undo, &QUndoStack::cleanChanged, this, [this, session, isCurrent](bool) {
            if (m_tabs) {
                m_tabs->UpdateTabLabel(session);
            }
            if (isCurrent()) {
                UpdateTitle();
            }
        });
        connect(undo, &QUndoStack::undoTextChanged, this, [this, isCurrent](const QString& text) {
            if (isCurrent() && m_undoAct) {
                m_undoAct->setToolTip(text.isEmpty() ? tr("Undo") : tr("Undo: %1").arg(text));
            }
        });
        connect(undo, &QUndoStack::redoTextChanged, this, [this, isCurrent](const QString& text) {
            if (isCurrent() && m_redoAct) {
                m_redoAct->setToolTip(text.isEmpty() ? tr("Redo") : tr("Redo: %1").arg(text));
            }
        });
    }

    if (view) {
        // 首份会话构造时工具栏动作还没建出来，等 BuildToolBars 之后统一挂。
        if (!m_boolActions.isEmpty()) {
            ApplyViewContextMenus(view);
        }
        connect(view, &CanvasView::zoomChanged, this, [this, isCurrent](qreal zoom) {
            if (isCurrent() && m_zoomBadge) {
                m_zoomBadge->setText(QStringLiteral("%1%").arg(qRound(zoom * 100.0)));
            }
        });
        connect(view, &CanvasView::modeChanged, this, [this, isCurrent](ToolMode) {
            if (isCurrent()) {
                UpdateModeUi();
            }
        });
        connect(view, &CanvasView::statusMessage, this, [this, isCurrent](const QString& message) {
            if (isCurrent()) {
                statusBar()->showMessage(message, 4000);
            }
        });
        connect(view, &CanvasView::textPlacementRequested, this, [this, isCurrent](const QPointF& scenePos) {
            if (isCurrent()) {
                InsertTextAt(scenePos);
            }
        });
    }

    if (scene) {
        connect(scene, &CanvasScene::contentChanged, this, [this, isCurrent]() {
            if (isCurrent()) {
                UpdateActions();
                UpdateStatusBar();
            }
        });
        connect(scene, &CanvasScene::layersChanged, this, [this, isCurrent]() {
            if (isCurrent()) {
                UpdateStatusBar();
            }
        });
        connect(scene, &CanvasScene::activeLayerChanged, this, [this, isCurrent](int) {
            if (isCurrent()) {
                UpdateStatusBar();
            }
        });
        connect(scene, &CanvasScene::selectionChanged, this, [this, isCurrent]() {
            if (isCurrent()) {
                UpdateActions();
                UpdatePropertiesPanelDockVisibility();
            }
        });
        connect(scene, &CanvasScene::backgroundChanged, this, [this, isCurrent]() {
            if (isCurrent()) {
                UpdateBackgroundModeActions();
                UpdateActions();
            }
        });
        connect(scene, &CanvasScene::canvasSizeChanged, this, [this, isCurrent, view](const QSizeF&) {
            if (isCurrent() && view && view->mode() == ToolMode::VIEW) {
                view->FitAll();
            }
        });
    }
}

QString MainWindow::CurrentCanvasName() const
{
    if (m_tabs && m_tabs->Current()) {
        return m_tabs->Current()->DisplayName();
    }
    return m_document ? m_document->DocumentName() : QString();
}

bool MainWindow::CurrentCanvasDirty() const
{
    return m_tabs && m_tabs->Current() && m_tabs->Current()->HasUnsavedChanges();
}

void MainWindow::ActivateSession(CanvasSession* session)
{
    if (!session) {
        return;
    }
    if (m_view && m_view != session->View()) {
        m_view->FinishCurrentStroke();
        m_view->EndTextEdit();
    }
    m_scene = session->Scene();
    m_view = session->View();
    m_undo = session->Undo();
    m_filePath = session->FilePath();
    if (m_autoSave) {
        m_autoSave->Watch(session);
    }
    if (m_docks) {
        m_docks->SetScene(m_scene);
    }
    UpdateModeUi();
    UpdateActions();
    UpdateStatusBar();
    UpdateTitle();
    UpdateBackgroundModeActions();
    UpdatePropertiesPanelDockVisibility();
    if (m_view) {
        m_view->setFocus();
    }
    if (m_fullScreen) {
        m_fullScreen->RefreshCanvasName();
    }
}

int MainWindow::OpenCanvasFiles(const QStringList& paths)
{
    return m_document ? m_document->OpenCanvasFiles(paths) : 0;
}

DocumentController* MainWindow::Document() const
{
    return m_document.get();
}

bool MainWindow::CloseCanvasSession(CanvasSession* session)
{
    return m_document && m_document->CloseCanvasSession(session);
}

void MainWindow::ForEachView(const std::function<void(CanvasView*)>& apply)
{
    if (!m_tabs) {
        return;
    }
    for (CanvasSession* session : m_tabs->Sessions()) {
        if (session && session->View()) {
            apply(session->View());
        }
    }
}

void MainWindow::ForEachScene(const std::function<void(CanvasScene*)>& apply)
{
    if (!m_tabs) {
        return;
    }
    for (CanvasSession* session : m_tabs->Sessions()) {
        if (session && session->Scene()) {
            apply(session->Scene());
        }
    }
}

void MainWindow::ApplyViewContextMenus(CanvasView* view)
{
    if (!view || m_boolActions.isEmpty() || view->property("StoryBoardContextMenus").toBool()) {
        return;
    }
    view->setProperty("StoryBoardContextMenus", true);
    view->AddContextMenuGroup(tr("Merge items"), m_boolActions);
}

void MainWindow::SwitchCanvasNext()
{
    if (m_tabs) {
        m_tabs->Next();
    }
}

void MainWindow::SwitchCanvasPrevious()
{
    if (m_tabs) {
        m_tabs->Previous();
    }
}

void MainWindow::SwitchCanvasNumber(int oneBasedNumber)
{
    if (m_tabs) {
        m_tabs->SwitchToNumber(oneBasedNumber);
    }
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

void MainWindow::ToggleFullScreen()
{
    if (m_fullScreen) {
        m_fullScreen->Toggle();
    }
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    if (m_fullScreen) {
        m_fullScreen->HandleWindowEvent(this, event);
    }
}

void MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    // 全屏状态被外部改动（窗口管理器踢出、其它程序抢占）时同步界面，避免菜单栏还藏着。
    if (event->type() == QEvent::WindowStateChange && m_fullScreen) {
        m_fullScreen->HandleWindowStateChange();
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_view) {
        m_view->FinishCurrentStroke();
    }
    // 多画布：逐页确认未保存改动，任一取消就放弃整个关闭。
    if (!m_document->CloseAllCanvases()) {
        event->ignore();
        return;
    }

    // 先退出全屏再落盘，否则会把全屏几何与浮层位置写进设置文件。
    if (m_fullScreen) {
        m_fullScreen->ExitIfActive();
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
