#include "SelfTest.h"

#include "I18n.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStringConverter>
#include <QStringList>
#include <QTextStream>
#include <QTimer>
#include <cstdlib>

int runSelfTest(const QString& reportPath)
{
    // 自检里的界面文案断言都按中文原文写，固定成中文，避免受机器语言影响。
    uiOverrideLanguage(UiLanguage::Chinese);

    // 看门狗：任何一步意外卡住（例如被别处弹出的模态框挡住），180 秒后直接结束，
    // 免得打包脚本或 CI 一直挂着等。定时器在嵌套事件循环里同样会触发。
    QTimer::singleShot(180000, []() {
        qWarning("self-test watchdog fired: writing nothing, exiting with 3");
        _exit(3);
    });

    selftest::Context ctx;

    // Qt 内置字体对话框的标签来自 qtbase 翻译，而不是应用自己的 StoryBoard 上下文。
    ctx.check(QCoreApplication::translate("QFontDialog", "Effects") == QStringLiteral("效果"),
              QStringLiteral("Qt 内置字体对话框已装载中文翻译"));
    // 视图 → 辅助网格 的菜单文案在 MainWindow 上下文里，缺译文就会显示英文。
    ctx.check(QCoreApplication::translate("MainWindow", "Helper grid") == QStringLiteral("辅助网格"),
              QStringLiteral("视图菜单的辅助网格项有中文译文"),
              QCoreApplication::translate("MainWindow", "Helper grid"));

    const QString resolvedReportPath = reportPath.isEmpty()
                                           ? QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                                 .filePath(QStringLiteral("storyboard_selftest.txt"))
                                           : reportPath;
    const QFileInfo reportInfo(resolvedReportPath);
    QDir().mkpath(reportInfo.absolutePath());
    ctx.workDirectory = QDir(reportInfo.absolutePath()).filePath(QStringLiteral("storyboard-selftest-work"));
    QDir().mkpath(ctx.workDirectory);
    ctx.check(QDir(ctx.workDirectory).exists(), QStringLiteral("准备自检工作目录"), ctx.workDirectory);

    // 各功能段按历史断言顺序依次执行；段间共享状态放在 selftest::Context 里。
    selftest::runSceneCanvasIoTests(ctx);
    selftest::runLayerModelTests(ctx);
    selftest::runStackingPriorityTests(ctx);
    selftest::runLayerVisibilityLockTests(ctx);
    selftest::runTextItemTests(ctx);
    selftest::runShapeItemTests(ctx);
    selftest::runLayerPanelTests(ctx);
    selftest::runPaintOrderTests(ctx);
    selftest::runViewInteractionTests(ctx);
    selftest::runPropertyDialogFieldGeometryTests(ctx);
    selftest::runPropertyDialogImageTests(ctx);
    selftest::runPropertyDialogTextNamingTests(ctx);
    selftest::runDuplicateItemTests(ctx);
    selftest::runGroupTests(ctx);
    selftest::runRubberBandTests(ctx);
    selftest::runLockedLayerTests(ctx);
    selftest::runMainWindowFlowTests(ctx);
    selftest::runMainWindowShapeMenuTests(ctx);
    selftest::runSettingsDialogBasicTests(ctx);
    selftest::runLineArtDialogParameterTests(ctx);
    selftest::runLineArtPresetAdaptiveTests(ctx);
    selftest::runFillStyleBasicTests(ctx);
    selftest::runFillStylePatternTests(ctx);
    selftest::runCanvasOcclusionBoundaryTests(ctx);
    selftest::runLayerExportPanelTests(ctx);
    selftest::runLineArtConverterTests(ctx);
    selftest::runAlignmentTests(ctx);
    selftest::runAlignmentMenuTests(ctx);
    selftest::runGroupBoolOpsTests(ctx);
    selftest::runCustomShapeDockTests(ctx);
    selftest::runDockPanelMainWindowTests(ctx);
    selftest::runDockOrderPersistenceTests(ctx);

    int passCount = 0;
    for (const QString& line : ctx.lines) {
        if (line.startsWith(QStringLiteral("PASS"))) {
            ++passCount;
        }
    }
    ctx.lines.append(QString());
    ctx.lines.append(QStringLiteral("SUMMARY  %1 passed, %2 failed").arg(passCount).arg(ctx.failures));

    const QString path = resolvedReportPath;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile report(path);
    if (report.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&report);
        stream.setEncoding(QStringConverter::Utf8);
        for (const QString& line : ctx.lines) {
            stream << line << '\n';
        }
    }

    return ctx.failures == 0 ? 0 : 1;
}
