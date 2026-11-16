# StoryBoard 快捷键现状梳理与重设计方案

> 依据代码：`src/UiActionFactory.cpp`、`src/UiMenuBuilder.cpp`、`src/CanvasContextMenuController.cpp`、
> `src/CanvasView.cpp`、`src/ShortcutController.cpp`、`src/selftest/`。
> 配置覆盖机制：`AppSettings::shortcutOverrides`，键为「默认快捷键的 PortableText」，可在
> 「帮助 → 快捷键一览…」里临时改键并持久化。

## 1. 现状清单（代码实测）

### 1.1 文件菜单

| 功能 | 当前默认快捷键 | 代码位置 | 备注 |
| --- | --- | --- | --- |
| 新建画布 | `Ctrl+N` | UiActionFactory.cpp:47 | 通用，保持 |
| 打开画布… | `Ctrl+O` | UiActionFactory.cpp:50 | 通用，保持 |
| 保存画布 | `Ctrl+S` | UiActionFactory.cpp:53 | 通用，保持 |
| 另存为… | `Ctrl+Shift+S` | UiActionFactory.cpp:56 | 通用，保持 |
| 插入图片… | `Ctrl+I` | UiActionFactory.cpp:59 | 见方案 |
| 导入图片线稿化 | `Ctrl+Shift+I` | UiActionFactory.cpp:62 | 保持 |
| 导出画布… | `Ctrl+E` | UiActionFactory.cpp:67 | 保持 |
| 退出 | `Ctrl+Q` | UiActionFactory.cpp:70 | Mac 习惯，Windows 标准应为 `Alt+F4` |

### 1.2 编辑菜单

| 功能 | 当前默认快捷键 | 代码位置 | 备注 |
| --- | --- | --- | --- |
| 撤销 | `Ctrl+Z` | UiActionFactory.cpp:73 | 通用，保持 |
| 重做 | `Ctrl+Y` | UiActionFactory.cpp:77 | README 声称也支持 `Ctrl+Shift+Z`，代码未绑定 |
| 删除选中图元 | `Delete` | UiActionFactory.cpp:81 | 通用；可补 `Backspace` 副键 |
| 副本（偏移复制） | `Ctrl+D` | UiActionFactory.cpp:84 | 通用，保持 |
| 组合 | `Ctrl+G` | UiActionFactory.cpp:98 | 通用，保持 |
| 取消组合 | `Ctrl+Shift+G` | UiActionFactory.cpp:114 | 通用，保持 |
| 简化选中曲线 | `Ctrl+Shift+M` | UiActionFactory.cpp:128 | 保持 |
| 全选 | `Ctrl+A` | UiActionFactory.cpp:143 | 通用，保持 |
| 图元属性… | `Q` | UiActionFactory.cpp:146 | 单字母、非标准，易误触 |
| 顺时针旋转 45° | `Ctrl+R` | UiActionFactory.cpp:149 | 保持 |
| 逆时针旋转 45° | `Ctrl+Shift+R` | UiActionFactory.cpp:152 | 保持 |
| 层内高度 +1（上移一层） | `]` | UiMenuBuilder.cpp:143 | 与 PS 图层上移一致，保持 |
| 层内高度 −1（下移一层） | `[` | UiMenuBuilder.cpp:150 | 与 PS 图层下移一致，保持 |
| 对齐 8 项（左/水平居中/右/上/垂直居中/下/水平分布/垂直分布） | 无 | UiMenuBuilder.cpp:168-194 | 菜单与卡片入口，不占键 |
| 合并图元 5 项（联合/组合/拆分/相交/剪除） | 无 | UiMenuBuilder.cpp:201-226 | 菜单与卡片入口，不占键 |

### 1.3 视图与工具模式

