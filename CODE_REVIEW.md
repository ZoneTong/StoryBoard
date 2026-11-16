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

静态审查节的 P0/P1/P2 已全部在主干修复并构建自测通过，提交序列：补丁应用 `2f3fa9c` → P0 `4e75c6a` → 双份括号 `7da390b` → nudge `bb6605d` → 键盘跟踪 `feaaecd` → P2 `02eac70` → I18n `bb8c497` → CLI 线稿化 `8914846` → 合并遮挡 `d0f8fcc`。

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

按 `main`（`2dd2a18`）的实际代码逐条复核 README，改动与结论如下：

| 项目 | 代码实际 | 处理 |
| --- | --- | --- |
| 图元属性窗的快捷键 | `m_itemPropsAct` 不绑键，入口是双击、编辑菜单项与编辑图元卡 | README 原写「按 `Q`」已改，并注明后两个入口不占键 |
| 编辑图元卡宽度 | `ItemDockPanel::preferredWidth()` 启动取 260–320 px，`DockPanelController::applyItemDockWidth()` 把可拖拽范围锁在 260–360 px | README 补上 360 px 上限 |
| 快捷键一览的排列 | 原按动作文字字典序排三栏，功能相关的键位被拆散 | 改为沿菜单栏顺序排列（文件 → 视图 → 编辑 → 插入 → 帮助），无菜单项的动作按文字序补末尾 |
| README 快捷键表 | 单表混排文件 / 编辑 / 视图 / 其他 | 拆成文件与画布、视图与工具模式、编辑与图元、其他四张分组表 |
| `]` / `[` 与方向键 | `UiMenuBuilder` 与 `CanvasView::keyPressEvent` 都要求编辑模式 | README 标注「仅编辑模式」 |
| 副键 | 插入图片、重做、删除、退出、全景、放大原本各挂一个副键 | 先按「通用标准优先、其次直觉」砍成单键，再恢复操作系统级副键：重做 `Ctrl+Shift+Z`、退出 `Ctrl+Q`、放大 `Ctrl++`（`Ctrl+P` / `Backspace` / `Home` 不恢复）；编辑模式回到 `V`（PS / Figma 指针），线稿化 `Ctrl+L`、简化曲线 `Ctrl+B`，自测按整组断言 |

遗留一项未改：关于窗口「Key operations」仍是旧口径（`V` 查看模式、`E` 编辑模式、编辑模式按住 `Ctrl` 平移），与现行 `H` / `V` 键位和「`Ctrl` 用于加选 / 减选」不符；这段文字走 `uiText()` 字典，需连同 `src/I18n.cpp` 与 `translations/storyboard_zh.ts` 一起改，留待单独提交。

## 代码审查（2026-11-16，静态）

审查范围：`main`（`2dd2a18`）+ 本分支的文档 / 快捷键改动；只读静态审查，云主机无 Qt 6，未编译、未跑自检，结论给出代码位置与置信度；标注「可复现」的路径由代码行号直接支撑，标「潜在」的需要实机确认。

### 结论摘要

| 级别 | 问题 | 影响 | 位置 |
| --- | --- | --- | --- |
| P0 | 打开「半坏」画布文件会先清空当前文档，`m_filePath` 仍指上一个文件，之后一次保存就把空画布写回原文件 | 用户既有画布被永久覆盖 | `CanvasSceneSerialization.cpp:128` / `DocumentController.cpp:185` |
| P1 | 提示文字出现双份快捷键括号 | 全部经 `makeAction` 的动作在启动时即显示「… (Ctrl+N) (Ctrl+N)」 | `UiActionFactory.cpp:29` / `ShortcutController.cpp:114-136` |
| P1 | 方向键微调与鼠标拖动的「组」语义不一致 | 局部选中组员时按方向键会把组拆散 | `CanvasSceneClipboard.cpp:74` 对比 `CanvasItemInteraction.cpp:79-89` |
| P1 | 图元卡数字框没有关闭键盘跟踪 | 每敲一个字符就改一次图元并压一条撤销 | `ItemDockPanelValueCard.cpp` / `ItemDockPanelFillBorderCard.cpp` |
| P2 | 多选「上移 / 下移」在层底部会把移动变成互换 | 叠放顺序与直觉不符（可撤销，不损坏文件） | `CanvasSceneItemStacking.cpp:109` |
| P2 | 高度归一化不压撤销 | `addItemsWithUndo` 顺带重排整层高度，历史里没有这一步 | `CanvasSceneItemStacking.cpp:54-96` |
| P2 | 画布尺寸 > 12000 的文件打开后被静默钳位 | 再保存即改写用户文件里的尺寸 | `CanvasSceneCanvas.cpp:26` |
| P2 | 图片解码失败时图元静默变成「Missing image」 | 再保存即永久丢掉原图数据 | `ImageItem.cpp:65-76` |

