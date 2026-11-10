#include "I18n.h"

#include "MainWindow.h"
#include "CanvasItem.h"
#include "CanvasScene.h"
#include "CanvasView.h"
#include "ExportDialog.h"
#include "ItemPropertiesDialog.h"
#include "SelfTest.h"
#include "SettingsDialog.h"
#include "ShapeItem.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QIcon>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QTimer>

#ifndef STORYBOARD_VERSION
#error "STORYBOARD_VERSION must be defined by the build system (see CMakeLists.txt project VERSION)"
#endif

namespace {

QString takeOptionPath(const QStringList& arguments, const QString& option)
{
    const int index = arguments.indexOf(option);
    if (index < 0 || index + 1 >= arguments.size())
        return QString();
    const QString value = arguments.at(index + 1);
    if (value.startsWith(QLatin1Char('-')))
        return QString();
    return value;
}

void applyApplicationStyle(QApplication& app)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QFont font(QStringLiteral("Microsoft YaHei UI"));
    font.setPointSizeF(9.0);
    app.setFont(font);

    app.setStyleSheet(QStringLiteral(R"(
QMainWindow { background: #F4F6F9; }
QMenuBar { background: #FFFFFF; border-bottom: 1px solid #E3E7EE; padding: 2px 8px; }
QMenuBar::item { background: transparent; padding: 6px 11px; border-radius: 6px; }
QMenuBar::item:selected { background: #EDF1F8; }
QMenuBar::item:pressed { background: #E3E9F4; }
QMenu { background: #FFFFFF; border: 1px solid #E0E4EB; border-radius: 8px; padding: 6px; }
QMenu::item { padding: 7px 28px 7px 26px; border-radius: 5px; }
QMenu::item:selected { background: #EBF0FF; color: #1F2937; }
QMenu::item:disabled { color: #A8AFBB; }
QMenu::separator { height: 1px; background: #EDF0F4; margin: 5px 8px; }
QToolBar#MainToolBar { background: #FFFFFF; border: none; border-bottom: 1px solid #E3E7EE; spacing: 3px; padding: 6px 10px; }
QToolBar#BrushToolBar, QToolBar#FillToolBar { background: #FFFFFF; border: none; border-bottom: 1px solid #E3E7EE; spacing: 3px; padding: 6px 4px; }
QToolBar#ModeToolBar { background: #FFFFFF; border: none; border-right: 1px solid #E3E7EE; spacing: 8px; padding: 12px 7px; }
QToolBar::separator { background: #E4E8EF; width: 1px; margin: 5px 6px; }
QToolButton { border: 1px solid transparent; border-radius: 8px; padding: 5px; color: #2B2F36; }
QToolButton:hover { background: #F1F4F9; }
QToolButton:pressed { background: #E7ECF5; }
QToolButton:checked { background: #E9EEFF; border-color: #C9D6FF; color: #2F52C7; }
QToolButton:disabled { color: #B4BAC5; }
QToolBar#ModeToolBar QToolButton { padding: 9px 4px; min-width: 58px; }
QLabel#ToolBarLabel { color: #6B7280; padding: 0 2px 0 6px; }
QLabel#Badge { background: #F0F3F8; border-radius: 9px; padding: 1px 10px; color: #4B5563; }
QLabel#StatusHint { color: #6B7280; padding-left: 8px; }
QStatusBar { background: #FFFFFF; border-top: 1px solid #E3E7EE; min-height: 28px; }
QStatusBar QLabel { color: #5B6472; }
QStatusBar::item { border: none; }
QSpinBox, QDoubleSpinBox { background: #FFFFFF; border: 1px solid #DDE2EA; border-radius: 6px; padding: 4px 24px 4px 8px; min-height: 20px; color: #2B2F36; }
QSpinBox:focus, QDoubleSpinBox:focus { border-color: #7C9BFF; }
QSpinBox::up-button, QDoubleSpinBox::up-button { subcontrol-origin: border; subcontrol-position: top right; width: 22px; height: 15px; border: none; background: transparent; }
QSpinBox::down-button, QDoubleSpinBox::down-button { subcontrol-origin: border; subcontrol-position: bottom right; width: 22px; height: 15px; border: none; background: transparent; }
QSpinBox::up-arrow, QDoubleSpinBox::up-arrow { image: url(:/icons/chevron-up.png); width: 8px; height: 8px; }
QSpinBox::down-arrow, QDoubleSpinBox::down-arrow { image: url(:/icons/chevron-down.png); width: 8px; height: 8px; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: #C9CFDA; min-height: 30px; border-radius: 4px; }
QScrollBar::handle:vertical:hover { background: #AEB7C6; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: #C9CFDA; min-width: 30px; border-radius: 4px; }
QScrollBar::handle:horizontal:hover { background: #AEB7C6; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0px; height: 0px; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QDockWidget { color: #374151; font-weight: 600; }
QDockWidget::title { background: #FFFFFF; padding: 7px 10px; border-bottom: 1px solid #E3E7EE; }
QWidget#LayerPanel { background: #FFFFFF; }
QLabel#LayerCaption { color: #8A909C; font-size: 8.5pt; font-weight: 600; }
QLabel#LayerHint { color: #8A909C; font-size: 8pt; }
QListWidget#LayerList { background: #F8F9FC; border: 1px solid #E6E9F0; border-radius: 8px; padding: 4px; outline: none; }
QListWidget#LayerList::item { padding: 7px 8px; border-radius: 6px; margin: 2px 1px; color: #374151; }
QListWidget#LayerList::item:hover { background: #EEF2F9; }
QListWidget#LayerList::item:selected { background: #E9EEFF; color: #2F52C7; }
QListWidget#LayerList::item:disabled { color: #A8AFBB; }
QToolButton#LayerToolButton { border: 1px solid #DDE2EA; border-radius: 6px; background: #FFFFFF; color: #4B5563; font-size: 13px; font-weight: 700; padding: 0; }
QToolButton#LayerToolButton:hover { background: #F3F6FB; }
QToolButton#LayerToolButton:disabled { color: #C4CAD4; background: #F7F8FA; }
QDialog { background: #F7F8FA; }
QGroupBox { background: #FFFFFF; border: 1px solid #E3E7EE; border-radius: 10px; margin-top: 10px; padding-top: 6px; font-weight: 600; }
QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 6px; color: #374151; }
QLineEdit, QComboBox, QFontComboBox { background: #FFFFFF; border: 1px solid #DDE2EA; border-radius: 6px; padding: 5px 8px; min-height: 20px; color: #2B2F36; }
QComboBox, QFontComboBox { padding-right: 26px; }
QLineEdit:focus, QComboBox:focus, QFontComboBox:focus { border-color: #7C9BFF; }
QPlainTextEdit { background: #FFFFFF; border: 1px solid #DDE2EA; border-radius: 6px; padding: 7px 8px; color: #2B2F36; selection-background-color: #DCE6FF; }
QPlainTextEdit:focus { border-color: #7C9BFF; }
QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right; width: 24px; border: none; background: transparent; }
QComboBox::down-arrow { image: url(:/icons/chevron-down.png); width: 10px; height: 10px; }
QComboBox QAbstractItemView { border: 1px solid #E0E4EB; background: #FFFFFF; selection-background-color: #EBF0FF; selection-color: #1F2937; }
QPushButton { background: #FFFFFF; border: 1px solid #D9DFE9; border-radius: 7px; padding: 6px 14px; min-height: 20px; color: #2B2F36; }
QPushButton:hover { background: #F3F6FB; }
QPushButton:pressed { background: #E8EDF6; }
QPushButton:default { background: #3E6BFF; border-color: #3E6BFF; color: #FFFFFF; }
QPushButton:default:hover { background: #4E78FF; }
QDialogButtonBox QPushButton { min-width: 82px; }
QLabel#DialogTitle { font-size: 17px; font-weight: 700; color: #1F2937; }
QLabel#DialogHint { color: #6B7280; }
QCheckBox { spacing: 7px; color: #374151; }
QToolTip { background: #1F2937; color: #F9FAFB; border: none; padding: 5px 8px; border-radius: 4px; }
)"));
}

} // namespace

int main(int argc, char** argv)
{
    QStringList arguments;
    arguments.reserve(argc);
    for (int i = 0; i < argc; ++i)
        arguments.append(QString::fromLocal8Bit(argv[i]));

    const bool selfTest = arguments.contains(QStringLiteral("--self-test"));
    const bool renderDemo = arguments.contains(QStringLiteral("--render-demo"));
    if (selfTest && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    if (selfTest && qEnvironmentVariableIsEmpty("STORYBOARD_CONFIG")) {
        // 自检会反复写设置，把配置指到自检工作目录，绝不碰用户自己的 StoryBoard.ini。
        const QString reportPath = takeOptionPath(arguments, QStringLiteral("--self-test"));
        const QString reportFile = reportPath.isEmpty()
            ? QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                  .filePath(QStringLiteral("storyboard_selftest.txt"))
            : reportPath;
        const QString workDirectory = QDir(QFileInfo(reportFile).absolutePath())
                                          .filePath(QStringLiteral("storyboard-selftest-work"));
        QDir().mkpath(workDirectory);
        qputenv("STORYBOARD_CONFIG",
                QDir(workDirectory).filePath(QStringLiteral("StoryBoard.ini")).toUtf8());
    }

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("ZoneTong"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("zonetong.github.io"));
    QCoreApplication::setApplicationName(QStringLiteral("StoryBoard"));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(STORYBOARD_VERSION));
    uiInitializeLanguage();
    app.setApplicationDisplayName(uiText(QStringLiteral("StoryBoard Canvas")));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));
    applyApplicationStyle(app);

    if (selfTest) {
        const QString reportPath = takeOptionPath(arguments, QStringLiteral("--self-test"));
        return runSelfTest(reportPath);
    }

    if (renderDemo) {
        const QString outputPath = takeOptionPath(arguments, QStringLiteral("--render-demo"));
        if (outputPath.isEmpty())
            return 2;
        return renderDemoCanvas(outputPath) ? 0 : 1;
    }

    if (arguments.contains(QStringLiteral("--demo-canvas"))) {
        const QString outputPath = takeOptionPath(arguments, QStringLiteral("--demo-canvas"));
        if (outputPath.isEmpty())
            return 2;
        return writeDemoCanvas(outputPath) ? 0 : 1;
    }

    if (arguments.contains(QStringLiteral("--ui-screenshot"))) {
        const QString outputPath = takeOptionPath(arguments, QStringLiteral("--ui-screenshot"));
        if (outputPath.isEmpty())
            return 2;

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
        return app.exec();
    }

    if (arguments.contains(QStringLiteral("--settings-screenshot"))) {
        const QString outputPath = takeOptionPath(arguments, QStringLiteral("--settings-screenshot"));
        if (outputPath.isEmpty())
            return 2;
        AppSettings settings;
        settings.load();
        SettingsDialog dialog(settings);
        dialog.show();
        QApplication::processEvents();
        return dialog.grab().save(outputPath) ? 0 : 1;
    }

    if (arguments.contains(QStringLiteral("--settings-apply-probe"))) {
        MainWindow window;
        window.resize(1000, 700);
        window.show();
        QApplication::processEvents();
        QAction* settingsAction = nullptr;
        for (QAction* action : window.findChildren<QAction*>()) {
            if (action->text() == uiText(QStringLiteral("Settings…"))) {
                settingsAction = action;
                break;
            }
        }
        if (!settingsAction)
            return 3;
        const UiLanguage originalLanguage = uiLanguageSetting();
        QTimer::singleShot(200, &window, [&window, originalLanguage]() {
            if (auto* dialog = window.findChild<SettingsDialog*>()) {
                auto* languageCombo = dialog->findChild<QComboBox*>();
                if (languageCombo) {
                    const UiLanguage target = originalLanguage == UiLanguage::Chinese
                        ? UiLanguage::English : UiLanguage::Chinese;
                    languageCombo->setCurrentIndex(languageCombo->findData(static_cast<int>(target)));
                }
                dialog->accept();
            }
        });
        settingsAction->trigger();
        setUiLanguageSetting(originalLanguage);
        return 0;
    }

    if (arguments.contains(QStringLiteral("--set-language"))) {
        const QString value = takeOptionPath(arguments, QStringLiteral("--set-language"));
        if (value == QLatin1String("en"))
            setUiLanguageSetting(UiLanguage::English);
        else if (value == QLatin1String("zh"))
            setUiLanguageSetting(UiLanguage::Chinese);
        else
            setUiLanguageSetting(UiLanguage::System);
        return 0;
    }

    if (arguments.contains(QStringLiteral("--settings-probe"))) {
        const QString outputPath = takeOptionPath(arguments, QStringLiteral("--settings-probe"));
        if (outputPath.isEmpty())
            return 2;
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
        if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return 1;
        output.write(text.toUtf8());
        return 0;
    }

    if (arguments.contains(QStringLiteral("--about-screenshot"))) {
        const QString outputPath = takeOptionPath(arguments, QStringLiteral("--about-screenshot"));
        if (outputPath.isEmpty())
            return 2;
        MainWindow window;
        bool saved = false;
        QTimer::singleShot(80, [&saved, &outputPath]() {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (!box)
                return;
            saved = box->grab().save(outputPath);
            box->accept();
        });
        QMetaObject::invokeMethod(&window, "showAbout", Qt::DirectConnection);
        return saved ? 0 : 1;
    }

    if (arguments.contains(QStringLiteral("--item-properties-screenshot"))) {
        const QString outputPath = takeOptionPath(arguments, QStringLiteral("--item-properties-screenshot"));
        if (outputPath.isEmpty())
            return 2;

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
        if (!target && demoScene.canvasItemCount() > 0)
            target = demoScene.canvasItems().constFirst();
        if (!target)
            return 3;
        demoScene.setActiveLayer(target->layerId());
        target->setSelected(true);

        ItemPropertiesDialog dialog(&demoScene, target);
        dialog.show();
        QApplication::processEvents();
        return dialog.grab().save(outputPath) ? 0 : 1;
    }

    if (arguments.contains(QStringLiteral("--shape-menu-screenshot"))) {
        const QString outputPath = takeOptionPath(arguments, QStringLiteral("--shape-menu-screenshot"));
        if (outputPath.isEmpty())
            return 2;

        MainWindow window;
        window.resize(1200, 800);
        window.show();
        QApplication::processEvents();

        QMenu* menu = window.findChild<QMenu*>(QStringLiteral("ShapeMenu"));
        if (!menu)
            return 3;
        menu->popup(window.mapToGlobal(QPoint(420, 80)));
        QApplication::processEvents();
        const QPixmap shot = menu->grab();
        menu->hide();
        return shot.save(outputPath) ? 0 : 1;
    }

    if (arguments.contains(QStringLiteral("--brush-style-menu-screenshot"))) {
        const QString outputPath = takeOptionPath(arguments, QStringLiteral("--brush-style-menu-screenshot"));
        if (outputPath.isEmpty())
            return 2;

        MainWindow window;
        window.resize(1200, 800);
        window.show();
        QApplication::processEvents();

        QMenu* menu = window.findChild<QMenu*>(QStringLiteral("BrushStyleMenu"));
        if (!menu)
            return 3;
        menu->popup(window.mapToGlobal(QPoint(500, 40)));
        QApplication::processEvents();
        const QPixmap shot = menu->grab();
        menu->hide();
        return shot.save(outputPath) ? 0 : 1;
    }

    if (arguments.contains(QStringLiteral("--brush-width-menu-screenshot"))) {
        const QString outputPath = takeOptionPath(arguments, QStringLiteral("--brush-width-menu-screenshot"));
        if (outputPath.isEmpty())
            return 2;

        MainWindow window;
        window.resize(1200, 800);
        window.show();
        QApplication::processEvents();

        QMenu* menu = window.findChild<QMenu*>(QStringLiteral("BrushWidthMenu"));
        if (!menu)
            return 3;
        menu->popup(window.mapToGlobal(QPoint(600, 40)));
        QApplication::processEvents();
        const QPixmap shot = menu->grab();
        menu->hide();
        return shot.save(outputPath) ? 0 : 1;
    }

    if (arguments.contains(QStringLiteral("--fill-style-menu-screenshot"))) {
        const QString outputPath = takeOptionPath(arguments, QStringLiteral("--fill-style-menu-screenshot"));
        if (outputPath.isEmpty())
            return 2;

        MainWindow window;
        window.resize(1200, 800);
        window.show();
        QApplication::processEvents();

        QMenu* menu = window.findChild<QMenu*>(QStringLiteral("FillStyleMenu"));
        if (!menu)
            return 3;
        menu->popup(window.mapToGlobal(QPoint(600, 40)));
        QApplication::processEvents();
        const QPixmap shot = menu->grab();
        menu->hide();
        return shot.save(outputPath) ? 0 : 1;
    }

    if (arguments.contains(QStringLiteral("--style-menu-screenshot"))) {
        const QString outputPath = takeOptionPath(arguments, QStringLiteral("--style-menu-screenshot"));
        if (outputPath.isEmpty())
            return 2;

        MainWindow window;
        window.resize(1200, 800);
        window.show();
        QApplication::processEvents();

        QMenu* menu = window.findChild<QMenu*>(QStringLiteral("StyleMenu"));
        if (!menu)
            return 3;
        menu->popup(window.mapToGlobal(QPoint(500, 40)));
        QApplication::processEvents();
        const QPixmap shot = menu->grab();
        menu->hide();
        return shot.save(outputPath) ? 0 : 1;
    }

    if (arguments.contains(QStringLiteral("--export-screenshot"))) {
        const QString outputPath = takeOptionPath(arguments, QStringLiteral("--export-screenshot"));
        if (outputPath.isEmpty())
            return 2;
        CanvasScene demoScene;
        populateDemoScene(demoScene);
        ExportDialog dialog(&demoScene,
                            QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
        dialog.show();
        QApplication::processEvents();
        return dialog.grab().save(outputPath) ? 0 : 1;
    }

    MainWindow window;
    window.show();

    const QStringList positional = arguments.mid(1);
    QString canvasPath;
    for (const QString& argument : positional) {
        if (argument.startsWith(QLatin1Char('-')))
            continue;
        const QString suffix = QFileInfo(argument).suffix().toLower();
        if (QFileInfo::exists(argument)
            && suffix == QStringLiteral("sbcanvas")) {
            canvasPath = argument;
            break;
        }
    }
    if (!canvasPath.isEmpty()) {
        QTimer::singleShot(0, &window, [&window, canvasPath]() {
            window.openCanvasFile(canvasPath);
        });
    }

    return app.exec();
}
