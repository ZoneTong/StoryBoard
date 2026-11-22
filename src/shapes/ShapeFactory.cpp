#include "shapes/ShapeFactory.h"

#include "shapes/ShapeRegistry.h"

ShapeAbstractItem* CreateShapeItem(ShapeType type)
{
    // 具体映射在 ShapeRegistry：工厂只做一次查表，不再按类型写 switch。
    return ShapeRegistry::Create(type);
}
