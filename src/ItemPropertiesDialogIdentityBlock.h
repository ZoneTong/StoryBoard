#pragma once

#include "CanvasItem.h"

#include <QPointer>
#include <QWidget>

class CanvasScene;
class QComboBox;
class QFormLayout;
class QLineEdit;
class QSpinBox;

// 属性窗的名称 / 图层 / 层内高度区块：名称可编辑，图层下拉按画布图层填充，
// 高度上限随目标图层现有高度联动；层级切换后由外壳决定是否同步自然尺寸。
class ItemPropertiesDialogIdentityBlock : public QWidget {
    Q_OBJECT

  public:
    ItemPropertiesDialogIdentityBlock(CanvasScene* scene, CanvasItem* item, QWidget* parent = nullptr);

    void addRows(QFormLayout* form);

    QLineEdit* nameEdit() const { return m_nameEdit; }
    QComboBox* layerCombo() const { return m_layerCombo; }
    QSpinBox* heightSpin() const { return m_heightSpin; }
    bool hasLayerControls() const { return m_layerCombo && m_heightSpin; }

    void updateHeightRange(bool selectTop);
    void setInitialized(bool initialized) { m_initializing = initialized; }

  signals:
    void layerChanged();

  private:
    CanvasScene* m_scene = nullptr;
    QPointer<CanvasItem> m_item;
    QLineEdit* m_nameEdit = nullptr;
    QComboBox* m_layerCombo = nullptr;
    QSpinBox* m_heightSpin = nullptr;
    bool m_initializing = true;
};
