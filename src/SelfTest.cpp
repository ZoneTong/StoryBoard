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

int RunSelfTest(const QString& reportPath)
{
    // 自检里的界面文案断言都按中文原文写，固定成中文，避免受机器语言影响。
    UiOverrideLanguage(UiLanguage::CHINESE);

    // 看门狗：任何一步意外卡住（例如被别处弹出的模态框挡住），180 秒后直接结束，
    // 免得打包脚本或 CI 一直挂着等。定时器在嵌套事件循环里同样会触发。
    QTimer::singleShot(180000, []() {
        qWarning("self-test watchdog fired: writing nothing, exiting with 3");
        _exit(3);
    });

    selftest::Context ctx;

    // Qt 内置字体对话框的标签来自 qtbase 翻译，而不是应用自己的 StoryBoard 上下文。
    ctx.Check(QCoreApplication::translate("QFontDialog", "Effects") == QStringLiteral("效果"),
              QStringLiteral("Qt 内置字体对话框已装载中文翻译"));
    // 视图 → 辅助网格 的菜单文案在 MainWindow 上下文里，缺译文就会显示英文。
    ctx.Check(QCoreApplication::translate("MainWindow", "Helper grid") == QStringLiteral("辅助网格"),
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
    ctx.Check(QDir(ctx.workDirectory).exists(), QStringLiteral("准备自检工作目录"), ctx.workDirectory);

    // 增量落盘：每条断言后重写报告，进程崩溃时也能看到最后的断言位置。
    ctx.progressHook = [&ctx, resolvedReportPath]() {
        QFile progress(resolvedReportPath);
        if (progress.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            QTextStream stream(&progress);
            stream.setEncoding(QStringConverter::Utf8);
            for (const QString& line : ctx.lines) {
                stream << line << "\n";
            }
        }
    };

    // 各功能段按历史断言顺序依次执行；段间共享状态放在 selftest::Context 里。
    selftest::RunSceneCanvasIoTests(ctx);
    selftest::RunLayerModelTests(ctx);
    selftest::RunStackingPriorityTests(ctx);
    selftest::RunLayerVisibilityLockTests(ctx);
    selftest::RunTextItemTests(ctx);
    selftest::RunShapeItemTests(ctx);
    selftest::RunLayerPanelTests(ctx);
    selftest::RunPaintOrderTests(ctx);
    selftest::RunViewInteractionTests(ctx);
    selftest::RunPropertyDialogFieldGeometryTests(ctx);
    selftest::RunPropertyDialogTextNamingTests(ctx);
    selftest::RunDuplicateItemTests(ctx);
    selftest::RunGroupTests(ctx);
    selftest::RunGroupingModelTests(ctx);
    selftest::RunRubberBandTests(ctx);
    selftest::RunLockedLayerTests(ctx);
    selftest::RunMainWindowFlowTests(ctx);
    selftest::RunMainWindowShapeMenuTests(ctx);
    selftest::RunSettingsDialogBasicTests(ctx);
    selftest::RunLineArtDialogParameterTests(ctx);
    selftest::RunLineArtPresetAdaptiveTests(ctx);
    selftest::RunFillStyleBasicTests(ctx);
    selftest::RunFillStylePatternTests(ctx);
    selftest::RunCanvasOcclusionBoundaryTests(ctx);
    selftest::RunLayerExportPanelTests(ctx);
    selftest::RunLineArtConverterTests(ctx);
    selftest::RunAlignmentTests(ctx);
    selftest::RunAlignmentMenuTests(ctx);
    selftest::RunGroupBoolOpsTests(ctx);
    selftest::RunBoolOpsSemanticTests(ctx);
    selftest::RunCustomShapeDockTests(ctx);
    selftest::RunShapesFolderTests(ctx);
    selftest::RunDockPanelMainWindowTests(ctx);
    selftest::RunDockPanelItemStyleTests(ctx);
    selftest::RunDockPanelPropertyTests(ctx);
    selftest::RunDockOrderPersistenceTests(ctx);
    selftest::RunClipboardShortcutTests(ctx);
    selftest::RunExcalidrawFormatTests(ctx);
    selftest::RunLineSplitTests(ctx);
    selftest::RunShapesClusterTests(ctx);
    selftest::RunImageFillTests(ctx);

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
