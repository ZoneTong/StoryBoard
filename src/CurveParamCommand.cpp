#include "CurveParamCommand.h"

#include "CanvasItem.h"
#include "CanvasScene.h"

CurveParamState curveParamState(const LineAbstractItem* item)
{
    return item ? item->paramState() : CurveParamState();
}

void ApplyCurveParamState(LineAbstractItem* item, const CurveParamState& state)
{
    if (item) {
        item->ApplyParamState(state);
    }
}

CurveParamCommand::CurveParamCommand(CanvasScene* scene, const QList<LineAbstractItem*>& items,
                                     const QList<CurveParamState>& paramsBefore,
                                     const QList<LineAbstractItem::GeometryState>& geometryBefore,
                                     const QList<CurveParamState>& paramsAfter,
                                     const QList<LineAbstractItem::GeometryState>& geometryAfter, const QString& text)
    : QUndoCommand(text), m_scene(scene), m_paramsBefore(paramsBefore), m_geometryBefore(geometryBefore),
      m_paramsAfter(paramsAfter), m_geometryAfter(geometryAfter)
{
    m_items.reserve(items.size());
    for (LineAbstractItem* item : items) {
        m_items.append(item);
    }
}

void CurveParamCommand::undo()
{
    Apply(m_paramsBefore, m_geometryBefore);
}

void CurveParamCommand::redo()
{
    Apply(m_paramsAfter, m_geometryAfter);
}

void CurveParamCommand::Apply(const QList<CurveParamState>& params,
                              const QList<LineAbstractItem::GeometryState>& geometry)
{
    if (!m_scene) {
        return;
    }
    const int count = qMin(m_items.size(), qMin(params.size(), geometry.size()));
    for (int i = 0; i < count; ++i) {
        LineAbstractItem* item = m_items.at(i).data();
        if (!item) {
            continue;
        }
        // 先按参数重建，再用快照覆盖：撤销 / 重做都回到记录时的精确几何。
        ApplyCurveParamState(item, params.at(i));
        item->ApplyGeometryState(geometry.at(i));
    }
    m_scene->UpdateSceneExtent();
    m_scene->update();
}
