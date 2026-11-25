#pragma once

#include <QString>
#include <QStringList>

class CanvasItem;

// 扇形 / 弧线 / 角度线三者互换：它们都派生自 SectorBase，起止角与椭圆框是同一套语义，
// 互换时把这部分几何与描线样式原样搬过去，只允许“进扇形丢箭头、进线型丢填充”这一处有损。
// 椭圆（EllipseItem）不在这套互换里：它没有起止角、端点手柄与读数。
namespace ItemTypeConversion {

// 当前类型：sector / arc / angle；其它图元返回空串。
QString TypeOf(const CanvasItem* item);
// 互换目标：除自身以外的另外两种（顺序固定为 弧线、角度线、扇形中的剩余项）。
QStringList TargetsOf(const CanvasItem* item);
// 类型的中文 / 界面显示名，供右键子菜单与提示使用。
QString DisplayNameOf(const QString& kind);
// 按 kind 造一个新图元并复制几何、角度、描线样式；不支持的目标返回 nullptr。
// 返回值尚未挂进场景，由调用方用 ConvertItemCommand 提交。
CanvasItem* CreateConverted(const CanvasItem* source, const QString& kind);

} // namespace ItemTypeConversion
