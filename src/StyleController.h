#pragma once

#include "CanvasTypes.h"
#include "ShapeItem.h"

#include <QColor>
#include <QIcon>

class MainWindow;

// 图元样式与画笔状态：填充样式 / 填充色 / 描边色，画笔颜色、粗细与平滑度，
// 以及对应动作与工具栏按钮的图标、勾选和提示刷新。
class StyleController {
  public:
    explicit StyleController(MainWindow* window);

    void setFillStyle(FillStyle style);
    void chooseFillColor();
    void chooseStrokeColor();
    void applyFillToSelection();
    void updateFillActions();

    void setBrushColor(const QColor& color, bool remember);
    void setBrushWidthUi(int width);
    void setStrokeSmoothingUi(int tolerance);
    void chooseBrushColor();

    // 填充 / 描边色板图标：动作创建与工具栏搭建阶段也复用这一份绘制逻辑。
    QIcon fillColorIcon(const QColor& color, int logicalSize) const;
    QIcon strokeColorIcon(const QColor& color, int logicalSize) const;
    QIcon fillStyleIcon(FillStyle style, const QColor& color, int logicalSize) const;

  private:
    MainWindow* m_window = nullptr;
};
