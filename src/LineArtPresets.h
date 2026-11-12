#pragma once

// 线稿化档位：每档是一组实测过的参数。数值按 tools/lineart-similarity 那张地图调过，
// “Balanced”就是默认值（f1 0.933，900 条轮廓）；往左更保小道具，往右更干净。
// 简化容差也属于档位：保留细节要贴像素走，大块概括要更少更直的线段。
// 最细条带是尺度相关的去噪阈值，不随档位切换。
// 设置对话框和导入图片时的参数对话框共用这张表，避免两处数值漂移。
struct LineArtPreset {
    const char* name;
    int levels;
    int palette;
    int adjacent;
    int minArea;
    double tolerance;
};

inline constexpr LineArtPreset kLineArtPresets[] = {
    {"Keep details", 6, 40, 44, 8, 0.5},
    {"Balanced", 6, 48, 48, 12, 0.6},
    {"Clean lines", 6, 64, 56, 20, 0.8},
    {"Bold blocks", 6, 80, 56, 32, 1.0},
};

inline int lineArtPresetCount()
{
    return static_cast<int>(sizeof(kLineArtPresets) / sizeof(kLineArtPresets[0]));
}

inline LineArtPreset lineArtPreset(int index)
{
    static const LineArtPreset fallback{};
    if (index < 0 || index >= lineArtPresetCount()) {
        return fallback;
    }
    return kLineArtPresets[index];
}
