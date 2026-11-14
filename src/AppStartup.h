#pragma once

class QApplication;
class CommandLineOptions;

// 应用级启动装配：QApplication 创建前后的环境预置、组织信息 / 版本 / 翻译 /
// 图标 / 全局样式表。命令行分支的执行不在此处。
namespace AppStartup {

// QApplication 创建之前调用：自检模式下设置离屏平台与自检配置目录。
void prepareEnvironment(const CommandLineOptions& options);

// QApplication 创建之后调用：组织名、版本、翻译装载、窗口图标与全局样式表。
void initialize(QApplication& app);

} // namespace AppStartup
