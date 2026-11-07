#include "MainWindow.h"

#include "BoardScene.h"
#include "BoardView.h"
#include "Commands.h"
#include "ExportDialog.h"
#include "IconFactory.h"
#include "ImageItem.h"
#include "LayerPanel.h"
#include "SettingsDialog.h"
#include "ShapeItem.h"
#include "TextDialog.h"
#include "TextItem.h"

#include <QAction>
#include <QActionGroup>
#include <QAbstractButton>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
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
#include <QPdfWriter>
#include <QPushButton>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>
#include <QUrl>
#include <QtMath>

namespace {

const char* kImageFilter = "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif);;所有文件 (*.*)";
const char* kTemplateFilter = "StoryBoard 模板 (*.sbboard);;所有文件 (*.*)";

QAction* makeAction(QObject* parent, IconId icon, const QString& text,
                    const QKeySequence& shortcut = QKeySequence(),
                    const QString& tip = QString())
{
    auto* action = new QAction(makeIcon(icon), text, parent);
    if (!shortcut.isEmpty())
        action->setShortcut(shortcut);
    action->setToolTip(tip.isEmpty() ? text : tip);
    action->setStatusTip(tip.isEmpty() ? text : tip);
    return action;
}

bool hasImageSuffix(const QString& path)
{
    static const QStringList suffixes = {
        QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
        QStringLiteral("bmp"), QStringLiteral("gif")
    };
    return suffixes.contains(QFileInfo(path).suffix().toLower());
}

bool validateTemplateRoot(const QJsonObject& root, QString* error)
{
    if (root.value(QStringLiteral("format")).toString() != QStringLiteral("StoryBoard")) {
        if (error)
            *error = QStringLiteral("这不是有效的 StoryBoard 模板文件。");
        return false;
    }

    const QJsonObject board = root.value(QStringLiteral("board")).toObject();
    if (board.value(QStringLiteral("width")).toInt() <= 0
        || board.value(QStringLiteral("height")).toInt() <= 0) {
        if (error)
            *error = QStringLiteral("模板中的画板尺寸无效。");
        return false;
    }

    const QJsonArray items = root.value(QStringLiteral("items")).toArray();
    for (const QJsonValue& value : items) {
        const QString kind = value.toObject().value(QStringLiteral("type")).toString();
        if (kind != QStringLiteral("stroke") && kind != QStringLiteral("image")
            && kind != QStringLiteral("text") && kind != QStringLiteral("shape")) {
            if (error)
                *error = QStringLiteral("模板中包含未知的图元类型：%1").arg(kind);
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

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));
    setMinimumSize(1080, 700);
    setAcceptDrops(true);

    m_settings.load();
    m_brushColor = m_settings.brushColor;
    m_brushWidth = m_settings.brushWidth;

    m_scene = new BoardScene(this);
    m_scene->applyBoardSize(m_settings.boardSize);
    m_scene->setShowGrid(m_settings.showGrid);

    m_undo = new QUndoStack(this);
    m_undo->setUndoLimit(300);
    m_scene->setUndoStack(m_undo);

    m_view = new BoardView(m_scene, this);
    m_view->setBrushColor(m_brushColor);
    m_view->setBrushWidth(m_brushWidth);
    m_view->setCanvasColor(m_settings.canvasColor);
    setCentralWidget(m_view);

    createActions();
    createShapeMenu();
    createLayerDock();
    createMenus();
    createToolBars();
    createStatusBar();
    connectSignals();

    if (!m_settings.windowGeometry.isEmpty())
        restoreGeometry(m_settings.windowGeometry);
    else
        resize(1440, 900);

    updateModeUi();
    updateTitle();
    updateStatusBar();
    updateActions();
    updateBackgroundModeActions();

    QTimer::singleShot(0, this, [this]() {
        if (m_view)
            m_view->fitAll();
    });
}

MainWindow::~MainWindow()
{
    // QObject deletes the scene child before the undo stack in some orders.
    // Clear commands while the scene is still alive, and make sure the view
    // no longer points at the scene during teardown.
    if (m_undo) {
        QObject::disconnect(m_undo, nullptr, this, nullptr);
        m_undo->clear();
    }
    if (m_view)
        m_view->setScene(nullptr);
    if (m_scene)
        m_scene->setUndoStack(nullptr);
}

void MainWindow::createActions()
{
    m_newAct = makeAction(this, IconId::New, QStringLiteral("新建画板"),
                          QKeySequence(QStringLiteral("Ctrl+N")),
                          QStringLiteral("新建空白画板 (Ctrl+N)"));
    m_openAct = makeAction(this, IconId::Open, QStringLiteral("打开模板…"),
                           QKeySequence(QStringLiteral("Ctrl+O")),
                           QStringLiteral("打开一个 StoryBoard 模板 (Ctrl+O)"));
    m_saveAct = makeAction(this, IconId::Save, QStringLiteral("保存模板"),
                           QKeySequence(QStringLiteral("Ctrl+S")),
                           QStringLiteral("保存当前画板为模板 (Ctrl+S)"));
    m_saveAsAct = makeAction(this, IconId::Save, QStringLiteral("模板另存为…"),
                             QKeySequence(QStringLiteral("Ctrl+Shift+S")),
                             QStringLiteral("将当前画板另存为模板 (Ctrl+Shift+S)"));
    m_importAct = makeAction(this, IconId::Image, QStringLiteral("导入图片…"),
                             QKeySequence(QStringLiteral("Ctrl+I")),
                             QStringLiteral("将图片导入为可移动、可缩放的图元 (Ctrl+I)"));
    m_exportAct = makeAction(this, IconId::Export, QStringLiteral("导出画板…"),
                             QKeySequence(QStringLiteral("Ctrl+E")),
                             QStringLiteral("导出为 PNG / JPEG / PDF (Ctrl+E)"));
    m_quitAct = new QAction(QStringLiteral("退出"), this);
    m_quitAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Q")));
    m_quitAct->setStatusTip(QStringLiteral("退出 StoryBoard"));

    m_undoAct = makeAction(this, IconId::Undo, QStringLiteral("撤销"),
                           QKeySequence::Undo, QStringLiteral("撤销上一步 (Ctrl+Z)"));
    m_undoAct->setEnabled(false);
    m_redoAct = makeAction(this, IconId::Redo, QStringLiteral("重做"),
                           QKeySequence(QStringLiteral("Ctrl+Y")),
                           QStringLiteral("重做 (Ctrl+Y / Ctrl+Shift+Z)"));
    m_redoAct->setShortcuts({ QKeySequence(QStringLiteral("Ctrl+Y")),
                              QKeySequence(QStringLiteral("Ctrl+Shift+Z")) });
    m_redoAct->setEnabled(false);

    m_deleteAct = makeAction(this, IconId::Delete, QStringLiteral("删除选中图元"),
                             QKeySequence::Delete, QStringLiteral("删除选中的图元 (Delete)"));
    m_selectAllAct = new QAction(QStringLiteral("全选图元"), this);
    m_selectAllAct->setShortcut(QKeySequence::SelectAll);
    m_selectAllAct->setStatusTip(QStringLiteral("全选图元 (Ctrl+A)"));

    m_viewModeAct = makeAction(this, IconId::View, QStringLiteral("查看模式"),
                               QKeySequence(QStringLiteral("V")),
                               QStringLiteral("查看模式：全景浏览、整体平移与缩放 (V)"));
    m_brushModeAct = makeAction(this, IconId::Brush, QStringLiteral("画笔模式"),
                                QKeySequence(QStringLiteral("B")),
                                QStringLiteral("画笔模式：按住左键自由绘制 (B)"));
    m_editModeAct = makeAction(this, IconId::Edit, QStringLiteral("编辑模式"),
                               QKeySequence(QStringLiteral("E")),
                               QStringLiteral("编辑模式：选择、移动、缩放图元 (E)"));
    m_textModeAct = makeAction(this, IconId::Text, QStringLiteral("文字模式"),
                                QKeySequence(QStringLiteral("T")),
                                QStringLiteral("文字模式：点击画板放置文字图元 (T)"));
    m_viewModeAct->setCheckable(true);
    m_brushModeAct->setCheckable(true);
    m_editModeAct->setCheckable(true);
    m_textModeAct->setCheckable(true);
    m_viewModeAct->setChecked(true);

    m_modeGroup = new QActionGroup(this);
    m_modeGroup->setExclusive(true);
    m_modeGroup->addAction(m_viewModeAct);
    m_modeGroup->addAction(m_brushModeAct);
    m_modeGroup->addAction(m_editModeAct);
    m_modeGroup->addAction(m_textModeAct);
    m_viewModeAct->setData(static_cast<int>(ToolMode::View));
    m_brushModeAct->setData(static_cast<int>(ToolMode::Brush));
    m_editModeAct->setData(static_cast<int>(ToolMode::Edit));
    m_textModeAct->setData(static_cast<int>(ToolMode::Text));

    m_fitAct = makeAction(this, IconId::Fit, QStringLiteral("回到全景视图"),
                          QKeySequence(QStringLiteral("Ctrl+0")),
                          QStringLiteral("一键查看全部图元 (Ctrl+0 / Home)"));
    m_fitAct->setShortcuts({ QKeySequence(QStringLiteral("Ctrl+0")),
                             QKeySequence(Qt::Key_Home) });
    m_zoomInAct = makeAction(this, IconId::ZoomIn, QStringLiteral("放大"),
                             QKeySequence(QStringLiteral("Ctrl+=")),
                             QStringLiteral("放大视图 (Ctrl+=)"));
    m_zoomInAct->setShortcuts({ QKeySequence(QStringLiteral("Ctrl+=")),
                                QKeySequence(Qt::CTRL | Qt::Key_Plus) });
    m_zoomOutAct = makeAction(this, IconId::ZoomOut, QStringLiteral("缩小"),
                              QKeySequence(QStringLiteral("Ctrl+-")),
                              QStringLiteral("缩小视图 (Ctrl+-)"));
    m_actualSizeAct = makeAction(this, IconId::ZoomIn, QStringLiteral("实际大小"),
                                 QKeySequence(QStringLiteral("Ctrl+1")),
                                 QStringLiteral("以 100% 显示 (Ctrl+1)"));

    m_replaceBgAct = makeAction(this, IconId::Background, QStringLiteral("替换背景图片…"),
                             QKeySequence(), QStringLiteral("选择一张图片作为画板背景"));
    m_bgColorAct = makeAction(this, IconId::Palette, QStringLiteral("设置背景颜色…"),
                           QKeySequence(), QStringLiteral("选择纯色作为画板背景"));
    m_clearBgAct = makeAction(this, IconId::Delete, QStringLiteral("清除背景"),
                           QKeySequence(), QStringLiteral("恢复为白色背景"));

    m_bgStretchAct = new QAction(QStringLiteral("拉伸铺满"), this);
    m_bgFitAct = new QAction(QStringLiteral("完整适应"), this);
    m_bgTileAct = new QAction(QStringLiteral("平铺"), this);
    m_bgStretchAct->setCheckable(true);
    m_bgFitAct->setCheckable(true);
    m_bgTileAct->setCheckable(true);
    m_backgroundModeGroup = new QActionGroup(this);
    m_backgroundModeGroup->setExclusive(true);
    m_backgroundModeGroup->addAction(m_bgStretchAct);
    m_backgroundModeGroup->addAction(m_bgFitAct);
    m_backgroundModeGroup->addAction(m_bgTileAct);

    m_settingsAct = makeAction(this, IconId::Settings, QStringLiteral("设置…"),
                               QKeySequence(QStringLiteral("Ctrl+,")),
                               QStringLiteral("打开画板配置 (Ctrl+,)"));

    m_aboutAct = new QAction(QIcon(QStringLiteral(":/icons/app.png")),
                             QStringLiteral("关于 StoryBoard"), this);
    m_aboutQtAct = new QAction(QStringLiteral("关于 Qt"), this);

    connect(m_newAct, &QAction::triggered, this, &MainWindow::newBoard);
    connect(m_openAct, &QAction::triggered, this, &MainWindow::openTemplateDialog);
    connect(m_saveAct, &QAction::triggered, this, [this]() { saveTemplate(); });
    connect(m_saveAsAct, &QAction::triggered, this, [this]() { saveTemplateAs(); });
    connect(m_importAct, &QAction::triggered, this, &MainWindow::importImagesDialog);
    connect(m_exportAct, &QAction::triggered, this, &MainWindow::exportBoard);
    connect(m_quitAct, &QAction::triggered, this, &QWidget::close);

    connect(m_undoAct, &QAction::triggered, m_undo, &QUndoStack::undo);
    connect(m_redoAct, &QAction::triggered, m_undo, &QUndoStack::redo);
    connect(m_deleteAct, &QAction::triggered, m_scene, &BoardScene::deleteSelected);
    connect(m_selectAllAct, &QAction::triggered, m_scene, &BoardScene::selectAllItems);

    connect(m_modeGroup, &QActionGroup::triggered, this, [this](QAction* action) {
        setToolMode(static_cast<ToolMode>(action->data().toInt()));
    });
    connect(m_fitAct, &QAction::triggered, m_view, &BoardView::fitAll);
    connect(m_zoomInAct, &QAction::triggered, m_view, &BoardView::zoomIn);
    connect(m_zoomOutAct, &QAction::triggered, m_view, &BoardView::zoomOut);
    connect(m_actualSizeAct, &QAction::triggered, m_view, &BoardView::actualSize);

    connect(m_replaceBgAct, &QAction::triggered, this, &MainWindow::replaceBackgroundImage);
    connect(m_bgColorAct, &QAction::triggered, this, &MainWindow::setBackgroundColor);
    connect(m_clearBgAct, &QAction::triggered, this, &MainWindow::clearBackground);
    connect(m_settingsAct, &QAction::triggered, this, &MainWindow::showSettings);

    m_fillNoneAct = new QAction(QStringLiteral("无填充"), this);
    m_fillSolidAct = new QAction(QStringLiteral("纯色填充"), this);
    m_fillHachureAct = new QAction(QStringLiteral("斜线填充"), this);
    m_fillCrossAct = new QAction(QStringLiteral("交叉线填充"), this);
    for (QAction* action : { m_fillNoneAct, m_fillSolidAct, m_fillHachureAct, m_fillCrossAct })
        action->setCheckable(true);
    m_fillStyleGroup = new QActionGroup(this);
    m_fillStyleGroup->setExclusive(true);
    m_fillStyleGroup->addAction(m_fillNoneAct);
    m_fillStyleGroup->addAction(m_fillSolidAct);
    m_fillStyleGroup->addAction(m_fillHachureAct);
    m_fillStyleGroup->addAction(m_fillCrossAct);
    m_fillSolidAct->setChecked(true);
    connect(m_fillNoneAct, &QAction::triggered, this, [this]() { setFillStyle(FillStyle::None); });
    connect(m_fillSolidAct, &QAction::triggered, this, [this]() { setFillStyle(FillStyle::Solid); });
    connect(m_fillHachureAct, &QAction::triggered, this, [this]() { setFillStyle(FillStyle::Hachure); });
    connect(m_fillCrossAct, &QAction::triggered, this, [this]() { setFillStyle(FillStyle::CrossHatch); });
    m_fillColorAct = new QAction(QStringLiteral("填充颜色…"), this);
    connect(m_fillColorAct, &QAction::triggered, this, &MainWindow::chooseFillColor);

    auto applyBackgroundMode = [this](BackgroundMode mode) {
        BackgroundState background = m_scene->background();
        if (background.type != BackgroundType::Image)
            return;
        background.mode = mode;
        m_scene->replaceBackground(background, QStringLiteral("调整背景填充方式"));
    };
    connect(m_bgStretchAct, &QAction::triggered, this, [applyBackgroundMode]() {
        applyBackgroundMode(BackgroundMode::Stretch);
    });
    connect(m_bgFitAct, &QAction::triggered, this, [applyBackgroundMode]() {
        applyBackgroundMode(BackgroundMode::Fit);
    });
    connect(m_bgTileAct, &QAction::triggered, this, [applyBackgroundMode]() {
        applyBackgroundMode(BackgroundMode::Tile);
    });

    connect(m_aboutAct, &QAction::triggered, this, &MainWindow::showAbout);
    connect(m_aboutQtAct, &QAction::triggered, qApp, &QApplication::aboutQt);
}

void MainWindow::createShapeMenu()
{
    m_shapeMenu = new QMenu(QStringLiteral("常见图形"), this);
    m_shapeMenu->setObjectName(QStringLiteral("ShapeMenu"));
    m_shapeMenu->setToolTipsVisible(true);

    for (ShapeType type : allShapeTypes()) {
        QAction* action = m_shapeMenu->addAction(
            makeShapeIcon(type, QColor(58, 64, 80), 18), shapeTypeName(type));
        action->setObjectName(QStringLiteral("ShapeAction_%1").arg(shapeTypeToString(type)));
        action->setData(static_cast<int>(type));
        action->setToolTip(QStringLiteral("插入%1（使用当前画笔颜色）").arg(shapeTypeName(type)));
        connect(action, &QAction::triggered, this, [this, type]() { insertShape(type); });
    }
}

void MainWindow::createLayerDock()
{
    m_layerDock = new QDockWidget(QStringLiteral("图层"), this);
    m_layerDock->setObjectName(QStringLiteral("LayerDock"));
    m_layerDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_layerDock->setFeatures(QDockWidget::DockWidgetMovable
                             | QDockWidget::DockWidgetFloatable
                             | QDockWidget::DockWidgetClosable);

    m_layerPanel = new LayerPanel(m_scene, m_layerDock);
    m_layerDock->setWidget(m_layerPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_layerDock);

    connect(m_layerPanel, &LayerPanel::statusMessage, this, [this](const QString& message) {
        statusBar()->showMessage(message, 3000);
    });
}

void MainWindow::createMenus()
{
    QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("文件(&F)"));
    fileMenu->addAction(m_newAct);
    fileMenu->addAction(m_openAct);
    fileMenu->addSeparator();
    fileMenu->addAction(m_saveAct);
    fileMenu->addAction(m_saveAsAct);
    fileMenu->addSeparator();
    fileMenu->addAction(m_importAct);
    fileMenu->addAction(m_exportAct);
    fileMenu->addSeparator();
    fileMenu->addAction(m_quitAct);