| 功能 | 当前默认快捷键 | 代码位置 | 备注 |
| --- | --- | --- | --- |
| 概览/平移模式 | `V` | UiActionFactory.cpp:156 | 通用手型键应为 `H` |
| 画笔模式 | `B` | UiActionFactory.cpp:159 | 通用，保持 |
| 编辑/选择模式 | `E` | UiActionFactory.cpp:162 | 通用指针键应为 `V` |
| 文字模式 | `T` | UiActionFactory.cpp:165 | 通用，保持 |
| 适应全部（回到全景） | `Ctrl+0`、`Home` | UiActionFactory.cpp:185-187 | 保持 |
| 放大 | `Ctrl+=` | UiActionFactory.cpp:189 | 保持；可补 `Ctrl++` 副键 |
| 缩小 | `Ctrl+-` | UiActionFactory.cpp:191 | 保持 |
| 实际大小 100% | `Ctrl+1` | UiActionFactory.cpp:193 | 保持 |
| 辅助网格开关 | 无 | UiActionFactory.cpp:196-207 | 需补键 |
| 图层面板显隐 | 无 | UiMenuBuilder.cpp:94-97 | QDock toggleViewAction，未绑键 |
| 图元编辑面板显隐 | 无 | UiMenuBuilder.cpp:99-105 | QDock toggleViewAction，未绑键 |
| 按住空格平移视图 | `Space`（长按） | CanvasView.cpp:362-367 | 通用，保持 |
| 取消当前笔画 | `Esc` | CanvasView.cpp:371-375 | 现状仅取消笔画，未清除选中 |

### 1.4 插入 / 样式 / 背景（均无快捷键）

| 功能 | 入口 | 备注 |
| --- | --- | --- |
| 常见图形 7 种 + 自定义图形 | 插入菜单 → 图形下拉 | 菜单项 + objectName `ShapeAction_*` |
| 填充样式 7 项（无/纯色/斜线/反斜线/交叉/网格/蜂窝） | 插入菜单 → 填充样式 | 互斥分组 |
| 填充颜色… | 插入菜单、工具栏 | objectName `FillColorAction` |
| 描线颜色… | 插入菜单、工具栏 | objectName `BorderColorAction` |
| 描线宽度 SpinBox | 菜单、工具栏、属性卡 | 控件，非动作 |
| 替换背景图 / 背景颜色 / 清除背景 | 插入菜单 | 三项动作均无键 |
| 背景填充方式（拉伸/适应/平铺） | 插入菜单 | 互斥分组，无键 |

### 1.5 其他

| 功能 | 当前默认快捷键 | 代码位置 | 备注 |
| --- | --- | --- | --- |
| 设置… | `Ctrl+,` | UiActionFactory.cpp:229 | 保持 |
| 快捷键一览… | 无 | UiActionFactory.cpp:234 | 建议 `F1` |
| 关于 StoryBoard / About Qt | 无 | UiActionFactory.cpp:232-236 | 不绑键 |

## 2. 现状问题

1. **工具键不符合通用习惯**：选择/编辑模式绑 `E`，主流绘图软件指针（选择）一律是 `V`；概览手型绑 `V`，主流手型是 `H`。
2. **图元属性绑单字母 `Q`**：非标准，画布输入/误触风险高；属性窗本就有双击与卡片入口。
3. **缺系统剪贴板三件套**：当前只有 `Ctrl+D` 副本，没有 `Ctrl+C` / `Ctrl+X` / `Ctrl+V`，也没有剪贴板实现。
4. **重做副键缺失**：README 写了 `Ctrl+Shift+Z`，代码只绑 `Ctrl+Y`，文档与实现不符。
5. **网格、面板、快捷键一览无键**：常用视图项缺少快捷入口。
6. **Esc 语义不完整**：只取消进行中的笔画，没有承担通用的「取消选中 / 退出当前状态」。
7. **退出键偏 Mac 习惯**：`Ctrl+Q` 之外未绑 Windows 标准 `Alt+F4`。
8. **无方向键微调**：主流软件方向键移动选中图元 1px、`Shift+方向键` 10px，当前未实现（可选增强）。

## 3. 重设计方案

设计原则：

- 文件、编辑通用项沿用 Windows 桌面与主流绘图软件（Figma / Photoshop / PPT）习惯；
- 单字母只留给工具，工具键对齐主流：指针 `V`、手型 `H`、画笔 `B`、文字 `T`；
- 视图缩放沿用 Photoshop 习惯；
- 低频、复杂的多选/对齐/合并/样式操作保留菜单与卡片入口，不占快捷键；
- 每个动作最多一个主键，允许少量通用副键（如重做、放大、退出）。

### 3.1 保持不变

