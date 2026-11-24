#pragma once

// 界面布局度量的单一来源：工具栏 / 全屏悬浮栏 / 全屏悬浮卡的尺寸与留白集中在这里维护，
// 既有类内常量保留为别名，调用点不必改。调整尺寸时只改本头文件，自检阈值随之对齐。
namespace LayoutMetrics {

// 主窗口工具栏：图标 20px，图标按钮 30×30，下拉框/数字框高 30px，三者对齐同一条 30px 基线。
constexpr int TOOL_BAR_ICON_SIZE = 20;
constexpr int TOOL_BAR_BUTTON_SIZE = 30;
constexpr int TOOL_BAR_CONTROL_HEIGHT = 30;
// 画笔颜色是带下拉的分裂按钮（点色块直接调色、点箭头选预设），比纯图标按钮宽出一条箭头区。
constexpr int TOOL_BAR_SPLIT_BUTTON_WIDTH = 34;

// 全屏悬浮栏：栏高 66px，图标边长 26px；按钮高度 = 栏高 − 8，保证键位字符不被下沿切掉。
constexpr int FULLSCREEN_BAR_HEIGHT = 66;
constexpr int FULLSCREEN_BAR_ICON_SIZE = 26;
constexpr int FULLSCREEN_BAR_BUTTON_WIDTH = 46;
constexpr int FULLSCREEN_BAR_BUTTON_HEIGHT_PAD = 8;
// 样式按钮与宽度数字框：图标 20px，按钮宽 34px，高度对齐工具栏基线 30px。
constexpr int FULLSCREEN_BAR_STYLE_ICON_SIZE = 20;
constexpr int FULLSCREEN_BAR_STYLE_BUTTON_WIDTH = 34;
constexpr int FULLSCREEN_BAR_SPIN_WIDTH = 74;

// 全屏期两张悬浮卡与窗口边缘的距离，以及编辑面板顶部让开悬浮工具栏的留白。
constexpr int FULLSCREEN_MARGIN = 16;
constexpr int FULLSCREEN_TOP_INSET = 104;
constexpr int FULLSCREEN_LAYER_WIDTH = 280;
// 小屏兜底：可用高度再矮也给图层卡留一点；正常屏幕直接吃到窗口底部，不再设上限。
constexpr int FULLSCREEN_LAYER_MIN_HEIGHT = 260;

// 编辑面板悬浮态没有可用几何时的落地区间与偏移（相对窗口框架）。
constexpr int PROPERTIES_DOCK_MIN_WIDTH = 260;
constexpr int PROPERTIES_DOCK_MAX_WIDTH = 360;
constexpr int PROPERTIES_DOCK_MIN_HEIGHT = 320;
constexpr int PROPERTIES_DOCK_MAX_HEIGHT = 560;
constexpr int PROPERTIES_DOCK_HEIGHT_CLEARANCE = 200;
constexpr int PROPERTIES_DOCK_OFFSET_X = 24;
constexpr int PROPERTIES_DOCK_OFFSET_Y = 88;

// 图层卡初始最小高度；全屏布局时窗口框架宽高都不低于该值才参与排布。
constexpr int LAYER_DOCK_MIN_HEIGHT = 200;
constexpr int DOCK_MIN_FRAME_DIMENSION = 320;

} // namespace LayoutMetrics