    QMenu* editMenu = menuBar()->addMenu(QStringLiteral("编辑(&E)"));
    editMenu->addAction(m_undoAct);
    editMenu->addAction(m_redoAct);
    editMenu->addSeparator();
    editMenu->addAction(m_deleteAct);
    editMenu->addAction(m_selectAllAct);

    QMenu* insertMenu = menuBar()->addMenu(QStringLiteral("插入(&I)"));
    insertMenu->addMenu(m_shapeMenu);
    insertMenu->addSeparator();
    insertMenu->addAction(m_importAct);

    QMenu* viewMenu = menuBar()->addMenu(QStringLiteral("视图(&V)"));
    viewMenu->addAction(m_viewModeAct);
    viewMenu->addAction(m_brushModeAct);
    viewMenu->addAction(m_editModeAct);
    viewMenu->addAction(m_textModeAct);
    viewMenu->addSeparator();
    viewMenu->addAction(m_fitAct);
    viewMenu->addAction(m_zoomInAct);
    viewMenu->addAction(m_zoomOutAct);
    viewMenu->addAction(m_actualSizeAct);
    viewMenu->addSeparator();
    QAction* layerDockAction = m_layerDock->toggleViewAction();
    layerDockAction->setText(QStringLiteral("图层面板"));
    layerDockAction->setStatusTip(QStringLiteral("显示或隐藏图层面板"));
    viewMenu->addAction(layerDockAction);