| 功能 | 快捷键 | 依据 |
| --- | --- | --- |
| 新建 / 打开 / 保存 / 另存为 | `Ctrl+N` / `Ctrl+O` / `Ctrl+S` / `Ctrl+Shift+S` | Windows 通用 |
| 导入图片线稿化 | `Ctrl+Shift+I` | 现状合理 |
| 导出画布 | `Ctrl+E` | 现状合理 |
| 撤销 | `Ctrl+Z` | 通用 |
| 副本 | `Ctrl+D` | Figma / PPT 通用 |
| 组合 / 取消组合 | `Ctrl+G` / `Ctrl+Shift+G` | PPT / 通用 |
| 简化曲线 | `Ctrl+Shift+M` | 现状合理 |
| 全选 | `Ctrl+A` | 通用 |
| 上移一层 / 下移一层 | `]` / `[` | PS 图层上下移 |
| 顺时针 / 逆时针旋转 45° | `Ctrl+R` / `Ctrl+Shift+R` | 现状合理 |
| 适应全部 | `Ctrl+0` / `Home` | PS 习惯 |
| 放大 / 缩小 / 实际大小 | `Ctrl+=` / `Ctrl+-` / `Ctrl+1` | PS 习惯 |
| 画笔模式 / 文字模式 | `B` / `T` | 主流工具键 |
| 设置 | `Ctrl+,` | 通用 |
| 空格平移 | 长按 `Space` | 主流平移习惯 |

### 3.2 调整（键位变更）

| 功能 | 现状 | 新方案 | 依据 |
| --- | --- | --- | --- |
| 编辑/选择模式 | `E` | `V` | Figma 指针 `V`、PS 移动工具 `V` |
| 概览/手型模式 | `V` | `H` | Figma 手型 `H`、PS 手型 `H`；`E` 释放留作未来橡皮擦/形状工具 |
| 重做 | `Ctrl+Y` | `Ctrl+Y` + 副键 `Ctrl+Shift+Z` | Figma / PS / PPT 通用；补齐 README 已声明的副键 |
| 退出 | `Ctrl+Q` | `Ctrl+Q` 首位、副键 `Alt+F4` | Windows 标准关闭；首位保留 `Ctrl+Q` 是为了兼容旧配置里以该键名存储的覆盖项 |
| 删除 | `Delete` | `Delete` + 副键 `Backspace` | 双删除键通用 |
| 放大 | `Ctrl+=` | `Ctrl+=` + 副键 `Ctrl++` | 等号与加号同键位的两种写法 |

### 3.3 新增（当前无键或缺实现）

| 功能 | 新方案 | 前置条件 / 说明 |
| --- | --- | --- |
| 复制选中图元 | `Ctrl+C` | **需先实现系统剪贴板**：序列化选中图元到 `QMimeData`；未实现则不绑键 |
| 剪切选中图元 | `Ctrl+X` | 复制后删除原选中；依赖剪贴板实现 |
| 粘贴 | `Ctrl+V` | 从剪贴板还原，落点偏移；依赖剪贴板实现 |
| 辅助网格开关 | `Ctrl+'` | PS 网格开关 `Ctrl+'`；备选 `Shift+G` 已被取消组合占用 |
| 图层面板显隐 | `Alt+1` | 面板切换，避免占用单字母工具键 |
| 图元编辑面板显隐 | `Alt+2` | 同上 |
| 快捷键一览 | `F1` | 帮助键惯例 |
| Esc 清除选中 | `Esc` | 扩展现有 Esc：取消笔画后再清除选中（视图/控制器层处理） |
| 方向键微调（可选） | 方向键 1px，`Shift+方向键` 10px | 属交互增强，不在纯键位重排范围内 |

### 3.4 解绑 / 不绑键

| 功能 | 处理 | 说明 |
| --- | --- | --- |
| 图元属性… | 解绑 `Q` | 保留双击、属性卡与菜单入口；单字母不分配给非工具动作 |
| 对齐 8 项 | 不绑键 | 菜单 + 编辑图元卡入口 |
| 合并图元 5 项 | 不绑键 | 菜单 + 编辑图元卡入口 |
| 图形 / 填充样式 / 颜色 / 宽度 / 背景项 | 不绑键 | 数量多、低频，单字母会与工具键冲突 |
| 关于 / About Qt | 不绑键 | 菜单项即可 |

### 3.5 工具键位最终布局

| 键 | 模式 | 对应动作 |
| --- | --- | --- |
| `V` | 选择/编辑 | 选中、移动、缩放图元 |
| `H` | 概览/手型 | 全景查看、拖拽平移 |
| `B` | 画笔 | 自由绘制曲线 |
| `T` | 文字 | 单击放置文字图元 |
| `E` | 释放 | 预留给未来橡皮擦/形状工具 |

