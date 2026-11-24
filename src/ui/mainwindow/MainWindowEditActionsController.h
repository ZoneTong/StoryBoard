#pragma once

#include "ops/Alignment.h"
#include "ops/BoolOps.h"

class MainWindow;
class CanvasItem;
class QMimeData;

// 图元编辑动作：选中图元旋转，多图元对齐与合并形状。
// 变更本身落回 CanvasScene 的撤销记录；本类只编排状态提示与命令转发。
class MainWindowEditActionsController {
  public:
    // 剪贴板自定义 MIME：只在本应用进程间互贴，不向其它软件暴露图元格式。
    static constexpr const char* CLIPBOARD_MIME_TYPE = "application/x-storyboard-items";

    explicit MainWindowEditActionsController(MainWindow* window);

    void RotateSelected(qreal degrees);
    void ApplyAlignment(Alignment::Mode mode);
    void ApplyBoolOperation(BoolOps::Operation operation);

    void CopySelectedItems();
    void CutSelectedItems();
    void PasteItems();
    // 编辑动作与剪贴板状态共用这一判定：自定义 MIME 或 Excalidraw 剪贴板。
    static bool CanPaste(const QMimeData* mime);

  private:
    MainWindow* m_window = nullptr;
};
