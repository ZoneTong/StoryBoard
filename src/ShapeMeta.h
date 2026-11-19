#pragma once

#include "ShapeItem.h"

#include <QList>
#include <QSizeF>
#include <QString>

// 形状 / 填充样式的元数据映射：枚举与持久化字符串互转、默认尺寸、是否有面积 /
// 是否纯描边。不含界面文案（带翻译上下文的名称仍由 ShapeItem 侧提供）。
class ShapeMeta {
  public:
    static QList<ShapeType> allTypes();
    static QString typeToString(ShapeType type);
    static ShapeType typeFromString(const QString& value);
    static QSizeF defaultSize(ShapeType type);
    static bool isFilled(ShapeType type);
    static bool isStrokeOnly(ShapeType type);
    // 需求 4：线条类图元（零面积、只有一条开放描线）两端可设箭头样式。
    static bool isLineLike(ShapeType type);

    static QList<FillStyle> allStyles();
    static QString styleToString(FillStyle style);
    static FillStyle styleFromString(const QString& value);
};
