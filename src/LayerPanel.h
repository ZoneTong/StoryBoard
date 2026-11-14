#pragma once

#include "CanvasTypes.h"

#include <QList>
#include <QWidget>

class CanvasScene;
class LayerActions;
class LayerPanelView;
class LayerRow;
class QCheckBox;
class QLabel;
class QListWidget;
class QToolButton;

class LayerPanel : public QWidget {
    Q_OBJECT
  public:
    explicit LayerPanel(CanvasScene* scene, QWidget* parent = nullptr);

    QList<int> selectedLayerIds() const;

  public slots:
    void refresh();

  signals:
    void statusMessage(const QString& message);

  private:
    LayerRow* createRow(const CanvasLayer& layer, int index, int itemCount);
    void scheduleRefresh();
    void activateCurrentLayer();
    void syncActiveLayer();
    void syncSceneSelection();
    void removeSelectedLayers();

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
