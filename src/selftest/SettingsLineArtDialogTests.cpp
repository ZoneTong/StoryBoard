#include "Alignment.h"
#include "BoolOps.h"
#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CanvasView.h"
#include "Commands.h"
#include "CompositeShapeItem.h"
#include "CustomShapeTemplates.h"
#include "ExportDialog.h"
#include "I18n.h"
#include "ImageItem.h"
#include "ItemDockPanel.h"
#include "ItemPropertiesDialog.h"
#include "LayerPanel.h"
#include "LineArtConverter.h"
#include "LineArtDialog.h"
#include "LineArtPresets.h"
#include "MainWindow.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "ShapeItem.h"
#include "StrokeItem.h"
#include "TextItem.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"
#include "selftest/SelftestHelpers.h"

#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>
#include <QImage>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLineF>
#include <QLinearGradient>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMarginsF>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPolygonF>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSize>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>
#include <QWheelEvent>
#include <QWidget>
#include <QWidgetAction>
#include <QtMath>
#include <cstdlib>


void selftest::runSettingsLineArtDialogTests(Context& ctx)
{
    {
        CanvasScene handleScene;
        handleScene.setMode(ToolMode::Edit);
        handleScene.setShowGrid(false);
        QUndoStack handleUndo;
        handleScene.setUndoStack(&handleUndo);

        auto* handleEllipse = new ShapeItem(ShapeType::Ellipse);
        handleEllipse->setStyle(QColor(QStringLiteral("#0F172A")), QColor(Qt::transparent), 4.0);
        handleEllipse->setContentSize(QSizeF(200, 120));
        handleEllipse->setPos(QPointF(60, 40));
        handleScene.attachItem(handleEllipse);
        handleEllipse->setSelected(true);

        ctx.check(handleEllipse->shape().contains(QPointF(-3.0, -3.0)), QStringLiteral("选中椭圆后左上角控制点可命中"));
        ctx.check(handleEllipse->shape().contains(QPointF(203.0, 123.0)),
                  QStringLiteral("选中椭圆后右下角控制点可命中"));
        ctx.check(!handleEllipse->shape().contains(QPointF(-40.0, -40.0)),
                  QStringLiteral("控制点命中区域不会无限外扩"));

        handleEllipse->setSelected(false);
        ctx.check(!handleEllipse->shape().contains(QPointF(-3.0, -3.0)), QStringLiteral("未选中时控制点不参与命中"));

        auto* thickRect = new ShapeItem(ShapeType::Rectangle);
        thickRect->setStyle(QColor(QStringLiteral("#1D4ED8")), QColor(29, 78, 216, 40), 40.0);
        thickRect->setContentSize(QSizeF(200, 120));
        thickRect->setPos(QPointF(400, 40));
        handleScene.attachItem(thickRect);

        ctx.check(thickRect->shape().contains(QPointF(-10.0, 60.0)), QStringLiteral("粗描边图形按描边宽度纳入命中"));
        ctx.check(!thickRect->shape().contains(QPointF(-40.0, 60.0)), QStringLiteral("粗描边命中范围不超过描边宽度"));
    }

    {
        QPixmap firstBackground(40, 30);
        firstBackground.fill(QColor(QStringLiteral("#0EA5E9")));
        QPixmap secondBackground(40, 30);
        secondBackground.fill(QColor(QStringLiteral("#0EA5E9")));

        BackgroundState firstState;
        firstState.type = BackgroundType::Image;
        firstState.image = firstBackground;
        BackgroundState secondState = firstState;
        secondState.image = secondBackground;

        ctx.check(firstState == secondState, QStringLiteral("内容相同的背景图片判定为相等"));

        CanvasScene dedupeScene;
        QUndoStack dedupeUndo;
        dedupeScene.setUndoStack(&dedupeUndo);
        dedupeScene.replaceBackground(firstState);
        const int stepsAfterFirst = dedupeUndo.count();
        dedupeScene.replaceBackground(secondState);
        ctx.check(dedupeUndo.count() == stepsAfterFirst, QStringLiteral("重复应用同一背景图片不增加撤销步骤"),
                  QString::number(dedupeUndo.count()));
    }

    {
        const AppSettings defaults;
        SettingsDialog dialog(defaults);
        dialog.setBrushColor(QColor(QStringLiteral("#FF8800")));
        dialog.setCanvasColor(QColor(QStringLiteral("#102A43")));
        auto* smoothingSpin = dialog.findChild<QSpinBox*>(QStringLiteral("StrokeSmoothingSpin"));
        ctx.check(smoothingSpin && smoothingSpin->value() == defaults.strokeSmoothing,
                  QStringLiteral("设置对话框默认启用画笔自动平滑"),
                  QString::number(smoothingSpin ? smoothingSpin->value() : -1));
        if (smoothingSpin) {
            smoothingSpin->setValue(0);
        }
        // 走真实的“确定”按钮，验证 accepted 处理器把平滑参数写回设置。
        if (auto* buttons = dialog.findChild<QDialogButtonBox*>()) {
            if (QPushButton* ok = buttons->button(QDialogButtonBox::Ok)) {
                ok->click();
            } else {
                dialog.accept();
            }
        } else {
            dialog.accept();
        }
        ctx.check(dialog.settings().brushColor == QColor(QStringLiteral("#FF8800")),
                  QStringLiteral("设置对话框保留画笔颜色"));
        ctx.check(dialog.settings().canvasColor == QColor(QStringLiteral("#102A43")),
                  QStringLiteral("设置对话框保留画布外围颜色"));
        ctx.check(dialog.settings().strokeSmoothing == 0, QStringLiteral("设置对话框可关闭自动平滑"),
                  QString::number(dialog.settings().strokeSmoothing));
    }

    {
        // 需求：把“相邻且颜色接近的色块并成一块”做成可调参数，并放在设置里。
        const AppSettings defaults;
        ctx.check(defaults.lineArtAdjacentMerge == qRound(LineArtConverter::Options().adjacentMergeDistance) &&
                      defaults.lineArtPaletteMerge == qRound(LineArtConverter::Options().paletteMergeDistance),
                  QStringLiteral("线稿化设置的默认值与转换器默认参数一致"),
                  QStringLiteral("adjacent=%1 palette=%2")
                      .arg(defaults.lineArtAdjacentMerge)
                      .arg(defaults.lineArtPaletteMerge));

        SettingsDialog dialog(defaults);
        auto* adjacentSpin = dialog.findChild<QSpinBox*>(QStringLiteral("LineArtAdjacentMergeSpin"));
        auto* paletteSpin = dialog.findChild<QSpinBox*>(QStringLiteral("LineArtPaletteMergeSpin"));
        auto* areaSpin = dialog.findChild<QSpinBox*>(QStringLiteral("LineArtMinAreaSpin"));
        if (adjacentSpin) {
            adjacentSpin->setValue(96);
        }
        if (paletteSpin) {
            paletteSpin->setValue(24);
        }
        if (areaSpin) {
            areaSpin->setValue(8);
        }
        if (auto* buttons = dialog.findChild<QDialogButtonBox*>()) {
            if (QPushButton* ok = buttons->button(QDialogButtonBox::Ok)) {
                ok->click();
            } else {
                dialog.accept();
            }
        } else {
            dialog.accept();
        }
        ctx.check(dialog.settings().lineArtAdjacentMerge == 96, QStringLiteral("设置对话框可调整相邻色块合并距离"),
                  QString::number(dialog.settings().lineArtAdjacentMerge));
        ctx.check(dialog.settings().lineArtPaletteMerge == 24 && dialog.settings().lineArtMinRegionArea == 8,
                  QStringLiteral("设置对话框可调整颜色聚类距离与最小色块"),
                  QStringLiteral("palette=%1 area=%2")
                      .arg(dialog.settings().lineArtPaletteMerge)
                      .arg(dialog.settings().lineArtMinRegionArea));
    }

    {
        // 「恢复初始值」：改完数值点一下，应该回到配置文件里当前保存的那一套。
        AppSettings fileSettings;
        fileSettings.load();
        const int fileLevels = fileSettings.lineArtColorLevels;
        LineArtDialog dialog(fileSettings);
        auto* levels = dialog.findChild<QSpinBox*>(QStringLiteral("LineArtLevelsSpin"));
        auto* restore = dialog.findChild<QPushButton*>(QStringLiteral("LineArtRestoreButton"));
        ctx.check(restore != nullptr, QStringLiteral("线稿化参数窗提供「恢复初始值」按钮"));
        if (levels) {
            levels->setValue(fileLevels == 2 ? 3 : 2);
        }
        const int editedLevels = levels ? levels->value() : -1;
        if (restore) {
            restore->click();
        }
        ctx.check(levels && levels->value() == fileLevels, QStringLiteral("恢复初始值后回到配置文件里的参数"),
                  QStringLiteral("edited=%1 restored=%2 file=%3")
                      .arg(editedLevels)
                      .arg(levels ? levels->value() : -1)
                      .arg(fileLevels));
    }

    {
        // 需求：选完图片先弹线稿化参数，点确定后才生成笔画。
        const AppSettings defaults;
        LineArtDialog dialog(defaults);
        auto* presetCombo = dialog.findChild<QComboBox*>(QStringLiteral("LineArtPresetCombo"));
        auto* levelsSpin = dialog.findChild<QSpinBox*>(QStringLiteral("LineArtLevelsSpin"));
        auto* paletteSpin = dialog.findChild<QSpinBox*>(QStringLiteral("LineArtPaletteMergeSpin"));
        auto* adjacentSpin = dialog.findChild<QSpinBox*>(QStringLiteral("LineArtAdjacentMergeSpin"));
        auto* areaSpin = dialog.findChild<QSpinBox*>(QStringLiteral("LineArtMinAreaSpin"));

        const LineArtConverter::Options initial = dialog.options();
        ctx.check(initial.colorLevels == defaults.lineArtColorLevels &&
                      qRound(initial.paletteMergeDistance) == defaults.lineArtPaletteMerge &&
                      qRound(initial.adjacentMergeDistance) == defaults.lineArtAdjacentMerge &&
                      initial.minRegionArea == defaults.lineArtMinRegionArea && qFuzzyCompare(initial.strokeWidth, 1.0),
                  QStringLiteral("参数对话框初始值来自设置且线宽固定为 1"),
                  QStringLiteral("levels=%1 palette=%2 adjacent=%3 area=%4 width=%5")
                      .arg(initial.colorLevels)
                      .arg(initial.paletteMergeDistance)
                      .arg(initial.adjacentMergeDistance)
                      .arg(initial.minRegionArea)
                      .arg(initial.strokeWidth));

        if (presetCombo && levelsSpin && paletteSpin && adjacentSpin && areaSpin) {
            presetCombo->setCurrentIndex(presetCombo->findData(2)); // 线条干净
            const LineArtConverter::Options clean = dialog.options();
            ctx.check(clean.paletteMergeDistance == 64.0 && clean.adjacentMergeDistance == 56.0 &&
                          clean.minRegionArea == 20,
                      QStringLiteral("参数对话框切档位后输出参数跟着变"),
                      QStringLiteral("palette=%1 adjacent=%2 area=%3")
                          .arg(clean.paletteMergeDistance)
                          .arg(clean.adjacentMergeDistance)
                          .arg(clean.minRegionArea));

            areaSpin->setValue(77);
            ctx.check(presetCombo->currentText() == QStringLiteral("自定义"),
                      QStringLiteral("参数对话框手改数值后档位回到自定义"), presetCombo->currentText());
            ctx.check(dialog.options().minRegionArea == 77 && dialog.options().paletteMergeDistance == 64.0,
                      QStringLiteral("参数对话框手改数值即时反映到输出参数"),
                      QStringLiteral("area=%1 palette=%2")
                          .arg(dialog.options().minRegionArea)
                          .arg(dialog.options().paletteMergeDistance));

            AppSettings written = defaults;
            dialog.applyToSettings(written);
            ctx.check(written.lineArtMinRegionArea == 77 && written.lineArtPaletteMerge == 64,
                      QStringLiteral("参数对话框确定后把参数写回设置"),
                      QStringLiteral("palette=%1 area=%2")
                          .arg(written.lineArtPaletteMerge)
                          .arg(written.lineArtMinRegionArea));
        }
    }
    {
        // 需求：设置项变多后页面不能再溢出屏幕，整页放进滚动区，线稿化说明可折叠。
        const AppSettings defaults;
        SettingsDialog dialog(defaults);
        QScrollArea* scroll = dialog.findChild<QScrollArea*>(QStringLiteral("SettingsScroll"));
        ctx.check(scroll && scroll->widgetResizable() && scroll->widget(),
                  QStringLiteral("设置对话框内容放在可滚动区域"), QStringLiteral("scroll=%1").arg(scroll ? 1 : 0));
        QToolButton* hintToggle = dialog.findChild<QToolButton*>(QStringLiteral("LineArtHintToggle"));
        QLabel* lineArtHint = nullptr;
        for (QLabel* label : dialog.findChildren<QLabel*>()) {
            if (label->text().startsWith(QStringLiteral("<ul>"))) {
                lineArtHint = label;
                break;
            }
        }
        ctx.check(hintToggle && lineArtHint && hintToggle->isChecked() && lineArtHint->isVisibleTo(&dialog),
                  QStringLiteral("线稿化用法提示默认展开并提供折叠开关"),
                  QStringLiteral("toggle=%1 hint=%2").arg(hintToggle ? 1 : 0).arg(lineArtHint ? 1 : 0));
        if (hintToggle && lineArtHint) {
            hintToggle->setChecked(false);
            ctx.check(!lineArtHint->isVisibleTo(&dialog), QStringLiteral("折叠后隐藏线稿化用法提示"));
            hintToggle->setChecked(true);
        }
        // 设置页内容不多于视口宽度、够高不用滚动条这类纯排布断言不再单列，
        // 只保留“内容能完整显示”的粗粒度检查。
        dialog.resize(1100, 960);
        dialog.show();
        QApplication::processEvents();
        QWidget* settingsContent = scroll ? scroll->widget() : nullptr;
        ctx.check(settingsContent != nullptr, QStringLiteral("设置页内容可滚动区域存在"));
        dialog.hide();
    }
    {
        // 需求：参数只在内存里生效，只有点“保存到配置文件”才覆盖配置文件。
        const AppSettings defaults;
        const QString configPath = AppSettings::settingsFilePath();
        auto readConfig = [&configPath]() {
            QFile file(configPath);
            if (!file.open(QIODevice::ReadOnly)) {
                return QByteArray();
            }
            return file.readAll();
        };

        LineArtDialog dialog(defaults);
        QPushButton* saveButton = dialog.findChild<QPushButton*>(QStringLiteral("LineArtSaveSettingsButton"));
        ctx.check(saveButton && saveButton->text() == QStringLiteral("保存到配置文件"),
                  QStringLiteral("参数对话框提供保存到配置文件按钮"),
                  saveButton ? saveButton->text() : QStringLiteral("missing"));

        QSpinBox* levelsSpin = dialog.findChild<QSpinBox*>(QStringLiteral("LineArtLevelsSpin"));
        if (levelsSpin) {
            levelsSpin->setValue(11);
        }
        const QByteArray beforeAccept = readConfig();
        dialog.accept();
        const QByteArray afterAccept = readConfig();
        ctx.check(!dialog.saveRequested(), QStringLiteral("点确定只改内存不写配置文件"));
        ctx.check(afterAccept == beforeAccept, QStringLiteral("点确定后配置文件内容原样未动"),
                  QStringLiteral("before=%1 after=%2").arg(beforeAccept.size()).arg(afterAccept.size()));

        LineArtDialog saveDialog(defaults);
        QPushButton* saveButton2 = saveDialog.findChild<QPushButton*>(QStringLiteral("LineArtSaveSettingsButton"));
        QSpinBox* saveAreaSpin = saveDialog.findChild<QSpinBox*>(QStringLiteral("LineArtMinAreaSpin"));
        if (saveAreaSpin) {
            saveAreaSpin->setValue(96);
        }
        if (saveButton2) {
            saveButton2->click();
        }
        ctx.check(
            saveDialog.saveRequested() && saveDialog.result() == QDialog::Accepted,
            QStringLiteral("点保存到配置文件会置位保存请求并继续"),
            QStringLiteral("requested=%1 result=%2").arg(saveDialog.saveRequested() ? 1 : 0).arg(saveDialog.result()));

        AppSettings memory = defaults;
        saveDialog.applyToSettings(memory);
        ctx.check(memory.lineArtMinRegionArea == 96, QStringLiteral("不点保存时参数仍写进行程内设置"),
                  QString::number(memory.lineArtMinRegionArea));

        // 只写线稿化键：先记下文件里的非线稿化值，保存后它们必须原样保留。
        AppSettings seeded;
        seeded.load();
        const int brushWidthBefore = seeded.brushWidth;
        AppSettings lineOnly = seeded;
        lineOnly.lineArtColorLevels = 11;
        lineOnly.lineArtMinThickness = 3.5;
        ctx.check(lineOnly.saveLineArt(), QStringLiteral("保存到配置文件只写线稿化参数"));
        AppSettings reloaded;
        reloaded.load();
        ctx.check(reloaded.lineArtColorLevels == 11 && qAbs(reloaded.lineArtMinThickness - 3.5) < 1e-6 &&
                      reloaded.brushWidth == brushWidthBefore,
                  QStringLiteral("只覆盖线稿化参数，其它设置保持不动"),
                  QStringLiteral("levels=%1 thickness=%2 width=%3")
                      .arg(reloaded.lineArtColorLevels)
                      .arg(reloaded.lineArtMinThickness)
                      .arg(reloaded.brushWidth));
    }
    {
        const AppSettings defaults;
        LineArtDialog dialog(defaults);
        auto* adaptiveCheck = dialog.findChild<QCheckBox*>(QStringLiteral("LineArtAdaptiveCheck"));
        auto* limitSpin = dialog.findChild<QSpinBox*>(QStringLiteral("LineArtAdaptiveLimitSpin"));
        ctx.check(adaptiveCheck && adaptiveCheck->isChecked() && limitSpin &&
                      limitSpin->value() == defaults.lineArtAdaptiveLimit && limitSpin->isEnabled(),
                  QStringLiteral("参数对话框默认勾选自适应降采样"),
                  QStringLiteral("checked=%1 limit=%2 enabled=%3")
                      .arg(adaptiveCheck && adaptiveCheck->isChecked() ? 1 : 0)
                      .arg(limitSpin ? limitSpin->value() : -1)
                      .arg(limitSpin && limitSpin->isEnabled() ? 1 : 0));

        const LineArtConverter::Options initial = dialog.options();
        ctx.check(initial.adaptiveDownscale && initial.adaptiveRegionLimit == defaults.lineArtAdaptiveLimit,
                  QStringLiteral("自适应降采样设置会交给转换器"),
                  QStringLiteral("adaptive=%1 limit=%2")
                      .arg(initial.adaptiveDownscale ? 1 : 0)
                      .arg(initial.adaptiveRegionLimit));

        if (adaptiveCheck && limitSpin) {
            adaptiveCheck->setChecked(false);
            ctx.check(!dialog.options().adaptiveDownscale && !limitSpin->isEnabled(),
                      QStringLiteral("取消勾选后自适应降采样关闭且阈值不可编辑"),
                      QStringLiteral("adaptive=%1 enabled=%2")
                          .arg(dialog.options().adaptiveDownscale ? 1 : 0)
                          .arg(limitSpin->isEnabled() ? 1 : 0));

            AppSettings written = defaults;
            dialog.applyToSettings(written);
            ctx.check(!written.lineArtAdaptiveDownscale, QStringLiteral("取消勾选会写回设置"),
                      QString::number(written.lineArtAdaptiveDownscale ? 1 : 0));
        }
    }
    {
        // 需求：简化容差与最细条带也暴露到界面，能在参数对话框和设置里调。
        const AppSettings defaults;
        LineArtDialog dialog(defaults);
        auto* toleranceSpin = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("LineArtToleranceSpin"));
        auto* thicknessSpin = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("LineArtThicknessSpin"));
        ctx.check(toleranceSpin && thicknessSpin &&
                      qAbs(toleranceSpin->value() - defaults.lineArtSimplifyTolerance) < 1e-6 &&
                      qAbs(thicknessSpin->value() - defaults.lineArtMinThickness) < 1e-6,
                  QStringLiteral("参数对话框显示简化容差与最细条带"),
                  QStringLiteral("tolerance=%1 thickness=%2")
                      .arg(toleranceSpin ? toleranceSpin->value() : -1.0)
                      .arg(thicknessSpin ? thicknessSpin->value() : -1.0));

        if (toleranceSpin && thicknessSpin) {
            toleranceSpin->setValue(1.5);
            thicknessSpin->setValue(2.0);
            const LineArtConverter::Options changed = dialog.options();
            ctx.check(qAbs(changed.simplifyTolerance - 1.5) < 1e-6 && qAbs(changed.minRegionThickness - 2.0) < 1e-6,
                      QStringLiteral("手改简化容差与最细条带即时反映到转换参数"),
                      QStringLiteral("tolerance=%1 thickness=%2")
                          .arg(changed.simplifyTolerance)
                          .arg(changed.minRegionThickness));

            AppSettings written = defaults;
            dialog.applyToSettings(written);
            ctx.check(qAbs(written.lineArtSimplifyTolerance - 1.5) < 1e-6 &&
                          qAbs(written.lineArtMinThickness - 2.0) < 1e-6,
                      QStringLiteral("简化容差与最细条带会写回设置"),
                      QStringLiteral("tolerance=%1 thickness=%2")
                          .arg(written.lineArtSimplifyTolerance)
                          .arg(written.lineArtMinThickness));
        }

        SettingsDialog settingsDialog(defaults);
    }
    {
        // 需求：简化容差并入档位，切档位一起换；手改就回“自定义”；容差不进档位的是最细条带。
        const AppSettings defaults;
        LineArtDialog dialog(defaults);
        auto* presetCombo = dialog.findChild<QComboBox*>(QStringLiteral("LineArtPresetCombo"));
        auto* toleranceSpin = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("LineArtToleranceSpin"));
        auto* areaSpin = dialog.findChild<QSpinBox*>(QStringLiteral("LineArtMinAreaSpin"));

        if (presetCombo && toleranceSpin && areaSpin) {
            presetCombo->setCurrentIndex(presetCombo->findData(0));

            presetCombo->setCurrentIndex(presetCombo->findData(3));

            presetCombo->setCurrentIndex(presetCombo->findData(1));
            ctx.check(presetCombo->currentData().toInt() == 1 && qAbs(toleranceSpin->value() - 0.6) < 1e-6,
                      QStringLiteral("均衡档位的容差是 0.6 且档位显示不被容差打回自定义"),
                      QStringLiteral("data=%1 tolerance=%2")
                          .arg(presetCombo->currentData().toInt())
                          .arg(toleranceSpin->value()));

            toleranceSpin->setValue(0.9);
            ctx.check(presetCombo->currentText() == QStringLiteral("自定义"),
                      QStringLiteral("手改简化容差后档位回到自定义"), presetCombo->currentText());

            toleranceSpin->setValue(0.8);
            areaSpin->setValue(77);
            ctx.check(presetCombo->currentText() == QStringLiteral("自定义"),
                      QStringLiteral("容差回到档位值但其它参数不同时仍是自定义"), presetCombo->currentText());

            areaSpin->setValue(12);
            toleranceSpin->setValue(0.6);
            ctx.check(presetCombo->currentData().toInt() == 1,
                      QStringLiteral("容差与其它四项都回到档位值后重新识别出均衡档位"),
                      QString::number(presetCombo->currentData().toInt()));
        }

        SettingsDialog settingsDialog(defaults);
        auto* settingsPresetCombo = settingsDialog.findChild<QComboBox*>(QStringLiteral("LineArtPresetCombo"));
        auto* settingsToleranceSpin = settingsDialog.findChild<QDoubleSpinBox*>(QStringLiteral("LineArtToleranceSpin"));
        if (settingsPresetCombo && settingsToleranceSpin) {
            settingsPresetCombo->setCurrentIndex(settingsPresetCombo->findData(2));
            ctx.check(qAbs(settingsToleranceSpin->value() - 0.8) < 1e-6,
                      QStringLiteral("设置对话框切档位时简化容差同步"),

                      QString::number(settingsToleranceSpin->value()));
            settingsToleranceSpin->setValue(0.65);
            ctx.check(settingsPresetCombo->currentText() == QStringLiteral("自定义"),
                      QStringLiteral("设置对话框手改简化容差后档位回到自定义"), settingsPresetCombo->currentText());
        }
        const LineArtPreset balanced = lineArtPreset(1);
        ctx.check(balanced.minArea == 12 && qAbs(balanced.tolerance - 0.6) < 1e-6 && lineArtPreset(0).minArea == 8 &&
                      qAbs(lineArtPreset(0).tolerance - 0.5) < 1e-6 && lineArtPreset(2).minArea == 20 &&
                      qAbs(lineArtPreset(2).tolerance - 0.8) < 1e-6 && lineArtPreset(3).minArea == 32 &&
                      qAbs(lineArtPreset(3).tolerance - 1.0) < 1e-6,
                  QStringLiteral("四档的简化容差顺序是 0.5/0.6/0.8/1.0"),
                  QStringLiteral("%1/%2/%3/%4")
                      .arg(lineArtPreset(0).tolerance)
                      .arg(lineArtPreset(1).tolerance)
                      .arg(lineArtPreset(2).tolerance)
                      .arg(lineArtPreset(3).tolerance));
    }
    {
        // 需求：色块超过阈值时自动降采样；取消勾选就按原处理尺寸硬算。
        QImage manyBlocks(640, 480, QImage::Format_RGB32);
        manyBlocks.fill(Qt::white);
        {
            QPainter painter(&manyBlocks);
            painter.setPen(Qt::NoPen);
            const QColor palette[] = {QColor(220, 60, 60), QColor(60, 90, 200), QColor(40, 160, 90),
                                      QColor(210, 180, 50)};
            for (int row = 0; row < 30; ++row) {
                for (int col = 0; col < 40; ++col) {
                    painter.fillRect(QRect(col * 16 + 1, row * 16 + 1, 13, 13), palette[(row + col) % 4]);
                }
            }
        }

        LineArtConverter::Options adaptive;
        adaptive.adaptiveDownscale = true;
        adaptive.adaptiveRegionLimit = 300;
        const LineArtConverter::Result downscaled = LineArtConverter::extract(manyBlocks, adaptive);
        ctx.check(downscaled.ok && downscaled.imageSize.height() < 480,
                  QStringLiteral("色块超过阈值时自动降低处理尺寸"),
                  QStringLiteral("size=%1x%2 candidate=%3 outlines=%4")
                      .arg(downscaled.imageSize.width())
                      .arg(downscaled.imageSize.height())
                      .arg(downscaled.candidateCount)
                      .arg(downscaled.outlines.size()));

        LineArtConverter::Options keepSize = adaptive;
        keepSize.adaptiveDownscale = false;
        const LineArtConverter::Result fullSize = LineArtConverter::extract(manyBlocks, keepSize);
        ctx.check(fullSize.ok && fullSize.imageSize == QSize(640, 480) && !fullSize.truncated,
                  QStringLiteral("取消自适应降采样后按原处理尺寸描边"),
                  QStringLiteral("size=%1x%2 outlines=%3")
                      .arg(fullSize.imageSize.width())
                      .arg(fullSize.imageSize.height())
                      .arg(fullSize.outlines.size()));
    }
    {
        // 需求：线稿化要给几档预设，一键换掉全部参数；手改任一项就回到“自定义”。
        const AppSettings defaults;
        SettingsDialog dialog(defaults);
        auto* presetCombo = dialog.findChild<QComboBox*>(QStringLiteral("LineArtPresetCombo"));
        auto* levelsSpin = dialog.findChild<QSpinBox*>(QStringLiteral("LineArtLevelsSpin"));
        auto* paletteSpin = dialog.findChild<QSpinBox*>(QStringLiteral("LineArtPaletteMergeSpin"));
        auto* adjacentSpin = dialog.findChild<QSpinBox*>(QStringLiteral("LineArtAdjacentMergeSpin"));
        auto* areaSpin = dialog.findChild<QSpinBox*>(QStringLiteral("LineArtMinAreaSpin"));

        // 默认参数正好是“均衡”档，打开时就应选中它。
        ctx.check(presetCombo && presetCombo->currentText() == QStringLiteral("均衡"),
                  QStringLiteral("线稿化档位默认选中均衡"), presetCombo ? presetCombo->currentText() : QString());

        if (presetCombo && areaSpin && paletteSpin && adjacentSpin && levelsSpin) {
            const int cleanIndex = presetCombo->findData(2); // 线条干净
            presetCombo->setCurrentIndex(cleanIndex);
            ctx.check(paletteSpin->value() == 64 && adjacentSpin->value() == 56 && areaSpin->value() == 20,
                      QStringLiteral("切换“线条干净”档位一次性改掉全部参数"),
                      QStringLiteral("palette=%1 adjacent=%2 area=%3")
                          .arg(paletteSpin->value())
                          .arg(adjacentSpin->value())
                          .arg(areaSpin->value()));

            const int detailIndex = presetCombo->findData(0); // 保留细节
            presetCombo->setCurrentIndex(detailIndex);
            ctx.check(levelsSpin->value() == 6 && paletteSpin->value() == 40 && adjacentSpin->value() == 44 &&
                          areaSpin->value() == 8,
                      QStringLiteral("切换“保留细节”档位一次性改掉全部参数"),
                      QStringLiteral("levels=%1 palette=%2 adjacent=%3 area=%4")
                          .arg(levelsSpin->value())
                          .arg(paletteSpin->value())
                          .arg(adjacentSpin->value())
                          .arg(areaSpin->value()));

            // 用户手改任一参数，档位要立刻回到“自定义”。
            areaSpin->setValue(33);
            ctx.check(presetCombo->currentText() == QStringLiteral("自定义"),
                      QStringLiteral("手动改动参数后档位回到自定义"), presetCombo->currentText());
            paletteSpin->setValue(51);
            // 再选回“均衡”，写回的值应当与默认一致。
            presetCombo->setCurrentIndex(presetCombo->findData(1));
            if (auto* buttons = dialog.findChild<QDialogButtonBox*>()) {
                if (QPushButton* ok = buttons->button(QDialogButtonBox::Ok)) {
                    ok->click();
                } else {
                    dialog.accept();
                }
            } else {
                dialog.accept();
            }
            ctx.check(dialog.settings().lineArtPaletteMerge == defaults.lineArtPaletteMerge &&
                          dialog.settings().lineArtAdjacentMerge == defaults.lineArtAdjacentMerge &&
                          dialog.settings().lineArtMinRegionArea == defaults.lineArtMinRegionArea,
                      QStringLiteral("“均衡”档位写回后与默认参数一致"),
                      QStringLiteral("palette=%1 adjacent=%2 area=%3")
                          .arg(dialog.settings().lineArtPaletteMerge)
                          .arg(dialog.settings().lineArtAdjacentMerge)
                          .arg(dialog.settings().lineArtMinRegionArea));
        }
    }
    // ---------------------------------------------------------------
}
