#include "shapes/ShapeFactory.h"

#include "shapes/ArrowItem.h"
#include "shapes/CalloutItem.h"
#include "shapes/CubeItem.h"
#include "shapes/CylinderItem.h"
#include "shapes/DiamondItem.h"
#include "shapes/DoubleArrowItem.h"
#include "shapes/EllipseItem.h"
#include "shapes/FlagWaveItem.h"
#include "shapes/HeartItem.h"
#include "shapes/HexagramItem.h"
#include "shapes/ParallelogramItem.h"
#include "shapes/RectangleItem.h"
#include "shapes/ScrollItem.h"
#include "shapes/SectorItem.h"
#include "shapes/Star4Item.h"
#include "shapes/StarItem.h"
#include "shapes/TriangleItem.h"

ShapeItem* createShapeItem(ShapeType type)
{
    switch (type) {
    case ShapeType::Ellipse:
        return new EllipseItem();
    case ShapeType::Triangle:
        return new TriangleItem();
    case ShapeType::Diamond:
        return new DiamondItem();
    case ShapeType::Star:
        return new StarItem();
    case ShapeType::Arrow:
        return new ArrowItem();
    case ShapeType::DoubleArrow:
        return new DoubleArrowItem();
    case ShapeType::Callout:
        return new CalloutItem();
    case ShapeType::Sector:
        return new SectorItem();
    case ShapeType::Star4:
        return new Star4Item();
    case ShapeType::Hexagram:
        return new HexagramItem();
    case ShapeType::Heart:
        return new HeartItem();
    case ShapeType::FlagWave:
        return new FlagWaveItem();
    case ShapeType::Scroll:
        return new ScrollItem();
    case ShapeType::Cube:
        return new CubeItem();
    case ShapeType::Cylinder:
        return new CylinderItem();
    case ShapeType::Parallelogram:
        return new ParallelogramItem();
    case ShapeType::Rectangle:
    case ShapeType::Polygon:
    default:
        return new RectangleItem();
    }
}
