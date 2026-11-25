#pragma once

#include <QString>

// CanvasScene 各功能单元共用的常量集中在这里，避免拆到多个 TU 后出现两份漂移的副本。
namespace scene_constants {

constexpr qreal MIN_CANVAS_SIDE = 200.0;
constexpr qreal MAX_CANVAS_SIDE = 12000.0;
constexpr int MAX_ITEM_HEIGHT = 999999;
constexpr qreal LAYER_Z_STRIDE = 1000000.0;
constexpr int FIRST_USER_LAYER_ID = 1;
// 自适应导出在最小包围框外留的余量（每边 2px）：贴边裁图会被抗锯齿与像素取整削掉一丝。
constexpr qreal EXPORT_FIT_PADDING = 2.0;
inline const QString BACKGROUND_LAYER_NAME = QStringLiteral("背景");

} // namespace scene_constants
