#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "LineItemAbstract.h"
#include "scene/SceneConstants.h"

#include <QSet>
#include <algorithm>
#include <utility>

namespace {

bool nearlyEqual(qreal a, qreal b)
{
    return qAbs(a - b) < 0.01;
}

bool sameGeometry(const ItemGeometry& a, const ItemGeometry& b)
{
    return nearlyEqual(a.pos.x(), b.pos.x()) && nearlyEqual(a.pos.y(), b.pos.y()) &&
           nearlyEqual(a.size.width(), b.size.width()) && nearlyEqual(a.size.height(), b.size.height());
}

} // namespace

QList<CanvasItem*> CanvasScene::canvasItems() const
{
    QList<CanvasItem*> result;
    const QList<QGraphicsItem*> allItems = items(Qt::AscendingOrder);
    result.reserve(allItems.size());
    for (QGraphicsItem* graphicsItem : allItems) {
        if (auto* item = qgraphicsitem_cast<CanvasItem*>(graphicsItem)) {
            result.append(item);
        }
    }
    return result;
}

int CanvasScene::canvasItemCount() const
{
    return canvasItems().size();
}

QList<CanvasItem*> CanvasScene::selectedCanvasItems() const
{
    QList<CanvasItem*> result;
    const QList<QGraphicsItem*> graphicsItems = selectedItems();
    for (QGraphicsItem* graphicsItem : graphicsItems) {
        if (auto* item = qgraphicsitem_cast<CanvasItem*>(graphicsItem)) {
            result.append(item);
        }
    }
    return result;
}

void CanvasScene::refreshViewDependentGeometry()
{
    const QList<CanvasItem*> items = canvasItems();
    for (CanvasItem* item : items) {
        if (item) {
            item->refreshViewDependentGeometry();
        }
    }
}

bool CanvasScene::canSmoothSelectedStrokes() const
{
    if (m_mode != ToolMode::Edit) {
        return false;
    }
    for (CanvasItem* item : selectedCanvasItems()) {
        if (!isItemEditable(item)) {
            continue;
        }
        if (auto* line = dynamic_cast<LineItemAbstract*>(item)) {
            // 只有具备简化能力的手绘笔迹（含图片线稿提取）可简化；
            // 直线 / 弧线 / 正弦 / 花括号是程序化几何，能力位为 false。
            const LineItemAbstract::GeometryState state = line->geometryState();
            if (line->supportsSimplify() && !state.hasDot && line->strokePoints().size() >= 3) {
                return true;
            }
        }
    }
    return false;
}

int CanvasScene::smoothSelectedStrokes(qreal tolerance)
{
    if (m_mode != ToolMode::Edit) {
        return 0;
    }

    // 参数 0 只关闭“收尾自动平滑”；手动按钮即使在该设置下也执行一次轻量平滑。
    const qreal effectiveTolerance = qMax<qreal>(2.0, tolerance);
    QList<LineItemAbstract*> items;
    QList<LineItemAbstract::GeometryState> before;
    QList<LineItemAbstract::GeometryState> after;
    for (CanvasItem* item : selectedCanvasItems()) {
        if (!isItemEditable(item)) {
            continue;
        }
        auto* line = dynamic_cast<LineItemAbstract*>(item);
        if (!line || !line->supportsSimplify()) {
            continue;
        }
        const LineItemAbstract::GeometryState beforeState = line->geometryState();
        if (beforeState.hasDot || line->strokePoints().size() < 3) {
            continue;
        }
        if (!line->simplifyCurve(effectiveTolerance)) {
            continue;
        }
        items.append(line);
        before.append(beforeState);
        after.append(line->geometryState());
    }
    if (items.isEmpty()) {
        return 0;
    }

    if (m_undoStack) {
        m_undoStack->push(new StrokeSmoothCommand(this, items, before, after, tr("Smooth selected strokes")));
    } else {
        for (int i = 0; i < items.size(); ++i) {
            items.at(i)->applyGeometryState(after.at(i));
        }
        updateSceneExtent();
        update();
        emit contentChanged();
    }
    return items.size();
}

QString CanvasScene::uniqueItemName(const QString& base, const CanvasItem* except) const
{
    const QString typeName = base.trimmed();
    const QString prefix = typeName + QLatin1Char(' ');
    int maxSuffix = 0;
    for (CanvasItem* item : canvasItems()) {
        if (!item || item == except) {
            continue;
        }
        const QString existing = item->name();
        if (!existing.startsWith(prefix, Qt::CaseInsensitive)) {
            continue;
        }
        bool ok = false;
        const int value = existing.mid(prefix.size()).toInt(&ok);
        if (ok && value > 0) {
            maxSuffix = qMax(maxSuffix, value);
        }
    }
    return QStringLiteral("%1 %2").arg(typeName).arg(maxSuffix + 1);
}

bool CanvasScene::isItemNameAvailable(const QString& name, const CanvasItem* except) const
{
    const QString candidate = name.trimmed();
    if (candidate.isEmpty()) {
        return true;
    }
    for (CanvasItem* item : canvasItems()) {
        if (!item || item == except) {
            continue;
        }
        if (item->name().trimmed().compare(candidate, Qt::CaseInsensitive) == 0) {
            return false;
        }
    }
    return true;
}

