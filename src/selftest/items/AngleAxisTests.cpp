#include "items/CanvasTypes.h"
#include "items/lines/AngleItem.h"
#include "items/properties/ItemPropertyCatalog.h"
#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"

#include <QColor>
#include <QPointF>
#include <QSizeF>
#include <QVariant>

// 角度线与弧线同源，半轴行在编辑面板上共用同一组属性键；这里守住三条：
// 属性表要上报这对键、面板走属性表写入要真的改到几何、改完回显与写入值一致。
void selftest::RunAngleAxisTests(Context& ctx)
{
    AngleItem angle;
    angle.SetAngleGeometry(QPointF(120.0, 90.0), QSizeF(128.0, 128.0), 30.0, 120.0,
                           QColor(QStringLiteral("#1E293B")), 3.0);

    const ItemPropertySet properties = angle.Properties();
    const qreal majorBefore = angle.PropertyValue(ItemProperty::MAJOR_AXIS).toDouble();
    const qreal minorBefore = angle.PropertyValue(ItemProperty::MINOR_AXIS).toDouble();
    ctx.Check(properties.Contains(ItemProperty::MAJOR_AXIS) && properties.Contains(ItemProperty::MINOR_AXIS),
              QStringLiteral("角度线上报长短半轴属性"), QStringLiteral("major=%1 minor=%2").arg(majorBefore).arg(minorBefore));
    ctx.Check(qAbs(majorBefore - 64.0) < 0.5 && qAbs(minorBefore - 64.0) < 0.5,
              QStringLiteral("角度线半轴按 128px 框回显为 64/64"),
              QStringLiteral("major=%1 minor=%2").arg(majorBefore).arg(minorBefore));

    // 编辑面板的半轴行走 SetPropertyValueByKey → 属性表 writer，不是图元自己的重载。
    angle.SetPropertyValueByKey(PropertyKey(ItemProperty::MAJOR_AXIS), 200.0);
    const qreal majorAfter = angle.PropertyValue(ItemProperty::MAJOR_AXIS).toDouble();
    ctx.Check(qAbs(majorAfter - 200.0) < 1.0, QStringLiteral("属性表写入长半轴后回显 200"),
              QStringLiteral("major=%1").arg(majorAfter));

    angle.SetPropertyValueByKey(PropertyKey(ItemProperty::MINOR_AXIS), 40.0);
    const qreal majorKept = angle.PropertyValue(ItemProperty::MAJOR_AXIS).toDouble();
    const qreal minorAfter = angle.PropertyValue(ItemProperty::MINOR_AXIS).toDouble();
    ctx.Check(qAbs(minorAfter - 40.0) < 1.0 && qAbs(majorKept - 200.0) < 1.0,
              QStringLiteral("属性表写入短半轴不改变长半轴"),
              QStringLiteral("major=%1 minor=%2").arg(majorKept).arg(minorAfter));
}
