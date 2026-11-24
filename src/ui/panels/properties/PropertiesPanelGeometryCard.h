#pragma once

#include <QPointF>
#include <QPointer>
#include <QSizeF>
#include <QWidget>

class CanvasItem;
class NumberValueRow;
class PairValueRow;

// 编辑面板的旋转 / 位置 / 尺寸 / 中心点行：成对数值行与单个数值行复用
// ValueRows，尺寸和位置一次性写入同一条撤销记录。半轴、起止角、内角等
// 成对属性行已并入数值卡片，这里只留纯位置类字段。
class PropertiesPanelGeometryCard : public QWidget {
    Q_OBJECT

  public:
    explicit PropertiesPanelGeometryCard(QWidget* parent = nullptr);

    void Refresh(CanvasItem* item);
    void SetCenterVisible(bool visible);

  signals:
    void geometryChosen(const QPointF& position, const QSizeF& size);
    void rotationChosen(qreal angle);
    // 中心点写回只平移位置：多选时绝对对齐到同一中心。
    void centerChosen(const QPointF& center);

  private:
    PairValueRow* m_positionRow = nullptr;
    PairValueRow* m_sizeRow = nullptr;
    NumberValueRow* m_rotationRow = nullptr;
    PairValueRow* m_centerRow = nullptr;
    QPointer<CanvasItem> m_item;
    bool m_refreshing = false;
};
