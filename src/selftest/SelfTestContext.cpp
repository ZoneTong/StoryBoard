#include "selftest/SelfTestContext.h"

namespace selftest {

void Context::Check(bool condition, const QString& name, const QString& detail)
{
    if (condition) {
        lines.append(QStringLiteral("PASS  %1").arg(name));
    } else {
        ++failures;
        lines.append(QStringLiteral("FAIL  %1%2")
                         .arg(name, detail.isEmpty() ? QString() : QStringLiteral("  [") + detail + QLatin1Char(']')));
    }
    if (progressHook) {
        progressHook();
    }
}

} // namespace selftest