    QMenu* canvasMenu = menuBar()->addMenu(QStringLiteral("画板(&C)"));
    canvasMenu->addAction(m_importAct);
    canvasMenu->addSeparator();
    canvasMenu->addAction(m_replaceBgAct);
    canvasMenu->addAction(m_bgColorAct);
    canvasMenu->addAction(m_clearBgAct);
    QMenu* fillMenu = canvasMenu->addMenu(QStringLiteral("背景填充方式"));
    fillMenu->addAction(m_bgStretchAct);
    fillMenu->addAction(m_bgFitAct);
    fillMenu->addAction(m_bgTileAct);
    canvasMenu->addSeparator();
    canvasMenu->addAction(m_settingsAct);

    QMenu* styleMenu = menuBar()->addMenu(QStringLiteral("样式(&S)"));
    styleMenu->addAction(m_fillNoneAct);
    styleMenu->addAction(m_fillSolidAct);
    styleMenu->addAction(m_fillHachureAct);
    styleMenu->addAction(m_fillCrossAct);
    styleMenu->addSeparator();
    styleMenu->addAction(m_fillColorAct);

    m_templatesMenu = menuBar()->addMenu(QStringLiteral("模板(&T)"));
    connect(m_templatesMenu, &QMenu::aboutToShow, this, &MainWindow::updateTemplateMenu);
    updateTemplateMenu();

    QMenu* helpMenu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    helpMenu->addAction(m_aboutAct);
    helpMenu->addAction(m_aboutQtAct);
}

