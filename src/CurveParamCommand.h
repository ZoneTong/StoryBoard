#pragma once

#include "CurveItem.h"

#include <QList>
#include <QPointer>
#include <QUndoCommand>

#include <functional>

class ArcItem;
class CanvasScene;
class SineItem;

// 弧线角度、正弦波形参数的快照。参数是几何真相：回放时先按参数重建，再把快照几何覆盖回去，
// 这样撤销后的点与内容框与原状态逐点一致，不会因重新采样产生漂移。
struct CurveParamState {
    qreal first = 0.0;  // 弧线起始角；正弦周期数
    qreal second = 0.0; // 弧线扫过角；正弦振幅（相对内容高度）
};

CurveParamState curveParamState(const CurveItem* item);

// 弧线角度 / 正弦参数的修改命令：一次编辑只占一条撤销记录，支持多选同时改。
class CurveParamCommand : public QUndoCommand {
  public:
    CurveParamCommand(CanvasScene* scene, const QList<CurveItem*>& items, const QList<CurveParamState>& paramsBefore,
                      const QList<CurveItem::GeometryState>& geometryBefore, const QList<CurveParamState>& paramsAfter,
                      const QList<CurveItem::GeometryState>& geometryAfter, const QString& text);
    void undo() override;
    void redo() override;

  private:
    void apply(const QList<CurveParamState>& params, const QList<CurveItem::GeometryState>& geometry);

    CanvasScene* m_scene = nullptr;
    QList<QPointer<CurveItem>> m_items;
    QList<CurveParamState> m_paramsBefore;
    QList<CurveItem::GeometryState> m_geometryBefore;
    QList<CurveParamState> m_paramsAfter;
    QList<CurveItem::GeometryState> m_geometryAfter;
};

// 把一份参数状态写回具体子型；不是弧线 / 正弦的曲线不做任何事。
void applyCurveParamState(CurveItem* item, const CurveParamState& state);
