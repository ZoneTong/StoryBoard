#pragma once

#include "Alignment.h"
#include "BoolOps.h"

class MainWindow;
class CanvasItem;

// 图元属性与编辑动作：属性窗打开 / 关闭、选中图元旋转，多图元对齐与合并形状。
// 变更本身落回 CanvasScene 的撤销记录；本类只编排对话框、状态提示与命令转发。
class MainWindowEditActionsController {
  public:
    explicit MainWindowEditActionsController(MainWindow* window);

    void showSelectedItemProperties();
    void showItemProperties(CanvasItem* item);
    void closeItemProperties();
    void rotateSelected(qreal degrees);
    void applyAlignment(Alignment::Mode mode);
    void applyBoolOperation(BoolOps::Operation operation);

  private:
    MainWindow* m_window = nullptr;
};
