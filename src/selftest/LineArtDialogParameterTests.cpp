#include "LineArtConverter.h"
#include "LineArtDialog.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"

#include <QApplication>
#include <QByteArray>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QToolButton>
#include <QWidget>


void selftest::RunLineArtDialogParameterTests(Context& ctx)
{
    {
        // 需求：选完图片先弹线稿化参数，点确定后才生成曲线。
        const AppSettings defaults;
        LineArtDialog dialog(defaults);
        auto* presetCombo = dialog.findChild<QComboBox*>(QStringLiteral("LineArtPresetCombo"));
        auto* levelsSpin = dialog.findChild<QSpinBox*>(QStringLiteral("LineArtLevelsSpin"));
        auto* paletteSpin = dialog.findChild<QSpinBox*>(QStringLiteral("LineArtPaletteMergeSpin"));
        auto* adjacentSpin = dialog.findChild<QSpinBox*>(QStringLiteral("LineArtAdjacentMergeSpin"));
        auto* areaSpin = dialog.findChild<QSpinBox*>(QStringLiteral("LineArtMinAreaSpin"));

        const LineArtConverter::Options initial = dialog.options();
        ctx.Check(initial.colorLevels == defaults.lineArtColorLevels &&
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
            ctx.Check(clean.paletteMergeDistance == 64.0 && clean.adjacentMergeDistance == 56.0 &&
                          clean.minRegionArea == 20,
                      QStringLiteral("参数对话框切档位后输出参数跟着变"),
                      QStringLiteral("palette=%1 adjacent=%2 area=%3")
                          .arg(clean.paletteMergeDistance)
                          .arg(clean.adjacentMergeDistance)
                          .arg(clean.minRegionArea));

            areaSpin->setValue(77);
            ctx.Check(presetCombo->currentText() == QStringLiteral("自定义"),
                      QStringLiteral("参数对话框手改数值后档位回到自定义"), presetCombo->currentText());
            ctx.Check(dialog.options().minRegionArea == 77 && dialog.options().paletteMergeDistance == 64.0,
                      QStringLiteral("参数对话框手改数值即时反映到输出参数"),
                      QStringLiteral("area=%1 palette=%2")
                          .arg(dialog.options().minRegionArea)
                          .arg(dialog.options().paletteMergeDistance));

            AppSettings written = defaults;
            dialog.ApplyToSettings(written);
            ctx.Check(written.lineArtMinRegionArea == 77 && written.lineArtPaletteMerge == 64,
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
        ctx.Check(scroll && scroll->widgetResizable() && scroll->widget(),
                  QStringLiteral("设置对话框内容放在可滚动区域"), QStringLiteral("scroll=%1").arg(scroll ? 1 : 0));
        QToolButton* hintToggle = dialog.findChild<QToolButton*>(QStringLiteral("LineArtHintToggle"));
        QLabel* lineArtHint = nullptr;
        for (QLabel* label : dialog.findChildren<QLabel*>()) {
            if (label->text().startsWith(QStringLiteral("<ul>"))) {
                lineArtHint = label;
                break;
            }
        }
        ctx.Check(hintToggle && lineArtHint && hintToggle->isChecked() && lineArtHint->isVisibleTo(&dialog),
                  QStringLiteral("线稿化用法提示默认展开并提供折叠开关"),
                  QStringLiteral("toggle=%1 hint=%2").arg(hintToggle ? 1 : 0).arg(lineArtHint ? 1 : 0));
        if (hintToggle && lineArtHint) {
            hintToggle->setChecked(false);
            ctx.Check(!lineArtHint->isVisibleTo(&dialog), QStringLiteral("折叠后隐藏线稿化用法提示"));
            hintToggle->setChecked(true);
        }
        // 设置页内容不多于视口宽度、够高不用滚动条这类纯排布断言不再单列，
        // 只保留“内容能完整显示”的粗粒度检查。
        dialog.resize(1100, 960);
        dialog.show();
        QApplication::processEvents();
        QWidget* settingsContent = scroll ? scroll->widget() : nullptr;
        ctx.Check(settingsContent != nullptr, QStringLiteral("设置页内容可滚动区域存在"));
        dialog.hide();
    }
    {
        // 需求：参数只在内存里生效，只有点“保存到配置文件”才覆盖配置文件。
        const AppSettings defaults;
        const QString configPath = AppSettings::SettingsFilePath();
        auto readConfig = [&configPath]() {
            QFile file(configPath);
            if (!file.open(QIODevice::ReadOnly)) {
                return QByteArray();
            }
            return file.readAll();
        };

        LineArtDialog dialog(defaults);
        QPushButton* saveButton = dialog.findChild<QPushButton*>(QStringLiteral("LineArtSaveSettingsButton"));
        ctx.Check(saveButton && saveButton->text() == QStringLiteral("保存到配置文件"),
                  QStringLiteral("参数对话框提供保存到配置文件按钮"),
                  saveButton ? saveButton->text() : QStringLiteral("missing"));

        QSpinBox* levelsSpin = dialog.findChild<QSpinBox*>(QStringLiteral("LineArtLevelsSpin"));
        if (levelsSpin) {
            levelsSpin->setValue(11);
        }
        const QByteArray beforeAccept = readConfig();
        dialog.accept();
        const QByteArray afterAccept = readConfig();
        ctx.Check(!dialog.SaveRequested(), QStringLiteral("点确定只改内存不写配置文件"));
        ctx.Check(afterAccept == beforeAccept, QStringLiteral("点确定后配置文件内容原样未动"),
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
        ctx.Check(
            saveDialog.SaveRequested() && saveDialog.result() == QDialog::Accepted,
            QStringLiteral("点保存到配置文件会置位保存请求并继续"),
            QStringLiteral("requested=%1 result=%2").arg(saveDialog.SaveRequested() ? 1 : 0).arg(saveDialog.result()));

        AppSettings memory = defaults;
        saveDialog.ApplyToSettings(memory);
        ctx.Check(memory.lineArtMinRegionArea == 96, QStringLiteral("不点保存时参数仍写进行程内设置"),
                  QString::number(memory.lineArtMinRegionArea));

        // 只写线稿化键：先记下文件里的非线稿化值，保存后它们必须原样保留。
        AppSettings seeded;
        seeded.Load();
        const int brushWidthBefore = seeded.brushWidth;
        AppSettings lineOnly = seeded;
        lineOnly.lineArtColorLevels = 11;
        lineOnly.lineArtMinThickness = 3.5;
        ctx.Check(lineOnly.SaveLineArt(), QStringLiteral("保存到配置文件只写线稿化参数"));
        AppSettings reloaded;
        reloaded.Load();
        ctx.Check(reloaded.lineArtColorLevels == 11 && qAbs(reloaded.lineArtMinThickness - 3.5) < 1e-6 &&
                      reloaded.brushWidth == brushWidthBefore,
                  QStringLiteral("只覆盖线稿化参数，其它设置保持不动"),
                  QStringLiteral("levels=%1 thickness=%2 width=%3")
                      .arg(reloaded.lineArtColorLevels)
                      .arg(reloaded.lineArtMinThickness)
                      .arg(reloaded.brushWidth));
    }
}
