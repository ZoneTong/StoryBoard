#pragma once

#include <QGuiApplication>

namespace TestMode {

// 自检（离屏）模式在 qApp 上打的属性键；AppModes 启动自检时置位，各报错出口据此抑制模态弹窗。
inline constexpr char SELF_TEST_PROPERTY[] = "storyboardSelfTest";

// 当前进程是否跑在自检模式；qApp 未就绪时按非自检处理。
inline bool IsSelfTestMode()
{
    return qApp && qApp->property(SELF_TEST_PROPERTY).toBool();
}

} // namespace TestMode