QString CanvasScene::uniqueItemNameFor(const QString& requested, const CanvasItem* except) const
{
    static const QSet<QString> reserved;
    return uniqueItemNameFor(requested, except, reserved);
}

QString CanvasScene::uniqueItemNameFor(const QString& requested, const CanvasItem* except,
                                       const QSet<QString>& reserved) const
{
    const QString trimmed = requested.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    // “Hero panel” → base=Hero panel, suffix=0；“矩形 1” → base=矩形, suffix=1。
    QString base = trimmed;
    int suffix = 0;
    const int split = base.lastIndexOf(QLatin1Char(' '));
    if (split > 0) {
        bool ok = false;
        const int value = base.mid(split + 1).toInt(&ok);
        if (ok && value > 0) {
            base = base.left(split).trimmed();
            suffix = value;
        }
    }
    if (base.isEmpty()) {
        base = trimmed;
    }

    const QString prefix = base + QLatin1Char(' ');
    bool exactBase = false;
    int maxSuffix = suffix;

    auto consider = [&](const QString& rawName) {
        const QString existing = rawName.trimmed();
        if (existing.isEmpty()) {
            return;
        }
        if (existing.compare(base, Qt::CaseInsensitive) == 0) {
            exactBase = true;
        }
        if (!existing.startsWith(prefix, Qt::CaseInsensitive)) {
            return;
        }
        bool ok = false;
        const int value = existing.mid(prefix.size()).toInt(&ok);
        if (ok && value > 0) {
            maxSuffix = qMax(maxSuffix, value);
        }
    };

    for (CanvasItem* item : canvasItems()) {
        if (!item || item == except) {
            continue;
        }
        consider(item->name());
    }
    for (const QString& name : reserved) {
        consider(name);
    }

    int next = qMax(maxSuffix + 1, exactBase ? 2 : 1);
    if (suffix > 0) {
        next = qMax(next, suffix + 1);
    }
    return prefix + QString::number(next);
}

void CanvasScene::assignAutoItemNames()
{
    for (CanvasItem* item : canvasItems()) {
        if (!item) {
            continue;
        }
        const QString existing = item->rawName().trimmed();
        if (existing.isEmpty()) {
            item->setName(uniqueItemName(item->typeDisplayName(), item));
        } else if (!isItemNameAvailable(existing, item)) {
            item->setName(uniqueItemNameFor(existing, item));
        }
    }
}

void CanvasScene::selectAllItems()
{
    if (m_mode != ToolMode::Edit) {
        return;
    }
    const QList<CanvasItem*> allItems = canvasItems();
    for (CanvasItem* item : allItems) {
        if (isItemEditable(item)) {
            item->setSelected(true);
        }
    }
}

void CanvasScene::pushGeometry(const ItemGeometryList& before, const ItemGeometryList& after, const QString& text,
                               const QString& mergeId)
{
    if (before.size() != after.size()) {
        return;
    }

    ItemGeometryList filteredBefore;
    ItemGeometryList filteredAfter;
    for (int i = 0; i < before.size(); ++i) {
        if (!before.at(i).item || !after.at(i).item) {
            continue;
        }
        if (sameGeometry(before.at(i), after.at(i))) {
            continue;
        }
        filteredBefore.append(before.at(i));
        filteredAfter.append(after.at(i));
    }
    if (filteredBefore.isEmpty()) {
        return;
    }

    if (m_undoStack) {
        auto* command = new GeometryCommand(this, filteredBefore, filteredAfter, text);
        if (!mergeId.isEmpty()) {
            command->setMergeId(mergeId);
        }
        m_undoStack->push(command);
    } else {
        for (const ItemGeometry& geometry : std::as_const(filteredAfter)) {
            if (!geometry.item) {
                continue;
            }
            geometry.item->setPos(geometry.pos);
            geometry.item->setContentSize(geometry.size);
        }
    }
    updateSceneExtent();
}

void CanvasScene::pushRotation(CanvasItem* item, qreal before, qreal after, const QString& text)
{
    if (!item || qAbs(before - after) < 0.0001) {
        return;
    }

    if (m_undoStack) {
        m_undoStack->push(new RotationCommand(this, {item}, {before}, {after}, text));
    } else {
        item->setRotationAngle(after);
    }
    updateSceneExtent();
}

int CanvasScene::rotateSelected(qreal degrees, const QString& commandText)
{
    const QList<CanvasItem*> items = selectedCanvasItems();
    QList<CanvasItem*> changed;
    QList<qreal> before;
    QList<qreal> after;
    for (CanvasItem* item : items) {
        if (!item || !isItemEditable(item)) {
            continue;
        }
        const qreal start = item->rotationAngle();
        const qreal target = CanvasItem::normalizeRotation(start + degrees);
        if (qAbs(start - target) < 0.0001) {
            continue;
        }
        changed.append(item);
        before.append(start);
        after.append(target);
    }
    if (changed.isEmpty()) {
        return 0;
    }

    const QString text = commandText.isEmpty() ? tr("Rotate items") : commandText;
    if (m_undoStack) {
        m_undoStack->push(new RotationCommand(this, changed, before, after, text));
    } else {
        for (int i = 0; i < changed.size(); ++i) {
            changed.at(i)->setRotationAngle(after.at(i));
        }
    }
    updateSceneExtent();
    update();
    return changed.size();
}
