#include "CurveParamCommand.h"

#include "ArcItem.h"
#include "CanvasItem.h"
#include "CanvasScene.h"
#include "SineItem.h"

CurveParamState curveParamState(const CurveItem* item)
{
    CurveParamState state;
    if (!item) {
        return state;
    }
    if (const auto* arc = dynamic_cast<const ArcItem*>(item)) {
        state.first = arc->startAngle();
        state.second = arc->sweepAngle();
    } else if (const auto* sine = dynamic_cast<const SineItem*>(item)) {
        state.first = sine->params().cycles;
        state.second = sine->params().amplitude;
    }
    return state;
}

void applyCurveParamState(CurveItem* item, const CurveParamState& state)
{
    if (!item) {
        return;
    }
    if (auto* arc = dynamic_cast<ArcItem*>(item)) {
        arc->setAngles(state.first, state.second);
    } else if (auto* sine = dynamic_cast<SineItem*>(item)) {
        SineItem::Params params = sine->params();
        params.cycles = qBound(1, qRound(state.first), 12);
        params.amplitude = qBound<qreal>(1.0, state.second, 200.0);
        sine->setParams(params);
    }
}

CurveParamCommand::CurveParamCommand(CanvasScene* scene, const QList<CurveItem*>& items,
                                     const QList<CurveParamState>& paramsBefore,
                                     const QList<CurveItem::GeometryState>& geometryBefore,
                                     const QList<CurveParamState>& paramsAfter,
                                     const QList<CurveItem::GeometryState>& geometryAfter, const QString& text)
    : QUndoCommand(text), m_scene(scene), m_paramsBefore(paramsBefore), m_geometryBefore(geometryBefore),
      m_paramsAfter(paramsAfter), m_geometryAfter(geometryAfter)
{
    m_items.reserve(items.size());
    for (CurveItem* item : items) {
        m_items.append(item);
    }
}

void CurveParamCommand::undo()
{
    apply(m_paramsBefore, m_geometryBefore);
}

void CurveParamCommand::redo()
{
    apply(m_paramsAfter, m_geometryAfter);
}

void CurveParamCommand::apply(const QList<CurveParamState>& params, const QList<CurveItem::GeometryState>& geometry)
{
    if (!m_scene) {
        return;
    }
    const int count = qMin(m_items.size(), qMin(params.size(), geometry.size()));
    for (int i = 0; i < count; ++i) {
        CurveItem* item = m_items.at(i).data();
        if (!item) {
            continue;
        }
        // 先按参数重建，再用快照覆盖：撤销 / 重做都回到记录时的精确几何。
        applyCurveParamState(item, params.at(i));
        item->applyGeometryState(geometry.at(i));
    }
    m_scene->updateSceneExtent();
    m_scene->update();
}
