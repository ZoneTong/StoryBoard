#pragma once

#include "items/properties/ItemProperties.h"

#include <QList>
#include <QMap>
#include <QString>
#include <QVariant>

class CanvasItem;
class CanvasScene;

// 属性编辑的统一提交口：编辑面板与属性窗都从图元当前属性取 before，
// 差异写入 after 后由这里落成一条 ItemPropertyCommand；没有撤销栈时直接执行。
class PropertyCommitService {
  public:
    static QMap<PropertyKey, QVariant> Snapshot(const CanvasItem* item);
    static void Push(CanvasScene* scene, const QList<CanvasItem*>& targets,
                     const QList<QMap<PropertyKey, QVariant>>& before, const QList<QMap<PropertyKey, QVariant>>& after,
                     const QString& text);
    // 调用方只给 after 时，在这里按目标图元属性全集补 before。
    static void PushAfter(CanvasScene* scene, const QList<CanvasItem*>& targets,
                          const QList<QMap<PropertyKey, QVariant>>& after, const QString& text);
};
