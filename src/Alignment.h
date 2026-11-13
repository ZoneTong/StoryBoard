#pragma once

#include "ItemGeometry.h"

class CanvasScene;
class CanvasItem;

// 多图元对齐：参照 Excalidraw 的 align/distribute 语义，以选中图元的整体外框为基准，
// 逐个平移（只改位置，不改尺寸与旋转），整体只占一条撤销记录。
namespace Alignment {

enum class Mode {
    Left,                // 左边缘对齐
    HorizontalCenter,    // 水平居中
    Right,               // 右边缘对齐
    Top,                 // 顶边对齐
    VerticalCenter,      // 垂直居中
    Bottom,              // 底边对齐
    DistributeHorizontal, // 水平等间距（至少 3 个）
    DistributeVertical,   // 垂直等间距（至少 3 个）
};

// 当前选择是否够用：对齐要 2 个及以上，等间距要 3 个及以上。
bool supports(CanvasScene* scene, Mode mode);
// 执行对齐，返回被移动的图元数量（0 表示没有可动的图元）。
int apply(CanvasScene* scene, Mode mode, const QString& commandText);

} // namespace Alignment
