#pragma once

#include <QSize>

class QWidget;

// 属性行的统一度量源（§D13）。编辑面板与属性窗共用同一组行控件，尺寸只从这里取，
// 不再各卡片各写 ICON_SIZE / BUTTON_SIZE 常量。
namespace RowMetrics {

// 行内输入框的真实高度（QDoubleSpinBox 的 sizeHint，按当前字体与样式实测后缓存）。
int FieldHeight();

// 行控件的外框高度：输入框高 + 7（≈32），编辑面板与属性窗取同一个值。编辑面板与属性窗必须取同一个值。
int RowHeight();

// 行图标的字形边长（16–20）：小于按钮，避免细笔画贴边。
int GlyphSize();

// 行图标按钮的边长：与输入框等高，所以永远不会比属性窗里的输入框高。
QSize IconButtonSize();

// 把行控件套到统一行高，并固定纵向策略（避免卡片布局把它压扁）。
void ApplyRow(QWidget* row);

// 把行内某个输入控件钉到统一字段高：数值框直接取字段高（它的 sizeHint 会被上下按钮撑高），
// 输入框 / 下拉框 / 工具按钮取“字段高”与自身 sizeHint 的较大值。避免被布局拉伸或被样式表压矮。
void ApplyFieldHeight(QWidget* widget);

} // namespace RowMetrics