void MainWindow::createToolBars()
{
    auto* mainBar = new QToolBar(QStringLiteral("主工具栏"), this);
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
    mainBar->addSeparator();

    auto* brushLabel = new QLabel(QStringLiteral("画笔"), mainBar);
    brushLabel->setObjectName(QStringLiteral("ToolBarLabel"));
    mainBar->addWidget(brushLabel);

    m_colorButton = new QToolButton(mainBar);
    m_colorButton->setObjectName(QStringLiteral("ColorButton"));
    m_colorButton->setIcon(makeColorIcon(m_brushColor, 20));
    m_colorButton->setIconSize(QSize(20, 20));
    m_colorButton->setToolTip(QStringLiteral("画笔颜色：%1（点击选择）")
                                  .arg(m_brushColor.name(QColor::HexArgb).toUpper()));
    m_colorButton->setPopupMode(QToolButton::MenuButtonPopup);

    auto* colorMenu = new QMenu(m_colorButton);
    const QList<QPair<QString, QColor>> presets = {
        { QStringLiteral("墨黑"), QColor(QStringLiteral("#111827")) },
        { QStringLiteral("红色"), QColor(QStringLiteral("#EF4444")) },
        { QStringLiteral("橙色"), QColor(QStringLiteral("#F59E0B")) },
        { QStringLiteral("绿色"), QColor(QStringLiteral("#10B981")) },
        { QStringLiteral("蓝色"), QColor(QStringLiteral("#3B82F6")) },
        { QStringLiteral("紫色"), QColor(QStringLiteral("#8B5CF6")) },
        { QStringLiteral("粉色"), QColor(QStringLiteral("#EC4899")) },
        { QStringLiteral("白色"), QColor(QStringLiteral("#FFFFFF")) }
    };
    for (const auto& preset : presets) {
        QAction* action = colorMenu->addAction(makeColorIcon(preset.second, 16), preset.first);
        connect(action, &QAction::triggered, this, [this, color = preset.second]() {
            setBrushColor(color, true);
        });
    }
    colorMenu->addSeparator();
    QAction* customColor = colorMenu->addAction(QStringLiteral("自定义颜色…"));
    connect(customColor, &QAction::triggered, this, &MainWindow::chooseBrushColor);
    m_colorButton->setMenu(colorMenu);
    connect(m_colorButton, &QToolButton::clicked, this, &MainWindow::chooseBrushColor);
    mainBar->addWidget(m_colorButton);

    m_fillButton = new QToolButton(mainBar);
    m_fillButton->setObjectName(QStringLiteral("FillButton"));
    m_fillButton->setIconSize(QSize(20, 20));
    m_fillButton->setPopupMode(QToolButton::InstantPopup);
    auto* fillMenu = new QMenu(m_fillButton);
    fillMenu->addAction(m_fillNoneAct);
    fillMenu->addAction(m_fillSolidAct);
    fillMenu->addAction(m_fillHachureAct);
    fillMenu->addAction(m_fillCrossAct);
    fillMenu->addSeparator();
    fillMenu->addAction(m_fillColorAct);
    m_fillButton->setMenu(fillMenu);
    updateFillActions();
    mainBar->addWidget(m_fillButton);

    m_widthCombo = new QComboBox(mainBar);
    m_widthCombo->setFixedWidth(88);
    m_widthCombo->setFixedHeight(28);
    m_widthCombo->setToolTip(QStringLiteral("画笔粗细"));
    const QList<int> presetWidths = { 1, 2, 3, 4, 6, 8, 10, 12, 16, 20, 24, 32, 40, 48, 64 };
    for (int width : presetWidths)
        m_widthCombo->addItem(QStringLiteral("%1 px").arg(width), width);
    connect(m_widthCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        const int value = m_widthCombo->itemData(index).toInt();
        if (value <= 0)
            return;
        m_brushWidth = value;
        m_view->setBrushWidth(value);
        m_settings.brushWidth = value;
    });
    setBrushWidthUi(m_brushWidth);
    auto* widthLabel = new QLabel(QStringLiteral("粗细"), mainBar);
    widthLabel->setObjectName(QStringLiteral("ToolBarLabel"));
    mainBar->addWidget(widthLabel);
    mainBar->addWidget(m_widthCombo);
    mainBar->addSeparator();

    mainBar->addAction(m_fitAct);
    mainBar->addAction(m_zoomInAct);
    mainBar->addAction(m_zoomOutAct);
    mainBar->addSeparator();
    auto* shapeButton = new QToolButton(mainBar);
    shapeButton->setObjectName(QStringLiteral("ShapeButton"));
    shapeButton->setIcon(makeIcon(IconId::Shapes, QColor(58, 64, 80), 21));
    shapeButton->setIconSize(QSize(21, 21));
    shapeButton->setPopupMode(QToolButton::InstantPopup);
    shapeButton->setMenu(m_shapeMenu);
    shapeButton->setToolTip(QStringLiteral("快速插入圆形、矩形、箭头、大括号等图形"));
    shapeButton->setStatusTip(QStringLiteral("快速插入常见图形（使用当前画笔颜色）"));
    mainBar->addWidget(shapeButton);
    mainBar->addAction(m_importAct);
    mainBar->addAction(m_replaceBgAct);
    mainBar->addAction(m_settingsAct);

    auto* modeBar = new QToolBar(QStringLiteral("模式"), this);
    modeBar->setObjectName(QStringLiteral("ModeToolBar"));
    modeBar->setMovable(false);
    modeBar->setFloatable(false);
    modeBar->setIconSize(QSize(23, 23));
    modeBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    modeBar->setContextMenuPolicy(Qt::PreventContextMenu);
    modeBar->setMinimumWidth(80);
    addToolBar(Qt::LeftToolBarArea, modeBar);
    modeBar->addAction(m_viewModeAct);
    modeBar->addAction(m_brushModeAct);
    modeBar->addAction(m_editModeAct);
    modeBar->addAction(m_textModeAct);
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
        m_undoAct->setToolTip(text.isEmpty() ? QStringLiteral("撤销")
                                             : QStringLiteral("撤销：%1").arg(text));
    });
    connect(m_undo, &QUndoStack::redoTextChanged, this, [this](const QString& text) {
        m_redoAct->setToolTip(text.isEmpty() ? QStringLiteral("重做")
                                             : QStringLiteral("重做：%1").arg(text));
    });

    connect(m_view, &BoardView::zoomChanged, this, [this](qreal zoom) {
        if (m_zoomBadge)
            m_zoomBadge->setText(QStringLiteral("%1%").arg(qRound(zoom * 100.0)));
    });
    connect(m_view, &BoardView::modeChanged, this, [this](ToolMode) { updateModeUi(); });
    connect(m_view, &BoardView::statusMessage, this, [this](const QString& message) {
        statusBar()->showMessage(message, 4000);
    });
    connect(m_view, &BoardView::textPlacementRequested, this, &MainWindow::insertTextAt);
    connect(m_view, &BoardView::editTextRequested, this, &MainWindow::editTextItem);

    connect(m_scene, &BoardScene::contentChanged, this, [this]() {
        updateActions();
        updateStatusBar();
    });
    connect(m_scene, &BoardScene::layersChanged, this, &MainWindow::updateStatusBar);
    connect(m_scene, &BoardScene::activeLayerChanged, this, [this](int) {
        updateStatusBar();
    });
    connect(m_scene, &BoardScene::selectionChanged, this, &MainWindow::updateActions);
    connect(m_scene, &BoardScene::backgroundChanged, this, [this]() {
        updateBackgroundModeActions();
        updateActions();
    });
    connect(m_scene, &BoardScene::boardSizeChanged, this, [this](const QSizeF&) {
        if (m_view->mode() == ToolMode::View)
            m_view->fitAll();
    });
}

void MainWindow::setToolMode(ToolMode mode)
{
    m_view->setMode(mode);
    updateModeUi();
}

