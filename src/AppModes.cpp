#include "AppModes.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "CanvasView.h"
#include "CommandLineOptions.h"
#include "ExportDialog.h"
#include "I18n.h"
#include "ItemPropertiesDialog.h"
#include "MainWindow.h"
#include "SelfTest.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "ShapeItem.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QPoint>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTimer>
#include <QWidget>

namespace {

int runSelfTest(QApplication& app, const CommandLineOptions& options)
{
    // 自检跑在离屏平台上，任何模态弹窗都会永久阻塞进程；打上标记后
    // MainWindow 的报错路径改为只写状态栏 / stderr。
    app.setProperty("storyboardSelfTest", true);
    return runSelfTest(options.selfTestPath);
}

int runRenderDemo(const CommandLineOptions& options)
{
    if (options.renderDemoPath.isEmpty()) {
        return 2;
    }
    return renderDemoCanvas(options.renderDemoPath) ? 0 : 1;
}

int runDemoCanvas(const CommandLineOptions& options)
{
    if (options.demoCanvasPath.isEmpty()) {
        return 2;
    }
    return writeDemoCanvas(options.demoCanvasPath) ? 0 : 1;
}

int runUiScreenshot(const CommandLineOptions& options)
{
    const QString& outputPath = options.uiScreenshotPath;

    MainWindow window;
    window.resize(1440, 900);
    window.show();
    QApplication::processEvents();

    CanvasScene demoScene;
    populateDemoScene(demoScene);
    window.scene()->fromJson(demoScene.toJson());
    window.view()->fitAll();
    QApplication::processEvents();

    QTimer::singleShot(350, &window, [&window, outputPath]() {
        const bool saved = window.grab().save(outputPath);
        QCoreApplication::exit(saved ? 0 : 1);
    });
    return QApplication::exec();
}

int runSettingsScreenshot(const CommandLineOptions& options)
{
    const QString& outputPath = options.settingsScreenshotPath;
    AppSettings settings;
    settings.load();
    SettingsDialog dialog(settings);
    dialog.show();
    QApplication::processEvents();
    // 截图要完整一张：把窗口撑到内容的真实高度，不受离屏平台小屏幕的夹取影响。
    if (QScrollArea* scroll = dialog.findChild<QScrollArea*>(QStringLiteral("SettingsScroll"))) {
        if (QWidget* content = scroll->widget()) {
            const int chrome = dialog.height() - scroll->height();
            int contentHeight = content->sizeHint().height();
            if (content->hasHeightForWidth()) {
                contentHeight = qMax(contentHeight, content->heightForWidth(content->width()));
            }
            dialog.resize(dialog.width(), qMax(dialog.height(), contentHeight + chrome + 12));
            QApplication::processEvents();
        }
    }
    return dialog.grab().save(outputPath) ? 0 : 1;
}

int runSettingsApplyProbe()
{
    MainWindow window;
    window.resize(1000, 700);
    window.show();
    QApplication::processEvents();
    QAction* settingsAction = nullptr;
    for (QAction* action : window.findChildren<QAction*>()) {
        if (action->text() == QCoreApplication::tr("Settings…")) {
            settingsAction = action;
            break;
        }
    }
    if (!settingsAction) {
        return 3;
    }
    const UiLanguage originalLanguage = uiLanguageSetting();
    QTimer::singleShot(200, &window, [&window, originalLanguage]() {
        if (auto* dialog = window.findChild<SettingsDialog*>()) {
            auto* languageCombo = dialog->findChild<QComboBox*>();
            if (languageCombo) {
                const UiLanguage target =
                    originalLanguage == UiLanguage::Chinese ? UiLanguage::English : UiLanguage::Chinese;
                languageCombo->setCurrentIndex(languageCombo->findData(static_cast<int>(target)));
            }
            dialog->accept();
        }
    });
    settingsAction->trigger();
    setUiLanguageSetting(originalLanguage);
    return 0;
}

int runSetLanguage(const CommandLineOptions& options)
{
    if (options.setLanguageValue == QLatin1String("en")) {
        setUiLanguageSetting(UiLanguage::English);
    } else if (options.setLanguageValue == QLatin1String("zh")) {
        setUiLanguageSetting(UiLanguage::Chinese);
    } else {
        setUiLanguageSetting(UiLanguage::System);
    }
    return 0;
}

int runSettingsProbe(const CommandLineOptions& options)
{
    const QString& outputPath = options.settingsProbePath;
    AppSettings probe;
    probe.load();
    const QString file = AppSettings::settingsFilePath();
    const QString text = QStringLiteral("file=%1\nexists=%2\nsetting=%3\neffective=%4\nstored=%5\n")
                             .arg(file)
                             .arg(QFileInfo::exists(file))
                             .arg(static_cast<int>(uiLanguageSetting()))
                             .arg(static_cast<int>(uiEffectiveLanguage()))
                             .arg(AppSettings::languageSetting());
    QFile output(outputPath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return 1;
    }
    output.write(text.toUtf8());
    return 0;
}

int runAboutScreenshot(const CommandLineOptions& options)
{
    const QString& outputPath = options.aboutScreenshotPath;
    MainWindow window;
    bool saved = false;
    QTimer::singleShot(80, [&saved, &outputPath]() {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (!box) {
            return;
        }
        saved = box->grab().save(outputPath);
        box->accept();
    });
    QMetaObject::invokeMethod(&window, "showAbout", Qt::DirectConnection);
    return saved ? 0 : 1;
}

int runItemPropertiesScreenshot(const CommandLineOptions& options)
{
    const QString& outputPath = options.itemPropertiesScreenshotPath;

    CanvasScene demoScene;
    populateDemoScene(demoScene);
    demoScene.assignAutoItemNames();
    demoScene.setMode(ToolMode::Edit);
    CanvasItem* target = nullptr;
    for (CanvasItem* item : demoScene.canvasItems()) {
        if (dynamic_cast<ShapeItem*>(item)) {
            target = item;
            break;
        }
    }
    if (!target && demoScene.canvasItemCount() > 0) {
        target = demoScene.canvasItems().constFirst();
    }
    if (!target) {
        return 3;
    }
    demoScene.setActiveLayer(target->layerId());
    target->setSelected(true);

    ItemPropertiesDialog dialog(&demoScene, target);
    dialog.show();
    QApplication::processEvents();
    return dialog.grab().save(outputPath) ? 0 : 1;
}

int grabMenu(const QString& menuName, const QPoint& popupPoint, const QString& outputPath)
{
    MainWindow window;
    window.resize(1200, 800);
    window.show();
    QApplication::processEvents();

    QMenu* menu = window.findChild<QMenu*>(menuName);
    if (!menu) {
        return 3;
    }
    menu->popup(window.mapToGlobal(popupPoint));
    QApplication::processEvents();
    const QPixmap shot = menu->grab();
    menu->hide();
    return shot.save(outputPath) ? 0 : 1;
}

int runExportScreenshot(const CommandLineOptions& options)
{
    const QString& outputPath = options.exportScreenshotPath;

    CanvasScene demoScene;
    populateDemoScene(demoScene);
    ExportDialog dialog(&demoScene, QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
    dialog.show();
    QApplication::processEvents();
    return dialog.grab().save(outputPath) ? 0 : 1;
}

int runNormalLaunch(QApplication& app, const CommandLineOptions& options)
{
    MainWindow window;
    window.show();

    if (!options.canvasPath.isEmpty()) {
        const QString canvasPath = options.canvasPath;
        QTimer::singleShot(0, &window, [&window, canvasPath]() { window.openCanvasFile(canvasPath); });
    }

    return app.exec();
}

} // namespace

namespace AppModes {

int run(QApplication& app, const CommandLineOptions& options)
{
    if (options.selfTest) {
        return runSelfTest(app, options);
    }
    if (options.renderDemo) {
        return runRenderDemo(options);
    }
    if (options.demoCanvas) {
        return runDemoCanvas(options);
    }
    if (options.uiScreenshot) {
        return runUiScreenshot(options);
    }
    if (options.settingsScreenshot) {
        return runSettingsScreenshot(options);
    }
    if (options.settingsApplyProbe) {
        return runSettingsApplyProbe();
    }
    if (options.setLanguage) {
        return runSetLanguage(options);
    }
    if (options.settingsProbe) {
        return runSettingsProbe(options);
    }
    if (options.aboutScreenshot) {
        return runAboutScreenshot(options);
    }
    if (options.itemPropertiesScreenshot) {
        return runItemPropertiesScreenshot(options);
    }
    if (options.shapeMenuScreenshot) {
        return grabMenu(QStringLiteral("ShapeMenu"), QPoint(420, 80), options.shapeMenuScreenshotPath);
    }
    if (options.brushStyleMenuScreenshot) {
        return grabMenu(QStringLiteral("BrushStyleMenu"), QPoint(500, 40), options.brushStyleMenuScreenshotPath);
    }
    if (options.brushWidthMenuScreenshot) {
        return grabMenu(QStringLiteral("InsertMenu"), QPoint(420, 40), options.brushWidthMenuScreenshotPath);
    }
    if (options.fillStyleMenuScreenshot) {
        return grabMenu(QStringLiteral("FillStyleMenu"), QPoint(600, 40), options.fillStyleMenuScreenshotPath);
    }
    if (options.styleMenuScreenshot) {
        return grabMenu(QStringLiteral("StyleMenu"), QPoint(500, 40), options.styleMenuScreenshotPath);
    }
    if (options.exportScreenshot) {
        return runExportScreenshot(options);
    }

    return runNormalLaunch(app, options);
}

} // namespace AppModes