## 4. 待确认决策点

1. **工具键互换**：选择改 `V`、手型改 `H`，老用户肌肉记忆需要重新适应，是否接受？
2. **剪贴板**：本轮是否一并实现 `Ctrl+C` / `Ctrl+X` / `Ctrl+V`（含序列化写入/读取剪贴板）？若暂不实现，方案中这三个键不绑定。
3. **图元属性 `Q`**：确认解绑，仅保留双击与卡片入口？
4. **网格开关**：采用 `Ctrl+'`，还是另选（如 `Ctrl+G` 已占组合，`Shift+G` 已占取消组合，空间有限）？
5. **退出键**：主键 `Alt+F4` + 副键 `Ctrl+Q`，还是维持仅 `Ctrl+Q`？
6. **可选增强**：是否本轮补方向键微调、Esc 清除选中、面板 `Alt+1/2`、`F1` 快捷键一览？

## 5. 实施影响（确认方案后）

### 5.1 代码改动点

- `src/UiActionFactory.cpp`：各动作默认 `QKeySequence`；新增复制/剪切/粘贴、网格、面板、帮助动作。
- `src/UiMenuBuilder.cpp` / `src/CanvasContextMenuController.cpp`：高度 `]` / `[` 保持；面板动作绑键。
- `src/CanvasView.cpp`：Esc 语义扩展、方向键微调（若采纳）。
- 剪贴板：`CanvasScene` 增加序列化/反序列化选中图元到内部格式的接口。
- `src/MainWindow.h`：新增动作成员（剪贴板、面板/帮助若需要）。

### 5.2 自测断言需要同步

- `MainWindowFlowTests.cpp:433-434`：`]` / `[` 保持不变。
- `MainWindowFlowTests.cpp:444`：图元属性 `Q` 断言随解绑调整。
- `MainWindowShapeMenuTests.cpp:323`：`Ctrl+Shift+M` 保持不变。
- `AlignmentBoolGroupTests.cpp:156-158`：重做、放大当前断言「只保留一个组合键」；若加副键需放宽为校验主键存在。
- 工具键 `V` / `E` / `H` 相关用例同步更新。

### 5.3 配置兼容（`shortcutOverrides`）

- 覆盖项以「旧默认键的 PortableText」为键存储。默认键改动后，旧 `StoryBoard.ini` 里的覆盖键名匹配不到新默认，会成为无效残留。
- 本轮采用口径 2：**不做迁移**，忽略旧覆盖残留；用户可在「快捷键一览」里「恢复默认」。代码未按动作 `objectName` 重写旧键名。

### 5.4 文档同步

- `README.md` 快捷键表（64-81 行）随最终方案更新；修正重做副键文档与代码不符的问题。

## 6. 落地记录（决策已拍板并实现）

四项关键决策全部采纳：

1. **工具键互换**：选择/编辑 `V`、概览/手型 `H`、画笔 `B`、文字 `T`；`E` 释放预留。
2. **剪贴板三件套**：已实现 `Ctrl+C` / `Ctrl+X` / `Ctrl+V`，自定义 MIME `application/x-storyboard-items`，负载为图元 JSON 数组；粘贴偏移 `+20,20`、清分组、名称唯一；剪切 = 复制后删除选中。
3. **图元属性解绑 `Q`**：仅保留双击、属性卡与菜单入口。
4. **可选增强全部落地**：`Esc` 先取消进行中笔画、否则清除选中；方向键微调 1px、`Shift+方向键` 10px；图层面板 `Alt+1`、图元编辑面板 `Alt+2`；快捷键一览 `F1`；网格开关 `Ctrl+'`。

最终默认键位：

