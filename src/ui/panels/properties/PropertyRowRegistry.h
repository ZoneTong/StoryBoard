#pragma once

#include "items/properties/ItemProperties.h"

#include <QList>

class QFormLayout;
class QWidget;
struct PropertyRowSpec;

// 属性窗的行注册表（§4.3）：一行按一个（或多个互斥）ItemProperty 建，
// 显隐只由图元上报的 Properties() 决定，不再按 shapeType 拼布尔开关。
// 所有行在构造时一次建齐并入表，换绑图元时整表重算一次。
class PropertyRowRegistry {
  public:
    // 同一行可接受多个属性：命中任意一个即显示。内角 A 就是三角形底角 A
    // 与平行四边形内角共用一行，两者互斥、不会同时出现。
    void Register(ItemProperty property, QWidget* field);
    void Register(const ItemPropertySet& accepted, QWidget* field);
    // 行描述已带可接受集合，注册时不再在调用点重复拼 ItemPropertySet。
    void Register(const PropertyRowSpec& spec, QWidget* field);

    // 按属性集合重排显隐；未注册的行保持调用前的状态。
    void Apply(QFormLayout* form, const ItemPropertySet& properties);

    int RowCount() const { return m_rows.size(); }

  private:
    struct Row {
        ItemPropertySet accepted;
        QWidget* field = nullptr;
    };

    QList<Row> m_rows;
};
