#include "shapes/ShapeRegistry.h"

#include "shapes/ArrowItem.h"
#include "shapes/CalloutItem.h"
#include "shapes/CubeItem.h"
#include "shapes/CylinderItem.h"
#include "shapes/DiamondItem.h"
#include "shapes/DoubleArrowItem.h"
#include "shapes/EllipseItem.h"
#include "shapes/FlagWaveItem.h"
#include "shapes/GeneralParallelogramItem.h"
#include "shapes/HeartItem.h"
#include "shapes/HexagramItem.h"
#include "shapes/RectangleItem.h"
#include "shapes/ScrollItem.h"
#include "shapes/SectorItem.h"
#include "shapes/Star4Item.h"
#include "shapes/StarItem.h"
#include "shapes/TriangleItem.h"

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

