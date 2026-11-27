#pragma once

#include <QList>
#include <QObject>
#include <QString>

class CanvasScene;
class QWidget;

// 图层动作：可见性、锁定、增删改与排序。动作只调用场景的撤销接口并回报状态文案，
// 列表选择、刷新调度与按钮状态仍由图层面板负责。
class LayerActions : public QObject {
    Q_OBJECT
  public:
    explicit LayerActions(CanvasScene* scene, QObject* parent = nullptr);
    void SetScene(CanvasScene* scene) { m_scene = scene; }

    void SetLayerVisible(int layerId, bool visible);
    void SetAllLayersVisible(bool visible);
    void SetLayerLocked(int layerId, bool locked);
    void AddLayer();
    void RemoveLayer(QWidget* dialogParent, const QList<int>& layerIds);
    void RenameLayer(QWidget* dialogParent, int layerId);
    void MoveLayer(int delta);

  signals:
    void statusMessage(const QString& message);
    void refreshRequested();

  private:
    CanvasScene* m_scene = nullptr;
};
