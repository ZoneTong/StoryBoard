#include "app/I18n.h"
#include "app/LifecycleLog.h"
#include "app/LogService.h"
#include "app/Settings.h"
#include "items/CanvasTypes.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "items/shapes/ShapeFactory.h"
#include "lineart/LineArtConverter.h"
#include "lineart/LineArtDialog.h"
#include "scene/CanvasScene.h"
#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"
#include "ui/settings/SettingsDialog.h"

#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QPixmap>
#include <QPointF>
#include <QPushButton>
#include <QSizeF>
#include <QSpinBox>
#include <QUndoStack>


void selftest::RunSettingsDialogBasicTests(Context& ctx)
{
    {
        CanvasScene handleScene;
        handleScene.SetMode(ToolMode::EDIT);
        handleScene.SetShowGrid(false);
        QUndoStack handleUndo;
        handleScene.SetUndoStack(&handleUndo);

        auto* handleEllipse = ShapeFactory::Create(ShapeType::ELLIPSE);
        handleEllipse->SetStyle(QColor(QStringLiteral("#0F172A")), QColor(Qt::transparent), 4.0);
        handleEllipse->SetContentSize(QSizeF(200, 120));
        handleEllipse->setPos(QPointF(60, 40));
        handleScene.AttachItem(handleEllipse);
        handleEllipse->setSelected(true);

        ctx.Check(handleEllipse->shape().contains(QPointF(-3.0, -3.0)), QStringLiteral("选中椭圆后左上角控制点可命中"));
        ctx.Check(handleEllipse->shape().contains(QPointF(203.0, 123.0)),
                  QStringLiteral("选中椭圆后右下角控制点可命中"));
        ctx.Check(!handleEllipse->shape().contains(QPointF(-40.0, -40.0)),
                  QStringLiteral("控制点命中区域不会无限外扩"));

        handleEllipse->setSelected(false);
        ctx.Check(!handleEllipse->shape().contains(QPointF(-3.0, -3.0)), QStringLiteral("未选中时控制点不参与命中"));

        auto* thickRect = ShapeFactory::Create(ShapeType::RECTANGLE);
        thickRect->SetStyle(QColor(QStringLiteral("#1D4ED8")), QColor(29, 78, 216, 40), 40.0);
        thickRect->SetContentSize(QSizeF(200, 120));
        thickRect->setPos(QPointF(400, 40));
        handleScene.AttachItem(thickRect);

        ctx.Check(thickRect->shape().contains(QPointF(-10.0, 60.0)), QStringLiteral("粗描边图形按描边宽度纳入命中"));
        ctx.Check(!thickRect->shape().contains(QPointF(-40.0, 60.0)), QStringLiteral("粗描边命中范围不超过描边宽度"));
    }

    {
        QPixmap firstBackground(40, 30);
        firstBackground.fill(QColor(QStringLiteral("#0EA5E9")));
        QPixmap secondBackground(40, 30);
        secondBackground.fill(QColor(QStringLiteral("#0EA5E9")));

        CanvasBackground firstState;
        firstState.type = BackgroundType::IMAGE;
        firstState.image = firstBackground;
        CanvasBackground secondState = firstState;
        secondState.image = secondBackground;

        ctx.Check(firstState == secondState, QStringLiteral("内容相同的背景图片判定为相等"));

        CanvasScene dedupeScene;
        QUndoStack dedupeUndo;
        dedupeScene.SetUndoStack(&dedupeUndo);
        dedupeScene.ReplaceCanvasBackground(firstState);
        const int stepsAfterFirst = dedupeUndo.count();
        dedupeScene.ReplaceCanvasBackground(secondState);
        ctx.Check(dedupeUndo.count() == stepsAfterFirst, QStringLiteral("重复应用同一背景图片不增加撤销步骤"),
                  QString::number(dedupeUndo.count()));
    }

    {
        // 日志默认只落错误与告警；事件级别才记录图元属性变化。
        const AppSettings defaults;
        ctx.Check(defaults.logLevel == LogLevel::LOG_WARNING, QStringLiteral("日志默认级别为错误与告警"));

        SettingsDialog dialog(defaults);
        auto* levelCombo = dialog.findChild<QComboBox*>(QStringLiteral("LogLevelCombo"));
        ctx.Check(levelCombo && levelCombo->currentData().toInt() == static_cast<int>(LogLevel::LOG_WARNING),
                  QStringLiteral("设置对话框显示默认日志级别"),
                  levelCombo ? QString::number(levelCombo->currentData().toInt()) : QStringLiteral("-1"));
        if (levelCombo) {
            const int eventIndex = levelCombo->findData(static_cast<int>(LogLevel::LOG_EVENT));
            if (eventIndex >= 0) {
                levelCombo->setCurrentIndex(eventIndex);
            }
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
        ctx.Check(dialog.Settings().logLevel == LogLevel::LOG_EVENT, QStringLiteral("设置对话框可切换到事件级别"),
                  QString::number(static_cast<int>(dialog.Settings().logLevel)));
    }

    {
        // 持久化：重新打开应用时仍按上次选的详细程度过滤。
        AppSettings persisted;
        persisted.Load();
        const LogLevel previous = persisted.logLevel;
        persisted.logLevel = LogLevel::LOG_EVENT;
        persisted.Save();
        AppSettings reloaded;
        reloaded.Load();
        ctx.Check(reloaded.logLevel == LogLevel::LOG_EVENT, QStringLiteral("日志级别写入配置文件并可重新读取"),
                  QString::number(static_cast<int>(reloaded.logLevel)));
        reloaded.logLevel = previous;
        reloaded.Save();
    }

    {
        LogService::SetLevel(LogLevel::LOG_WARNING);
        ctx.Check(!LogService::ShouldLog(LogLevel::LOG_EVENT) && LogService::ShouldLog(LogLevel::LOG_WARNING),
                  QStringLiteral("默认级别过滤图元事件但保留告警"));
        LogService::SetLevel(LogLevel::LOG_EVENT);
        ctx.Check(LogService::ShouldLog(LogLevel::LOG_EVENT) && !LogService::ShouldLog(LogLevel::LOG_INFO),
                  QStringLiteral("事件级别记录属性变化但仍过滤画布信息"));
        LogService::SetLevel(LogLevel::LOG_WARNING);
    }

    {
        // 真正落盘检查：默认不进文件，事件级别才出现图元变化。
        const QString uid = QStringLiteral("selftest-log-filter");
        const QString path =
            QDir(LogService::LogDirectory()).filePath(QStringLiteral("canvases/canvas-%1.log").arg(uid));
        QFile::remove(path);
        LifecycleLog::LogEdit(uid, QStringLiteral("#自检"), QStringLiteral("编辑"), QStringLiteral("测试事件"));
        ctx.Check(!QFileInfo::exists(path), QStringLiteral("默认级别不把图元事件写进文件"));
        LogService::SetLevel(LogLevel::LOG_EVENT);
        LifecycleLog::LogEdit(uid, QStringLiteral("#自检"), QStringLiteral("编辑"), QStringLiteral("测试事件"));
        QFile file(path);
        const bool opened = file.open(QIODevice::ReadOnly | QIODevice::Text);
        const QString text = opened ? QString::fromUtf8(file.readAll()) : QString();
        ctx.Check(opened && text.contains(QStringLiteral("测试事件")),
                  QStringLiteral("事件级别把图元属性变化写进画布日志"));
        QFile::remove(path);
        LogService::SetLevel(LogLevel::LOG_WARNING);
    }

    {
        // 同一份画布日志追加超过 14 天后，旧条目要在下一次写入时裁掉，文件头保留。
        const QString uid = QStringLiteral("selftest-log-retention-%1").arg(QDateTime::currentMSecsSinceEpoch());
        const QString path =
            QDir(LogService::LogDirectory()).filePath(QStringLiteral("canvases/canvas-%1.log").arg(uid));
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile::remove(path);
        const QDateTime now = QDateTime::currentDateTime();
        const QString oldStamp =
            now.addDays(-20).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) + QStringLiteral(".000");
        QFile seed(path);
        if (seed.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            seed.write(QStringLiteral("# StoryBoard canvas lifecycle log\n"
                                      "# canvas uid=%1\n"
                                      "%2 [INFO ] storyboard.lifecycle: 二十天前事件\n")
                           .arg(uid, oldStamp)
                           .toUtf8());
            seed.close();
        }
        LogService::SetLevel(LogLevel::LOG_EVENT);
        LifecycleLog::LogEdit(uid, QStringLiteral("#自检"), QStringLiteral("编辑"), QStringLiteral("十四天内新事件"));
        QFile file(path);
        const bool opened = file.open(QIODevice::ReadOnly | QIODevice::Text);
        const QString text = opened ? QString::fromUtf8(file.readAll()) : QString();
        ctx.Check(!text.contains(QStringLiteral("二十天前事件")), QStringLiteral("画布日志裁掉 14 天以前的条目"));
        ctx.Check(text.contains(QStringLiteral("# StoryBoard canvas lifecycle log")) &&
                      text.contains(QStringLiteral("十四天内新事件")),
                  QStringLiteral("裁剪后保留文件头并继续追加新日志"));
        QFile::remove(path);
        LogService::SetLevel(LogLevel::LOG_WARNING);
    }

    {
        const AppSettings defaults;
        SettingsDialog dialog(defaults);
        dialog.SetBrushColor(QColor(QStringLiteral("#FF8800")));
        dialog.SetCanvasColor(QColor(QStringLiteral("#102A43")));
        auto* smoothingSpin = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("StrokeSmoothingSpin"));
        ctx.Check(smoothingSpin && smoothingSpin->minimum() == 0.0 && smoothingSpin->maximum() == 20.0 &&
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
        ctx.Check(dialog.Settings().brushColor == QColor(QStringLiteral("#FF8800")),
                  QStringLiteral("设置对话框保留画笔颜色"));
        ctx.Check(dialog.Settings().canvasColor == QColor(QStringLiteral("#102A43")),
                  QStringLiteral("设置对话框保留画布外围颜色"));
        ctx.Check(dialog.Settings().strokeSmoothing == 0, QStringLiteral("设置对话框可关闭自动平滑"),
                  QString::number(dialog.Settings().strokeSmoothing));
    }

    {
        // 需求：把“相邻且颜色接近的色块并成一块”做成可调参数，并放在设置里。
        const AppSettings defaults;
        ctx.Check(defaults.lineArtAdjacentMerge == qRound(LineArtConverter::Options().adjacentMergeDistance) &&
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
        ctx.Check(dialog.Settings().lineArtAdjacentMerge == 96, QStringLiteral("设置对话框可调整相邻色块合并距离"),
                  QString::number(dialog.Settings().lineArtAdjacentMerge));
        ctx.Check(dialog.Settings().lineArtPaletteMerge == 24 && dialog.Settings().lineArtMinRegionArea == 8,
                  QStringLiteral("设置对话框可调整颜色聚类距离与最小色块"),
                  QStringLiteral("palette=%1 area=%2")
                      .arg(dialog.Settings().lineArtPaletteMerge)
                      .arg(dialog.Settings().lineArtMinRegionArea));
    }

    {
        // 「恢复初始值」：改完数值点一下，应该回到配置文件里当前保存的那一套。
        AppSettings fileSettings;
        fileSettings.Load();
        const int fileLevels = fileSettings.lineArtColorLevels;
        LineArtDialog dialog(fileSettings);
        auto* levels = dialog.findChild<QSpinBox*>(QStringLiteral("LineArtLevelsSpin"));
        auto* restore = dialog.findChild<QPushButton*>(QStringLiteral("LineArtRestoreButton"));
        ctx.Check(restore != nullptr, QStringLiteral("线稿化参数窗提供「恢复初始值」按钮"));
        if (levels) {
            levels->setValue(fileLevels == 2 ? 3 : 2);
        }
        const int editedLevels = levels ? levels->value() : -1;
        if (restore) {
            restore->click();
        }
        ctx.Check(levels && levels->value() == fileLevels, QStringLiteral("恢复初始值后回到配置文件里的参数"),
                  QStringLiteral("edited=%1 restored=%2 file=%3")
                      .arg(editedLevels)
                      .arg(levels ? levels->value() : -1)
                      .arg(fileLevels));
    }

    {
        // 属性行标签以 ItemPropertiesDialog 这个 i18n 上下文翻译，换语言时只剩反向表可查；
        // 这里校验 ts 全上下文的正续表能把英文源串送回中文，且冲突串不来回抖。
        const UiLanguage previous = UiEffectiveLanguage();
        UiOverrideLanguage(UiLanguage::ENGLISH);
        UiInitializeLanguage();
        const QString enSemi = UiTranslateEitherWay(QStringLiteral("长半轴"));
        ctx.Check(enSemi == QStringLiteral("Semi-major axis"), QStringLiteral("中文译串翻回英文源串"),
                  QStringLiteral("en=%1").arg(enSemi));
        UiOverrideLanguage(UiLanguage::CHINESE);
        UiInitializeLanguage();
        const QString zhSemi = UiTranslateEitherWay(QStringLiteral("Semi-major axis"));
        ctx.Check(zhSemi == QStringLiteral("长半轴"), QStringLiteral("英文源串跨上下文翻回中文"),
                  QStringLiteral("zh=%1").arg(zhSemi));
        ctx.Check(UiTranslateEitherWay(QStringLiteral("Stroke")) != QStringLiteral("Stroke"),
                  QStringLiteral("冲突串仍按 StoryBoard 上下文照常翻译，未被正续表劫持"));
        // D3：计数文案走 %n + numerusform，中文按数量替换、英文保留 Item(s)。
        const QString zhCount = QCoreApplication::translate("MainWindow", "Duplicated %n Item(s)", nullptr, 2);
        ctx.Check(zhCount == QStringLiteral("已复制 2 个图元"), QStringLiteral("中文复数条目按 %n 替换数量"),
                  QStringLiteral("zh=%1").arg(zhCount));
        UiOverrideLanguage(UiLanguage::ENGLISH);
        UiInitializeLanguage();
        const QString enCount = QCoreApplication::translate("MainWindow", "Duplicated %n Item(s)", nullptr, 1);
        ctx.Check(enCount == QStringLiteral("Duplicated 1 Item(s)"), QStringLiteral("英文复数条目保留 Item(s) 写法"),
                  QStringLiteral("en=%1").arg(enCount));
        UiOverrideLanguage(previous);
        UiInitializeLanguage();
    }
}
