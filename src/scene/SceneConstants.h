#pragma once

#include <QString>

// CanvasScene 各功能单元共用的常量集中在这里，避免拆到多个 TU 后出现两份漂移的副本。
namespace scene_constants {

constexpr qreal kMinCanvasSide = 200.0;
constexpr qreal kMaxCanvasSide = 12000.0;
constexpr int kMaxItemHeight = 999999;
constexpr qreal kLayerZStride = 1000000.0;
constexpr int kFirstUserLayerId = 1;
inline const QString kBackgroundLayerName = QStringLiteral("背景");

} // namespace scene_constants
