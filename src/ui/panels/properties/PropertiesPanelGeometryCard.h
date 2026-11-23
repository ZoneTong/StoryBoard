#pragma once

#include <QPointF>
#include <QPointer>
#include <QSizeF>
#include <QWidget>

class CanvasItem;
class NumberValueRow;
class PairValueRow;

// 编辑面板的长短半轴 / 旋转 / 位置 / 尺寸 / 中心点 / 内角 A 行：
// 属性窗几何区的同一批字段在面板里也能直接改，成对数值行与单个数值行复用
// PropertyRowWidgets，尺寸和位置一次性写入同一条撤销记录。
class PropertiesPanelGeometryCard : public QWidget {
    Q_OBJECT

  public:
    explicit PropertiesPanelGeometryCard(QWidget* parent = nullptr);

    void Refresh(CanvasItem* item);
    // 椭圆 / 扇形 / 弧线才显示长短半轴；平行四边形族才显示内角 A；对称图形显示中心点。
    void SetAxisVisible(bool visible);
    void SetInteriorAngleVisible(bool visible);
    void SetCenterVisible(bool visible);

  signals:
    void geometryChosen(const QPointF& position, const QSizeF& size);
    void rotationChosen(qreal angle);
    // 长短半轴统一走属性写入：椭圆 / 扇形换算尺寸，弧线按真实椭圆框重采样。
    void axesChosen(qreal majorAxis, qreal minorAxis);
    void interiorAngleChosen(qreal angle);
    // 中心点写回只平移位置：多选时绝对对齐到同一中心。
    void centerChosen(const QPointF& center);

  private:
    void ApplyAxes(bool majorChanged);

    PairValueRow* m_positionRow = nullptr;
    PairValueRow* m_sizeRow = nullptr;
    NumberValueRow* m_rotationRow = nullptr;
    NumberValueRow* m_majorAxisRow = nullptr;
    NumberValueRow* m_minorAxisRow = nullptr;
    NumberValueRow* m_interiorAngleRow = nullptr;
    PairValueRow* m_centerRow = nullptr;
    QPointer<CanvasItem> m_item;
    bool m_refreshing = false;
};
