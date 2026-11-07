#pragma once

#include <QWidget>

class BoardScene;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QToolButton;

class LayerPanel : public QWidget
{
    Q_OBJECT
public:
    explicit LayerPanel(BoardScene* scene, QWidget* parent = nullptr);

public slots:
    void refresh();

signals:
    void statusMessage(const QString& message);

private:
    int selectedLayerId() const;
    void activateCurrentLayer();
    void syncActiveLayer();
    void addLayer();
    void removeLayer();
    void moveLayer(int delta);
    void toggleLayerVisibility(QListWidgetItem* item);
    void updateButtons();

    BoardScene* m_scene = nullptr;
    QListWidget* m_list = nullptr;
    QToolButton* m_addButton = nullptr;
    QToolButton* m_removeButton = nullptr;
    QToolButton* m_upButton = nullptr;
    QToolButton* m_downButton = nullptr;
    QLabel* m_summaryLabel = nullptr;
    bool m_refreshing = false;
};
