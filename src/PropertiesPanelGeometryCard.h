#pragma once

#include <QPointF>
#include <QPointer>
#include <QSizeF>
#include <QWidget>

class CanvasItem;
class NumberAttributeRow;
class PairAttributeRow;

// 编辑面板的旋转 / 位置 / 尺寸 / 长短半轴 / 平行四边形内角行：
// 属性窗几何区的同一批字段在面板里也能直接改，成对数值行与单个数值行复用
// PropertyRowWidgets，尺寸和位置一次性写入同一条撤销记录。
class PropertiesPanelGeometryCard : public QWidget {
    Q_OBJECT

  public:
    explicit PropertiesPanelGeometryCard(QWidget* parent = nullptr);

    void Refresh(CanvasItem* item);
    // 椭圆 / 扇形才显示长短半轴；平行四边形才显示内角。
    void SetAxisVisible(bool visible);
    void SetParallelogramVisible(bool visible);

  signals:
    void geometryChosen(const QPointF& position, const QSizeF& size);
    void rotationChosen(qreal angle);
    void parallelogramAngleChosen(qreal angle);

  private:
    void ApplyAxes(bool majorChanged);

    PairAttributeRow* m_positionRow = nullptr;
    PairAttributeRow* m_sizeRow = nullptr;
    NumberAttributeRow* m_rotationRow = nullptr;
    NumberAttributeRow* m_majorAxisRow = nullptr;
    NumberAttributeRow* m_minorAxisRow = nullptr;
    NumberAttributeRow* m_parallelogramRow = nullptr;
    QPointer<CanvasItem> m_item;
    bool m_refreshing = false;
};
