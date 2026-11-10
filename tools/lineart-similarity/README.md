# 线稿化相似度实验台

「导入图片线稿化」的参数不靠肉眼拍板：这里把 `LineArtConverter` 单独编成一个小程序，
把结果落成 `outlines.json` + `strokes.png`，再用 `metrics.py` 跟原图边缘比相似度。
主程序不依赖这个目录，它只是调参和回归的工具。

## 指标口径

- 真值边缘：原图（缩到线稿化的处理尺寸，默认长边 900）逐通道求 Sobel 梯度取最大值，阈值 40。
- `recall`：真值边缘像素中，落在生成笔画 2px 内的比例。
- `precision`：生成笔画像素中，落在真值边缘 2px 内的比例。
- `f1`：两者调和平均。

指标偏宽松（这张地图约 15% 像素算边缘），适合横向比较参数，不适合当绝对分数。

## 构建

```powershell
$env:PATH = "D:\Qt\Tools\mingw1310_64\bin;D:\Qt\6.12.0\mingw_64\bin;" + $env:PATH
& "D:\Qt\Tools\mingw1310_64\bin\g++.exe" -O2 -std=c++17 `
  -I "D:\Qt\6.12.0\mingw_64\include" -I "D:\Qt\6.12.0\mingw_64\include\QtCore" `
  -I "D:\Qt\6.12.0\mingw_64\include\QtGui" -I "..\..\src" `
  lineart_dump.cpp "..\..\src\LineArtConverter.cpp" -o lineart_dump.exe `
  -L "D:\Qt\6.12.0\mingw_64\lib" -lQt6Gui -lQt6Core
```

## 跑一次

```powershell
.\lineart_dump.exe ..\..\examples\540451553-d6032a86-5676-4ee1-8f13-aee8db52e8bf.png .\run
python .\metrics.py .\run ..\..\examples\540451553-d6032a86-5676-4ee1-8f13-aee8db52e8bf.png
```

参数按键值对传，留空就是 `LineArtConverter::Options` 的默认值：

```
levels=6 colors=24 merge=64 minarea=24 minthick=1.2 tol=0.6 maxregions=800 side=900 width=1
```

## 基线

`example` 里那张 1568×1712 游戏地图，默认参数（`levels=6 merge=64 minthick=1.2 tol=0.6`）：

| 版本 | outlines | points | recall | precision | f1 |
| --- | --- | --- | --- | --- | --- |
| 调优前（levels=5 / colors=24 / merge 未聚类 / minarea=24） | 555 | 12506 | 0.798 | 0.780 | 0.789 |
| 调优后 | 524 | 12451 | 0.889 | 0.894 | 0.892 |

扫参结论：`colorLevels=6` 最优（8 以上会把明暗层次切成碎块，precision 掉到 0.75 左右）；
`paletteMergeDistance` 56/64/72 都在 0.89–0.90，取 64 兼顾串色风险；
`minRegionThickness=1.2`（更细的条带直接丢，recall 会掉到 0.83）；
`simplifyTolerance` 主要影响点数，0.6 时圆弧不至于成多边形。
