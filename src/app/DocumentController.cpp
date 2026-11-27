#include "app/DocumentController.h"

#include "app/AutoSaveService.h"
#include "app/CanvasSession.h"
#include "app/CanvasTabsController.h"
#include "app/LifecycleLog.h"
#include "app/Settings.h"
#include "app/TestMode.h"
#include "canvas/CanvasView.h"
#include "io/ExcalidrawFormat.h"
#include "scene/CanvasScene.h"
#include "ui/icons/IconFactory.h"
#include "ui/mainwindow/MainWindow.h"

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
#include <QTimer>
#include <QUndoStack>
#include <QUrl>
#include <QVBoxLayout>

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

QString CanvasFilter()
{
    return MainWindow::tr("StoryBoard canvases (*.sbcanvas);;Excalidraw scenes (*.excalidraw);;All files (*.*)");
}

bool SceneHasImages(const QJsonObject& scene)
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

// 导入含图片的 Excalidraw 场景时给一次选择：三态，取消表示“这次不导入”，
// 与“取消勾选跳过图片”区分开，避免点了取消还继续加载文档。
enum class KeepImagesChoice { KEEP, SKIP, CANCEL };

KeepImagesChoice AskKeepImagesAsBitmap(QWidget* parent)
{
    if (TestMode::IsSelfTestMode()) {
        return KeepImagesChoice::KEEP;
    }
    QDialog dialog(parent);
    dialog.setWindowTitle(MainWindow::tr("Import Excalidraw"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* text = new QLabel(MainWindow::tr("This scene contains images. Keep them as bitmaps?"), &dialog);
    auto* keep = new QCheckBox(MainWindow::tr("Keep images as bitmaps"), &dialog);
    keep->setChecked(true);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(text);
    layout->addWidget(keep);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) {
        return KeepImagesChoice::CANCEL;
    }
    return keep->isChecked() ? KeepImagesChoice::KEEP : KeepImagesChoice::SKIP;
}

bool ValidateCanvasRoot(const QJsonObject& root, QString* error)
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
                           kind == QStringLiteral("angle") || kind == QStringLiteral("sine") ||
                           kind == QStringLiteral("freehand") || kind == QStringLiteral("brace") ||
                           kind == QStringLiteral("text") || kind == QStringLiteral("shape") ||
                           kind == QStringLiteral("polygon");
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
    // 多画布后「新建」不再重置当前页，而是新开一个标签，原有画布保持原样。
    if (!m_window->m_tabs) {
        return;
    }
    CanvasSession* session = m_window->m_tabs->CreateSession(true);
    if (session && session->View()) {
        session->View()->SetMode(ToolMode::VIEW);
        CanvasView* view = session->View();
        QTimer::singleShot(0, view, [view]() { view->FitAll(); });
    }
    m_window->statusBar()->showMessage(MainWindow::tr("New empty canvas created"), 3000);
}

void DocumentController::OpenCanvasDialog()
{
    const QStringList paths = QFileDialog::getOpenFileNames(m_window, MainWindow::tr("Open canvas"),
                                                            AppSettings::CanvasesDir(), CanvasFilter());
    if (!paths.isEmpty()) {
        OpenCanvasFiles(paths);
    }
}

