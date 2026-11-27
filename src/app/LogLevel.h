#pragma once

// 日志详细程度：数值越大放出的消息越多，配置的级别是“上限”。
// LOG_ERROR / LOG_WARNING 对应 Qt 的错误与告警；LOG_EVENT 是图元创建、销毁与属性变化；
// LOG_INFO 是画布新建、打开、保存等成功操作；LOG_DEBUG 是开发期调试输出。
enum class LogLevel { LOG_ERROR = 0, LOG_WARNING = 1, LOG_EVENT = 2, LOG_INFO = 3, LOG_DEBUG = 4 };
