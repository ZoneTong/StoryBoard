#pragma once

#include <QList>
#include <QWidget>

#include "BoardTypes.h"

class BoardScene;
class QCheckBox;
class QLabel;
class QListWidget;
class QToolButton;

class LayerPanel : public QWidget
{
    Q_OBJECT
public:
    explicit LayerPanel(BoardScene* scene, QWidget* parent = nullptr);

    QList<int> selectedLayerIds() const;

public slots:
    void refresh();

signals:
    void statusMessage(const QString& message);

private:
    QWidget* createLayerRow(const BoardLayer& layer, int index, int itemCount);
    int currentLayerId() const;
    void scheduleRefresh();
    void activateCurrentLayer();
    void syncActiveLayer();
    void syncSceneSelection();
    void addLayer();
    void removeLayer();
    void moveLayer(int delta);
    void setLayerVisible(int layerId, bool visible);
    void setLayerLocked(int layerId, bool locked);
    void setAllLayersVisible(bool visible);
    void updateAllVisibleState();
    void updateButtons();
    void updateRowStyles();
    void updateSummary();

    BoardScene* m_scene = nullptr;
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