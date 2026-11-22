#pragma once

class ShapeAbstractItem;
class LineAbstractItem;
class CurveItem;
class TextItem;

// 跨类型操作的通用访问者：只用于无法用标量属性表达的定点能力，
// 例如多边形内部图层遍历、曲线简化。默认实现为空（Null Object），
// 访问者只需覆写自己关心的一类，不被迫处理全部分支。
class CanvasItemVisitor {
  public:
    virtual ~CanvasItemVisitor() = default;
    virtual void VisitShape(ShapeAbstractItem&);
    virtual void VisitLine(LineAbstractItem&);
    // 手绘曲线 / 图片线稿：曲线简化只属于这一类，描线基类不再带能力位。
    virtual void VisitCurve(CurveItem&);
    virtual void VisitText(TextItem&);
};