void MainWindow::updateModeUi()
{
    if (!m_view)
        return;

    const ToolMode mode = m_view->mode();
    m_viewModeAct->setChecked(mode == ToolMode::View);
    m_brushModeAct->setChecked(mode == ToolMode::Brush);
    m_editModeAct->setChecked(mode == ToolMode::Edit);
    m_textModeAct->setChecked(mode == ToolMode::Text);

    if (m_modeBadge)
        m_modeBadge->setText(toolModeName(mode));

    if (m_hintLabel) {
        switch (mode) {
        case ToolMode::View:
            m_hintLabel->setText(QStringLiteral("查看模式：拖拽平移 · 滚轮缩放 · 切换模式时自动回到全景"));
            break;
        case ToolMode::Brush:
            m_hintLabel->setText(QStringLiteral("画笔模式：左键自由绘制 · 滚轮缩放 · 空格 / 中键平移"));
            break;
        case ToolMode::Edit:
            m_hintLabel->setText(QStringLiteral("编辑模式：可见且未锁定图层的图元均可编辑 · 拖拽/框选 · 控制点缩放 · 拖动画板边缘调整画板 · Delete 删除"));
            break;
        case ToolMode::Text:
            m_hintLabel->setText(QStringLiteral("文字模式：点击画板放置文字 · 可设置字体、字号、颜色与粗斜体"));
            break;
        }
    }

    updateStatusBar();
    updateActions();
}

void MainWindow::updateActions()
{
    if (!m_undo || !m_scene || !m_view)
        return;

    m_undoAct->setEnabled(m_undo->canUndo());
    m_redoAct->setEnabled(m_undo->canRedo());

    const bool editing = m_view->mode() == ToolMode::Edit;
    m_deleteAct->setEnabled(editing && !m_scene->selectedItems().isEmpty());
    m_selectAllAct->setEnabled(editing && m_scene->boardItemCount() > 0);

    updateBackgroundModeActions();
}

void MainWindow::updateBackgroundModeActions()
{
    if (!m_scene || !m_bgStretchAct)
        return;

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
    if (!m_view || !m_scene)
        return;

    if (m_modeBadge)
        m_modeBadge->setText(toolModeName(m_view->mode()));
    if (m_zoomBadge)
        m_zoomBadge->setText(QStringLiteral("%1%").arg(qRound(m_view->zoom() * 100.0)));
    if (m_countBadge) {
        m_countBadge->setText(QStringLiteral("%1 个图元 · %2 层")
                                  .arg(m_scene->boardItemCount())
                                  .arg(m_scene->userLayerCount()));
    }
}

QString MainWindow::documentName() const
{
    if (m_filePath.isEmpty())
        return QStringLiteral("未命名画板");
    const QString name = QFileInfo(m_filePath).completeBaseName();
    return name.isEmpty() ? QStringLiteral("未命名画板") : name;
}

void MainWindow::updateTitle()
{
    setWindowTitle(QStringLiteral("%1[*] - StoryBoard 画板").arg(documentName()));
    setWindowModified(m_undo && !m_undo->isClean());
    if (!m_filePath.isEmpty())
        setWindowFilePath(m_filePath);
}

void MainWindow::setBrushColor(const QColor& color, bool remember)
{
    if (!color.isValid())
        return;

    m_brushColor = color;
    m_view->setBrushColor(color);
    if (m_colorButton) {
        m_colorButton->setIcon(makeColorIcon(color, 20));
        m_colorButton->setToolTip(QStringLiteral("画笔颜色：%1（点击选择）")
                                      .arg(color.name(QColor::HexArgb).toUpper()));
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
    if (m_view)
        m_view->setBrushWidth(width);
    if (m_widthCombo) {
        int index = m_widthCombo->findData(width);
        if (index < 0) {
            m_widthCombo->addItem(QStringLiteral("%1 px").arg(width), width);
            index = m_widthCombo->count() - 1;
        }
        const QSignalBlocker blocker(m_widthCombo);
        m_widthCombo->setCurrentIndex(index);
    }
}

void MainWindow::chooseBrushColor()
{
    const QColor color = QColorDialog::getColor(m_brushColor, this,
                                                QStringLiteral("选择画笔颜色"),
                                                QColorDialog::ShowAlphaChannel);
    if (color.isValid())
        setBrushColor(color, true);
}

void MainWindow::newBoard()
{
    if (!maybeSave())
        return;

    m_view->cancelCurrentStroke();
    m_scene->resetDocument();
    m_scene->applyBoardSize(m_settings.boardSize);
    m_scene->setShowGrid(m_settings.showGrid);
    m_scene->applyBackground(BackgroundState());
    m_filePath.clear();
    m_undo->clear();
    m_undo->setClean();

    if (m_view->mode() != ToolMode::View)
        m_view->setMode(ToolMode::View);
    else
        m_view->fitAll();

    updateModeUi();
    updateTitle();
    updateStatusBar();
    updateActions();
    statusBar()->showMessage(QStringLiteral("已新建空白画板"), 3000);
}

void MainWindow::openTemplateDialog()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开模板"), AppSettings::templatesDir(),
        QString::fromUtf8(kTemplateFilter));
    if (!path.isEmpty())
        loadTemplateFile(path, true);
}

bool MainWindow::openTemplateFile(const QString& path)
{
    return loadTemplateFile(path, true);
}

bool MainWindow::loadTemplateFile(const QString& path, bool showErrors)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (showErrors) {
            QMessageBox::warning(this, QStringLiteral("打开模板"),
                                 QStringLiteral("无法读取文件：\n%1").arg(QDir::toNativeSeparators(path)));
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (showErrors) {
            QMessageBox::warning(this, QStringLiteral("打开模板"),
                                 QStringLiteral("模板文件解析失败：\n%1").arg(parseError.errorString()));
        }
        return false;
    }

    const QJsonObject root = document.object();
    QString validationError;
    if (!validateTemplateRoot(root, &validationError)) {
        if (showErrors)
            QMessageBox::warning(this, QStringLiteral("打开模板"), validationError);
        return false;
    }

    if (!maybeSave())
        return false;

    m_view->finishCurrentStroke();
    QString loadError;
    if (!m_scene->fromJson(root, &loadError)) {
        if (showErrors)
            QMessageBox::warning(this, QStringLiteral("打开模板"), loadError);
        return false;
    }

    m_filePath = QFileInfo(path).absoluteFilePath();
    m_undo->setClean();
    if (m_view->mode() != ToolMode::View)
        m_view->setMode(ToolMode::View);
    else
        m_view->fitAll();

    m_settings.lastTemplatePath = m_filePath;
    updateModeUi();
    updateTitle();
    updateStatusBar();
    updateActions();
    statusBar()->showMessage(QStringLiteral("已打开模板：%1").arg(QFileInfo(m_filePath).fileName()), 4000);
    return true;
}

bool MainWindow::saveTemplate()
{
    if (m_filePath.isEmpty())
        return saveTemplateAs();
    return saveToFile(m_filePath);
}

