#pragma once

#include "Alignment.h"
#include "BoolOps.h"

class MainWindow;
class CanvasItem;

// 图元属性与编辑动作：属性窗打开 / 关闭、选中图元旋转，多图元对齐与合并形状。
// 变更本身落回 CanvasScene 的撤销记录；本类只编排对话框、状态提示与命令转发。
class MainWindowEditActionsController {
  public:
    // 剪贴板自定义 MIME：只在本应用进程间互贴，不向其它软件暴露图元格式。
    static constexpr const char* CLIPBOARD_MIME_TYPE = "application/x-storyboard-items";

    explicit MainWindowEditActionsController(MainWindow* window);

    void showSelectedItemProperties();
    void showItemProperties(CanvasItem* item);
    void closeItemProperties();
    void rotateSelected(qreal degrees);
    void applyAlignment(Alignment::Mode mode);
    void applyBoolOperation(BoolOps::Operation operation);

    void copySelectedItems();
    void cutSelectedItems();
    void pasteItems();

  private:
    MainWindow* m_window = nullptr;
};
