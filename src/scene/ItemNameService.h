#pragma once

#include <QList>
#include <QSet>
#include <QString>

class CanvasItem;

// 图元名称的唯一性与自动命名。只扫描传入的图元快照，不持有图元生命周期，
// 也不缓存名称表；CanvasScene 每次把当前图元集合交进来即可。
class ItemNameService {
  public:
    static bool IsAvailable(const QList<CanvasItem*>& items, const QString& name, const CanvasItem* except = nullptr);
    static QString Unique(const QList<CanvasItem*>& items, const QString& base, const CanvasItem* except = nullptr);
    static QString UniqueFor(const QList<CanvasItem*>& items, const QString& requested,
                             const CanvasItem* except = nullptr);
    static QString UniqueFor(const QList<CanvasItem*>& items, const QString& requested, const CanvasItem* except,
                             const QSet<QString>& reserved);
    static void AssignAutoNames(const QList<CanvasItem*>& items);
};
