#pragma once

#include <QList>
#include <QVariant>
#include <QtGlobal>
#include <initializer_list>

// 图元属性的统一标识。顺序即编辑面板的行序（样式行在颜色行之前已固化），
// 各图元只上报自己真正拥有的属性，面板按选中集合求交后逐行显隐。
enum class ItemProperty {
    TEXT_CONTENT,
    TEXT_COLOR,
    TEXT_FONT,
    FILL_STYLE,
    FILL_COLOR,
    FILL_IMAGE,
    FILL_IMAGE_MODE,
    STROKE_STYLE,
    STROKE_COLOR,
    STROKE_WIDTH,
    OPACITY,
    CORNER_RADIUS,
    START_ANGLE,
    SWEEP_ANGLE,
    // 长短半轴是几何行：椭圆 / 扇形按内容尺寸派生，弧线按真实椭圆外接框读写。
    MAJOR_AXIS,
    MINOR_AXIS,
    SINE_CYCLES,
    SINE_AMPLITUDE,
    BOTTOM_ANGLE1,
    BOTTOM_ANGLE2,
    PARALLELOGRAM_ANGLE,
    ARROW_START,
    ARROW_END,
    NAME,
    LAYER,
    HEIGHT,
    POSITION,
    SIZE,
    ROTATION,
    // 中心点是派生属性：不落盘、不新增存储字段，读写只折算到位置。
    // 安排在枚举末尾，属性窗里固定为最后一行（第 28 行）。
    CENTER
};

// 属性的轻量集合：枚举值不足 64 个，用一个位集即可，省掉 QSet 的 qHash 依赖。
// 求交 / 并集语义与面板显隐一一对应，避免各处再写一份循环。
class ItemPropertySet {
  public:
    ItemPropertySet() = default;
    ItemPropertySet(std::initializer_list<ItemProperty> values);

    void Insert(ItemProperty property);
    bool Contains(ItemProperty property) const;
    bool IsEmpty() const { return m_bits == 0; }
    int Size() const;

    ItemPropertySet United(const ItemPropertySet& other) const;
    ItemPropertySet Intersected(const ItemPropertySet& other) const;

    // 按枚举声明顺序返回，面板与属性窗直接照此排列行。
    QList<ItemProperty> Values() const;

  private:
    quint64 m_bits = 0;
};
