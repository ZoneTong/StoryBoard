#pragma once

#include <QList>

class CanvasScene;
class QCheckBox;
class QLabel;
class QListWidget;
class QToolButton;

// 图层面板的只读视图状态：行选中样式、全显勾选、按钮可用态与摘要文案。
// 只读取场景与列表控件并刷新显示，不持有可见性、锁定、排序、增删改等编辑动作。
class LayerPanelView {
  public:
    LayerPanelView(CanvasScene* scene, QListWidget* list, QCheckBox* allVisibleBox, QToolButton* addButton,
                   QToolButton* removeButton, QToolButton* upButton, QToolButton* downButton, QLabel* summaryLabel);

    int currentLayerId() const;
    QList<int> selectedLayerIds() const;

    void refreshRowStyles();
    void refreshAllVisibleState();
    void refreshButtons();
    void refreshSummary();

  private:
    CanvasScene* m_scene = nullptr;
    QListWidget* m_list = nullptr;
    QCheckBox* m_allVisibleBox = nullptr;
    QToolButton* m_addButton = nullptr;
    QToolButton* m_removeButton = nullptr;
    QToolButton* m_upButton = nullptr;
    QToolButton* m_downButton = nullptr;
    QLabel* m_summaryLabel = nullptr;
};
