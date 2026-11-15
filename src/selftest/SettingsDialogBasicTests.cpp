#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "LineArtConverter.h"
#include "LineArtDialog.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "ShapeItem.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"

#include <QColor>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QPixmap>
#include <QPointF>
#include <QPushButton>
#include <QSizeF>
#include <QSpinBox>
#include <QUndoStack>


void selftest::runSettingsDialogBasicTests(Context& ctx)
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
        auto* smoothingSpin = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("StrokeSmoothingSpin"));
        ctx.check(smoothingSpin && smoothingSpin->minimum() == 0.0 && smoothingSpin->maximum() == 20.0 &&
                      qAbs(smoothingSpin->singleStep() - 0.1) < 0.001 && smoothingSpin->decimals() == 1 &&
                      smoothingSpin->value() == defaults.strokeSmoothing,
                  QStringLiteral("设置对话框默认启用 0.1 精度的画笔拟合容差"),
                  smoothingSpin ? QString::number(smoothingSpin->value()) : QStringLiteral("-1"));
        if (smoothingSpin) {
            smoothingSpin->setValue(0.0);
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
}
