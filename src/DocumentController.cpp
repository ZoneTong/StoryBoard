#include "DocumentController.h"

#include "CanvasScene.h"
#include "CanvasView.h"
#include "ExcalidrawFormat.h"
#include "IconFactory.h"
#include "MainWindow.h"
#include "Settings.h"

#include <QAction>
#include <QCheckBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QSaveFile>
#include <QStatusBar>
#include <QUndoStack>
#include <QUrl>
#include <QVBoxLayout>

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

QString canvasFilter()
{
    return MainWindow::tr("StoryBoard canvases (*.sbcanvas);;Excalidraw scenes (*.excalidraw);;All files (*.*)");
}

bool sceneHasImages(const QJsonObject& scene)
{
    for (const QJsonValue& value : scene.value(QStringLiteral("elements")).toArray()) {
        const QJsonObject element = value.toObject();
        if (element.value(QStringLiteral("type")).toString() == QStringLiteral("image") &&
            !element.value(QStringLiteral("isDeleted")).toBool(false)) {
            return true;
        }
    }
    return false;
}

// 导入含图片的 Excalidraw 场景时给一次选择：默认跳过图片，勾选后按 dataURL 保留成位图图元。
bool askKeepImagesAsBitmap(QWidget* parent)
{
    if (selfTestMode()) {
        return false;
    }
    QDialog dialog(parent);
    dialog.setWindowTitle(MainWindow::tr("Import Excalidraw"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* text = new QLabel(MainWindow::tr("This scene contains images. Keep them as bitmaps?"), &dialog);
    auto* keep = new QCheckBox(MainWindow::tr("Keep images as bitmaps"), &dialog);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(text);
    layout->addWidget(keep);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    return dialog.exec() == QDialog::Accepted && keep->isChecked();
}

bool validateCanvasRoot(const QJsonObject& root, QString* error)
{
    if (root.value(QStringLiteral("format")).toString() != QStringLiteral("StoryBoardCanvas")) {
        if (error) {
            *error = MainWindow::tr("This is not a valid StoryBoard canvas file.");
        }
        return false;
    }

    const QJsonObject canvas = root.value(QStringLiteral("canvas")).toObject();
    // 画布尺寸允许带小数（拖拽边缘时通常不是整数）；toInt() 会把 800.5 截成 800，
    // 更会把 0.5 判成 0 而误报“尺寸无效”。
    const qreal width = canvas.value(QStringLiteral("width")).toDouble(0.0);
    const qreal height = canvas.value(QStringLiteral("height")).toDouble(0.0);
    if (width <= 0.0 || height <= 0.0) {
        if (error) {
            *error = MainWindow::tr("The canvas size in the canvas file is invalid.");
        }
        return false;
    }

    // layers 必须是非空数组：缺失 / 置空的「半坏」文件在这里就拦下，
    // 不进 MaybeSave() 消耗用户的未保存内容。
    if (!root.value(QStringLiteral("layers")).isArray() || root.value(QStringLiteral("layers")).toArray().isEmpty()) {
        if (error) {
            *error = MainWindow::tr("This is not a valid StoryBoard canvas file.");
        }
        return false;
    }

    const QJsonArray items = root.value(QStringLiteral("items")).toArray();
    for (const QJsonValue& value : items) {
        const QJsonObject object = value.toObject();
        QString kind = object.value(QStringLiteral("kind")).toString();
        // 旧画布 / 旧模板用 type 记类型，曲线族还共用 curve；折算后再做白名单校验。
        if (kind.isEmpty()) {
            kind = object.value(QStringLiteral("type")).toString();
        }
        if (kind == QStringLiteral("curve")) {
            kind = QStringLiteral("freehand");
        } else if (kind == QStringLiteral("line")) {
            kind = QStringLiteral("straightline");
        }
        // 需求 14：图片类型图元已并入矩形（图片只作背景），"image" 不再是合法类型。
        // 线条族的五个形态各有自己的 kind，不再共用族级 "curve"。
        const bool known = kind == QStringLiteral("straightline") || kind == QStringLiteral("arc") ||
                           kind == QStringLiteral("sine") || kind == QStringLiteral("freehand") ||
                           kind == QStringLiteral("brace") || kind == QStringLiteral("text") ||
                           kind == QStringLiteral("shape") || kind == QStringLiteral("polygon");
        if (!known) {
            if (kind == QStringLiteral("stroke")) {
                // 旧笔画图元不做兼容：加载时整体跳过，由加载流程统一提示。
                continue;
            }
            if (error) {
                *error = MainWindow::tr("The canvas contains an unknown item type: %1").arg(kind);
            }
            return false;
        }
    }
    return true;
}

} // namespace

DocumentController::DocumentController(MainWindow* window) : m_window(window) {}

void DocumentController::NewCanvas()
{
    if (!MaybeSave()) {
        return;
    }

    m_window->m_view->CancelCurrentStroke();
    m_window->m_scene->ResetDocument();
    m_window->m_scene->ApplyCanvasSize(m_window->m_settings.canvasSize);
    m_window->m_scene->SetShowGrid(m_window->m_settings.showGrid);
    m_window->m_scene->SetCanvasBackground(CanvasBackground());
    m_window->m_filePath.clear();
    m_window->m_undo->clear();
    m_window->m_undo->setClean();

    if (m_window->m_view->mode() != ToolMode::VIEW) {
        m_window->m_view->SetMode(ToolMode::VIEW);
    } else {
        m_window->m_view->FitAll();
    }

    m_window->UpdateModeUi();
    m_window->UpdateTitle();
    m_window->UpdateStatusBar();
    m_window->UpdateActions();
    m_window->statusBar()->showMessage(MainWindow::tr("New empty canvas created"), 3000);
}

void DocumentController::OpenCanvasDialog()
{
    const QString path = QFileDialog::getOpenFileName(m_window, MainWindow::tr("Open canvas"),
                                                      AppSettings::CanvasesDir(), canvasFilter());
    if (!path.isEmpty()) {
        LoadCanvasFile(path, true);
    }
}

bool DocumentController::OpenCanvasFile(const QString& path)
{
    return LoadCanvasFile(path, true);
}

bool DocumentController::LoadCanvasFile(const QString& path, bool showErrors)
{
    // 需求 2：文件体积闸门，20MB 以上的场景不读进内存，避免崩溃或长时间假死。
    constexpr qint64 MAX_SCENE_BYTES = 20LL * 1024 * 1024;
    if (QFileInfo(path).size() > MAX_SCENE_BYTES) {
        if (showErrors) {
            reportWarning(m_window, MainWindow::tr("Open canvas"),
                          MainWindow::tr("This file is larger than 20MB and was not opened:\n%1")
                              .arg(QDir::toNativeSeparators(path)));
        }
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (showErrors) {
            reportWarning(m_window, MainWindow::tr("Open canvas"),
                          MainWindow::tr("Could not read the file:\n%1").arg(QDir::toNativeSeparators(path)));
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (showErrors) {
            reportWarning(m_window, MainWindow::tr("Open canvas"),
                          MainWindow::tr("Failed to parse the canvas file:\n%1").arg(parseError.errorString()));
        }
        return false;
    }

    const QJsonObject root = document.object();
    // 需求 9：直接打开 .excalidraw 时当成导入，转换成一个新的 StoryBoard 文档。
    bool importedExcalidraw = false;
    QJsonObject usableRoot = root;
    QStringList excalidrawNotes;
    if (Excalidraw::IsScene(root)) {
        // 含图片的场景先问一次是否保留位图，默认按老行为跳过图片。
        const bool keepImages = sceneHasImages(root) && askKeepImagesAsBitmap(m_window);
        usableRoot = Excalidraw::SceneToStoryBoard(root, &excalidrawNotes, keepImages);
        if (usableRoot.isEmpty()) {
            if (showErrors) {
                reportWarning(m_window, MainWindow::tr("Import Excalidraw"),
                              MainWindow::tr("This Excalidraw scene has no supported elements."));
            }
            return false;
        }
        importedExcalidraw = true;
    }

    QString validationError;
    if (!validateCanvasRoot(usableRoot, &validationError)) {
        if (showErrors) {
            reportWarning(m_window, MainWindow::tr("Open canvas"), validationError);
        }
        return false;
    }

    int legacyStrokes = 0;
    for (const QJsonValue& value : usableRoot.value(QStringLiteral("items")).toArray()) {
        if (value.toObject().value(QStringLiteral("kind")).toString() == QStringLiteral("stroke")) {
            ++legacyStrokes;
        }
    }

    if (!MaybeSave()) {
        return false;
    }

    m_window->m_view->FinishCurrentStroke();
    QString loadError;
    if (!m_window->m_scene->FromJson(usableRoot, &loadError)) {
        if (showErrors) {
            reportWarning(m_window, MainWindow::tr("Open canvas"), loadError);
        }
        return false;
    }

    // 导入的 Excalidraw 场景作为一份新文档，原 .excalidraw 文件仍可再导入一次。
    m_window->m_filePath = importedExcalidraw ? QString() : QFileInfo(path).absoluteFilePath();
    m_window->m_undo->setClean();
    if (m_window->m_view->mode() != ToolMode::VIEW) {
        m_window->m_view->SetMode(ToolMode::VIEW);
    } else {
        m_window->m_view->FitAll();
    }

    if (!importedExcalidraw) {
        m_window->m_settings.lastCanvasPath = m_window->m_filePath;
    }
    m_window->UpdateModeUi();
    m_window->UpdateTitle();
    m_window->UpdateStatusBar();
    m_window->UpdateActions();
    if (importedExcalidraw) {
        QString message = MainWindow::tr("Imported an Excalidraw scene as a new canvas");
        if (!excalidrawNotes.isEmpty()) {
            message += QStringLiteral(" (") + excalidrawNotes.join(QStringLiteral("; ")) + QStringLiteral(")");
        }
        m_window->statusBar()->showMessage(message, 6000);
    } else {
        m_window->statusBar()->showMessage(
            MainWindow::tr("Canvas opened: %1").arg(QFileInfo(m_window->m_filePath).fileName()), 4000);
    }
    if (legacyStrokes > 0) {
        m_window->statusBar()->showMessage(
            MainWindow::tr("Skipped %1 legacy stroke Item(s); curves are not backward compatible").arg(legacyStrokes),
            6000);
    }
    return true;
}

bool DocumentController::SaveCanvas()
{
    if (m_window->m_filePath.isEmpty()) {
        return SaveCanvasAs();
    }
    return SaveToFile(m_window->m_filePath);
}

bool DocumentController::SaveCanvasAs()
{
    QString suggestion = m_window->m_filePath;
    if (suggestion.isEmpty()) {
        const QString name = DocumentName();
        suggestion = QDir(AppSettings::CanvasesDir()).filePath(name + QStringLiteral(".sbcanvas"));
    }

    QString path = QFileDialog::getSaveFileName(m_window, MainWindow::tr("Save canvas as"), suggestion,
                                                MainWindow::tr("StoryBoard canvases (*.sbcanvas)"));
    if (path.isEmpty()) {
        return false;
    }
    if (!path.endsWith(QStringLiteral(".sbcanvas"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".sbcanvas");
    }
    return SaveToFile(path);
}

bool DocumentController::SaveToFile(const QString& path)
{
    m_window->m_view->FinishCurrentStroke();

    const QJsonDocument document(m_window->m_scene->ToJson());
    const QByteArray data = document.toJson(QJsonDocument::Indented);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        reportWarning(m_window, MainWindow::tr("Save canvas"),
                      MainWindow::tr("Could not write the file:\n%1").arg(QDir::toNativeSeparators(path)));
        return false;
    }
    file.write(data);
    if (!file.commit()) {
        reportWarning(m_window, MainWindow::tr("Save canvas"),
                      MainWindow::tr("Failed to save the canvas:\n%1").arg(QDir::toNativeSeparators(path)));
        return false;
    }

    m_window->m_filePath = QFileInfo(path).absoluteFilePath();
    m_window->m_settings.lastCanvasPath = m_window->m_filePath;
    m_window->m_settings.Save();
    m_window->m_undo->setClean();
    m_window->UpdateTitle();
    m_window->UpdateActions();
    m_window->statusBar()->showMessage(
        MainWindow::tr("Canvas saved: %1").arg(QFileInfo(m_window->m_filePath).fileName()), 4000);
    return true;
}

void DocumentController::ExportCustomShape()
{
    const QJsonArray items = m_window->m_scene->ClipboardItems();
    if (items.isEmpty()) {
        m_window->statusBar()->showMessage(MainWindow::tr("Select at least one editable item first"), 3000);
        return;
    }

    // 选区归一化：坐标平移到包围盒原点，画布尺寸等于包围盒，单层、无背景，
    // 这样插到画布时按长边 64 重绘，不会带着原文档的坐标跑偏。
    QRectF bounds;
    for (const QJsonValue& value : items) {
        const QJsonObject item = value.toObject();
        const QRectF rect(item.value(QStringLiteral("x")).toDouble(), item.value(QStringLiteral("y")).toDouble(),
                          item.value(QStringLiteral("w")).toDouble(), item.value(QStringLiteral("h")).toDouble());
        bounds = bounds.isNull() ? rect : bounds.united(rect);
    }
    if (bounds.width() <= 0.0 || bounds.height() <= 0.0) {
        m_window->statusBar()->showMessage(MainWindow::tr("The selection has no visible outline"), 3000);
        return;
    }

    // 默认存到可写模板目录：配置同级不可写时（装在 Program Files）不会把文件丢到只读路径。
    QString shapesDir = AppSettings::WritableShapesDir();
    if (shapesDir.isEmpty()) {
        shapesDir = AppSettings::ShapesDir();
    }
    QString suggestion = QDir(shapesDir).filePath(DocumentName() + QStringLiteral(".sbcanvas"));
    QString path = QFileDialog::getSaveFileName(m_window, MainWindow::tr("Export custom shape"), suggestion,
                                                MainWindow::tr("StoryBoard canvases (*.sbcanvas)"));
    if (path.isEmpty()) {
        return;
    }
    if (!path.endsWith(QStringLiteral(".sbcanvas"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".sbcanvas");
    }

    QJsonArray shifted;
    for (const QJsonValue& value : items) {
        QJsonObject item = value.toObject();
        item.insert(QStringLiteral("x"), item.value(QStringLiteral("x")).toDouble() - bounds.left());
        item.insert(QStringLiteral("y"), item.value(QStringLiteral("y")).toDouble() - bounds.top());
        item.insert(QStringLiteral("layer"), 1);
        item.remove(QStringLiteral("group"));
        shifted.append(item);
    }

    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("StoryBoardCanvas"));
    root.insert(QStringLiteral("version"), 2);
    QJsonObject canvas;
    canvas.insert(QStringLiteral("x"), 0);
    canvas.insert(QStringLiteral("y"), 0);
    canvas.insert(QStringLiteral("width"), bounds.width());
    canvas.insert(QStringLiteral("height"), bounds.height());
    root.insert(QStringLiteral("canvas"), canvas);
    QJsonObject background;
    background.insert(QStringLiteral("type"), QStringLiteral("none"));
    root.insert(QStringLiteral("background"), background);
    QJsonObject layer;
    layer.insert(QStringLiteral("id"), 1);
    layer.insert(QStringLiteral("name"), QStringLiteral("图层 1"));
    layer.insert(QStringLiteral("visible"), true);
    layer.insert(QStringLiteral("locked"), false);
    root.insert(QStringLiteral("layers"), QJsonArray{layer});
    root.insert(QStringLiteral("activeLayer"), 1);
    root.insert(QStringLiteral("items"), shifted);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        reportWarning(m_window, MainWindow::tr("Export custom shape"),
                      MainWindow::tr("Could not write the file:\n%1").arg(QDir::toNativeSeparators(path)));
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        reportWarning(m_window, MainWindow::tr("Export custom shape"),
                      MainWindow::tr("Failed to save the file:\n%1").arg(QDir::toNativeSeparators(path)));
        return;
    }

    m_window->statusBar()->showMessage(MainWindow::tr("Custom shape saved: %1").arg(QFileInfo(path).fileName()), 4000);
}

void DocumentController::ExportExcalidraw()
{
    m_window->m_view->FinishCurrentStroke();
    QString suggestion = m_window->m_filePath;
    if (suggestion.isEmpty()) {
        suggestion = QDir(AppSettings::CanvasesDir()).filePath(DocumentName() + QStringLiteral(".excalidraw"));
    } else {
        suggestion = QFileInfo(suggestion).absolutePath() + QStringLiteral("/") +
                     QFileInfo(suggestion).completeBaseName() + QStringLiteral(".excalidraw");
    }

    QString path = QFileDialog::getSaveFileName(m_window, MainWindow::tr("Export Excalidraw"), suggestion,
                                                MainWindow::tr("Excalidraw scenes (*.excalidraw)"));
    if (path.isEmpty()) {
        return;
    }
    if (!path.endsWith(QStringLiteral(".excalidraw"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".excalidraw");
    }

    QStringList notes;
    const QJsonObject scene = Excalidraw::StoryBoardToScene(m_window->m_scene->ToJson(), &notes);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        reportWarning(m_window, MainWindow::tr("Export Excalidraw"),
                      MainWindow::tr("Could not write the file:\n%1").arg(QDir::toNativeSeparators(path)));
        return;
    }
    file.write(QJsonDocument(scene).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        reportWarning(m_window, MainWindow::tr("Export Excalidraw"),
                      MainWindow::tr("Failed to save the file:\n%1").arg(QDir::toNativeSeparators(path)));
        return;
    }

    QString message = MainWindow::tr("Excalidraw scene exported: %1").arg(QFileInfo(path).fileName());
    if (!notes.isEmpty()) {
        message += QStringLiteral(" (") + notes.join(QStringLiteral("; ")) + QStringLiteral(")");
    }
    m_window->statusBar()->showMessage(message, 5000);
}

bool DocumentController::MaybeSave()
{
    if (!m_window->m_undo || m_window->m_undo->isClean()) {
        return true;
    }

    QMessageBox box(m_window);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(MainWindow::tr("Unsaved changes"));
    box.setText(MainWindow::tr("The current canvas has unsaved changes."));
    box.setInformativeText(MainWindow::tr("Save this canvas first?"));
    QPushButton* saveButton = box.addButton(MainWindow::tr("Save canvas"), QMessageBox::AcceptRole);
    QPushButton* discardButton = box.addButton(MainWindow::tr("Don't save"), QMessageBox::DestructiveRole);
    box.addButton(MainWindow::tr("Cancel"), QMessageBox::RejectRole);
    box.setDefaultButton(saveButton);
    box.exec();

    if (box.clickedButton() == saveButton) {
        return SaveCanvas();
    }
    if (box.clickedButton() == discardButton) {
        return true;
    }
    return false;
}

void DocumentController::UpdateCanvasesMenu()
{
    if (!m_window->m_fileMenu || !m_window->m_boardSectionSeparator) {
        return;
    }

    // 先清掉上一轮动态生成的项：分隔线之后、退出之前的内容全部重建。
    const QList<QAction*> existing = m_window->m_fileMenu->actions();
    const int anchor = existing.indexOf(m_window->m_boardSectionSeparator);
    if (anchor >= 0) {
        for (int i = anchor + 1; i < existing.size(); ++i) {
            QAction* action = existing.at(i);
            if (!action || action == m_window->m_quitAct) {
                continue;
            }
            m_window->m_fileMenu->removeAction(action);
            if (action != m_window->m_settingsAct) {
                action->deleteLater();
            }
        }
    }

    QList<QAction*> items;
    // 需求 37：设置留在文件菜单里；最近画布列表每次都重新读目录。
    items.append(m_window->m_settingsAct);
    items.append(nullptr); // 占位：之后换成真正的分隔线

    QDir directory(AppSettings::CanvasesDir());
    const QFileInfoList files = directory.entryInfoList({QStringLiteral("*.sbcanvas")}, QDir::Files, QDir::Time);
    if (files.isEmpty()) {
        QAction* emptyAction = new QAction(MainWindow::tr("(Canvas folder is empty)"), m_window->m_fileMenu);
        emptyAction->setEnabled(false);
        items.append(emptyAction);
    } else {
        const int maximum = qMin(15, static_cast<int>(files.size()));
        for (int i = 0; i < maximum; ++i) {
            const QFileInfo info = files.at(i);
            QAction* action = new QAction(MakeIcon(IconId::SAVE, QColor(90, 98, 112), 16), info.completeBaseName(),
                                          m_window->m_fileMenu);
            action->setCheckable(true);
            action->setChecked(info.absoluteFilePath() == m_window->m_filePath);
            action->setToolTip(QDir::toNativeSeparators(info.absoluteFilePath()));
            const QString filePath = info.absoluteFilePath();
            QObject::connect(action, &QAction::triggered, m_window,
                             [this, filePath]() { LoadCanvasFile(filePath, true); });
            items.append(action);
        }
        if (files.size() > maximum) {
            QAction* moreAction =
                new QAction(MainWindow::tr("… %1 canvases in total").arg(files.size()), m_window->m_fileMenu);
            moreAction->setEnabled(false);
            items.append(moreAction);
        }
    }

    items.append(nullptr); // 占位：画布列表之后的分隔线
    QAction* openFolderAction = new QAction(MainWindow::tr("Open canvases folder"), m_window->m_fileMenu);
    QObject::connect(openFolderAction, &QAction::triggered, m_window, &MainWindow::OpenCanvasesFolder);
    items.append(openFolderAction);

    // 全部插到“退出”之前，保持原来画板菜单的顺序：设置 → 最近画布 → 画布文件夹。
    for (QAction* item : items) {
        if (!item) {
            QAction* separator = new QAction(m_window->m_fileMenu);
            separator->setSeparator(true);
            m_window->m_fileMenu->insertAction(m_window->m_quitAct, separator);
            continue;
        }
        m_window->m_fileMenu->insertAction(m_window->m_quitAct, item);
    }
}

void DocumentController::OpenCanvasesFolder()
{
    const QString directory = AppSettings::CanvasesDir();
    QDir().mkpath(directory);
    QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
}

QString DocumentController::DocumentName() const
{
    if (m_window->m_filePath.isEmpty()) {
        return MainWindow::tr("Untitled canvas");
    }
    const QString name = QFileInfo(m_window->m_filePath).completeBaseName();
    return name.isEmpty() ? MainWindow::tr("Untitled canvas") : name;
}

void DocumentController::UpdateTitle()
{
    m_window->setWindowTitle(MainWindow::tr("%1[*] - StoryBoard").arg(DocumentName()));
    m_window->setWindowModified(m_window->m_undo && !m_window->m_undo->isClean());
    if (!m_window->m_filePath.isEmpty()) {
        m_window->setWindowFilePath(m_window->m_filePath);
    }
}
