#pragma once

#include <QList>
#include <QSet>
#include <QString>
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

// 属性键：公共属性占用枚举 0–29 的固定 ID，图元专属参数由注册表按稳定字符串分配
// 256 之后的 ID。两者共用一套集合与命令快照，新增参数不再改 ItemProperty 枚举。
class PropertyKey {
  public:
    PropertyKey() = default;
    // 公共枚举到键的隐式转换，既有调用点不必逐处改写。
    PropertyKey(ItemProperty property) : m_id(static_cast<int>(property)) {}
    explicit PropertyKey(int id) : m_id(id) {}

    // 按稳定字符串取键；首次出现时分配新 ID，同一字符串在进程内恒定。
    static PropertyKey FromStorageKey(const QString& storageKey);
    static int AllocatedCount();

    bool IsValid() const { return m_id >= 0; }
    bool IsCommon() const;
    ItemProperty CommonProperty() const;
    int Id() const { return m_id; }
    QString StorageKey() const;

    bool operator==(const PropertyKey& other) const { return m_id == other.m_id; }
    bool operator!=(const PropertyKey& other) const { return m_id != other.m_id; }
    bool operator<(const PropertyKey& other) const { return m_id < other.m_id; }

  private:
    int m_id = -1;
};

// 属性的轻量集合：内部按整数键存储，公共属性与动态属性同集合，不再受 64 位上限约束。
// 求交 / 并集语义与面板显隐一一对应，避免各处再写一份循环。
class ItemPropertySet {
  public:
    ItemPropertySet() = default;
    ItemPropertySet(std::initializer_list<ItemProperty> values);
    ItemPropertySet(std::initializer_list<PropertyKey> values);

    void Insert(ItemProperty property);
    void Insert(PropertyKey property);
    bool Contains(ItemProperty property) const;
    bool Contains(PropertyKey property) const;
    bool IsEmpty() const { return m_keys.isEmpty(); }
    int Size() const { return m_keys.size(); }

    ItemPropertySet United(const ItemPropertySet& other) const;
    ItemPropertySet Intersected(const ItemPropertySet& other) const;

    // 按枚举声明顺序返回公共属性，面板与属性窗直接照此排列行。
    QList<ItemProperty> Values() const;
    // 公共属性在前、动态属性按分配顺序在后；命令快照与属性集合求交使用这一份。
    QList<PropertyKey> Keys() const;

  private:
    QSet<int> m_keys;
};