int DocumentController::OpenCanvasFiles(const QStringList& paths)
{
    if (paths.isEmpty()) {
        return 0;
    }
    int opened = 0;
    QStringList failures;
    for (const QString& path : paths) {
        // 同一批次里的重复路径只处理一次；已在标签打开的直接切过去，不算新开。
        const QString absolutePath = QFileInfo(path).absoluteFilePath();
        if (m_window->m_tabs) {
            if (CanvasSession* existing = m_window->m_tabs->FindByPath(absolutePath)) {
                m_window->m_tabs->SetCurrent(existing);
                continue;
            }
        }
        QString error;
        if (LoadCanvasFile(path, false, &error)) {
            ++opened;
        } else {
            failures.append(QStringLiteral("%1: %2").arg(QFileInfo(path).fileName(), error));
        }
    }
    if (!failures.isEmpty() && !TestMode::IsSelfTestMode()) {
        QMessageBox::warning(
            m_window, MainWindow::tr("Open canvas"),
            MainWindow::tr("Some canvases could not be opened:\n%1").arg(failures.join(QStringLiteral("\n"))));
    }
    if (opened > 0) {
        m_window->statusBar()->showMessage(
            failures.isEmpty() ? MainWindow::tr("Opened %n canvas(es)", nullptr, opened)
                               : MainWindow::tr("Opened %1, skipped %2").arg(opened).arg(failures.size()),
            4000);
    }
    return opened;
}

bool DocumentController::OpenCanvasFile(const QString& path)
{
    return LoadCanvasFile(path, true);
}

