#pragma once

#include <QString>

// 进程日志：接管 qDebug / qInfo / qWarning / qCritical，按天写进配置目录下的 logs/。
// 只依赖 Qt，不引入 spdlog：项目里本来就用 Qt 日志宏，接管后既有调用点零改动；
// 启动时顺手删掉 14 天前的日志，长期运行也不会无限增长。
namespace LogService {

// 安装消息处理器并做一次过期清理；可重复调用（后一次替换前一次）。
void Install();
// 当前日志目录（配置目录/logs），安装前后都可查询。
QString LogDirectory();
// 保留天数，默认 14；自检可传临时目录与更短周期。
void SetRetentionDaysForTest(int days);
// 立即清理一次过期日志，返回删除的文件数。
int PruneOldLogs();

} // namespace LogService
