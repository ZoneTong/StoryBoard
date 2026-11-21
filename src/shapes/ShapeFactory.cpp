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

ShapeItem* CreateShapeItem(ShapeType type)
{
    switch (type) {
    case ShapeType::ELLIPSE:
        return new EllipseItem();
    case ShapeType::TRIANGLE:
        return new TriangleItem();
    case ShapeType::STAR:
        return new StarItem();
    case ShapeType::ARROW:
        return new ArrowItem();
    case ShapeType::DOUBLE_ARROW:
        return new DoubleArrowItem();
    case ShapeType::CALLOUT:
        return new CalloutItem();
    case ShapeType::SECTOR:
        return new SectorItem();
    case ShapeType::STAR4:
        return new Star4Item();
    case ShapeType::HEXAGRAM:
        return new HexagramItem();
    case ShapeType::HEART:
        return new HeartItem();
    case ShapeType::FLAG_WAVE:
        return new FlagWaveItem();
    case ShapeType::SCROLL:
        return new ScrollItem();
    case ShapeType::CUBE:
        return new CubeItem();
    case ShapeType::CYLINDER:
        return new CylinderItem();
    case ShapeType::PARALLELOGRAM:
        return new ParallelogramItem();
    case ShapeType::DIAMOND:
        return new DiamondItem();
    case ShapeType::RECTANGLE:
    case ShapeType::POLYGON:
    default:
        return new RectangleItem();
    }
}
