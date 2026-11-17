"""线稿化相似度评估。

真值边缘：把原图缩到线稿化的处理尺寸后逐通道算 Sobel 梯度，三通道取最大值，
超过阈值的像素算“真值边缘”（彩色边界和明暗边界都能抓到）。

- recall    真值边缘像素里，落在生成笔画 2px 内的比例（漏描了多少）
- precision 生成笔画像素里，落在真值边缘 2px 内的比例（画了多少没用的）
- f1        两者调和平均

用法：
    python metrics.py <outDir> <source.png>
"""

import json
import os
import sys

import numpy as np
from PIL import Image

EDGE_THRESHOLD = 40.0
TOLERANCE_RADIUS = 2


def load_edges(path, size):
    im = Image.open(path).convert("RGB").resize(size, Image.LANCZOS)
    a = np.asarray(im).astype(np.float32)
    grad = np.zeros(a.shape[:2], dtype=np.float32)
    for c in range(3):
        ch = a[:, :, c]
        gx = np.zeros_like(ch)
        gy = np.zeros_like(ch)
        gx[:, 1:-1] = ch[:, 2:] - ch[:, :-2]
        gy[1:-1, :] = ch[2:, :] - ch[:-2, :]
        grad = np.maximum(grad, np.hypot(gx, gy))
    return im, grad


def dilate(mask, radius):
    out = mask.copy()
    h, w = mask.shape
    for dy in range(-radius, radius + 1):
        for dx in range(-radius, radius + 1):
            ys = slice(max(0, dy), h + min(0, dy))
            xs = slice(max(0, dx), w + min(0, dx))
            ys2 = slice(max(0, -dy), h + min(0, -dy))
            xs2 = slice(max(0, -dx), w + min(0, -dx))
            out[ys, xs] |= mask[ys2, xs2]
    return out


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    out_dir = sys.argv[1]
    src = sys.argv[2] if len(sys.argv) > 2 else os.path.join(out_dir, "source.png")

    with open(os.path.join(out_dir, "outlines.json"), encoding="utf-8") as f:
        data = json.load(f)
    size = (data["width"], data["height"])

    _, grad = load_edges(src, size)
    edge = grad > EDGE_THRESHOLD
    print("edge pixels: %d (%.2f%%)" % (edge.sum(), 100.0 * edge.sum() / edge.size))

    stroke_img = Image.open(os.path.join(out_dir, "strokes.png")).convert("RGB")
    stroke_arr = np.asarray(stroke_img)
    stroke = np.abs(stroke_arr.astype(np.int16) - 255).sum(axis=2) > 8

    stroke_near = dilate(stroke, TOLERANCE_RADIUS)
    edge_near = dilate(edge, TOLERANCE_RADIUS)
    recall = float((edge & stroke_near).sum()) / max(1, edge.sum())
    precision = float((stroke & edge_near).sum()) / max(1, stroke.sum())
    f1 = 0.0 if recall + precision == 0 else 2 * recall * precision / (recall + precision)
    print("outlines=%d points=%d regions=%d ms=%s truncated=%s"
          % (len(data["outlines"]), data["totalPoints"], data["regions"],
             data["ms"], data["truncated"]))
    print("recall=%.3f precision=%.3f f1=%.3f" % (recall, precision, f1))
    print("stroke pixels: %d" % stroke.sum())
    return 0


if __name__ == "__main__":
    sys.exit(main())
