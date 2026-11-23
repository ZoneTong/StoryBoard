#include "ui/panels/properties/PropertyCommitService.h"

#include "items/CanvasItem.h"
#include "scene/CanvasScene.h"
#include "scene/Commands.h"

#include <QUndoStack>
#include <utility>

QMap<PropertyKey, QVariant> PropertyCommitService::Snapshot(const CanvasItem* item)
{
    QMap<PropertyKey, QVariant> values;
    if (!item) {
        return values;
    }
    for (PropertyKey property : item->Properties().Keys()) {
        values.insert(property, item->PropertyValueByKey(property));
    }
    return values;
}

void PropertyCommitService::Push(CanvasScene* scene, const QList<CanvasItem*>& targets,
                                 const QList<QMap<PropertyKey, QVariant>>& before,
                                 const QList<QMap<PropertyKey, QVariant>>& after, const QString& text)
{
    if (!scene || targets.isEmpty() || targets.size() != before.size() || targets.size() != after.size()) {
        return;
    }

    bool changed = false;
    for (int index = 0; index < after.size(); ++index) {
        if (before.at(index) != after.at(index)) {
            changed = true;
            break;
        }
    }
    if (!changed) {
        return;
    }

    auto* command = new ItemPropertyCommand(scene, targets, before, after, text);
    if (auto* undo = scene->UndoStack()) {
        undo->push(command);
    } else {
        command->redo();
        delete command;
    }
}

void PropertyCommitService::PushAfter(CanvasScene* scene, const QList<CanvasItem*>& targets,
                                      const QList<QMap<PropertyKey, QVariant>>& after, const QString& text)
{
    if (!scene || targets.size() != after.size()) {
        return;
    }
    QList<QMap<PropertyKey, QVariant>> before;
    before.reserve(targets.size());
    for (CanvasItem* target : targets) {
        before.append(Snapshot(target));
    }
    Push(scene, targets, before, after, text);
}
