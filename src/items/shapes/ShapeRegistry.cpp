#include "items/shapes/ShapeRegistry.h"

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
#include "items/shapes/HexagramItem.h"
#include "items/shapes/RectangleItem.h"
#include "items/shapes/ScrollItem.h"
#include "items/shapes/SectorItem.h"
#include "items/shapes/Star4Item.h"
#include "items/shapes/StarItem.h"
#include "items/shapes/TriangleItem.h"

const QList<ShapeRegistry::Entry>& ShapeRegistry::Entries()
{
    // 唯一一份“类型 → 创建器”映射：顺序即默认插入顺序，与 ShapeMeta::AllTypes 无关。
    static const QList<Entry> ENTRIES{
        {ShapeType::RECTANGLE, []() -> ShapeAbstractItem* { return new RectangleItem(); }},
        {ShapeType::ELLIPSE, []() -> ShapeAbstractItem* { return new EllipseItem(); }},
        {ShapeType::TRIANGLE, []() -> ShapeAbstractItem* { return new TriangleItem(); }},
        {ShapeType::STAR, []() -> ShapeAbstractItem* { return new StarItem(); }},
        {ShapeType::ARROW, []() -> ShapeAbstractItem* { return new ArrowItem(); }},
        {ShapeType::DOUBLE_ARROW, []() -> ShapeAbstractItem* { return new DoubleArrowItem(); }},
        {ShapeType::CALLOUT, []() -> ShapeAbstractItem* { return new CalloutItem(); }},
        {ShapeType::SECTOR, []() -> ShapeAbstractItem* { return new SectorItem(); }},
        {ShapeType::STAR4, []() -> ShapeAbstractItem* { return new Star4Item(); }},
        {ShapeType::HEXAGRAM, []() -> ShapeAbstractItem* { return new HexagramItem(); }},
        {ShapeType::HEART, []() -> ShapeAbstractItem* { return new HeartItem(); }},
        {ShapeType::FLAG_WAVE, []() -> ShapeAbstractItem* { return new FlagWaveItem(); }},
        {ShapeType::SCROLL, []() -> ShapeAbstractItem* { return new ScrollItem(); }},
        {ShapeType::CUBE, []() -> ShapeAbstractItem* { return new CubeItem(); }},
        {ShapeType::CYLINDER, []() -> ShapeAbstractItem* { return new CylinderItem(); }},
        {ShapeType::PARALLELOGRAM, []() -> ShapeAbstractItem* { return new GeneralParallelogramItem(); }},
        {ShapeType::DIAMOND, []() -> ShapeAbstractItem* { return new DiamondItem(); }},
    };
    return ENTRIES;
}

ShapeAbstractItem* ShapeRegistry::Create(ShapeType type)
{
    for (const Entry& entry : Entries()) {
        if (entry.type == type && entry.create) {
            return entry.create();
        }
    }
    // 未知类型（POLYGON 占位或旧文件）退回矩形。
    return new RectangleItem();
}
