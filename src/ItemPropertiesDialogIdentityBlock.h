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
// 三行在构造时一次建齐；SetItem 按图元回填并决定显隐（无图元时整块隐藏）。
class ItemPropertiesDialogIdentityBlock : public QWidget {
    Q_OBJECT

  public:
    ItemPropertiesDialogIdentityBlock(CanvasScene* scene, QWidget* parent = nullptr);

    void AddRows(QFormLayout* form);

    void SetItem(CanvasScene* scene, CanvasItem* item);

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
    QFormLayout* m_form = nullptr;
    TextAttributeRow* m_nameRow = nullptr;
    LayerAttributeRow* m_layerRow = nullptr;
    HeightAttributeRow* m_heightRow = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QComboBox* m_layerCombo = nullptr;
    QSpinBox* m_heightSpin = nullptr;
    bool m_initializing = true;
};
