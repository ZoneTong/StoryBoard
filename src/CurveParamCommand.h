#pragma once

#include "LineAbstractItem.h"

#include <QList>
#include <QPointer>
#include <QUndoCommand>

// 弧线角度、正弦波形参数的快照。参数是几何真相：回放时先按参数重建，再把快照几何覆盖回去，
// 这样撤销后的点与内容框与原状态逐点一致，不会因重新采样产生漂移。
using CurveParamState = LineAbstractItem::ParamState;

// 参数读取走 LineAbstractItem 的虚接口，只有弧线 / 正弦给出有效值，不按叶子类强转。
CurveParamState curveParamState(const LineAbstractItem* item);

// 弧线角度 / 正弦参数的修改命令：一次编辑只占一条撤销记录，支持多选同时改。
class CurveParamCommand : public QUndoCommand {
  public:
    CurveParamCommand(CanvasScene* scene, const QList<LineAbstractItem*>& items,
                      const QList<CurveParamState>& paramsBefore,
                      const QList<LineAbstractItem::GeometryState>& geometryBefore,
                      const QList<CurveParamState>& paramsAfter,
                      const QList<LineAbstractItem::GeometryState>& geometryAfter, const QString& text);
    void undo() override;
    void redo() override;

  private:
    void Apply(const QList<CurveParamState>& params, const QList<LineAbstractItem::GeometryState>& geometry);

    CanvasScene* m_scene = nullptr;
    QList<QPointer<LineAbstractItem>> m_items;
    QList<CurveParamState> m_paramsBefore;
    QList<LineAbstractItem::GeometryState> m_geometryBefore;
    QList<CurveParamState> m_paramsAfter;
    QList<LineAbstractItem::GeometryState> m_geometryAfter;
};

// 把一份参数状态写回具体子型；不是弧线 / 正弦的描线图元不做任何事。
void ApplyCurveParamState(LineAbstractItem* item, const CurveParamState& state);
