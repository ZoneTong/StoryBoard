#pragma once

#include <QPointer>
#include <QWidget>

class CanvasItem;
class CanvasScene;
class HeightAttributeRow;
class LayerAttributeRow;
class TextAttributeRow;

// 编辑面板的名称 / 图层 / 层内高度三行：与属性窗的身份区同源，
// 直接给出目标值而不只是 ±1 微调；多选时整块隐藏，避免一次把多个图元改成同一位置。
class PropertiesPanelIdentityCard : public QWidget {
    Q_OBJECT

  public:
    explicit PropertiesPanelIdentityCard(QWidget* parent = nullptr);

    void Refresh(CanvasScene* scene, CanvasItem* item);

  signals:
    void nameChosen(const QString& name);
    void layerChosen(int layerId);
    void heightChosen(int height);

  private:
    TextAttributeRow* m_nameRow = nullptr;
    LayerAttributeRow* m_layerRow = nullptr;
    HeightAttributeRow* m_heightRow = nullptr;
    CanvasScene* m_scene = nullptr;
    QPointer<CanvasItem> m_item;
    bool m_refreshing = false;
};
