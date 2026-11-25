#pragma once

#include <QPointer>
#include <QWidget>

class CanvasItem;
class CanvasScene;
class LayerHeightRow;
class QComboBox;
class TextValueRow;
class QWidget;

// 编辑面板的类型 / 名称 / 图层与层内高度三行：与属性窗的身份区同源，
// 直接给出目标值而不只是 ±1 微调；类型行只对扇形 / 弧线 / 角度线开放，
// 用于三型互换。多选时整块隐藏，避免一次把多个图元改成同一位置。
class PropertiesPanelIdentityCard : public QWidget {
    Q_OBJECT

  public:
    explicit PropertiesPanelIdentityCard(QWidget* parent = nullptr);

    void Refresh(CanvasScene* scene, CanvasItem* item);

  signals:
    void TypeChosen(const QString& kind);
    void nameChosen(const QString& name);
    void layerChosen(int layerId);
    void heightChosen(int height);

  private:
    QWidget* m_typeRow = nullptr;
    QComboBox* m_typeCombo = nullptr;
    TextValueRow* m_nameRow = nullptr;
    LayerHeightRow* m_layerRow = nullptr;
    CanvasScene* m_scene = nullptr;
    QPointer<CanvasItem> m_item;
    bool m_refreshing = false;
};