### P0 数据损坏级：打开失败会清空当前文档

复现（代码可达，无需构造极端文件）：把任意一个 `.sbcanvas` 的 `layers` 数组删掉或置空（`format`、`canvas`、`items` 都合法），打开时：

1. `DocumentController::loadCanvasFile` 的 `validateCanvasRoot`（`DocumentController.cpp:49-87`）只校验 `format`、画布宽高与图元 `type`，**不校验 `layers`**，所以放行；
2. 接着 `maybeSave()` 提示保存当前文档，用户选「不保存」即丢掉当前内容；
3. `CanvasScene::fromJson` 在 `CanvasSceneSerialization.cpp:128` 已经 `resetDocument()`（清场景、清图层、清撤销栈）之后，才在 `:157` 发现 `layers` 缺失并 `fail`；
4. `loadCanvasFile` 在 `:178` 返回失败，**没有把 `m_filePath` 清空**（它只在成功分支 `:185` 赋值），于是窗口仍认为当前文档就是刚才那个旧文件，`m_saveAct` 全程没有禁用逻辑；
5. 用户随手 `Ctrl+S`（或再画点东西后保存），空画布被写回旧文件；关闭程序也因为撤销栈已清空而不再提示保存。

同一条路径还有第二个触发点：`CanvasSceneSerialization.cpp:213-215` 的未知类型分支同样是「先 reset，再失败」。`validateCanvasRoot` 目前把已知类型都拦下了，所以这条暂时打不到，但只要哪天新增图元类型而只改了一边，就会立刻变成同款事故。

修复建议（三条都做，任一单独做都还有漏网分支）：

- `CanvasScene::fromJson` 只在**全部校验通过后**才 `resetDocument()`：把 `layers` 非空 / 图元类型白名单 / 画布尺寸这三段检查提到 `:128` 之前，解析失败时不动现有场景；
- `validateCanvasRoot` 补 `layers` 必须是非空数组的检查，让「文件坏」在 `maybeSave()` 之前就返回，不消耗用户的未保存内容；
- `loadCanvasFile` 失败分支清空 `m_filePath`（或把窗口切到「未命名空画布」状态），保证失败后不可能用旧路径覆盖原文件。

### P1 双份括号（设计与实现不符）

`UiActionFactory::makeAction` 把作者写好的提示（已带「(Ctrl+N)」）原样存进 `storyBoardBaseTip`（`UiActionFactory.cpp:29`），只在局部变量 `tip` 上做了去重（`:32`）；而 `ShortcutController::refreshShortcutTips` 取到的是那份**未去重**的基串，再拼一次 `" (" + keys + ")"`（`ShortcutController.cpp:135-136`）。

`applyShortcutOverrides()` 末尾无条件调用 `refreshShortcutTips()`（`ShortcutController.cpp:105`），`MainWindow` 构造时就会跑一遍（`MainWindow.cpp:81`），所以**不用改任何键，启动后所有经 `makeAction` 的提示就已经是「Create an empty canvas (Ctrl+N) (Ctrl+N)」**；`changeShortcut` 之后再刷新还会继续叠加第三份。源码注释写的是「已有的 “(Ctrl+X)” 不会重复追加」，实现没做到。

修复：`makeAction` 存基串前剥离尾部键位括号（复用 `refreshShortcutTips` 里的正则，抽成 `stripShortcutTail()`）；或 `refreshShortcutTips` 先去掉基串尾部的 `(…)` 再拼。前者更符合「基串 = 不带键位的纯描述」这个约定，建议顺带把该正则移到一处共享。

### P1 方向键组语义与拖动不一致

- 鼠标拖动：`CanvasItemInteraction::beginMove` 把**同一棵组树的所有成员**收进快照（`CanvasItemInteraction.cpp:79-89`），组员不必选中，所以拖动一定是整组移动；`CanvasView::mousePressEvent` 还会先把单击展开成整组选择（`CanvasView.cpp:255-272`）。
- 方向键：`CanvasScene::nudgeSelected` 只取 `selectedCanvasItems()`（`CanvasSceneClipboard.cpp:80-88`），**不做组树展开**。

`Ctrl` 点击减选与框选（`Qt::ContainsItemShape` 橡皮筋）都能造出「只选中部分组员」的状态，此时按方向键只移动被选中的那几个，撕裂组内相对位置；而同样这个选择用鼠标拖一下，整组会一起走。README 写的是「拖动任一成员即整组移动」，方向键没跟上这条规则。