| 分组 | 快捷键 |
| --- | --- |
| 文件 | 新建 `Ctrl+N`、打开 `Ctrl+O`、保存 `Ctrl+S`、另存为 `Ctrl+Shift+S`、插入图片 `Ctrl+P`（副 `Ctrl+I`）、线稿化 `Ctrl+Shift+I`、导出 `Ctrl+E`、退出 `Ctrl+Q` / `Alt+F4` |
| 编辑 | 撤销 `Ctrl+Z`、重做 `Ctrl+Y` / `Ctrl+Shift+Z`、复制 `Ctrl+C`、剪切 `Ctrl+X`、粘贴 `Ctrl+V`、副本 `Ctrl+D`、删除 `Delete` / `Backspace`、组合 `Ctrl+G`、取消组合 `Ctrl+Shift+G`、全选 `Ctrl+A`、简化曲线 `Ctrl+Shift+M`、顺时针 `Ctrl+R`、逆时针 `Ctrl+Shift+R` |
| 工具 | 选择 `V`、手型 `H`、画笔 `B`、文字 `T` |
| 视图 | 适应全部 `Ctrl+0` / `Home`、放大 `Ctrl+=` / `Ctrl++`、缩小 `Ctrl+-`、实际大小 `Ctrl+1`、网格 `Ctrl+'`、图层面板 `Alt+1`、图元面板 `Alt+2`、帮助 `F1` |
| 其他 | 设置 `Ctrl+,`、层内高度 `]` / `[`、方向键微调（`Shift` 加速）、空格长按平移、`Esc` 取消笔画/清选中 |

实现要点：场景侧 `CanvasScene::clipboardItems()` / `pasteClipboardItems()` / `nudgeSelected()`，新文件 `src/scene/CanvasSceneClipboard.cpp`；控制器 `MainWindowEditActionsController` 编排剪贴板读写；粘贴动作启用状态跟随剪贴板 `dataChanged` 刷新。

## 7. 一览窗口与文档的分组排列（2026-11-16）

键位定稿后，「快捷键一览」窗口仍按动作文字字典序排三栏，同一功能的键位被拆到不同列，找键比按菜单找入口更费劲。改动：

- `ShortcutController::showShortcuts()`：先沿 `QMenuBar` 顺序递归收集菜单项里的动作（文件 → 视图 → 编辑 → 插入 → 帮助），再补上只有工具栏 / 卡片入口、没有菜单项的动作（按文字序）；同一菜单下的动作列出时天然相邻，换语言或改键都不影响分组。
- 排布仍是三栏、按列连续填充，一列内的条目保持原分组顺序，组与组之间不再交叉。
- README 的快捷键一节按同一分组拆成「文件与画布」「视图与工具模式」「编辑与图元」「其他」四张表；`CODE_REVIEW.md` 记录本次按代码复核的差异。
- 自检没有针对窗口排序的断言，基线仍为 850 passed / 0 failed。

## 8. 取消副键，每个动作只留一个主键（2026-11-16）

§6 表格里的副键以下一节为准、全部作废。§6 落地时给六个动作留了副键，实际使用中两套键位既没有增加便利，又让「改键 / 恢复默认」多一层歧义（换掉主键后副键仍在）。改为每个动作只保留一个键，取舍以「通用软件标准优先，其次是直觉」为准：

| 动作 | 删掉的副键 | 保留的主键 | 依据 |
| --- | --- | --- | --- |
| 插入图片 | `Ctrl+P` | `Ctrl+I` | `Ctrl+P` 是打印的通用标准键，不该占用；`I` 对应 image / insert，最直觉 |
| 重做 | `Ctrl+Shift+Z` | `Ctrl+Y` | Windows 桌面应用的通用标准 |
| 删除选中图元 | `Backspace` | `Delete` | Windows 标准；`Backspace` 在文本与浏览器场景带「回退」语义，容易误删 |
| 退出 | `Ctrl+Q` | `Alt+F4` | Windows 标准关闭窗口键 |
| 回到全景视图 | `Home` | `Ctrl+0` | Photoshop / Figma 的缩放归位习惯 |
| 放大 | `Ctrl++` | `Ctrl+=` | 与 `Ctrl++` 是同一个物理键，`Ctrl+=` 不必按 Shift，PS 与浏览器同样接受 |

改动点：`src/UiActionFactory.cpp` 去掉六处 `setShortcuts` 多键赋值，提示文字同步改成单键；`translations/storyboard_zh.ts` 里带旧键位的译文跟着改；`src/selftest/ClipboardShortcutTests.cpp` 的 `hasShortcut` 换成 `hasOnlyShortcut`（要求 `shortcuts().size() == 1`），新增「回到全景」「退出」两条用例；`AlignmentBoolGroupTests` 的重做 / 放大用例补上单键判断。

配置兼容沿用 §5.3 的口径：`shortcutOverrides` 以默认键的 PortableText 为键名，默认键变了以后旧覆盖条目匹配不到新默认，属于无效残留，在「快捷键一览」里点「恢复默认」即可清空。

