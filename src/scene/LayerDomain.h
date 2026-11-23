#pragma once

#include "items/CanvasTypes.h"

#include <QList>

// 图层状态的唯一持有者：图层数组、活动图层与下一个图层号。
// CanvasScene 保留公共方法 facade；本类不碰图元与撤销栈。
class LayerDomain {
  public:
    void Reset();
    // 非 const 供 CanvasScene 的图层编辑方法原地修改；const 供查询。
    QList<CanvasLayer>& Layers() { return m_layers; }
    const QList<CanvasLayer>& Layers() const { return m_layers; }
    void SetLayers(const QList<CanvasLayer>& layers);

    int Count() const { return m_layers.size(); }
    bool HasLayer(int layerId) const;
    CanvasLayer Layer(int layerId) const;
    int Index(int layerId) const;
    QList<CanvasLayer> UserLayers() const;

    int ActiveLayerId() const { return m_activeLayerId; }
    void SetActiveLayerId(int layerId) { m_activeLayerId = layerId; }

    int NextId();
    void ObserveId(int layerId);
    // 载入 / 重建后重新扫描已有图层号前，把计数器收回起点。
    void ResetNextId() { m_nextLayerId = 1; }

  private:
    QList<CanvasLayer> m_layers;
    int m_activeLayerId = 1;
    int m_nextLayerId = 2;
};
