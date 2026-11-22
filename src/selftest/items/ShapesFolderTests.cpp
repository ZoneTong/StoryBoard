#include "app/Settings.h"
#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>

namespace {

// 用「父路径被一个普通文件占位」制造必然失败：跨平台都成立，也不依赖目录权限位。
QString BlockedConfigPath(const QString& workDirectory, QString* blockerPath)
{
    const QString blocker = QDir(workDirectory).filePath(QStringLiteral("shapes-blocker"));
    // 上一次自检可能留下了同名目录，先清掉再占位成普通文件。
    QDir(blocker).removeRecursively();
    QFile file(blocker);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write("blocker");
        file.close();
    }
    if (blockerPath) {
        *blockerPath = blocker;
    }
    return QDir(blocker).filePath(QStringLiteral("StoryBoard.ini"));
}

} // namespace

void selftest::RunShapesFolderTests(Context& ctx)
{
    const QByteArray previousConfig = qgetenv("STORYBOARD_CONFIG");

    {
        // 配置同级可写时保持既有语义：优先用配置文件同级的 shapes。
        const QString writableConfig = QDir(ctx.workDirectory).filePath(QStringLiteral("shapes-writable.ini"));
        qputenv("STORYBOARD_CONFIG", writableConfig.toLocal8Bit());
        const QString configuredDir = AppSettings::ShapesDir();
        const QString chosenDir = AppSettings::WritableShapesDir();
        ctx.Check(!chosenDir.isEmpty() && QDir::cleanPath(chosenDir) == QDir::cleanPath(configuredDir) &&
                      QFileInfo(chosenDir).isDir() && QFileInfo(chosenDir).isWritable(),
                  QStringLiteral("配置同级目录可写时优先使用它"),
                  QStringLiteral("configured=%1 chosen=%2").arg(configuredDir, chosenDir));
    }

    QString blockedDir;
    {
        // 配置文件放进被普通文件占位的路径里：同级 shapes 建不出来，必须降级到其它可写候选，
        // 而不是返回一个不存在的目录去打开（这正是「无法打开 shapes 目录」的根因）。
        QString blockerPath;
        const QString blockedConfig = BlockedConfigPath(ctx.workDirectory, &blockerPath);
        qputenv("STORYBOARD_CONFIG", blockedConfig.toLocal8Bit());
        blockedDir = AppSettings::ShapesDir();
        const QString fallbackDir = AppSettings::WritableShapesDir();
        ctx.Check(!fallbackDir.isEmpty() && QDir::cleanPath(fallbackDir) != QDir::cleanPath(blockedDir) &&
                      QFileInfo(fallbackDir).isDir() && QFileInfo(fallbackDir).isWritable(),
                  QStringLiteral("配置同级目录建不出来时降级到其它可写候选"),
                  QStringLiteral("blocked=%1 chosen=%2 blocker=%3").arg(blockedDir, fallbackDir, blockerPath));
    }

    {
        // 失败文案：面板按钮与菜单项共用同一句，必须带路径与原因，不能再是一句笼统提示。
        AppSettings::ShapesFolderOpenResult result;
        result.directory = blockedDir;

        // 文案里的目录会转成本地分隔符（Windows 反斜杠），断言前对 blockedDir 做同样归一化。
        const QString nativeBlockedDir = QDir::toNativeSeparators(blockedDir);
        result.error = AppSettings::ShapesFolderError::CREATE_FAILED;
        const QString createText = AppSettings::ShapesFolderFailureText(result);
        ctx.Check(createText.contains(nativeBlockedDir) && (createText.contains(QStringLiteral("写权限")) ||
                                                            createText.contains(QStringLiteral("write permission"))),
                  QStringLiteral("创建失败文案带目录与权限原因"), createText);

        result.error = AppSettings::ShapesFolderError::SHELL_FAILED;
        result.shellCode = 2;
        const QString shellText = AppSettings::ShapesFolderFailureText(result);
        ctx.Check(shellText.contains(nativeBlockedDir) &&
                      (shellText.contains(QStringLiteral("外壳")) || shellText.contains(QStringLiteral("shell"))) &&
                      shellText.contains(QStringLiteral("2")),
                  QStringLiteral("外壳失败文案带目录与返回码"), shellText);

        result.error = AppSettings::ShapesFolderError::DESKTOP_FAILED;
        const QString desktopText = AppSettings::ShapesFolderFailureText(result);
        ctx.Check(desktopText.contains(nativeBlockedDir) && (desktopText.contains(QStringLiteral("文件管理器")) ||
                                                             desktopText.contains(QStringLiteral("file manager"))),
                  QStringLiteral("桌面服务失败文案带目录与原因"), desktopText);

        result.error = AppSettings::ShapesFolderError::NONE;
        ctx.Check(AppSettings::ShapesFolderFailureText(result).isEmpty(),
                  QStringLiteral("没有失败原因时不产出多余文案"));
    }

    if (previousConfig.isEmpty()) {
        qunsetenv("STORYBOARD_CONFIG");
    } else {
        qputenv("STORYBOARD_CONFIG", previousConfig);
    }
}