## 9. 副键二次修订：恢复操作系统级副键（2026-11-16）

§8 的「全部单键」再修正一次：自创的、与功能无关的副键仍然不要，但**操作系统级的标准键恢复为副键**；自创组合键保持少量，且一律取自英文词义。

| 动作 | 恢复的副键 | 依据 |
| --- | --- | --- |
| 重做 | `Ctrl+Shift+Z` | macOS / GNOME 级重做标准，与主键 `Ctrl+Y` 并存 |
| 退出 | `Ctrl+Q` | Linux / macOS 级退出标准，主键仍为 `Alt+F4` |
| 放大 | `Ctrl++` | 系统级放大标准绑定（含小键盘加号），与 `Ctrl+=` 同义并存 |

不恢复的三个：`Ctrl+P`（打印的 OS 级标准键，语义冲突）、`Backspace`（非 OS 级删除标准，带回退语义易误删）、`Home`（自创绑定，非 OS 级）。自创键位收敛后的清单与英文依据：`Ctrl+I`=Insert、`H`=Hand、`E`=Edit、`B`=Brush、`T`=Text、`Ctrl+R`=Rotate；`Alt+1` / `Alt+2` 是面板索引约定；`]` / `[` 仅编辑模式的层内高度微调，为仅有的两处符号键。

编辑模式键由 `V` 改为 `E`（Edit 取自英文词义；`V` 是 PS 移动工具的沿用，不符合本原则）。`E` 与导出的 `Ctrl+E` 不冲突（修饰键不同）。

改动点：`src/UiActionFactory.cpp` 恢复三处 `setShortcuts` 双键赋值，编辑模式改绑 `E`；`ClipboardShortcutTests` 新增 `hasExactShortcuts` 按「主键在前、副键在后」整组校验重做 / 放大 / 退出，编辑模式断言改 `E`；`AlignmentBoolGroupTests` 重做 / 放大改断言双键。配置兼容不变：`shortcutOverrides` 仍以默认主键的 PortableText 为键名（`Ctrl+Y` / `Alt+F4` / `Ctrl+=`），主键未变、旧覆盖条目继续有效；手动改键会把动作整体换成单键，点「恢复默认」找回整组。

## 10. 键位再收敛：编辑模式回 `V`，两个弱词源组合键替换（2026-11-16）

§9 的 `E` 方案作废：主流绘图软件的选择 / 指针工具一律是 `V`（PS 移动工具、Figma 指针），行业直觉优先于 Edit 字面；同时清理两个说不出清晰词源的自创组合键。

| 功能 | 旧键 | 新键 | 依据 |
| --- | --- | --- | --- |
| 编辑 / 选择模式 | `E` | `V` | PS / Figma 指针工具通用键 |
| 导入图片线稿化 | `Ctrl+Shift+I` | `Ctrl+L` | Line / Lineart 线稿，词源清晰 |
| 简化选中曲线 | `Ctrl+Shift+M` | `Ctrl+B` | Brush 画笔：简化的对象是画笔（Brush）生成的曲线；与画笔单键 `B` 靠修饰键隔离，不冲突 |

现存自定义组合核验：`Ctrl+I` Insert、`Ctrl+R` Rotate、`Ctrl+E` Export、`Ctrl+L` Lineart、`Ctrl+B` Brush（简化画笔曲线）、`Ctrl+G` / `Ctrl+Shift+G` Group / Ungroup、`Ctrl+D` Duplicate、`Ctrl+'` 网格（Figma）；工具单键 `H` / `V` / `B` / `T`（PS / Figma）；符号特例 `]` / `[` 与面板索引 `Alt+1` / `Alt+2`。无剩余词源不明的功能组合键。

改动点：`UiActionFactory.cpp` 三处键位与提示文字；`ClipboardShortcutTests` 编辑模式断言改 `V`；`MainWindowShapeMenuTests` 简化曲线断言改 `Ctrl+B`；`storyboard_zh.ts` 同步旧键位译文。配置兼容：本轮未改动副键与配置键名（`Ctrl+Y` / `Alt+F4` / `Ctrl+=` 不变）；线稿化与简化曲线的旧 `shortcutOverrides` 条目匹配不到新默认，按 §5.3 口径属无效残留，「恢复默认」即可清空。
