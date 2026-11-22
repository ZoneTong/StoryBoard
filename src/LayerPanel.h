#pragma once

#include "CanvasTypes.h"

#include <QList>
#include <QListWidget>
#include <QWidget>

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

    // 行数据角色：UserRole 存图层 / 组 id，ROLE_ROW_TYPE 区分图层行与组行。
    static constexpr int ROLE_ROW_TYPE = Qt::UserRole + 1;
    enum RowType { ROW_LAYER = 0, ROW_GROUP = 1 };

  public slots:
    void Refresh();

  signals:
    void statusMessage(const QString& message);

  private:
    LayerRow* CreateRow(const CanvasLayer& layer, int index, int itemCount);
    QListWidgetItem* CreateGroupRow(int groupId);
    void ScheduleRefresh();
    void ActivateCurrentLayer();
    void SyncActiveLayer();
    void SyncSceneSelection();
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
    QToolButton* m_addButton = nullptr;
    QToolButton* m_removeButton = nullptr;
    QToolButton* m_upButton = nullptr;
    QToolButton* m_downButton = nullptr;
    QLabel* m_summaryLabel = nullptr;
    bool m_refreshing = false;
    bool m_syncingScene = false;
    bool m_refreshPending = false;
};
