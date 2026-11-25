#include "items/shapes/ShapeFactory.h"

#include "items/shapes/ArrowItem.h"
#include "items/shapes/CalloutItem.h"
#include "items/shapes/CubeItem.h"
#include "items/shapes/CylinderItem.h"
#include "items/shapes/DiamondItem.h"
#include "items/shapes/DoubleArrowItem.h"
#include "items/shapes/EllipseItem.h"
#include "items/shapes/FlagWaveItem.h"
#include "items/shapes/GeneralParallelogramItem.h"
#include "items/shapes/HeartItem.h"
#include "items/shapes/HexagonItem.h"
#include "items/shapes/HexagramItem.h"
#include "items/shapes/RectangleItem.h"
#include "items/shapes/ScrollItem.h"
#include "items/shapes/SectorItem.h"
#include "items/shapes/Star4Item.h"
#include "items/shapes/StarItem.h"
#include "items/shapes/TriangleItem.h"

// 描述器与创建器同表：JSON 字符串、兼容别名、面积 / 中心点能力、菜单顺序都只有这一份。
// menuOrder 即「常见图形」的默认顺序（需求 7）；-1 表示该图形不进工具栏顺位。

namespace ShapeFactory {

const QList<Entry>& Entries()
{
    static const QList<Entry> ENTRIES{
        {{ShapeType::RECTANGLE, "rectangle", "rect rounded_rectangle roundedrect roundrect", true, true, 0},
         []() -> ShapeAbstractItem* { return new RectangleItem(); }},
        {{ShapeType::ELLIPSE, "ellipse", "circle oval", true, true, 1},
         []() -> ShapeAbstractItem* { return new EllipseItem(); }},
        // 需求：空心箭头不再进「常见图形」，箭头改由直线箭头预置项承担；
        // 类型本身保留，旧画布里的 arrow 仍能读回、也能互相转换。
        {{ShapeType::ARROW, "arrow", nullptr, true, false, -1}, []() -> ShapeAbstractItem* { return new ArrowItem(); }},
        {{ShapeType::DOUBLE_ARROW, "double_arrow", "doublearrow bidirectional", true, true, 3},
         []() -> ShapeAbstractItem* { return new DoubleArrowItem(); }},
        {{ShapeType::TRIANGLE, "triangle", nullptr, true, false, 4},
         []() -> ShapeAbstractItem* { return new TriangleItem(); }},
        {{ShapeType::CALLOUT, "callout", "bubble speech", true, false, 5},
         []() -> ShapeAbstractItem* { return new CalloutItem(); }},
        {{ShapeType::PARALLELOGRAM, "parallelogram", "rhomboid", true, true, 6},
         []() -> ShapeAbstractItem* { return new GeneralParallelogramItem(); }},
        {{ShapeType::DIAMOND, "diamond", "rhombus", true, true, 7},
         []() -> ShapeAbstractItem* { return new DiamondItem(); }},
        {{ShapeType::HEXAGON, "hexagon", "regular_hexagon", true, true, 8},
         []() -> ShapeAbstractItem* { return new HexagonItem(); }},
        {{ShapeType::SECTOR, "sector", "pie", true, true, 9}, []() -> ShapeAbstractItem* { return new SectorItem(); }},
        {{ShapeType::CUBE, "cube", nullptr, true, false, 18}, []() -> ShapeAbstractItem* { return new CubeItem(); }},
        {{ShapeType::CYLINDER, "cylinder", nullptr, true, false, 19},
         []() -> ShapeAbstractItem* { return new CylinderItem(); }},
        {{ShapeType::STAR4, "star4", "four_point_star fourpointstar", true, true, 12},
         []() -> ShapeAbstractItem* { return new Star4Item(); }},
        {{ShapeType::STAR, "star", nullptr, true, false, 13}, []() -> ShapeAbstractItem* { return new StarItem(); }},
        {{ShapeType::HEXAGRAM, "hexagram", "david_star six_point_star", true, true, 14},
         []() -> ShapeAbstractItem* { return new HexagramItem(); }},
        {{ShapeType::HEART, "heart", "love", true, false, 15}, []() -> ShapeAbstractItem* { return new HeartItem(); }},
        {{ShapeType::FLAG_WAVE, "flag_wave", "wavy_flag", true, false, 16},
         []() -> ShapeAbstractItem* { return new FlagWaveItem(); }},
        // 需求 3：卷形旗帜替换为卷轴；旧文件的 flag_curl / banner 兼容读入。
        {{ShapeType::SCROLL, "scroll", "flag_curl banner", true, true, 17},
         []() -> ShapeAbstractItem* { return new ScrollItem(); }},
    };
    return ENTRIES;
}

const Descriptor* Find(ShapeType type)
{
    for (const Entry& entry : Entries()) {
        if (entry.descriptor.type == type) {
            return &entry.descriptor;
        }
    }
    return nullptr;
}

ShapeAbstractItem* Create(ShapeType type)
{
    for (const Entry& entry : Entries()) {
        if (entry.descriptor.type == type && entry.create) {
            return entry.create();
        }
    }
    // 未知类型（POLYGON 占位或旧文件）退回矩形。
    return new RectangleItem();
}

} // namespace ShapeFactory
