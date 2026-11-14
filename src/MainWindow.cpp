#include "MainWindow.h"

#include "BackgroundController.h"
#include "CanvasScene.h"
#include "CanvasView.h"
#include "Commands.h"
#include "CustomShapeTemplates.h"
#include "DockPanelController.h"
#include "DocumentController.h"
#include "ExportDialog.h"
#include "I18n.h"
#include "IconFactory.h"
#include "ImageItem.h"
#include "ImportExportController.h"
#include "ItemDockPanel.h"
#include "ItemPropertiesDialog.h"
#include "LayerPanel.h"
#include "LineArtConverter.h"
#include "LineArtDialog.h"
#include "SettingsDialog.h"
#include "ShapeItem.h"
#include "ShortcutController.h"
#include "StyleController.h"
#include "TextItem.h"
#include "UiBuilderController.h"
#include "ViewStatusController.h"

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
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequenceEdit>
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
#include <QRegularExpression>
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
#include <algorithm>

namespace {

bool selfTestMode()
{
    return qApp && qApp->property("storyboardSelfTest").toBool();
}

// 统一的报错出口：自检（离屏）时不许弹模态框，否则没人点“确定”，进程会一直挂着。
void reportWarning(QWidget* parent, const QString& title, const QString& text)
{
    if (selfTestMode()) {
        qWarning("self-test suppressed dialog [%s]: %s", qUtf8Printable(title), qUtf8Printable(text));
        return;
    }
    QMessageBox::warning(parent, title, text);
}

bool hasImageSuffix(const QString& path)
{
    static const QStringList suffixes = {QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
                                         QStringLiteral("bmp"), QStringLiteral("gif")};
    return suffixes.contains(QFileInfo(path).suffix().toLower());
}

} // namespace

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

    createActions();
    createShapeMenu();
    m_docks->createLayerDock();
    m_docks->createItemDock();
    m_docks->applyDockLayout();
    createMenus();
    createToolBars();
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
    if (!m_customShapeMenu) {
        return;
    }
    m_customShapeMenu->clear();

    QStringList errors;
    const QList<CustomShapeTemplate> templates = loadCustomShapeTemplatesFromDirs(AppSettings::shapesDirs(), &errors);
    if (templates.isEmpty()) {
        QAction* empty = m_customShapeMenu->addAction(tr("(No custom shapes in the shapes folder)"));
        empty->setEnabled(false);
    } else {
        for (const CustomShapeTemplate& entry : templates) {
            QAction* action =
                m_customShapeMenu->addAction(makeCustomShapeIcon(entry.unitPath, QColor(58, 64, 80), 18), entry.name);
            const QString filePath = entry.filePath;
            action->setToolTip(tr("Insert the custom shape from %1").arg(QFileInfo(filePath).fileName()));
            connect(action, &QAction::triggered, this, [this, filePath]() { insertCustomShape(filePath); });
        }
    }
    m_customShapeMenu->addSeparator();
    connect(m_customShapeMenu->addAction(tr("Open shapes folder…")), &QAction::triggered, this,
            &MainWindow::openShapesFolder);
    if (!errors.isEmpty()) {
        QAction* broken = m_customShapeMenu->addAction(tr("%1 shape file(s) could not be read").arg(errors.size()));
        broken->setEnabled(false);
        broken->setToolTip(errors.join(QLatin1Char('\n')));
    }
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

