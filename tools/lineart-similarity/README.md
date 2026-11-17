# 线稿化相似度实验台

`LineArtConverter` 的调参与回归工具：单独编译一个小程序，把线稿结果落成 `outlines.json` + `strokes.png`，再用 `metrics.py` 与原图边缘比较。主程序不依赖本目录。

## 指标

- 真值边缘：原图缩到处理尺寸（默认长边 900）后逐通道求 Sobel 梯度取最大值，阈值 40。
- `recall`：真值边缘像素落在生成笔画 2 px 内的比例。
- `precision`：生成笔画像素落在真值边缘 2 px 内的比例。
- `f1`：两者调和平均。

指标偏宽松，适合横向比较参数，不适合当绝对分数。

## 构建与运行

Linux / macOS：

```bash
QT_DIR=${QT_DIR:-$HOME/Qt/6.12.0/gcc_64}
g++ -O2 -std=c++17 \
  -I"$QT_DIR/include" -I"$QT_DIR/include/QtCore" -I"$QT_DIR/include/QtGui" \
  -I../../src lineart_dump.cpp ../../src/LineArtConverter.cpp \
  -o lineart_dump -L"$QT_DIR/lib" -lQt6Gui -lQt6Core

IMG=$(ls ../../examples/*.png | head -n 1)
./lineart_dump "$IMG" run
PY=$(command -v python3 || command -v python)
"$PY" -m pip install pillow   # 首次运行需要 Pillow
"$PY" metrics.py run "$IMG"
```

Windows 在 Git Bash 中先准备 GCC 15 与 Qt 的 `PATH`，并把 `QT_DIR` 指向 `mingw_64`：

```bash
export PATH="/d/Qt/Tools/mingw1510_64/bin:/d/Qt/6.12.0/mingw_64/bin:$PATH"
QT_DIR=/d/Qt/6.12.0/mingw_64
```

其余命令相同，产物为 `lineart_dump.exe`；运行、指标脚本均可在 bash 下执行。

参数按键值对传入，留空即使用默认值：

```text
levels=6 colors=24 merge=48 adjmerge=48 minarea=12 minthick=1.0 tol=0.6 maxregions=0 side=900 width=1
```

## 基线

示例图 1568×1712，默认档位“均衡”：

| 档位 | levels / merge / adjmerge / minarea / tol | 轮廓 | 点数 | recall | precision | f1 |
| --- | --- | --- | --- | --- | --- | --- |
| 保留细节 | 6 / 40 / 44 / 8 / 0.5 | 1088 | 15563 | 0.932 | 0.916 | 0.924 |
| 均衡（默认） | 6 / 48 / 48 / 12 / 0.6 | 900 | 15964 | 0.962 | 0.906 | 0.933 |
| 线条干净 | 6 / 64 / 56 / 20 / 0.8 | 551 | 10098 | 0.885 | 0.935 | 0.909 |
| 大块概括 | 6 / 80 / 56 / 32 / 1.0 | 429 | 6880 | 0.856 | 0.933 | 0.893 |

结论：

- `colorLevels=6` 最稳；更高会把明暗层次切成碎块。
- `paletteMergeDistance` / `adjacentMergeDistance` 在 44–52 是安全区，48 时小道具保留最好。
- `simplifyTolerance` 主要影响点数，对边缘重合指标影响很小，因此跟随档位在 0.5–1.0 取值。
- 产品默认 `maxRegions=0`（不截断）；块数超过 2000 时走 `adaptiveDownscale` 缩小处理尺寸重算，本实验台可用 `maxregions` 复现旧的截断结果。