bool MainWindow::saveTemplateAs()
{
    QString suggestion = m_filePath;
    if (suggestion.isEmpty()) {
        QString name = documentName();
        if (name == QStringLiteral("未命名画板"))
            name = QStringLiteral("未命名模板");
        suggestion = QDir(AppSettings::templatesDir()).filePath(name + QStringLiteral(".sbboard"));
    }

    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存为模板"), suggestion,
        QStringLiteral("StoryBoard 模板 (*.sbboard)"));
    if (path.isEmpty())
        return false;
    if (!path.endsWith(QStringLiteral(".sbboard"), Qt::CaseInsensitive))
        path += QStringLiteral(".sbboard");
    return saveToFile(path);
}

bool MainWindow::saveToFile(const QString& path)
{
    m_view->finishCurrentStroke();

    const QJsonDocument document(m_scene->toJson());
    const QByteArray data = document.toJson(QJsonDocument::Indented);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, QStringLiteral("保存模板"),
                             QStringLiteral("无法写入文件：\n%1").arg(QDir::toNativeSeparators(path)));
        return false;
    }
    file.write(data);
    if (!file.commit()) {
        QMessageBox::warning(this, QStringLiteral("保存模板"),
                             QStringLiteral("保存模板失败：\n%1").arg(QDir::toNativeSeparators(path)));
        return false;
    }

    m_filePath = QFileInfo(path).absoluteFilePath();
    m_settings.lastTemplatePath = m_filePath;
    m_settings.save();
    m_undo->setClean();
    updateTitle();
    updateActions();
    statusBar()->showMessage(QStringLiteral("模板已保存：%1").arg(QFileInfo(m_filePath).fileName()), 4000);
    return true;
}

bool MainWindow::maybeSave()
{
    if (!m_undo || m_undo->isClean())
        return true;

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(QStringLiteral("未保存的更改"));
    box.setText(QStringLiteral("当前画板有未保存的更改。"));
    box.setInformativeText(QStringLiteral("是否先保存为模板？"));
    QPushButton* saveButton = box.addButton(QStringLiteral("保存模板"), QMessageBox::AcceptRole);
    QPushButton* discardButton = box.addButton(QStringLiteral("不保存"), QMessageBox::DestructiveRole);
    box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
    box.setDefaultButton(saveButton);
    box.exec();

    if (box.clickedButton() == saveButton)
        return saveTemplate();
    if (box.clickedButton() == discardButton)
        return true;
    return false;
}

void MainWindow::exportBoard()
{
    m_view->finishCurrentStroke();
    QString startDirectory = m_settings.lastExportDir;
    if (startDirectory.isEmpty())
        startDirectory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);

    ExportDialog dialog(m_scene, startDirectory, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const ExportDialog::Options options = dialog.options();
    m_settings.lastExportDir = QFileInfo(options.filePath).absolutePath();
    m_settings.save();
    exportToFile(options.filePath, options.area, options.scale, options.format);
}

bool MainWindow::exportToFile(const QString& path, const QRectF& area, qreal scale,
                              const QString& format)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QImage image = m_scene->renderImage(area, scale);
    QApplication::restoreOverrideCursor();

    if (image.isNull()) {
        QMessageBox::warning(this, QStringLiteral("导出画板"),
                             QStringLiteral("渲染画板失败，没有可导出的内容。"));
        return false;
    }

    bool success = false;
    QString errorMessage;
    if (format == QStringLiteral("PDF")) {
        {
            QPdfWriter writer(path);
            writer.setResolution(96);
            writer.setTitle(QStringLiteral("StoryBoard 画板"));
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
                errorMessage = QStringLiteral("无法创建 PDF 输出。");
            }
        }
        if (success && QFileInfo(path).size() <= 0) {
            success = false;
            errorMessage = QStringLiteral("PDF 文件写入失败。");
        }
    } else {
        QImageWriter writer(path, format == QStringLiteral("JPEG") ? "JPEG" : "PNG");
        if (format == QStringLiteral("JPEG"))
            writer.setQuality(92);
        success = writer.write(image);
        if (!success)
            errorMessage = writer.errorString();
    }

    if (!success) {
        QMessageBox::critical(this, QStringLiteral("导出失败"),
                              QStringLiteral("无法导出画板：\n%1\n\n%2")
                                  .arg(QDir::toNativeSeparators(path), errorMessage));
        return false;
    }

    m_settings.lastExportDir = QFileInfo(path).absolutePath();
    m_settings.save();
    statusBar()->showMessage(QStringLiteral("画板已导出：%1").arg(QFileInfo(path).fileName()), 5000);

    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(QStringLiteral("导出完成"));
    box.setText(QStringLiteral("画板已成功导出。"));
    box.setInformativeText(QDir::toNativeSeparators(path));
    QPushButton* openFileButton = box.addButton(QStringLiteral("打开文件"), QMessageBox::AcceptRole);
    QPushButton* openFolderButton = box.addButton(QStringLiteral("打开文件夹"), QMessageBox::ActionRole);
    box.addButton(QStringLiteral("完成"), QMessageBox::RejectRole);
    box.exec();

    if (box.clickedButton() == openFileButton)
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    else if (box.clickedButton() == openFolderButton)
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));

    return true;
}

void MainWindow::importImagesDialog()
{
    QString startDirectory = m_settings.lastImageDir;
    if (startDirectory.isEmpty())
        startDirectory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);

    const QStringList files = QFileDialog::getOpenFileNames(
        this, QStringLiteral("导入图片"), startDirectory, QString::fromUtf8(kImageFilter));
    if (!files.isEmpty())
        importImageFiles(files);
}

void MainWindow::importImageFiles(const QStringList& files)
{
    if (files.isEmpty())
        return;

    if (m_view->mode() != ToolMode::Edit)
        setToolMode(ToolMode::Edit);

    if (!m_scene->isActiveLayerWritable()) {
        statusBar()->showMessage(QStringLiteral(
            "当前图层已锁定或已隐藏，无法导入图片。请先在右侧图层面板解锁或切换图层。"), 5000);
        return;
    }

    const QPointF viewCenter = m_view->mapToScene(m_view->viewport()->rect().center());
    const QSizeF maxSize(m_scene->boardSize().width() * 0.6,
                         m_scene->boardSize().height() * 0.6);

    QList<BoardItem*> items;
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
            scaleFactor = qMin(1.0, qMin(maxSize.width() / pixmap.width(),
                                         maxSize.height() / pixmap.height()));
        }
        QSizeF size(pixmap.width() * scaleFactor, pixmap.height() * scaleFactor);
        size.setWidth(qMax<qreal>(16.0, size.width()));
        size.setHeight(qMax<qreal>(16.0, size.height()));

        auto* item = new ImageItem();
        item->setImage(pixmap, QFileInfo(file).fileName());
        item->setContentSize(size);
        item->setPos(viewCenter - QPointF(size.width() / 2.0, size.height() / 2.0)
                     + QPointF(offsetIndex * 26.0, offsetIndex * 26.0));
        items.append(item);
        ++offsetIndex;
    }

    if (items.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("导入图片"),
                             QStringLiteral("没有可用的图片文件。"));
        return;
    }

    m_scene->addItemsWithUndo(items, QStringLiteral("导入图片"));
    for (BoardItem* item : items)
        item->setSelected(true);

    m_settings.lastImageDir = QFileInfo(files.constFirst()).absolutePath();
    m_settings.save();

    statusBar()->showMessage(QStringLiteral("已导入 %1 张图片").arg(items.size()), 4000);
    if (!failed.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("部分图片未导入"),
                             QStringLiteral("以下文件无法读取：\n%1")
                                 .arg(QDir::toNativeSeparators(failed.join(QLatin1Char('\n')))));
    }
}