QString MainWindow::documentName() const
{
    return m_document->documentName();
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

void MainWindow::setStrokeSmoothingUi(int tolerance)
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

bool MainWindow::loadCanvasFile(const QString& path, bool showErrors)
{
    return m_document->loadCanvasFile(path, showErrors);
}

bool MainWindow::saveCanvas()
{
    return m_document->saveCanvas();
}

bool MainWindow::saveCanvasAs()
{
    return m_document->saveCanvasAs();
}

bool MainWindow::saveToFile(const QString& path)
{
    return m_document->saveToFile(path);
}

bool MainWindow::maybeSave()
{
    return m_document->maybeSave();
}

void MainWindow::exportCanvas()
{
    m_io->exportCanvas();
}

bool MainWindow::exportToFile(const QString& path, const QRectF& area, qreal scale, const QString& format,
                              bool showCompletion, int layerId)
{
    return m_io->exportToFile(path, area, scale, format, showCompletion, layerId);
}

void MainWindow::importImagesDialog()
{
    m_io->importImagesDialog();
}

void MainWindow::importOutlineImageDialog()
{
    m_io->importOutlineImageDialog();
}

void MainWindow::importOutlineImageFile(const QString& file, const LineArtConverter::Options& options)
{
    m_io->importOutlineImageFile(file, options);
}

void MainWindow::importImageFiles(const QStringList& files)
{
    m_io->importImageFiles(files);
}

void MainWindow::insertShape(ShapeType type)
{
    if (m_view->mode() != ToolMode::Edit) {
        setToolMode(ToolMode::Edit);
    }

    if (!m_scene->isActiveLayerWritable()) {
        statusBar()->showMessage(tr("The current layer is locked or hidden; shapes cannot be "
                                    "inserted. Unlock or switch layers in the layer panel first."),
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
    const bool filled = shapeTypeIsFilled(type) && m_fillStyle != FillStyle::None;
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
    m_scene->addItemsWithUndo({item}, tr("Insert %1").arg(shapeTypeName(type)));
    item->setSelected(true);
    m_view->setFocus();
    if (type == ShapeType::Ellipse) {
        statusBar()->showMessage(tr("Inserted a circle; in edit mode drag a handle to stretch it into an ellipse"),
                                 5000);
    } else {
        statusBar()->showMessage(
            tr("Inserted %1; drag to move, drag handles to resize, color follows the current brush")
                .arg(shapeTypeName(type)),
            5000);
    }
}

void MainWindow::insertCustomShape(const QString& filePath)
{
    if (m_view->mode() != ToolMode::Edit) {
        setToolMode(ToolMode::Edit);
    }

    if (!m_scene->isActiveLayerWritable()) {
        statusBar()->showMessage(tr("The current layer is locked or hidden; shapes cannot be inserted. "
                                    "Unlock or switch layers in the layer panel first."),
                                 5000);
        return;
    }

    CustomShapeTemplate customShape;
    QString error;
    if (!loadCustomShapeTemplate(filePath, &customShape, &error)) {
        reportWarning(this, tr("Custom shape"),
                      tr("Could not read %1:\n%2").arg(QDir::toNativeSeparators(filePath), error));
        return;
    }

    auto* item = new ShapeItem();
    item->setCustomShape(customShape.name, customShape.unitPath);
    const QColor stroke = m_brushColor.alpha() < 8 ? QColor(QStringLiteral("#1E293B")) : m_brushColor;
    QColor fill = m_fillColor.isValid() ? m_fillColor : stroke;
    if (!m_fillColor.isValid()) {
        fill.setAlpha(36);
    }
    const bool filled = m_fillStyle != FillStyle::None;
    item->setStyle(stroke, filled ? fill : QColor(Qt::transparent), qBound<qreal>(2.5, m_brushWidth, 10.0));
    item->setFillStyle(filled ? m_fillStyle : FillStyle::None);

    // 按模板原始宽高比插入，最长边不超过画布的 72%，小图形也不会被拉到画布大小。
    const QRectF canvas = m_scene->canvasRect();
    const qreal aspect = qMax<qreal>(0.05, customShape.aspectRatio);
    qreal designWidth = 320.0;
    qreal designHeight = 320.0;
    if (aspect >= 1.0) {
        designHeight = 320.0 / aspect;
    } else {
        designWidth = 320.0 * aspect;
    }
    QSizeF size(designWidth, designHeight);
    const qreal fitScale = qMin<qreal>(1.0, qMin(canvas.width() * 0.72 / qMax<qreal>(1.0, size.width()),
                                                 canvas.height() * 0.72 / qMax<qreal>(1.0, size.height())));
    size *= fitScale;
    size.setWidth(qMax<qreal>(24.0, size.width()));
    size.setHeight(qMax<qreal>(24.0, size.height()));

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
    m_scene->addItemsWithUndo({item}, tr("Insert custom shape “%1”").arg(customShape.name));
    item->setSelected(true);
    m_view->setFocus();
    statusBar()->showMessage(tr("Inserted custom shape “%1” from the shapes folder").arg(customShape.name), 4000);
}

void MainWindow::openShapesFolder()
{
    const QString directory = AppSettings::shapesDir();
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(directory))) {
        statusBar()->showMessage(tr("Could not open the shapes folder: %1").arg(QDir::toNativeSeparators(directory)),
                                 5000);
    }
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

void MainWindow::applyFillToSelection()
{
    m_style->applyFillToSelection();
}

void MainWindow::updateFillActions()
{
    m_style->updateFillActions();
}

void MainWindow::insertTextAt(const QPointF& scenePos)
{
    auto* item = new TextItem();
    item->setTextFormat(tr("Text"), item->textFont(), m_brushColor);

    const QRectF canvas = m_scene->canvasRect();
    const QSizeF size = item->contentSize();
    qreal x = scenePos.x() - size.width() / 2.0;
    qreal y = scenePos.y() - size.height() / 2.0;
    x = qBound(canvas.left(), x, qMax(canvas.left(), canvas.right() - size.width()));
    y = qBound(canvas.top(), y, qMax(canvas.top(), canvas.bottom() - size.height()));
    item->setPos(QPointF(x, y));

    setToolMode(ToolMode::Edit);
    m_scene->addItemsWithUndo({item}, tr("Add text"));
    item->setSelected(true);
    m_view->setFocus();
    statusBar()->showMessage(tr("Text item added"), 4000);

    // 文字正文现在由属性窗直接编辑；添加后立即打开属性窗，避免再维护独立
    // 的文字输入对话框。
    showItemProperties(item);
}

void MainWindow::showSelectedItemProperties()
{
    const QList<CanvasItem*> items = m_scene->selectedCanvasItems();
    if (items.size() != 1) {
        statusBar()->showMessage(tr("Select a single item to show its properties"), 3000);
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
        statusBar()->showMessage(tr("Rotation is available in edit mode only"), 3000);
        return;
    }
    const int changed = m_scene->rotateSelected(degrees, degrees > 0 ? tr("Rotate items clockwise")
                                                                     : tr("Rotate items counterclockwise"));
    if (changed == 0) {
        statusBar()->showMessage(tr("No selected items can be rotated"), 3000);
        return;
    }
    statusBar()->showMessage(tr("Rotated %1 item(s) by %2°").arg(changed).arg(qAbs(degrees)), 3000);
}

void MainWindow::applyAlignment(Alignment::Mode mode)
{
    if (m_view->mode() != ToolMode::Edit) {
        statusBar()->showMessage(tr("Alignment is available in edit mode only"), 3000);
        return;
    }
    QString commandText;
    switch (mode) {
    case Alignment::Mode::Left:
        commandText = tr("Align items left");
        break;
    case Alignment::Mode::HorizontalCenter:
        commandText = tr("Align items horizontal centers");
        break;
    case Alignment::Mode::Right:
        commandText = tr("Align items right");
        break;
    case Alignment::Mode::Top:
        commandText = tr("Align items top");
        break;
    case Alignment::Mode::VerticalCenter:
        commandText = tr("Align items vertical centers");
        break;
    case Alignment::Mode::Bottom:
        commandText = tr("Align items bottom");
        break;
    case Alignment::Mode::DistributeHorizontal:
        commandText = tr("Distribute items horizontally");
        break;
    case Alignment::Mode::DistributeVertical:
        commandText = tr("Distribute items vertically");
        break;
    }
    const int moved = Alignment::apply(m_scene, mode, commandText);
    if (moved == 0) {
        statusBar()->showMessage(tr("The selected items are already aligned"), 3000);
        return;
    }
    statusBar()->showMessage(tr("Aligned %1 item(s)").arg(moved), 3000);
}

void MainWindow::applyBoolOperation(BoolOps::Operation operation)
{
    if (m_view->mode() != ToolMode::Edit) {
        statusBar()->showMessage(tr("Merge shapes is available in edit mode only"), 3000);
        return;
    }
    QString commandText;
    switch (operation) {
    case BoolOps::Operation::Union:
        commandText = tr("Union shapes");
        break;
    case BoolOps::Operation::Combine:
        commandText = tr("Combine shapes and cut the overlap");
        break;
    case BoolOps::Operation::Fragment:
        commandText = tr("Fragment shapes");
        break;
    case BoolOps::Operation::Intersect:
        commandText = tr("Intersect shapes");
        break;
    case BoolOps::Operation::Subtract:
        commandText = tr("Subtract shapes");
        break;
    }
    if (!BoolOps::apply(m_scene, operation, commandText)) {
        statusBar()->showMessage(tr("Select at least two shapes with a visible outline"), 3000);
        return;
    }
    statusBar()->showMessage(tr("Merged into one shape (undo to restore)"), 3000);
    updateActions();
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
    // 先按字典把已经创建出来的文字整体切到目标语言。
    uiRetranslateWidgetTree(this);

    // 形状菜单的文字与提示是拼出来的，字典无法整体命中，按当前语言重新生成。
    if (m_shapeMenu) {
        for (QAction* action : m_shapeMenu->actions()) {
            const ShapeType type = static_cast<ShapeType>(action->data().toInt());
            action->setText(shapeTypeName(type));
            action->setToolTip(tr("Insert %1 (uses the current brush color)").arg(shapeTypeName(type)));
        }
    }

    // 图层面板每行文字（图层名、图元数、锁定/隐藏后缀）是动态拼的，重建一次。
    if (m_docks && m_docks->layerPanel()) {
        m_docks->layerPanel()->refresh();
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
    // 设置窗里的线稿化参数已经随整份保存落盘，进程内副本跟着刷新。
    m_lineArtSettings.copyLineArtFrom(m_settings);

    setBrushColor(m_settings.brushColor, false);
    setBrushWidthUi(m_settings.brushWidth);
    setStrokeSmoothingUi(m_settings.strokeSmoothing);
    m_view->setCanvasColor(m_settings.canvasColor);
    m_scene->setShowGrid(m_settings.showGrid);
    // 设置里改过辅助网格时，视图菜单的勾选状态跟着走（屏蔽信号，避免回写一遍配置）。
    if (m_gridAct) {
        const QSignalBlocker blocker(m_gridAct);
        m_gridAct->setChecked(m_settings.showGrid);
    }
    m_scene->setCanvasSizeWithUndo(m_settings.canvasSize);

    if (m_view->mode() == ToolMode::View) {
        m_view->fitAll();
    }

    m_settings.save();
    updateStatusBar();
    statusBar()->showMessage(tr("Settings saved"), 3000);
}

void MainWindow::fitAll()
{
    m_viewStatus->fitAll();
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
    QString html;
    html += QStringLiteral("<h3>%1 %2</h3>").arg(tr("StoryBoard Canvas"), QCoreApplication::applicationVersion());
    html += QStringLiteral("<p>%1</p>")
                .arg(tr("A lightweight canvas app for freehand strokes, image items, overview "
                        "navigation, canvas files and export."));
    // 关于窗口的线稿化备注：说明入口、参数顺序和输出，和功能实际行为保持一致。
    html += QStringLiteral("<p><b>%1</b></p><ul>").arg(tr("Image outline tracing"));
    html +=
        QStringLiteral("<li>%1</li>")
            .arg(tr(
                "Import an image and the parameter dialog opens first; strokes are created only after you confirm."));
    html +=
        QStringLiteral("<li>%1</li>")
            .arg(
                tr("Four presets (keep details / balanced / clean lines / bold blocks) set color levels, "
                   "same-color similarity, adjacent-color similarity, smallest block and simplify tolerance in one go; "
                   "editing any value switches back to Custom."));
    html += QStringLiteral("<li>%1</li>")
                .arg(tr("Simplify tolerance and thinnest band are adjustable; OK / Cancel apply to this run only, "
                        "Save to configuration file writes the values into StoryBoard.ini."));
    html +=
        QStringLiteral("<li>%1</li>")
            .arg(tr("Block count limit with automatic downsampling: checked by default, the image is processed at a "
                    "smaller size instead of dropping blocks."));
    html += QStringLiteral("<li>%1</li>")
                .arg(tr("Output line width is fixed at 1 and the result is inserted as plain stroke items."));
    html += QStringLiteral("</ul><p><b>%1</b></p><ul>").arg(tr("Key operations"));
    const QStringList operations = {
        QStringLiteral("V View mode: pan / zoom the whole canvas, automatic overview"),
        QStringLiteral("E Edit mode: items on visible, unlocked layers can be edited; locked layers cannot; ] / [ "
                       "adjust the height of the selection; right-click adjusts layer / height"),
        QStringLiteral("B Brush mode: freehand strokes, with color and width options"),
        QStringLiteral("T Text mode: click the canvas to add text, double-click text to edit it again"),
        QStringLiteral("Layers: the checkbox in the right panel toggles visibility, the lock button locks a layer; "
                       "layers and items both support multi-select"),
        QStringLiteral("Insert → Shapes: quickly insert rectangles, ellipses, arrows, braces, speech bubbles and more"),
        QStringLiteral("Mouse: wheel zooms, Space / middle button pans in any mode; in edit mode holding Ctrl "
                       "and dragging also pans the whole view"),
        QStringLiteral("Ctrl+0 returns to the overview, Ctrl+Z / Ctrl+Y undo / redo"),
    };
    for (const QString& operation : operations) {
        html += QStringLiteral("<li>%1</li>").arg(uiText(operation));
    }
    html += QStringLiteral("</ul><p>%1</p>").arg(tr("Built with Qt %1.").arg(QString::fromLatin1(qVersion())));
    QMessageBox box(QMessageBox::Information, tr("About StoryBoard Canvas"), html, QMessageBox::NoButton, this);
    box.addButton(tr("OK"), QMessageBox::AcceptRole);
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
    // 退出时按实际上下位置再记一次卡片顺序，拖拽换位后即使没有触发移动事件也能落盘。
    m_settings.dockOrder = m_docks ? m_docks->currentDockOrder() : QStringList();
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