修复：`nudgeSelected` 在收集目标时对每个选中图元补 `itemsInGroupTree(outermostGroup(groupId))`（与 `beginMove` 同一套过滤：`isItemEditable` + 去重），保证两条移动路径同语义。

附带问题（同一条代码路径）：`nudgeSelected` 每按一次都 `pushGeometry` 一条 `"Nudge items"`；长按自动重复 100 px 就压 100 条，`MainWindow.cpp:48` 的 `setUndoLimit(300)` 很容易被挤满并丢弃更早的编辑历史。建议给 `GeometryCommand` 加 `setCommandId` + `mergeWith`（同方向、同目标、连续按键合并成一条），也可顺带解决拖拽后连续微调各留一条的问题。

### P1 卡片数字框的键盘跟踪

全仓库没有一处 `setKeyboardTracking(false)`（`grep keyboardTracking` 无命中）。`ItemDockPanelValueCard`（透明度 0–100、圆角 0–400、扫过角 1–359、形状半径 1–1000）与 `ItemDockPanelFillBorderCard`（描线宽度 1–64）都用默认 `keyboardTracking = true`，而 `valueChanged` 直连处理器：`ItemDockPanel::applyOpacity` / `applyBorderWidth` / `changeSelectedShapes` 每收到一次值就**改图元 + `push` 一条命令 + `refresh()`**（`ItemDockPanel.cpp:480-540`、`:765-778`）。

后果：想输「50」时先按下的「5」会立刻把透明度改成 5% 并压一条撤销，再按「0」又压一条；同时 `refresh()` 在输入过程中 `setValue()` 回填，会和正在编辑的输入框抢文本与光标。`m_refreshing` 只是防「刷新 → 回写」的回环，挡不住这条逐字符提交路径。属性对话框那套（`ItemPropertiesDialog` 的 `QDoubleSpinBox`）只在 `accepted` 时写模型，没有这个问题，卡片与它不一致。

修复：卡片的数字框统一 `setKeyboardTracking(false)`（输入完按回车或失焦才提交），`apply*` 里的 `refresh()` 在 `hasFocus()` 时跳过回填；再把 `ItemOpacityCommand` / `ItemStrokeStyleCommand` 一类加 `mergeWith`，让逐次微调合成一条撤销。

### P2 其余发现

- 多选上移 / 下移：`adjustSelectedHeights` 逐个调用 `adjustItemHeight`，碰上下方已有图元就走「互换」分支（`CanvasSceneItemStacking.cpp:121-136`）。底层两个图元（height 0、1）同时下移时，结果是两者互换而不是都不动，顺序反而变了。同类 `moveSelectedItemsToAdjacentLayer` 也有逐项独立处理的隐患。
- 高度归一化：`normalizeLayerHeights`（`CanvasSceneItemStacking.cpp:54-87`）发现重复 / 非法 height 就把整层重排成 `1..n` 并 `applyItemStacking`，**不压命令**；`addItemsWithUndo` 每次都会调 `normalizeAllHeights()`（`CanvasSceneItemLifecycle.cpp:184`），所以这类重排可能夹在别的一次操作里发生，撤销不会还原高度与 z。
- 画布尺寸：`applyCanvasRect` 把宽高钳在 200–12000（`CanvasSceneCanvas.cpp:26`），而 `validateCanvasRoot` 只要求 > 0；已有 > 12000 的画布文件打开即被缩到 12000，保存后文件里的尺寸就被改写了。建议加载时按文件原值保留、只在编辑操作里钳位，或至少给一条提示。
- 图片图元：`ImageItem::toJson` 用 `QPixmap::save(..., "PNG")`，失败时 `image` 字段缺失，`fromJson` 拿到空字节后静默降级为「Missing image」占位（`ImageItem.cpp:55-76`），再保存就把原图数据彻底丢掉了。建议编码失败时保留原文件路径或直接拒绝保存并报警。
- `CompositeShapeItem::pathFromJson`（`CompositeShapeItem.cpp:298-327`）对不足 3 个控制点的曲线组静默丢弃，坏文件里的多边形会缺一段边，无提示。
- `ItemHeightPlanner::nextHeight` 在整层 0–999999 全占满时返回 `-1`，`fromJson` 直接 `qMin(kMaxItemHeight, -1)` 会写入 -1，随后靠 `normalizeAllHeights()` 兜底（`CanvasSceneSerialization.cpp:246-256`）；`kMaxItemHeight = 999999` 实际极难触发，但这条「先写非法值再修」的链路建议改成明确拒绝。
- 序列化里 `"generator": "StoryBoard 0.2"` 是硬编码，与当前版本号不一致，只影响文件可读性，不影响加载。
