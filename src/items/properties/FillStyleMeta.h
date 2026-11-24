#pragma once

#include "items/properties/FillAttributes.h"

#include <QList>
#include <QString>

// 填充样式的元数据注册表：枚举、持久化串、旧画布别名、界面源文案与「是否图案填充」
// 集中在这一张表。新增 / 删除样式只改本表；图标与绘制仍是按枚举分支的过程式代码，
// 归 ItemStyleVisuals / ShapePainter 各自维护。
namespace FillStyleMeta {

struct Entry {
    FillStyle style;
    // 序列化写入的规范串；读取时规范串与 aliases 等价识别。
    QString key;
    // 界面英文源文案；翻译仍走 ShapeAbstractItem 上下文，调用方负责 tr。
    QString nameSource;
    // 主窗口菜单 / 工具栏动作的英文源文案（带 “ fill” 后缀，MainWindow 上下文）。
    QString actionSource;
    // 除纯色 / 无填充外，其余都是带线条或点阵的图案填充。
    bool isPattern;
    QStringList aliases;
};

const QList<Entry>& Registry();
QList<FillStyle> Styles();
QString ToString(FillStyle style);
FillStyle FromString(const QString& value);
bool IsPattern(FillStyle style);
QString NameSource(FillStyle style);
QString ActionSource(FillStyle style);

} // namespace FillStyleMeta
