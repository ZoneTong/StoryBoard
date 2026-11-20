#include "CanvasBackgroundController.h"

#include "CanvasScene.h"
#include "MainWindow.h"

#include <QAction>
#include <QColorDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QMessageBox>
#include <QPixmap>
#include <QStandardPaths>
#include <QStatusBar>

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

QString imageFilter()
{
    return MainWindow::tr("Image files (*.png *.jpg *.jpeg *.bmp *.gif);;All files (*.*)");
}

} // namespace

CanvasBackgroundController::CanvasBackgroundController(MainWindow* window) : m_window(window) {}

void CanvasBackgroundController::replaceCanvasBackgroundImage()
{
    QString startDirectory = m_window->m_settings.lastImageDir;
    if (startDirectory.isEmpty()) {
        startDirectory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }

    const QString path = QFileDialog::getOpenFileName(m_window, MainWindow::tr("Replace canvas background image"),
                                                      startDirectory, imageFilter());
    if (path.isEmpty()) {
        return;
    }

    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        reportWarning(m_window, MainWindow::tr("Replace canvas background image"),
                      MainWindow::tr("Could not read the image:\n%1").arg(reader.errorString()));
        return;
    }

    CanvasBackground background;
    background.type = BackgroundType::Image;
    background.image = QPixmap::fromImage(image);
    background.mode = m_window->m_scene->canvasBackground().type == BackgroundType::Image
                          ? m_window->m_scene->canvasBackground().mode
                          : BackgroundMode::Stretch;
    m_window->m_scene->replaceCanvasBackground(background, MainWindow::tr("Replace canvas background image"));

    m_window->m_settings.lastImageDir = QFileInfo(path).absolutePath();
    m_window->m_settings.save();
    m_window->statusBar()->showMessage(MainWindow::tr("Background replaced"), 3000);
}

void CanvasBackgroundController::setBackgroundColor()
{
    QColor initial(255, 255, 255);
    if (m_window->m_scene->canvasBackground().type == BackgroundType::Color) {
        initial = m_window->m_scene->canvasBackground().color;
    }

    const QColor color = QColorDialog::getColor(initial, m_window, MainWindow::tr("Choose background color"));
    if (!color.isValid()) {
        return;
    }

    CanvasBackground background;
    background.type = BackgroundType::Color;
    background.color = color;
    m_window->m_scene->replaceCanvasBackground(background, MainWindow::tr("Set background color"));
    m_window->statusBar()->showMessage(MainWindow::tr("Background color updated"), 3000);
}

void CanvasBackgroundController::clearBackground()
{
    m_window->m_scene->replaceCanvasBackground(CanvasBackground(), MainWindow::tr("Clear canvas background"));
    m_window->statusBar()->showMessage(MainWindow::tr("Background cleared"), 3000);
}

void CanvasBackgroundController::updateModeActions()
{
    if (!m_window->m_scene || !m_window->m_bgStretchAct) {
        return;
    }

    const CanvasBackground background = m_window->m_scene->canvasBackground();
    const bool hasImage = background.type == BackgroundType::Image;
    m_window->m_bgStretchAct->setEnabled(hasImage);
    m_window->m_bgFitAct->setEnabled(hasImage);
    m_window->m_bgTileAct->setEnabled(hasImage);
    m_window->m_bgStretchAct->setChecked(hasImage && background.mode == BackgroundMode::Stretch);
    m_window->m_bgFitAct->setChecked(hasImage && background.mode == BackgroundMode::Fit);
    m_window->m_bgTileAct->setChecked(hasImage && background.mode == BackgroundMode::Tile);
    m_window->m_clearBgAct->setEnabled(background.type != BackgroundType::None);
}
