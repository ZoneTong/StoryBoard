#include "LineArtConverter.h"
#include "LineArtDialog.h"
#include "LineArtPresets.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QImage>
#include <QPainter>
#include <QPushButton>
#include <QSize>
#include <QSpinBox>


void selftest::runLineArtPresetAdaptiveTests(Context& ctx)
{
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
}
