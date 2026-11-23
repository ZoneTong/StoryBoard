#include "items/CanvasItem.h"
#include "items/CanvasItemVisitor.h"
#include "items/lines/CurveItem.h"
#include "items/lines/LineAbstractItem.h"
#include "scene/CanvasScene.h"
#include "scene/Commands.h"
#include "scene/ItemNameService.h"
#include "scene/SceneConstants.h"

#include <QSet>
#include <algorithm>
#include <utility>

namespace {

// 曲线简化只属于手绘曲线（含图片线稿）：用 Visitor 定点访问，描线基类不再带能力位。
// apply=false 时只探测“有没有可简化的曲线”，不改几何（用于菜单可用性）。
class CurveSimplifyVisitor : public CanvasItemVisitor {
  public:
    CurveSimplifyVisitor(qreal tolerance, bool apply) : m_tolerance(tolerance), m_apply(apply) {}

    void VisitCurve(CurveItem& curve) override
    {
        if (!curve.CanSimplify()) {
            return;
        }
        m_canSimplify = true;
        if (!m_apply) {
            return;
        }
        const LineAbstractItem::GeometryState before = curve.geometryState();
        if (!curve.SimplifyCurve(m_tolerance)) {
            return;
        }
        m_curves.append(&curve);
        m_before.append(before);
        m_after.append(curve.geometryState());
    }

    bool CanSimplify() const { return m_canSimplify; }
    const QList<CurveItem*>& Curves() const { return m_curves; }
    const QList<LineAbstractItem::GeometryState>& BeforeStates() const { return m_before; }
    const QList<LineAbstractItem::GeometryState>& AfterStates() const { return m_after; }

  private:
    qreal m_tolerance = 2.0;
    bool m_apply = false;
    bool m_canSimplify = false;
    QList<CurveItem*> m_curves;
    QList<LineAbstractItem::GeometryState> m_before;
    QList<LineAbstractItem::GeometryState> m_after;
};

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
    CurveSimplifyVisitor probe(2.0, false);
    for (CanvasItem* item : SelectedCanvasItems()) {
        if (item && IsItemEditable(item)) {
            item->Accept(probe);
            if (probe.CanSimplify()) {
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
    CurveSimplifyVisitor visitor(effectiveTolerance, true);
    for (CanvasItem* item : SelectedCanvasItems()) {
        if (item && IsItemEditable(item)) {
            item->Accept(visitor);
        }
    }
    if (visitor.Curves().isEmpty()) {
        return 0;
    }

    const QList<CurveItem*> curves = visitor.Curves();
    QList<LineAbstractItem*> lines;
    lines.reserve(curves.size());
    for (CurveItem* curve : curves) {
        lines.append(curve);
    }
    if (m_undoStack) {
        m_undoStack->push(new StrokeSmoothCommand(this, lines, visitor.BeforeStates(), visitor.AfterStates(),
                                                  tr("Smooth selected strokes")));
    } else {
        for (int i = 0; i < lines.size(); ++i) {
            lines.at(i)->ApplyGeometryState(visitor.AfterStates().at(i));
        }
        UpdateSceneExtent();
        update();
        emit contentChanged();
    }
    return lines.size();
}

QString CanvasScene::UniqueItemName(const QString& base, const CanvasItem* except) const
{
    return ItemNameService::Unique(CanvasItems(), base, except);
}

bool CanvasScene::IsItemNameAvailable(const QString& name, const CanvasItem* except) const
{
    return ItemNameService::IsAvailable(CanvasItems(), name, except);
}

QString CanvasScene::UniqueItemNameFor(const QString& requested, const CanvasItem* except) const
{
    return ItemNameService::UniqueFor(CanvasItems(), requested, except);
}

QString CanvasScene::UniqueItemNameFor(const QString& requested, const CanvasItem* except,
                                       const QSet<QString>& reserved) const
{
    return ItemNameService::UniqueFor(CanvasItems(), requested, except, reserved);
}

void CanvasScene::AssignAutoItemNames()
{
    ItemNameService::AssignAutoNames(CanvasItems());
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

void CanvasScene::PushGroupRotation(const QList<CanvasItem*>& items, const QList<qreal>& anglesBefore,
                                    const QList<qreal>& anglesAfter, const ItemGeometryList& geometryBefore,
                                    const ItemGeometryList& geometryAfter, const QString& text)
{
    if (items.isEmpty() || anglesBefore.size() != items.size() || anglesAfter.size() != items.size()) {
        return;
    }
    if (m_undoStack) {
        m_undoStack->push(
            new RotationCommand(this, items, anglesBefore, anglesAfter, text, geometryBefore, geometryAfter));
    } else {
        for (const ItemGeometry& entry : std::as_const(geometryAfter)) {
            if (entry.item) {
                entry.item->setPos(entry.pos);
                entry.item->SetContentSize(entry.size);
            }
        }
        for (int i = 0; i < items.size(); ++i) {
            if (items.at(i)) {
                items.at(i)->SetRotationAngle(anglesAfter.at(i));
            }
        }
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
