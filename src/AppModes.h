#pragma once

class QApplication;
class CommandLineOptions;

// 命令行各执行分支：自检、演示 / 画布导出、探针、各类截图，以及正常启动主窗口。
// app 已完成 AppStartup 初始化；返回进程退出码，语义与原 main 分支逐字一致。
namespace AppModes {

int run(QApplication& app, const CommandLineOptions& options);

} // namespace AppModes