bool DocumentController::LoadCanvasFile(const QString& path, bool showErrors, QString* errorOut)
{
    // 需求 2：文件体积闸门，20MB 以上的场景不读进内存，避免崩溃或长时间假死。
    constexpr qint64 MAX_SCENE_BYTES = 20LL * 1024 * 1024;
    if (QFileInfo(path).size() > MAX_SCENE_BYTES) {
        if (errorOut) {
            *errorOut = MainWindow::tr("This file is larger than 20MB");
        }
        if (showErrors) {
            ReportWarning(m_window, MainWindow::tr("Open canvas"),
                          MainWindow::tr("This file is larger than 20MB and was not opened:\n%1")
                              .arg(QDir::toNativeSeparators(path)));
        }
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorOut) {
            *errorOut = MainWindow::tr("Could not read the file");
        }
        if (showErrors) {
            ReportWarning(m_window, MainWindow::tr("Open canvas"),
                          MainWindow::tr("Could not read the file:\n%1").arg(QDir::toNativeSeparators(path)));
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorOut) {
            *errorOut = MainWindow::tr("Failed to parse the canvas file: %1").arg(parseError.errorString());
        }
        if (showErrors) {
            ReportWarning(m_window, MainWindow::tr("Open canvas"),
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
        // 含图片的场景先问一次是否保留位图；点取消表示放弃本次导入，不再往下改文档。
        bool keepImages = false;
        if (SceneHasImages(root)) {
            const KeepImagesChoice choice = AskKeepImagesAsBitmap(m_window);
            if (choice == KeepImagesChoice::CANCEL) {
                return false;
            }
            keepImages = choice == KeepImagesChoice::KEEP;
        }
        usableRoot = Excalidraw::SceneToStoryBoard(root, &excalidrawNotes, keepImages);
        if (usableRoot.isEmpty()) {
            if (errorOut) {
                *errorOut = MainWindow::tr("This Excalidraw scene has no supported elements.");
            }
            if (showErrors) {
                ReportWarning(m_window, MainWindow::tr("Import Excalidraw"),
                              MainWindow::tr("This Excalidraw scene has no supported elements."));
            }
            return false;
        }
        importedExcalidraw = true;
    }

    QString validationError;
    if (!ValidateCanvasRoot(usableRoot, &validationError)) {
        if (errorOut) {
            *errorOut = validationError;
        }
        if (showErrors) {
            ReportWarning(m_window, MainWindow::tr("Open canvas"), validationError);
        }
        return false;
    }

    int legacyStrokes = 0;
    for (const QJsonValue& value : usableRoot.value(QStringLiteral("items")).toArray()) {
        if (value.toObject().value(QStringLiteral("kind")).toString() == QStringLiteral("stroke")) {
            ++legacyStrokes;
        }
    }

    // 同一文件已经在某个标签里打开时直接切过去，避免两份互写。
    const QString absolutePath = QFileInfo(path).absoluteFilePath();
    if (!importedExcalidraw && m_window->m_tabs) {
        if (CanvasSession* existing = m_window->m_tabs->FindByPath(absolutePath)) {
            m_window->m_tabs->SetCurrent(existing);
            m_window->statusBar()->showMessage(
                MainWindow::tr("Canvas is already open: %1").arg(QFileInfo(path).fileName()), 3000);
            return true;
        }
    }

    // 当前页是干净的空画布就复用它，否则新开一个标签；打开不再是破坏性操作，
    // 不再对当前脏页弹保存提示。
    CanvasSession* target = m_window->m_tabs ? m_window->m_tabs->Current() : nullptr;
    const bool reused = target && target->IsPristine();
    if (!target || !target->IsPristine()) {
        target = m_window->m_tabs->CreateSession(false);
    }
    if (!target) {
        if (errorOut) {
            *errorOut = MainWindow::tr("Could not create a new canvas tab");
        }
        return false;
    }

    target->View()->FinishCurrentStroke();
    const QString logLabel =
        target->LogLabel(importedExcalidraw ? QStringLiteral("导入画布") : QFileInfo(path).completeBaseName());
    if (target->Scene()) {
        target->Scene()->SetLogLabel(logLabel);
    }
    QString loadError;
    if (!target->Scene()->FromJson(usableRoot, &loadError)) {
        if (!reused) {
            m_window->m_tabs->RemoveSession(target);
        }
        if (errorOut) {
            *errorOut = loadError;
        }
        if (showErrors) {
            ReportWarning(m_window, MainWindow::tr("Open canvas"), loadError);
        }
        return false;
    }

    // 导入的 Excalidraw 场景作为一份新文档，原 .excalidraw 文件仍可再导入一次。
    target->SetFilePath(importedExcalidraw ? QString() : absolutePath);
    target->Undo()->setClean();
    // 只有从真实文件读入的图元才算「已落盘」；Excalidraw 导入还没有目标文件。
    if (!importedExcalidraw && target->Scene()) {
        target->Scene()->MarkSavedItemStates();
    }
    m_window->m_tabs->SetCurrent(target);
    m_window->ActivateSession(target);
    // 打开 / 导入后同样回到查看模式并整幅适配。
    CanvasView* targetView = target->View();
    targetView->SetMode(ToolMode::VIEW);
    QTimer::singleShot(0, targetView, [targetView]() { targetView->FitAll(); });

    if (!importedExcalidraw) {
        m_window->m_settings.lastCanvasPath = absolutePath;
    }
    m_window->m_tabs->UpdateTabLabel(target);
    LifecycleLog::LogCanvas(logLabel, importedExcalidraw ? QStringLiteral("导入") : QStringLiteral("打开"),
                            QDir::toNativeSeparators(absolutePath));
    if (importedExcalidraw) {
        QString message = MainWindow::tr("Imported an Excalidraw scene as a new canvas");
        if (!excalidrawNotes.isEmpty()) {
            message += QStringLiteral(" (") + excalidrawNotes.join(QStringLiteral("; ")) + QStringLiteral(")");
        }
        m_window->statusBar()->showMessage(message, 6000);
    } else {
        m_window->statusBar()->showMessage(
            MainWindow::tr("Canvas opened: %1").arg(QFileInfo(target->FilePath()).fileName()), 4000);
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
        ReportWarning(m_window, MainWindow::tr("Save canvas"),
                      MainWindow::tr("Could not write the file:\n%1").arg(QDir::toNativeSeparators(path)));
        return false;
    }
    file.write(data);
    if (!file.commit()) {
        ReportWarning(m_window, MainWindow::tr("Save canvas"),
                      MainWindow::tr("Failed to save the canvas:\n%1").arg(QDir::toNativeSeparators(path)));
        return false;
    }

    m_window->m_filePath = QFileInfo(path).absoluteFilePath();
    if (m_window->m_tabs && m_window->m_tabs->Current()) {
        CanvasSession* session = m_window->m_tabs->Current();
        session->SetFilePath(m_window->m_filePath);
        m_window->m_tabs->UpdateTabLabel(session);
        LifecycleLog::LogCanvas(session->LogLabel(), QStringLiteral("保存"),
                                QDir::toNativeSeparators(m_window->m_filePath));
    }
    m_window->m_settings.lastCanvasPath = m_window->m_filePath;
    m_window->m_settings.Save();
    m_window->m_undo->setClean();
    if (m_window->m_scene) {
        m_window->m_scene->MarkSavedItemStates();
    }
    if (m_window->AutoSave()) {
        m_window->AutoSave()->MarkSaved(m_window->m_tabs ? m_window->m_tabs->Current() : nullptr);
    }
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
        ReportWarning(m_window, MainWindow::tr("Export custom shape"),
                      MainWindow::tr("Could not write the file:\n%1").arg(QDir::toNativeSeparators(path)));
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        ReportWarning(m_window, MainWindow::tr("Export custom shape"),
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
        ReportWarning(m_window, MainWindow::tr("Export Excalidraw"),
                      MainWindow::tr("Could not write the file:\n%1").arg(QDir::toNativeSeparators(path)));
        return;
    }
    file.write(QJsonDocument(scene).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        ReportWarning(m_window, MainWindow::tr("Export Excalidraw"),
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
    return MaybeSaveSession(m_window->m_tabs ? m_window->m_tabs->Current() : nullptr);
}

bool DocumentController::MaybeSaveSession(CanvasSession* session)
{
    if (!session || !session->HasUnsavedChanges()) {
        return true;
    }
    // 询问前先切到该会话：保存动作永远作用在当前会话上。
    if (m_window->m_tabs) {
        m_window->m_tabs->SetCurrent(session);
    }

    QMessageBox box(m_window);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(MainWindow::tr("Unsaved changes"));
    box.setText(MainWindow::tr("\"%1\" has unsaved changes.").arg(session->DisplayName()));
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

bool DocumentController::CloseCanvasSession(CanvasSession* session)
{
    if (!session) {
        return false;
    }
    if (!MaybeSaveSession(session)) {
        return false;
    }
    if (m_window->m_tabs) {
        m_window->m_tabs->RemoveSession(session);
    }
    m_window->UpdateCanvasesMenu();
    return true;
}

bool DocumentController::CloseAllCanvases()
{
    if (!m_window->m_tabs) {
        return true;
    }
    // 按标签顺序逐页确认；已经确认过的脏页在保存后自然变干净，不会重复询问。
    const QList<CanvasSession*> sessions = m_window->m_tabs->Sessions();
    for (CanvasSession* session : sessions) {
        if (!MaybeSaveSession(session)) {
            return false;
        }
    }
    return true;
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
            QAction* action = new QAction(IconFactory::Make(IconId::SAVE, QColor(90, 98, 112), 16),
                                          info.completeBaseName(), m_window->m_fileMenu);
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
    if (m_window->m_tabs && m_window->m_tabs->Current()) {
        return m_window->m_tabs->Current()->DisplayName();
    }
    return MainWindow::tr("Untitled canvas 1");
}

void DocumentController::UpdateTitle()
{
    CanvasSession* session = m_window->m_tabs ? m_window->m_tabs->Current() : nullptr;
    if (session && m_window->m_tabs) {
        m_window->m_tabs->UpdateTabLabel(session);
    }
    m_window->setWindowTitle(MainWindow::tr("%1[*] - StoryBoard").arg(DocumentName()));
    m_window->setWindowModified(session ? session->HasUnsavedChanges()
                                        : (m_window->m_undo && !m_window->m_undo->isClean()));
    if (session && !session->FilePath().isEmpty()) {
        m_window->setWindowFilePath(session->FilePath());
    } else {
        m_window->setWindowFilePath(QString());
    }
}
