# StoryBoard 代码审查与回归记录

审查对象：Qt 6 / C++17 / CMake 工程；初查时间：2026-11-07 至 2026-11-09，2026-11-14 按最新代码复核。本文记录审查结论，功能细节以 [README](README.md) 为准。

## 已修复缺陷

| 编号 | 问题 | 根因 | 修复 |
| --- | --- | --- | --- |
| B1 | 设置对话框改的画笔颜色、画布外围颜色保存不下来 | 取色写入局部副本，`accepted` 只回写部分字段 | 改为 `pickColor()` 返回值语义，新增 `setBrushColor()` / `setCanvasColor()` 作为唯一写入口 |
| B2 | 选中图元后控制点外半区点不中，椭圆 / 三角形等四角手柄几乎拖不动 | `shape()` 只返回内容矩形或图形路径，与画出的手柄区域不一致 | `CanvasItem::selectionHandleShape()` 把 8 个手柄矩形外扩 2 px 并入命中区域 |
| B3 | 粗描边图形的描边区域点不中 | `ShapeItem::shape()` 只认填充路径，忽略线宽外扩 | 封闭图形用 `QPainterPathStroker` 生成描边环并合并命中；箭头类保持精确路径 |
| B4 | 重复设置同一张背景图仍产生撤销步骤 | 用 `QPixmap::cacheKey()` 比较，同像素不同对象判不等 | `BackgroundState::operator==` 改为按 `QImage` 逐像素比较 |

新增回归：控制点命中 4 项、粗描边命中 2 项、背景内容等价与去重 2 项、设置颜色回写 2 项。

## 当前回归

- 自检基线：**853 passed, 0 failed**（2026-11-16 实跑 `out15/bin/StoryBoard.exe --self-test`，Windows 11 + GCC 15.1 + Qt 6.12）。
- 主干已完成场景、自检、MainWindow 与界面卡片按职责拆分；编辑图元卡默认悬浮、停靠时常驻，视图模式卡在右上，图层卡在右下。
- 实机环境：Windows 11 + GCC 15.1 + Qt 6.12；便携包经 `windeployqt` 部署后，隔离 PATH 运行包内自检同样通过。
- macOS / Linux 分支已提供构建与打包路径，尚未在对应主机实机验证。

## 静态审查修复记录（2026-11-16，本地主干实修）

静态审查节的 P0/P1/P2 已全部在主干修复并构建自测通过，提交序列：补丁应用 `2f3fa9c` → P0 `4e75c6a` → 双份括号 `7da390b` → nudge `bb6605d` → 键盘跟踪 `feaaecd` → P2 `02eac70` → I18n `bb8c497` → CLI 线稿化 `8914846` → 合并遮挡 `d0f8fcc` → 文档回填 `6b11893`。

| 级别 | 修复内容 | 落点 |
| --- | --- | --- |
| P0 | `fromJson` 全部校验与图元预创建通过后才 `resetDocument()`，坏文件不再清空当前文档；画布尺寸按文件原值加载不钳位；`validateCanvasRoot` 补 layers 非空检查（失败发生在 `maybeSave` 消耗未保存内容之前） | `CanvasSceneSerialization.cpp`、`DocumentController.cpp` |
| P1 | `stripShortcutTail()` 共享：`makeAction` 基串入库前剥掉尾部键位括号，双份括号根除 | `UiActionFactory.*`、`ShortcutController.cpp` |
| P1 | 方向键微调按最外层组树展开；`GeometryCommand` 增加 mergeId/mergeWith，连续 nudge 并成一条撤销记录 | `CanvasSceneClipboard.cpp`、`Commands.*`、`CanvasSceneItems.cpp` |
| P1 | 属性卡数字框全部 `setKeyboardTracking(false)`，输入过程不再触发提交与刷新 | `ItemDockPanelValueCard.cpp`、`ItemDockPanelFillBorderCard.cpp` |
| P2 | 多选上移/下移「全或不动」；`normalizeLayerHeights` 修复压入撤销栈（`m_restoringDocument` 守卫排除加载期）；图片图元保留原始 base64 字节防再保存丢图；`pathFromJson` 短曲线段降级折线；`nextHeight -1` 退化取同层最大高度 +1；generator 版本号随 CMake `STORYBOARD_VERSION` 注入 | `CanvasSceneItemStacking.cpp`、`CanvasScene.h`、`ImageItem.*`、`CompositeShapeItem.cpp`、`CanvasSceneSerialization.cpp` |
| 合并遮挡 | 补丁的优先级排序曾连带改变结果层绘制顺序（零面积图元被先画、被有填充图元盖掉）。改为：优先级只决定样式 donor，绘制顺序按合并前叠放序号 `stackRank`；文字合并用例断言随 donor 语义更新（字形保持可见 + 零描边） | `BoolOps.cpp`、`AlignmentBoolGroupTests.cpp` |
| 新功能 | 命令行线稿化：`--lineart 图片 [--lineart-out 画布.sbcanvas]`，默认均衡档 + 1px 最细条带；`--lineart-preset` 选档位，`--lineart-max-side / -color-levels / -palette-colors / -palette-merge / -adjacent-merge / -min-area / -max-regions / -adaptive / -adaptive-limit / -min-adaptive-side / -tolerance / -min-thickness` 全量覆盖转换参数（线宽按需求固定 1） | `CommandLineOptions.*`、`AppModes.cpp` |
| I18n | 关于窗口键位文案对齐现行键位（H 查看 / V 编辑 / 新组合键行），`storyboard_zh.ts` 同步；`Ctrl+B` 词源更正为 Brush | `MainWindowAppController.cpp`、`translations/storyboard_zh.ts`、`docs/shortcut-redesign.md` |

## 参考与引用

- 交互与图标参考：Excalidraw（填充样式与色块交互思路）、PPT（合并图元与五项子操作的命名与图标语义）。
- 未拷贝其源码、图标或资源；无 `third_party` 目录、无第三方版权头或 SPDX 标识。
- 构建依赖 Qt 6、CMake 与编译器工具链，不计入参考代码；引入源码 0 个。

## 文档一致性核对（2026-11-16）

按 `main`（`2dd2a18`）的实际代码逐条复核 README，已全部落实：

- 图元属性窗不绑键（入口为双击、编辑菜单项与编辑图元卡），README 移除「按 `Q`」。
- 编辑图元卡宽度：启动取 260–320 px，可拖拽范围锁在 260–360 px。
- 快捷键一览与 README 表改为沿菜单栏顺序分组（文件 → 视图 → 编辑 → 插入 → 帮助），同一功能的键位互相靠近。
- `]` / `[` 与方向键微调标注「仅编辑模式」。
- 副键口径：先按「通用标准优先、其次直觉」砍成单键，再恢复操作系统级副键（重做 `Ctrl+Shift+Z`、退出 `Ctrl+Q`、放大 `Ctrl++`；`Ctrl+P` / `Backspace` / `Home` 不恢复）；编辑模式回到 `V`，线稿化 `Ctrl+L`、简化曲线 `Ctrl+B`（Brush 词源）。自测只断言绑定键生效且互不冲突，不做副键整组守卫。
- 关于窗口旧「Key operations」文案已连同 `I18n.cpp` / `storyboard_zh.ts` 修复（`bb8c497`）。
