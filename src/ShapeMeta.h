#pragma once

#include "ShapeAbstractItem.h"

#include <QList>
#include <QString>

// 形状 / 填充样式的元数据映射：枚举与持久化字符串互转、默认尺寸、是否有面积 /
// 是否纯描边。不含界面文案（带翻译上下文的名称仍由 ShapeAbstractItem 侧提供）。
class ShapeMeta {
  public:
    static QList<ShapeType> AllTypes();
    static QString TypeToString(ShapeType type);
    static ShapeType TypeFromString(const QString& value);
    static bool IsFilled(ShapeType type);

    // 需求 4：线条类图元（零面积、只有一条开放描线）两端可设箭头样式。

    static QList<FillStyle> AllStyles();
    static QString StyleToString(FillStyle style);
    static FillStyle StyleFromString(const QString& value);
};
