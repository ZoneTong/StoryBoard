#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "LineAbstractItem.h"
#include "scene/SceneConstants.h"

#include <QSet>
#include <algorithm>
#include <utility>

namespace {

bool NearlyEqual(qreal a, qreal b)
{
    return qAbs(a - b) < 0.01;
}

bool SameGeometry(const ItemGeometry& a, const ItemGeometry& b)
{
    return NearlyEqual(a.pos.x(), b.pos.x()) && NearlyEqual(a.pos.y(), b.pos.y()) &&
           NearlyEqual(a.size.width(), b.size.width()) && NearlyEqual(a.size.height(), b.size.height());
}

} // namespace

QList<CanvasItem*> CanvasScene::CanvasItems() const
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

int CanvasScene::CanvasItemCount() const
{
    return CanvasItems().size();
}

QList<CanvasItem*> CanvasScene::SelectedCanvasItems() const
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

void CanvasScene::RefreshViewDependentGeometry()
{
    const QList<CanvasItem*> items = CanvasItems();
    for (CanvasItem* item : items) {
        if (item) {
            item->RefreshViewDependentGeometry();
        }
    }
}

bool CanvasScene::CanSmoothSelectedStrokes() const
{
    if (m_mode != ToolMode::EDIT) {
        return false;
    }
    for (CanvasItem* item : SelectedCanvasItems()) {
        if (!IsItemEditable(item)) {
            continue;
        }
        if (auto* line = dynamic_cast<LineAbstractItem*>(item)) {
            // 只有具备简化能力的手绘笔迹（含图片线稿提取）可简化；
            // 直线 / 弧线 / 正弦 / 花括号是程序化几何，能力位为 false。
            const LineAbstractItem::GeometryState state = line->geometryState();
            if (line->SupportsSimplify() && !state.hasDot && line->StrokePoints().size() >= 3) {
                return true;
            }
        }
    }
    return false;
}

int CanvasScene::SmoothSelectedStrokes(qreal tolerance)
{
    if (m_mode != ToolMode::EDIT) {
        return 0;
    }

    // 参数 0 只关闭“收尾自动平滑”；手动按钮即使在该设置下也执行一次轻量平滑。
    const qreal effectiveTolerance = qMax<qreal>(2.0, tolerance);
    QList<LineAbstractItem*> items;
    QList<LineAbstractItem::GeometryState> before;
    QList<LineAbstractItem::GeometryState> after;
    for (CanvasItem* item : SelectedCanvasItems()) {
        if (!IsItemEditable(item)) {
            continue;
        }
        auto* line = dynamic_cast<LineAbstractItem*>(item);
        if (!line || !line->SupportsSimplify()) {
            continue;
        }
        const LineAbstractItem::GeometryState beforeState = line->geometryState();
        if (beforeState.hasDot || line->StrokePoints().size() < 3) {
            continue;
        }
        if (!line->SimplifyCurve(effectiveTolerance)) {
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
            items.at(i)->ApplyGeometryState(after.at(i));
        }
        UpdateSceneExtent();
        update();
        emit contentChanged();
    }
    return items.size();
}

QString CanvasScene::UniqueItemName(const QString& base, const CanvasItem* except) const
{
    const QString typeName = base.trimmed();
    const QString prefix = typeName + QLatin1Char(' ');
    int maxSuffix = 0;
    for (CanvasItem* item : CanvasItems()) {
        if (!item || item == except) {
            continue;
        }
        const QString existing = item->Name();
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

bool CanvasScene::IsItemNameAvailable(const QString& name, const CanvasItem* except) const
{
    const QString candidate = name.trimmed();
    if (candidate.isEmpty()) {
        return true;
    }
    for (CanvasItem* item : CanvasItems()) {
        if (!item || item == except) {
            continue;
        }
        if (item->Name().trimmed().compare(candidate, Qt::CaseInsensitive) == 0) {
            return false;
        }
    }
    return true;
}

QString CanvasScene::UniqueItemNameFor(const QString& requested, const CanvasItem* except) const
{
    static const QSet<QString> RESERVED;
    return UniqueItemNameFor(requested, except, RESERVED);
}

QString CanvasScene::UniqueItemNameFor(const QString& requested, const CanvasItem* except,
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

    auto consider = [&](const QString& customName) {
        const QString existing = customName.trimmed();
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

    for (CanvasItem* item : CanvasItems()) {
        if (!item || item == except) {
            continue;
        }
        consider(item->Name());
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

void CanvasScene::AssignAutoItemNames()
{
    for (CanvasItem* item : CanvasItems()) {
        if (!item) {
            continue;
        }
        const QString existing = item->CustomName().trimmed();
        if (existing.isEmpty()) {
            item->SetName(UniqueItemName(item->TypeDisplayName(), item));
        } else if (!IsItemNameAvailable(existing, item)) {
            item->SetName(UniqueItemNameFor(existing, item));
        }
    }
}

void CanvasScene::SelectAllItems()
{
    if (m_mode != ToolMode::EDIT) {
        return;
    }
    const QList<CanvasItem*> allItems = CanvasItems();
    for (CanvasItem* item : allItems) {
        if (IsItemEditable(item)) {
            item->setSelected(true);
        }
    }
}

void CanvasScene::PushGeometry(const ItemGeometryList& before, const ItemGeometryList& after, const QString& text,
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
        if (SameGeometry(before.at(i), after.at(i))) {
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
            command->SetMergeId(mergeId);
        }
        m_undoStack->push(command);
    } else {
        for (const ItemGeometry& geometry : std::as_const(filteredAfter)) {
            if (!geometry.item) {
                continue;
            }
            geometry.item->setPos(geometry.pos);
            geometry.item->SetContentSize(geometry.size);
        }
    }
    UpdateSceneExtent();
}

void CanvasScene::PushRotation(CanvasItem* item, qreal before, qreal after, const QString& text)
{
    if (!item || qAbs(before - after) < 0.0001) {
        return;
    }

    if (m_undoStack) {
        m_undoStack->push(new RotationCommand(this, {item}, {before}, {after}, text));
    } else {
        item->SetRotationAngle(after);
    }
    UpdateSceneExtent();
}

int CanvasScene::RotateSelected(qreal degrees, const QString& commandText)
{
    const QList<CanvasItem*> items = SelectedCanvasItems();
    QList<CanvasItem*> changed;
    QList<qreal> before;
    QList<qreal> after;
    for (CanvasItem* item : items) {
        if (!item || !IsItemEditable(item)) {
            continue;
        }
        const qreal start = item->RotationAngle();
        const qreal target = CanvasItem::NormalizeRotation(start + degrees);
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
            changed.at(i)->SetRotationAngle(after.at(i));
        }
    }
    UpdateSceneExtent();
    update();
    return changed.size();
}