void MainWindow::insertShape(ShapeType type)
{
    if (m_view->mode() != ToolMode::Edit)
        setToolMode(ToolMode::Edit);

    if (!m_scene->isActiveLayerWritable()) {
        statusBar()->showMessage(QStringLiteral(
            "当前图层已锁定或已隐藏，无法插入图形。请先在右侧图层面板解锁或切换图层。"), 5000);
        return;
    }

    auto* item = new ShapeItem(type);
    const QColor stroke = m_brushColor.alpha() < 8
        ? QColor(QStringLiteral("#1E293B"))
        : m_brushColor;
    QColor fill = stroke;
    fill.setAlpha(36);
    if (m_fillColor.isValid())
        fill = m_fillColor;
    const bool filled = shapeTypeIsFilled(type) && m_fillStyle != FillStyle::None;
    item->setStyle(stroke,
                   filled ? fill : QColor(Qt::transparent),
                   qBound<qreal>(2.5, m_brushWidth, 10.0));
    item->setFillStyle(filled ? m_fillStyle : FillStyle::None);

    const QRectF board = m_scene->boardRect();
    QSizeF size = defaultShapeSize(type);
    const qreal fitScale = qMin<qreal>(1.0,
        qMin(board.width() * 0.72 / qMax<qreal>(1.0, size.width()),
             board.height() * 0.72 / qMax<qreal>(1.0, size.height())));
    size *= fitScale;

    QPointF center = m_view->mapToScene(m_view->viewport()->rect().center());
    if (!board.contains(center))
        center = board.center();
    const qreal cascade = (m_scene->boardItemCount() % 5) * 18.0;
    center += QPointF(cascade, cascade);
    center.setX(qBound(board.left() + size.width() / 2.0,
                       center.x(), board.right() - size.width() / 2.0));
    center.setY(qBound(board.top() + size.height() / 2.0,
                       center.y(), board.bottom() - size.height() / 2.0));

    item->setPos(center - QPointF(size.width() / 2.0, size.height() / 2.0));
    item->setContentSize(size);

    m_scene->clearSelection();
    m_scene->addItemsWithUndo({ item }, QStringLiteral("插入%1").arg(shapeTypeName(type)));
    item->setSelected(true);
    m_view->setFocus();
    if (type == ShapeType::Ellipse) {
        statusBar()->showMessage(
            QStringLiteral("已插入正圆；编辑模式下拖动控制点可把它拉伸成椭圆"), 5000);
    } else {
        statusBar()->showMessage(
            QStringLiteral("已插入%1；拖动可移动，拖动控制点可缩放，颜色跟随当前画笔")
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
    const QColor initial = m_fillColor.isValid() ? m_fillColor
                                                 : QColor(59, 130, 246, 96);
    const QColor color = QColorDialog::getColor(initial, this,
                                                QStringLiteral("选择填充颜色"),
                                                QColorDialog::ShowAlphaChannel);
    if (!color.isValid())
        return;
    m_fillColor = color;
    updateFillActions();
    applyFillToSelection();
}

void MainWindow::applyFillToSelection()
{
    if (!m_scene || !m_undo)
        return;

    QList<ShapeItem*> shapes;
    int outlineOnlyCount = 0;
    const QList<BoardItem*> selected = m_scene->selectedBoardItems();
    for (BoardItem* item : selected) {
        auto* shape = dynamic_cast<ShapeItem*>(item);
        if (!shape)
            continue;
        if (shapeTypeIsFilled(shape->shapeType()))
            shapes.append(shape);
        else
            ++outlineOnlyCount;
    }

    if (shapes.isEmpty()) {
        if (outlineOnlyCount > 0) {
            statusBar()->showMessage(
                QStringLiteral("所选图形只有描边（直线、箭头、大括号等），暂不支持填充"), 4000);
        } else {
            statusBar()->showMessage(QStringLiteral("填充已设为「%1」，将用于新插入的图形")
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
        if (fill.alpha() == 0)
            fill = shape->strokeColor();
        shape->setFill(fill, m_fillStyle);
        after.append(shape->state());
    }

    if (before == after) {
        statusBar()->showMessage(QStringLiteral("填充样式没有变化"), 3000);
        return;
    }

    m_undo->push(new ShapeStyleCommand(m_scene, shapes, before, after,
                                       QStringLiteral("调整填充样式")));
    statusBar()->showMessage(QStringLiteral("已对 %1 个图形应用「%2」")
                                 .arg(shapes.size())
                                 .arg(fillStyleName(m_fillStyle)),
                             4000);
}

void MainWindow::updateFillActions()
{
    if (!m_fillButton)
        return;

    if (m_fillNoneAct)
        m_fillNoneAct->setChecked(m_fillStyle == FillStyle::None);
    if (m_fillSolidAct)
        m_fillSolidAct->setChecked(m_fillStyle == FillStyle::Solid);
    if (m_fillHachureAct)
        m_fillHachureAct->setChecked(m_fillStyle == FillStyle::Hachure);
    if (m_fillCrossAct)
        m_fillCrossAct->setChecked(m_fillStyle == FillStyle::CrossHatch);

    const QColor swatch = m_fillColor.isValid() ? m_fillColor : QColor(30, 41, 59, 36);
    m_fillButton->setIcon(makeColorIcon(swatch, 20));
    const QString colorText = m_fillColor.isValid()
        ? m_fillColor.name(QColor::HexArgb).toUpper()
        : QStringLiteral("跟随画笔色");
    m_fillButton->setToolTip(QStringLiteral("填充：%1 · %2（点击选择）")
                                 .arg(fillStyleName(m_fillStyle), colorText));
    if (m_fillColorAct)
        m_fillColorAct->setIcon(makeColorIcon(swatch, 16));
}

void MainWindow::insertTextAt(const QPointF& scenePos)
{
    TextDialog dialog(this);
    dialog.setHeading(QStringLiteral("添加文字图元"));
    dialog.setTextColor(m_brushColor);
    if (dialog.exec() != QDialog::Accepted)
        return;

    auto* item = new TextItem();
    item->setTextFormat(dialog.textValue(), dialog.textFont(), dialog.textColor());

    const QRectF board = m_scene->boardRect();
    const QSizeF size = item->contentSize();
    qreal x = scenePos.x() - size.width() / 2.0;
    qreal y = scenePos.y() - size.height() / 2.0;
    x = qBound(board.left(), x, qMax(board.left(), board.right() - size.width()));
    y = qBound(board.top(), y, qMax(board.top(), board.bottom() - size.height()));
    item->setPos(QPointF(x, y));

    setToolMode(ToolMode::Edit);
    m_scene->addItemsWithUndo({ item }, QStringLiteral("添加文字"));
    item->setSelected(true);
    m_view->setFocus();
    statusBar()->showMessage(QStringLiteral("已添加文字图元"), 4000);
}

void MainWindow::editTextItem(TextItem* item)
{
    if (!item || item->scene() != m_scene)
        return;

    TextDialog dialog(this);
    dialog.setHeading(QStringLiteral("编辑文字图元"));
    dialog.setTextValue(item->text());
    dialog.setTextFont(item->textFont());
    dialog.setTextColor(item->textColor());
    if (dialog.exec() != QDialog::Accepted)
        return;

    if (m_scene->changeTextWithUndo(item, dialog.textValue(), dialog.textFont(),
                                    dialog.textColor(), QStringLiteral("编辑文字"))) {
        item->setSelected(true);
        statusBar()->showMessage(QStringLiteral("文字已更新"), 4000);
    }
}

void MainWindow::replaceBackgroundImage()
{
    QString startDirectory = m_settings.lastImageDir;
    if (startDirectory.isEmpty())
        startDirectory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);

    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("替换背景图片"), startDirectory, QString::fromUtf8(kImageFilter));
    if (path.isEmpty())
        return;

    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        QMessageBox::warning(this, QStringLiteral("替换背景"),
                             QStringLiteral("无法读取图片：\n%1").arg(reader.errorString()));
        return;
    }

    BackgroundState background;
    background.type = BackgroundType::Image;
    background.image = QPixmap::fromImage(image);
    background.mode = m_scene->background().type == BackgroundType::Image
        ? m_scene->background().mode
        : BackgroundMode::Stretch;
    m_scene->replaceBackground(background, QStringLiteral("替换背景图片"));

    m_settings.lastImageDir = QFileInfo(path).absolutePath();
    m_settings.save();
    statusBar()->showMessage(QStringLiteral("背景已替换"), 3000);
}

void MainWindow::setBackgroundColor()
{
    QColor initial(255, 255, 255);
    if (m_scene->background().type == BackgroundType::Color)
        initial = m_scene->background().color;

    const QColor color = QColorDialog::getColor(initial, this, QStringLiteral("选择背景颜色"));
    if (!color.isValid())
        return;

    BackgroundState background;
    background.type = BackgroundType::Color;
    background.color = color;
    m_scene->replaceBackground(background, QStringLiteral("设置背景颜色"));
    statusBar()->showMessage(QStringLiteral("背景颜色已更新"), 3000);
}

void MainWindow::clearBackground()
{
    m_scene->replaceBackground(BackgroundState(), QStringLiteral("清除背景"));
    statusBar()->showMessage(QStringLiteral("背景已清除"), 3000);
}

void MainWindow::showSettings()
{
    SettingsDialog dialog(m_settings, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const AppSettings updated = dialog.settings();
    m_settings.brushColor = updated.brushColor;
    m_settings.brushWidth = updated.brushWidth;
    m_settings.boardSize = updated.boardSize;
    m_settings.showGrid = updated.showGrid;
    m_settings.canvasColor = updated.canvasColor;

    setBrushColor(m_settings.brushColor, false);
    setBrushWidthUi(m_settings.brushWidth);
    m_view->setCanvasColor(m_settings.canvasColor);
    m_scene->setShowGrid(m_settings.showGrid);
    m_scene->setBoardSizeWithUndo(m_settings.boardSize);

    if (m_view->mode() == ToolMode::View)
        m_view->fitAll();

    m_settings.save();
    updateStatusBar();
    statusBar()->showMessage(QStringLiteral("设置已保存"), 3000);
}

void MainWindow::fitAll()
{
    m_view->fitAll();
}

void MainWindow::updateTemplateMenu()
{
    if (!m_templatesMenu)
        return;

    m_templatesMenu->clear();
    m_templatesMenu->addAction(m_saveAct);
    m_templatesMenu->addAction(m_saveAsAct);
    m_templatesMenu->addAction(m_openAct);
    m_templatesMenu->addSeparator();

    QDir directory(AppSettings::templatesDir());
    const QFileInfoList files = directory.entryInfoList({ QStringLiteral("*.sbboard") },
                                                        QDir::Files, QDir::Time);
    if (files.isEmpty()) {
        QAction* emptyAction = m_templatesMenu->addAction(QStringLiteral("（模板文件夹为空）"));
        emptyAction->setEnabled(false);
    } else {
        const int maximum = qMin(15, static_cast<int>(files.size()));
        for (int i = 0; i < maximum; ++i) {
            const QFileInfo info = files.at(i);
            QAction* action = m_templatesMenu->addAction(makeIcon(IconId::Save, QColor(90, 98, 112), 16),
                                                         info.completeBaseName());
            action->setCheckable(true);
            action->setChecked(info.absoluteFilePath() == m_filePath);
            action->setToolTip(QDir::toNativeSeparators(info.absoluteFilePath()));
            const QString filePath = info.absoluteFilePath();
            connect(action, &QAction::triggered, this, [this, filePath]() {
                loadTemplateFile(filePath, true);
            });
        }
        if (files.size() > maximum) {
            QAction* moreAction = m_templatesMenu->addAction(
                QStringLiteral("… 共 %1 个模板").arg(files.size()));
            moreAction->setEnabled(false);
        }
    }

    m_templatesMenu->addSeparator();
    QAction* openFolderAction = m_templatesMenu->addAction(QStringLiteral("打开模板文件夹"));
    connect(openFolderAction, &QAction::triggered, this, &MainWindow::openTemplatesFolder);
}

void MainWindow::openTemplatesFolder()
{
    const QString directory = AppSettings::templatesDir();
    QDir().mkpath(directory);
    QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
}

void MainWindow::showAbout()
{
    QMessageBox::about(
        this, QStringLiteral("关于 StoryBoard 画板"),
        QStringLiteral(
            "<h3>StoryBoard 画板 %1</h3>"
            "<p>一个支持自由笔画、图片图元、全景查看、模板与导出的小型画板工具。</p>"
            "<p><b>主要操作</b></p>"
            "<ul>"
            "<li><b>V</b> 查看模式：整体平移 / 缩放，自动全景</li>"
            "<li><b>B</b> 画笔模式：自由曲线，可选颜色与粗细</li>"
            "<li><b>E</b> 编辑模式：显示且未锁定图层上的图元都能编辑；锁定图层不可编辑；右键调整图层/高度</li>"
            "<li><b>T</b> 文字模式：点击画板添加文字，双击文字可再次编辑</li>"
            "<li><b>图层</b>：右侧面板勾选框控制显示/隐藏、锁形按钮锁定图层；图层与图元都支持多选</li>"
            "<li><b>插入 → 常见图形</b>：快速插入矩形、圆形、箭头、大括号、对话气泡等</li>"
            "<li><b>Ctrl+0</b> 回到全景视图，<b>Ctrl+Z / Ctrl+Y</b> 撤销 / 重做</li>"
            "</ul>"
            "<p>基于 Qt %2 构建。</p>")
            .arg(QCoreApplication::applicationVersion(), QString::fromLatin1(qVersion())));
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
        if (!url.isLocalFile())
            continue;
        const QString path = url.toLocalFile();
        if (hasImageSuffix(path) || path.endsWith(QStringLiteral(".sbboard"), Qt::CaseInsensitive)) {
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    QStringList images;
    QString templatePath;

    for (const QUrl& url : event->mimeData()->urls()) {
        if (!url.isLocalFile())
            continue;
        const QString path = url.toLocalFile();
        if (path.endsWith(QStringLiteral(".sbboard"), Qt::CaseInsensitive))
            templatePath = path;
        else if (hasImageSuffix(path))
            images.append(path);
    }

    if (!templatePath.isEmpty() && images.isEmpty()) {
        if (loadTemplateFile(templatePath, true))
            event->acceptProposedAction();
        return;
    }

    if (!images.isEmpty()) {
        importImageFiles(images);
        event->acceptProposedAction();
    }
}
