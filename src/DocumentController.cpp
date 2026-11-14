#include "DocumentController.h"

#include "CanvasScene.h"
#include "CanvasView.h"
#include "IconFactory.h"
#include "MainWindow.h"
#include "Settings.h"

#include <QAction>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QSaveFile>
#include <QStatusBar>
#include <QUndoStack>
#include <QUrl>

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
    return MainWindow::tr("StoryBoard canvases (*.sbcanvas);;All files (*.*)");
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
    if (canvas.value(QStringLiteral("width")).toInt() <= 0 || canvas.value(QStringLiteral("height")).toInt() <= 0) {
        if (error) {
            *error = MainWindow::tr("The canvas size in the canvas file is invalid.");
        }
        return false;
    }

    const QJsonArray items = root.value(QStringLiteral("items")).toArray();
    for (const QJsonValue& value : items) {
        const QString kind = value.toObject().value(QStringLiteral("type")).toString();
        if (kind != QStringLiteral("stroke") && kind != QStringLiteral("image") && kind != QStringLiteral("text") &&
            kind != QStringLiteral("shape")) {
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

void DocumentController::newCanvas()
{
    if (!maybeSave()) {
        return;
    }

    m_window->m_view->cancelCurrentStroke();
    m_window->m_scene->resetDocument();
    m_window->m_scene->applyCanvasSize(m_window->m_settings.canvasSize);
    m_window->m_scene->setShowGrid(m_window->m_settings.showGrid);
    m_window->m_scene->applyBackground(BackgroundState());
    m_window->m_filePath.clear();
    m_window->m_undo->clear();
    m_window->m_undo->setClean();

    if (m_window->m_view->mode() != ToolMode::View) {
        m_window->m_view->setMode(ToolMode::View);
    } else {
        m_window->m_view->fitAll();
    }

    m_window->updateModeUi();
    m_window->updateTitle();
    m_window->updateStatusBar();
    m_window->updateActions();
    m_window->statusBar()->showMessage(MainWindow::tr("New empty canvas created"), 3000);
}

void DocumentController::openCanvasDialog()
{
    const QString path = QFileDialog::getOpenFileName(m_window, MainWindow::tr("Open canvas"),
                                                      AppSettings::canvasesDir(), canvasFilter());
    if (!path.isEmpty()) {
        loadCanvasFile(path, true);
    }
}

bool DocumentController::openCanvasFile(const QString& path)
{
    return loadCanvasFile(path, true);
}

bool DocumentController::loadCanvasFile(const QString& path, bool showErrors)
{
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
    QString validationError;
    if (!validateCanvasRoot(root, &validationError)) {
        if (showErrors) {
            reportWarning(m_window, MainWindow::tr("Open canvas"), validationError);
        }
        return false;
    }

    if (!maybeSave()) {
        return false;
    }

    m_window->m_view->finishCurrentStroke();
    QString loadError;
    if (!m_window->m_scene->fromJson(root, &loadError)) {
        if (showErrors) {
            reportWarning(m_window, MainWindow::tr("Open canvas"), loadError);
        }
        return false;
    }

    m_window->m_filePath = QFileInfo(path).absoluteFilePath();
    m_window->m_undo->setClean();
    if (m_window->m_view->mode() != ToolMode::View) {
        m_window->m_view->setMode(ToolMode::View);
    } else {
        m_window->m_view->fitAll();
    }

    m_window->m_settings.lastCanvasPath = m_window->m_filePath;
    m_window->updateModeUi();
    m_window->updateTitle();
    m_window->updateStatusBar();
    m_window->updateActions();
    m_window->statusBar()->showMessage(
        MainWindow::tr("Canvas opened: %1").arg(QFileInfo(m_window->m_filePath).fileName()), 4000);
    return true;
}

bool DocumentController::saveCanvas()
{
    if (m_window->m_filePath.isEmpty()) {
        return saveCanvasAs();
    }
    return saveToFile(m_window->m_filePath);
}

bool DocumentController::saveCanvasAs()
{
    QString suggestion = m_window->m_filePath;
    if (suggestion.isEmpty()) {
        const QString name = documentName();
        suggestion = QDir(AppSettings::canvasesDir()).filePath(name + QStringLiteral(".sbcanvas"));
    }

    QString path = QFileDialog::getSaveFileName(m_window, MainWindow::tr("Save canvas as"), suggestion,
                                                MainWindow::tr("StoryBoard canvases (*.sbcanvas)"));
    if (path.isEmpty()) {
        return false;
    }
    if (!path.endsWith(QStringLiteral(".sbcanvas"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".sbcanvas");
    }
    return saveToFile(path);
}

bool DocumentController::saveToFile(const QString& path)
{
    m_window->m_view->finishCurrentStroke();

    const QJsonDocument document(m_window->m_scene->toJson());
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
    m_window->m_settings.save();
    m_window->m_undo->setClean();
    m_window->updateTitle();
    m_window->updateActions();
    m_window->statusBar()->showMessage(
        MainWindow::tr("Canvas saved: %1").arg(QFileInfo(m_window->m_filePath).fileName()), 4000);
    return true;
}

bool DocumentController::maybeSave()
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
        return saveCanvas();
    }
    if (box.clickedButton() == discardButton) {
        return true;
    }
    return false;
}

void DocumentController::updateCanvasesMenu()
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

    QDir directory(AppSettings::canvasesDir());
    const QFileInfoList files = directory.entryInfoList({QStringLiteral("*.sbcanvas")}, QDir::Files, QDir::Time);
    if (files.isEmpty()) {
        QAction* emptyAction = new QAction(MainWindow::tr("(Canvas folder is empty)"), m_window->m_fileMenu);
        emptyAction->setEnabled(false);
        items.append(emptyAction);
    } else {
        const int maximum = qMin(15, static_cast<int>(files.size()));
        for (int i = 0; i < maximum; ++i) {
            const QFileInfo info = files.at(i);
            QAction* action = new QAction(makeIcon(IconId::Save, QColor(90, 98, 112), 16), info.completeBaseName(),
                                          m_window->m_fileMenu);
            action->setCheckable(true);
            action->setChecked(info.absoluteFilePath() == m_window->m_filePath);
            action->setToolTip(QDir::toNativeSeparators(info.absoluteFilePath()));
            const QString filePath = info.absoluteFilePath();
            QObject::connect(action, &QAction::triggered, m_window,
                             [this, filePath]() { loadCanvasFile(filePath, true); });
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
    QObject::connect(openFolderAction, &QAction::triggered, m_window, &MainWindow::openCanvasesFolder);
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

void DocumentController::openCanvasesFolder()
{
    const QString directory = AppSettings::canvasesDir();
    QDir().mkpath(directory);
    QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
}

QString DocumentController::documentName() const
{
    if (m_window->m_filePath.isEmpty()) {
        return MainWindow::tr("Untitled canvas");
    }
    const QString name = QFileInfo(m_window->m_filePath).completeBaseName();
    return name.isEmpty() ? MainWindow::tr("Untitled canvas") : name;
}

void DocumentController::updateTitle()
{
    m_window->setWindowTitle(MainWindow::tr("%1[*] - StoryBoard").arg(documentName()));
    m_window->setWindowModified(m_window->m_undo && !m_window->m_undo->isClean());
    if (!m_window->m_filePath.isEmpty()) {
        m_window->setWindowFilePath(m_window->m_filePath);
    }
}
