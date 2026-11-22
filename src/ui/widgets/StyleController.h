#pragma once

#include "items/CanvasTypes.h"
#include "items/shapes/ShapeAbstractItem.h"

#include <QColor>
#include <QIcon>
#include <QString>

class MainWindow;

// 图元样式与画笔状态：填充样式 / 填充色 / 描边色，画笔颜色、粗细与平滑度，
// 以及对应动作与工具栏按钮的图标、勾选和提示刷新。
class StyleController {
  public:
    explicit StyleController(MainWindow* window);

    void SetFillStyle(FillStyle style);
    void ChooseFillColor();
    void ChooseStrokeColor();
    void ApplyStrokeColorToSelection(const QColor& color);
    void ApplyStrokeWidthToSelection(qreal width);
    // 描线样式（需求 1）：设置全局描线样式并同步到选中图元与各处图标。
    void SetStrokeStyle(Qt::PenStyle style);
    void UpdateStrokeStyleActions();
    void ApplyFillToSelection();
    void UpdateFillActions();

    void SetBrushColor(const QColor& color, bool remember);
    void SetBrushWidthUi(int width);
    void SetStrokeSmoothingUi(qreal tolerance);
    void ChooseBrushColor();

    // 填充 / 描边色板图标：动作创建与工具栏搭建阶段也复用这一份绘制逻辑。
    QIcon FillColorIcon(const QColor& color, int logicalSize) const;
    QIcon StrokeColorIcon(const QColor& color, int logicalSize) const;
    QIcon FillStyleIcon(FillStyle style, const QColor& color, int logicalSize) const;

  private:
    int ApplyStrokeStyleToSelection(const QColor& color, qreal width, bool changeColor, bool changeWidth,
                                    const QString& text);
    int ApplyPenStyleToSelection(Qt::PenStyle style, const QString& text);

    MainWindow* m_window = nullptr;
};
