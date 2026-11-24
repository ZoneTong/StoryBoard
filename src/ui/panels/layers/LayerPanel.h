#pragma once

#include "items/CanvasTypes.h"
#include "ui/panels/layers/LayerTreeModel.h"

#include <QHash>
#include <QList>
#include <QListWidget>
#include <QPointer>
#include <QWidget>

class CanvasItem;
class CanvasScene;
class LayerActions;
class LayerPanelView;
class LayerRow;
class QCheckBox;
class QDropEvent;
class QLabel;
class QToolButton;

class LayerPanel : public QWidget {
    Q_OBJECT
  public:
    explicit LayerPanel(CanvasScene* scene, QWidget* parent = nullptr);

    QList<int> SelectedLayerIds() const;

    // 行数据角色：UserRole 存图层 / 组 id（图元行固定 -1），ROLE_ROW_TYPE 区分三种行。
    static constexpr int ROLE_ROW_TYPE = Qt::UserRole + 1;
    enum RowType { ROW_LAYER = 0, ROW_GROUP = 1, ROW_ITEM = 2 };

  public slots:
    void Refresh();

  signals:
    void statusMessage(const QString& message);

  private:
    LayerRow* CreateRow(const CanvasLayer& layer, int index, int itemCount);
    QListWidgetItem* CreateGroupRow(const LayerTreeRow& spec);
    QListWidgetItem* CreateItemRow(const LayerTreeRow& spec);
    void ScheduleRefresh();
    void ActivateCurrentLayer();
    void SyncActiveLayer();
    void SyncSceneSelection();
    void OnSelectionChanged();
    void ApplyPanelSelectionToScene(const QList<CanvasItem*>& items);
    void MirrorItemRowsFromScene();
    void ClearItemRowSelection();
    void RevealSelectedItemRow();
    QList<CanvasItem*> SelectedRowItems() const;
    QList<int> PreferredLayerIdsFromSelection() const;
    void RemoveSelectedLayers();
    void OnRowClicked(QListWidgetItem* item);
    void RenameGroup(int groupId);
    bool HandleLayerDrop(QDropEvent* event);
    int OwnerLayerRow(int row) const;

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

    CanvasScene* m_scene = nullptr;
    LayerActions* m_actions = nullptr;
    LayerPanelView* m_view = nullptr;
    QListWidget* m_list = nullptr;
    QCheckBox* m_allVisibleBox = nullptr;
    QCheckBox* m_groupByGroupBox = nullptr;
    QToolButton* m_addButton = nullptr;
    QToolButton* m_removeButton = nullptr;
    QToolButton* m_upButton = nullptr;
    QToolButton* m_downButton = nullptr;
    QLabel* m_summaryLabel = nullptr;
    // 图元行 → 图元。QPointer 在行存活期内自动置空，图元被删时不会留下悬垂指针。
    QHash<QListWidgetItem*, QPointer<CanvasItem>> m_itemRows;
    bool m_refreshing = false;
    bool m_syncingScene = false;
    bool m_refreshPending = false;
};
