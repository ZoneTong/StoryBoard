#pragma once

#include "CanvasTypes.h"

class CanvasView;

// 视图光标：按当前工具模式与是否正在平移选择光标形状。手柄命中光标由拉伸控制器
// 在悬停时直接覆盖，离开手柄后再回到这里的模式光标。
class CanvasViewCursor {
  public:
    explicit CanvasViewCursor(CanvasView* view);

    void Refresh(ToolMode mode, bool panning);

  private:
    CanvasView* m_view = nullptr;
};
