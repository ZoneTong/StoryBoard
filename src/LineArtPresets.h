#pragma once

// 线稿化档位：每档是一组实测过的参数。数值按 tools/lineart-similarity 那张地图调过，
// “Balanced”就是默认值（f1 0.933，900 条轮廓）；往左更保小道具，往右更干净。
// 设置对话框和导入图片时的参数对话框共用这张表，避免两处数值漂移。
struct LineArtPreset {
    const char* name;
    int levels;
    int palette;
    int adjacent;
    int minArea;
};

inline constexpr LineArtPreset kLineArtPresets[] = {
    { "Keep details", 6, 40, 44, 8 },
    { "Balanced", 6, 48, 48, 12 },
    { "Clean lines", 6, 64, 56, 20 },
    { "Bold blocks", 6, 80, 56, 32 },
};

inline int lineArtPresetCount()
{
    return static_cast<int>(sizeof(kLineArtPresets) / sizeof(kLineArtPresets[0]));
}

inline LineArtPreset lineArtPreset(int index)
{
    static const LineArtPreset fallback {};
    if (index < 0 || index >= lineArtPresetCount())
        return fallback;
    return kLineArtPresets[index];
}
