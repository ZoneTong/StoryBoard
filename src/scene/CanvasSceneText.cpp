#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "TextItem.h"

#include <QUndoStack>

bool CanvasScene::changeTextWithUndo(TextItem* item, const QString& text, const QFont& font, const QColor& color,
                                     const QString& textCommand)
{
    if (!item || item->scene() != this) {
        return false;
    }

    const TextItemState before = item->state();
    TextItemState after = before;
    after.text = text;
    after.font = font;
    after.color = color.isValid() ? color : before.color;
    after.baseSize = TextItem::measure(after.text, after.font);
    after.size = after.baseSize;

    if (before == after) {
        return false;
    }

    const QString commandText = textCommand.isEmpty() ? tr("Edit text") : textCommand;
    if (m_undoStack) {
        m_undoStack->push(new TextItemCommand(this, item, before, after, commandText));
    } else {
        applyTextState(item, after);
    }
    return true;
}

void CanvasScene::applyTextState(TextItem* item, const TextItemState& state)
{
    if (!item) {
        return;
    }
    item->applyState(state);
    updateSceneExtent();
    update();
    emit contentChanged();
}
