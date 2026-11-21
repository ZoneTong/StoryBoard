#include "AppModes.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "CanvasView.h"
#include "CommandLineOptions.h"
#include "CurveItem.h"
#include "ExportDialog.h"
#include "I18n.h"
#include "ItemPropertiesDialog.h"
#include "LineArtConverter.h"
#include "LineArtPresets.h"
#include "MainWindow.h"
#include "SelfTest.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "ShapeItem.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonDocument>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QPoint>
#include <QSaveFile>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QWidget>
#include <cstdio>

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

// 诊断：用与面板按钮完全相同的代码路径跑一次“打开 shapes 目录”，把结果
// 写到指定文件（opened / error / shellCode / 目录），用于真机排查外壳通道。
int runOpenShapesProbe(const CommandLineOptions& options)
{
    QFile output(options.openShapesProbePath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return 2;
    }
    const AppSettings::ShapesFolderOpenResult result = AppSettings::openShapesFolder();
    QTextStream stream(&output);
    stream << "opened=" << (result.opened ? 1 : 0) << "\n";
    stream << "error=" << static_cast<int>(result.error) << "\n";
    stream << "shellCode=" << result.shellCode << "\n";
    stream << "directory=" << result.directory << "\n";
    if (!result.diagnostics.isEmpty()) {
        stream << "diagnostics=" << result.diagnostics << "\n";
    }
    return 0;
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
                    originalLanguage == UiLanguage::CHINESE ? UiLanguage::ENGLISH : UiLanguage::CHINESE;
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
        setUiLanguageSetting(UiLanguage::ENGLISH);
    } else if (options.setLanguageValue == QLatin1String("zh")) {
        setUiLanguageSetting(UiLanguage::CHINESE);
    } else {
        setUiLanguageSetting(UiLanguage::SYSTEM);
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
    demoScene.setMode(ToolMode::EDIT);
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

int runLineArt(const CommandLineOptions& options)
{
    // 默认均衡档（Balanced）+ 1px 最细条带；显式给出的细项覆盖档位对应值。
    // 覆盖范围 = 档位 5 项之外的 Options 全部可调字段（除固定为 1 的线宽）。
    LineArtConverter::Options lineArtOptions;
    const LineArtPreset preset = lineArtPreset(options.lineArtPreset);
    lineArtOptions.colorLevels = preset.levels;
    lineArtOptions.maxPaletteColors = preset.palette;
    lineArtOptions.adjacentMergeDistance = preset.adjacent;
    lineArtOptions.minRegionArea = preset.minArea;
    lineArtOptions.simplifyTolerance = preset.tolerance;
    lineArtOptions.minRegionThickness = 1.0;

    if (options.lineArtMaxSideSeen) {
        lineArtOptions.maxProcessingSide = options.lineArtMaxSide;
    }
    if (options.lineArtColorLevelsSeen) {
        lineArtOptions.colorLevels = qBound(2, options.lineArtColorLevels, 64);
    }
    if (options.lineArtPaletteColorsSeen) {
        lineArtOptions.maxPaletteColors = qMax(1, options.lineArtPaletteColors);
    }
    if (options.lineArtPaletteMergeSeen) {
        lineArtOptions.paletteMergeDistance = qMax(0.0, options.lineArtPaletteMerge);
    }
    if (options.lineArtAdjacentMergeSeen) {
        lineArtOptions.adjacentMergeDistance = qMax(0.0, options.lineArtAdjacentMerge);
    }
    if (options.lineArtMinAreaSeen) {
        lineArtOptions.minRegionArea = qMax(0, options.lineArtMinArea);
    }
    if (options.lineArtMaxRegionsSeen) {
        lineArtOptions.maxRegions = qMax(0, options.lineArtMaxRegions);
    }
    if (options.lineArtAdaptiveSeen) {
        lineArtOptions.adaptiveDownscale = options.lineArtAdaptive;
    }
    if (options.lineArtAdaptiveLimitSeen) {
        lineArtOptions.adaptiveRegionLimit = qMax(1, options.lineArtAdaptiveLimit);
    }
    if (options.lineArtMinAdaptiveSideSeen) {
        lineArtOptions.minAdaptiveSide = qMax(32, options.lineArtMinAdaptiveSide);
    }
    if (options.lineArtToleranceSeen) {
        lineArtOptions.simplifyTolerance = qMax(0.0, options.lineArtTolerance);
    }
    if (options.lineArtMinThicknessSeen) {
        lineArtOptions.minRegionThickness = qMax(0.0, options.lineArtMinThickness);
    }

    if (options.lineArtInputPath.isEmpty()) {
        std::fprintf(stderr, "lineart: missing --lineart <image path>\n");
        return 2;
    }

    QImageReader reader(options.lineArtInputPath);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        std::fprintf(stderr, "lineart: could not read image: %s\n", qPrintable(options.lineArtInputPath));
        return 1;
    }

    const LineArtConverter::Result traced = LineArtConverter::extract(image, lineArtOptions);
    if (!traced.ok) {
        std::fprintf(stderr, "lineart: %s\n", qPrintable(traced.error));
        return 1;
    }
    if (traced.outlines.isEmpty()) {
        std::fprintf(stderr, "lineart: no color blocks were found in the image.\n");
        return 1;
    }

    // 画布按图片 1:1 摆放（留 20px 边距）；超过单边 12000 时整体等比缩小，
    // 保证生成画布仍可正常继续编辑（编辑操作的钳位范围是 200–12000）。
    const qreal longest = qMax<qreal>(traced.imageSize.width(), traced.imageSize.height());
    const qreal scale = longest > 11800.0 ? 11800.0 / longest : 1.0;
    const QSizeF canvasSize(traced.imageSize.width() * scale + 40.0, traced.imageSize.height() * scale + 40.0);

    CanvasScene scene;
    scene.applyCanvasSize(canvasSize);

    QList<CanvasItem*> items;
    items.reserve(traced.outlines.size());
    for (const LineArtConverter::Outline& outline : traced.outlines) {
        if (outline.points.size() < 4) {
            continue;
        }
        QVector<QPointF> scenePoints;
        scenePoints.reserve(outline.points.size());
        for (const QPointF& point : outline.points) {
            scenePoints.append(QPointF(20.0 + point.x() * scale, 20.0 + point.y() * scale));
        }
        // 与「导入图片线稿化」一致：不额外平滑，折线模式保持方角。
        auto* stroke = new CurveItem();
        stroke->setStrokePoints(scenePoints, outline.color, lineArtOptions.strokeWidth, 0.0, true);
        stroke->setSource(CurveSource::LINE_ART);
        items.append(stroke);
    }
    if (items.isEmpty()) {
        std::fprintf(stderr, "lineart: no color blocks were found in the image.\n");
        return 1;
    }
    scene.addItemsWithUndo(items, QStringLiteral("Import outline image"));
    scene.assignAutoItemNames();

    QString outputPath = options.lineArtOutputPath;
    if (outputPath.isEmpty()) {
        const QFileInfo info(options.lineArtInputPath);
        outputPath = info.absolutePath() + QLatin1Char('/') + info.completeBaseName() + QStringLiteral(".sbcanvas");
    }

    QSaveFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly)) {
        std::fprintf(stderr, "lineart: could not write the file: %s\n", qPrintable(outputPath));
        return 1;
    }
    const QJsonDocument document(scene.toJson());
    file.write(document.toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        std::fprintf(stderr, "lineart: could not write the file: %s\n", qPrintable(outputPath));
        return 1;
    }

    std::fprintf(stdout, "lineart: %d stroke(s) -> %s (canvas %.0fx%.0f, regions %d%s)\n",
                 static_cast<int>(items.size()), qPrintable(QDir::toNativeSeparators(outputPath)), canvasSize.width(),
                 canvasSize.height(), traced.regionCount, traced.truncated ? ", truncated" : "");
    return 0;
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
    if (options.openShapesProbe) {
        return runOpenShapesProbe(options);
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
    if (options.lineArt) {
        return runLineArt(options);
    }

    return runNormalLaunch(app, options);
}

} // namespace AppModes
