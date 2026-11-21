#pragma once

#include "CanvasItem.h"

#include <QPointer>
#include <QWidget>

class CanvasScene;
class QComboBox;
class QFormLayout;
class QLineEdit;
class QSpinBox;
class TextAttributeRow;
class LayerAttributeRow;
class HeightAttributeRow;

// 属性窗的名称 / 图层 / 层内高度区块：名称可编辑，图层下拉按画布图层填充，
// 高度上限随目标图层现有高度联动；三行直接复用编辑面板的 PropertyRowWidgets，
// 属性窗只把图标关掉，控件规格与编辑面板保持一致。
class ItemPropertiesDialogIdentityBlock : public QWidget {
    Q_OBJECT

  public:
    ItemPropertiesDialogIdentityBlock(CanvasScene* scene, CanvasItem* item, QWidget* parent = nullptr);

    void AddRows(QFormLayout* form);

    QLineEdit* NameEdit() const { return m_nameEdit; }
    QComboBox* LayerCombo() const { return m_layerCombo; }
    QSpinBox* HeightSpin() const { return m_heightSpin; }
    bool HasLayerControls() const { return m_layerCombo && m_heightSpin; }

    void UpdateHeightRange(bool selectTop);
    void SetInitialized(bool initialized) { m_initializing = initialized; }

  signals:
    void layerChanged();

  private:
    CanvasScene* m_scene = nullptr;
    QPointer<CanvasItem> m_item;
    TextAttributeRow* m_nameRow = nullptr;
    LayerAttributeRow* m_layerRow = nullptr;
    HeightAttributeRow* m_heightRow = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QComboBox* m_layerCombo = nullptr;
    QSpinBox* m_heightSpin = nullptr;
    bool m_initializing = true;
};
