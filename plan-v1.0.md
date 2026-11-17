# StoryBoard-九项需求总方案.md_

# StoryBoard 九项需求总方案（设计定稿）

|项|内容|
|-|-|
|项目|StoryBoard（Qt 6 / C++17 / CMake 轻量画板）|
|仓库|D:\workspace\github.com\ZoneTong\StoryBoard|
|文档性质|需求收集与软件方案设计（只读调查，不改仓库）|
|状态|ADOPTED，9 条需求决策全部闭合；遗留 3 个待校准参数（见 §9）|
|版本|v1.0|
|日期|2026-11-17|

本方案不含代码实现。所有"现状"结论均可回链到本轮实际读取的源码（形如 path:line）；未读到的位置明确标注为待实施时核对。

## 0. 摘要

九条需求分三类：

1. 1. 度量体系重定（需求 1、8）：棋盘格 32px、画布 1536×1024、图形基准 64px、三角形角度优先、A 边刻度线。
2. 图元能力扩展（需求 3、4、5、6）：直线/曲线箭头、圆角并入矩形、合并图元按 PPT 逻辑重做、直线箭头预置项。
3. 素材与数据互通（需求 2、7、9）：Excalidraw 双向、25 个儿童故事模板、shapes 目录分组 + 标签页面板。


由此产生的四个新机制（其余均为既有模块改造）：

- 预置项机制：菜单项从"只带类型"升级为"类型 + 预置参数 + 显示名"。
- 选择顺序记录：场景需维护 selection sequence，服务"剪除以先选中为底"与"多选以先选为样式基准"。
- Chord 控制器：Ctrl+M 前缀 + 超时状态机，不进已有快捷键覆盖表。
- CustomShapeCatalog + CustomShapePanel：分组扫描+缓存的单一数据源，供独立浮动面板渲染。


## 1. 取证基线

|区域|关键事实|证据|
|-|-|-|
|画布默认|1920×1080|src/Settings.h:16、src/CanvasScene.h:216|
|启动视图|fitAll()，3.5% 边距|src/MainWindow.cpp:95-99、src/CanvasZoomController.cpp:23-50|
|棋盘格|仅屏幕+背景无色+非导出，按画布 16×16 等分|src/scene/CanvasSceneCanvas.cpp:238-241,286-311|
|棋盘格自检|固化等分与像素|src/selftest/SceneCanvasIoTests.cpp:314-315|
|辅助网格|step 40 / 主网格每 5 条 200，默认关闭|CanvasSceneCanvas.cpp:252,265、Settings.h:17-18|
|导出标志|设置 m_exporting=true|CanvasSceneCanvas.cpp:150-152|
|形状枚举|14 种|src/ShapeItem.h:10-25|
|形状菜单|allShapeTypes() 生成，objectName=ShapeAction_<typeString>|src/UiMenuBuilder.cpp:38-45|
|类型顺序|直线第 9 位|src/ShapeMeta.cpp:3-12|
|默认尺寸|矩形/圆角 200×200、三角 260×210、菱形 230×180、星 190、箭头 340×120、双箭头 380×120、直线 320×72、括号 100×300、气泡 300×200、扇形 200×200、弧 220×160|ShapeMeta.cpp:103-135|
|零面积判定|线条类判定表|ShapeMeta.cpp:152-165|
|三角形几何|顶点在内容矩形水平中点|src/ShapePathBuilder.cpp:144-149|
|直线几何|中心线|ShapePathBuilder.cpp:195-198|
|命中/遮挡|命中=描带宽+6；遮挡=描带宽；块箭头用整轮廓|src/ShapeHitGeometry.cpp:17-22,47-51,13-16|
|插入规则|默认尺寸 + 72% 上限 + 18px 层叠 + 视口中心|src/MainWindowInsertController.cpp:94-110|
|图元基类|先画内容再画选择框；导出/视图/单选/句柄判定|src/CanvasItem.cpp:173-189,197-201,208-225|
|前景层先例|被遮挡虚线，View/导出 直接返回|src/scene/CanvasSceneHit.cpp:16,22-62|
|合并枚举|{Union, Combine, Fragment, Intersect, Subtract}|src/BoolOps.h:7-13|
|合并-有填充空间|hasFillSpace|src/BoolOps.cpp:44-58|
|合并-样式优先级|priorityLess|BoolOps.cpp:78-86|
|合并-线条参与|线条按 band 参与|BoolOps.cpp:135-142,423-438,480,564,582,598-606|
|组合=并集减重叠|已有实现|BoolOps.cpp:453-486|
|拆分上限|2^n，上限 8，含残余块|BoolOps.cpp:534-566|
|剪除底|现有实现取最上层|BoolOps.cpp:586-606|
|合并结果|CompositeShapeItem + 一条撤销宏|BoolOps.cpp:631-643|
|合并启用|≥2 参与者|BoolOps.cpp:403-406、src/ViewStatusController.cpp:113-119|
|选区来源|selectedItems()，无选择先后|src/scene/CanvasSceneItems.cpp:44-54|
|快捷键|setShortcut + 覆盖表，键=默认快捷键字符串|src/ShortcutController.cpp:62-70、src/Settings.cpp:198-215,279-285|
|自定义图形加载|只收 shape/stroke/polygon，归一化 0–1|src/CustomShapeTemplates.cpp:39-52,80-127|
|扫描范围|单层目录|CustomShapeTemplates.cpp:138|
|多目录优先|先扫到优先|CustomShapeTemplates.cpp:151-167|
|菜单重建|每次 aboutToShow 全量重扫|src/MainWindowInsertController.cpp:207-235|
|工具栏按钮|InstantPopup 挂 m_customShapeMenu|src/UiToolBarBuilder.cpp:121-129|
|子菜单挂载|同一 QMenu 作"自定义图形"子菜单|src/UiMenuBuilder.cpp:49-54|
|曲线描带|outlineShape() 为中心线，paintedShape() 加线宽|src/CurveItem.cpp:301-315|
|文档校验/保存|QSaveFile 原子写|src/scene/CanvasSceneSerialization.cpp:40-107、src/DocumentController.cpp:44-97,246-275|
|导出格式|仅 PNG/JPEG/PDF|src/ExportDialog.cpp:69-73|
|剪贴板 MIME|application/x-storyboard-items|src/MainWindowEditActionsController.h:14|
|文字字号|pt|src/TextItem.h:40-46|

Excalidraw 契约来源（已 fetch）：官方 JSON schema、packages/excalidraw/clipboard.ts、plus.excalidraw.com/docs/api/scene-content-schema，以及"多点闭合 line 可填充"的社区说明。

未逐行核对、实施时必须复核的位置：椭圆默认尺寸（ShapeMeta.cpp:103-135 内具体行）、画布尺寸写入文档的字段名与位置、CompositeShapeItem 的持久化字段。

## 2. 需求 1：三角形 45°/60° + 棋盘格长度单位 + 统一默认尺寸

### 2.1 现状

默认画布 1920×1080；启动 fitAll() 按视图自适应缩放；棋盘格按画布 16×16 等分（与 32px 无关）；辅助网格 step 40；三角形顶点固定在内容矩形水平中点，底边宽 = 内容矩形宽，高度不可与角度联动；14 种形状默认尺寸互不相干（见 §1 表）。

### 2.2 定稿口径

|项|定值|
|-|-|
|棋盘格边长|32 逻辑像素|
|新画布尺寸|1536×1024（48×32 格，宽度由本方案取"随意"值）|
|尺寸约束|宽高必须为 32 的整数倍；棋盘格以画布左上角为原点铺满|
|棋盘格显示|仅背景=无色时绘制，语义"该区域透明无色"；纯色/图片背景不画|
|有色背景的长度参照|自动叠加 32px 浅色参考网格（主网格每 5 格=160px）|
|辅助网格|step 40 → 32，主网格 200 → 160|
|启动视图|保持现有 fitAll() 缩放不变，只把视图居中到画布中心|
|图形基准边长|2 格 = 64px|
|三角形参数|底边 b、底角 α、底角 β 为真相；γ=180−α−β；H=b/(1/tanα+1/tanβ)；顶点横向偏移=H/tanα|
|拖高语义|等价于等比改底边（高度不可独立拖）|
|45°/60° 预置|底边可调，角度锁定（45/45/90、30/60/90）|
|参数入口|属性窗 + 编辑图元卡都放|

默认尺寸换算表（长边或基准边长 64）：

|形状|现状|定稿|
|-|-|-|
|矩形 / 圆角矩形|200×200|64×64|
|椭圆|200×200（待复核）|64×64（直径=基准边）|
|三角形 60/60|260×210|64×55（H=√3/2×64）|
|三角形 45°|—|64×64（两直角边等）|
|三角形 60°|—|64×111（短边 64，H=tan60×64）|
|菱形|230×180|64×50|
|星|190|64×64|
|箭头（块状）|340×120|64×23|
|双箭头|380×120|64×20|
|直线 / 直线箭头|320×72|64×32（半格格高，避免高>长）|
|括号|100×300|21×64|
|气泡|300×200|64×43|
|扇形|200×200|64×64|
|弧|220×160|64×47|

### 2.3 实现落点

- 常量层：src/Settings.h（画布默认尺寸、网格步长、棋盘格边长）、src/CanvasScene.h:216。
- 棋盘格：CanvasSceneCanvas.cpp:238-241,286-311 改为按 32px 锚定原点铺格，不再用"16×16 等分"；同步改 src/selftest/SceneCanvasIoTests.cpp:314-315 与 README:42。
- 网格：CanvasSceneCanvas.cpp:252,265。
- 启动居中：src/MainWindow.cpp:95-99 之后加"视图居中到画布中心"，不改 CanvasZoomController 的缩放。
- 默认尺寸：src/ShapeMeta.cpp:103-135 换成 64 基准表；新增 45°/60° 预置项（见 §3.1 预置项机制）。
- 三角形：src/ShapePathBuilder.cpp:144-149 改为按 b/α/β 计算三顶点；ShapeItem 增加两个角度字段与序列化。
- 属性窗/卡片：见 §5。


### 2.4 兼容与迁移

- 旧文档保持原画布尺寸（1920×1080 等），棋盘格与网格按 32px 锚定原点自适应；非 32 倍数尺寸下允许边缘残格，但新文档保证整数倍。
- 三角形旧文档无角度字段 → 按 contentSize 反算 α、β，视觉零变化。
- 不新增缩放行为：升级后启动视图与升级前一致（仍是 fitAll），因此不构成用户可见的视图跳变。


### 2.5 自检

- 32 倍数画布上棋盘格边缘闭合；1920×1080 旧画布不崩、不越界。
- 14 种默认尺寸逐条等于 64 基准表数值（推翻"漏改某一类"）。
- 改角 A 后底边不变、高按公式变化（推翻"高度仍独立"）。


## 3. 需求 2：Excalidraw 导入导出与粘贴

### 3.1 现状

自有格式 .sbcanvas / type:"StoryBoardCanvas"；剪贴板只认 application/x-storyboard-items；导出仅 PNG/JPEG/PDF。

### 3.2 定稿口径

|项|口径|
|-|-|
|导入|生成新文档（不并入当前画布），成功后提示；失败不清空当前文档|
|导出|文件菜单新增"导出 Excalidraw"，写 type:"excalidraw"、version:2、elements、appState.gridSize、背景色|
|粘贴|识别 type:"excalidraw/clipboard"，落点=视口中心，72% 上限，不保留原分组|
|无对应图形|用可编辑近似（多点闭合 line / 折线曲线），提供"作为图片保留保真"选项|
|半透明填充|写实色 + customData 记 alpha|
|文字|pt ↔ px ≈ 1.333|
|2 点 arrow|→ Line + 两端箭头（配合需求 3/6 的箭头字段）|
|多点头箭头|→ 曲线折线模式，保留头|
|frame|→ 图层|
|头样式映射|bar→竖杠、dot→圆点、triangle→实心三角、arrow→开口箭头；空心三角无对应，降级为实心三角并记 customData|

元素类型仅 rectangle/ellipse/diamond/arrow/line/freedraw/text/image/frame/iframe/embeddable；fillStyle 仅 hachure/cross-hatch/solid/zigzag → 全部落为纯色（solid），其余三种记 customData 以便回写。

### 3.3 实现落点

新增 src/excalidraw/ 三个文件：ExcalidrawImporter（JSON→图元）、ExcalidrawExporter（图元→JSON）、ExcalidrawClipboard（粘贴 MIME 适配）。挂接到 src/DocumentController.cpp（新文档创建）、src/ExportDialog.cpp（格式选择）、src/MainWindowEditActionsController.cpp:150-191（剪贴板）。

### 3.4 兼容与迁移

- 自有格式为唯一"完整"存储；Excalidraw 只做互通，不承诺往返无损（近似降级的项在 customData 留标记）。
- 粘贴路径必须先判断自有 MIME，再判断 Excalidraw MIME，避免误判剪贴板内容。


### 3.5 自检

- 导入失败（坏 JSON / 超大文件）后当前文档仍在、无部分图元残留。
- 2 点 arrow 导入落到 Line 且两端头样式非"无"。
- 导出后重新导入，矩形/椭圆/文字的位置与尺寸一致（误差 ≤ 1px）。


## 4. 需求 3：直线箭头（复用直线类）

### 4.1 现状

直线是零面积描线图元（ShapeMeta.cpp:152-165），几何为中心线（ShapePathBuilder.cpp:195-198），命中=描带宽+6、遮挡=描带宽（ShapeHitGeometry.cpp:17-22,47-51）。曲线 outlineShape() 返回中心线，paintedShape() 才加线宽（CurveItem.cpp:301-315）。

### 4.2 定稿口径

|项|定值|
|-|-|
|适用范围|直线 与曲线（含折线模式），两端各自独立|
|样式集合|无 / 实心三角 / 空心三角 / 开口箭头 / 圆点 / 竖杠|
|头部尺寸|长 = 4×线宽，张角 90°，不出内容矩形|
|方向|直线按线段方向；曲线按首/末段切线|
|几何|头部并入描带（命中=描带宽+6，遮挡=描带宽），不改变零面积语义|
|合并|不参与合并|
|默认|普通直线无头；"直线箭头"预置项 = 末端开口箭头、起点无|
|编辑入口|卡片两行（起点/终点），属性窗一行两个下拉|
|多选基准|首个选中图元|
|旧文件|默认"无"，零变化|

### 4.3 实现落点

- 字段：ShapeItem / CurveItem 增加 arrowStartStyle、arrowEndStyle（枚举 int），写入 toJson/fromJson（ShapeItem.cpp:369-438）；ShapeItemState + operator==（ShapeItem.h:52-71）同步。
- 绘制：ShapePainter.cpp:114-118 之后追加头绘制；曲线在 CurveItem 的描线路径后追加。
- 命中：ShapeHitGeometry.cpp:17-22 的描带需包含头部三角/圆点。
- 编辑面：ItemDockPanelValueCard.cpp 新增两行（见 §5）；ItemPropertiesDialogStyleBlock.cpp:87-142 的类型分支加入下拉。
- 图标：ShapePainter.cpp:136-174 给"直线箭头"预置项画带头的图标。


### 4.4 兼容与迁移

旧文件无字段 → 两端"无"；operator== 需覆盖新字段，否则撤销/属性窗"是否有变化"判断失效。

### 4.5 自检

- 只改一端不影响另一端（推翻"共用一端字段"）。
- 头部在缩放后随线宽缩放，但屏幕尺寸恒定倍率下不出内容矩形。
- 导出 PNG 含头部、不含选择框。


## 5. 需求 4：圆角矩形并入矩形

### 5.1 现状

Rectangle 与 RoundedRectangle 是两个枚举（ShapeItem.h:12）；两者都开放圆角编辑；cornerRadius<0 表示"用类型默认（短边 16%）"（ShapeItem.h:140-141、ItemPropertiesDialogStyleBlock.cpp:16-22 硬编码 0.16）；setCornerRadius 钳制在短边一半（ShapeItem.cpp:244-254），但卡片圆角范围写死 0–400（ItemDockPanelValueCard.cpp:72-73）——既有缺陷。

### 5.2 定稿口径

|项|定值|
|-|-|
|枚举|删除 RoundedRectangle，只留 Rectangle|
|菜单|保留两个入口：0 圆角、16% 预置（由预置项机制实现）|
|自动态|保留 <0 = 自动 16%|
|存储|圆角存 px|
|上限|统一为短边一半|
|旧文件|rounded_rectangle 读入即"自动态"，视觉不变|
|向后写|不写 rounded_rectangle，新版本文件在旧版本打开会退化为直角矩形（明确接受）|

### 5.3 实现落点

ShapeItem.h:10-25（枚举）、ShapeMeta.cpp:3-12,19-20,52-55,107,142（表项）、ShapePathBuilder.cpp:135-140（路径）、ItemDockPanel.cpp:273-275（卡片显隐）、ItemDockPanelValueCard.cpp:72-73（范围改 0–短边一半）、ItemPropertiesDialogStyleBlock.cpp:125-126。全仓约 19 处引用需逐一收敛。

### 5.4 自检

- 旧 rounded_rectangle 打开后圆角与缩放行为与改前一致（推翻"迁移丢圆角"）。
- 卡片圆角上限随短边变化，不再固定 400（推翻既有缺陷）。
- 自检 ShapeItemTests.cpp:101,126-127、PropertyDialogFieldGeometryTests.cpp:30 需按新枚举改写。


## 6. 需求 5：合并图元按 PPT 逻辑重做

### 6.1 现状

- 枚举 {Union, Combine, Fragment, Intersect, Subtract}（BoolOps.h:7-13）。
- 组合（Combine）= 并集减重叠，已实现（BoolOps.cpp:453-486）。
- 线条按 band 参与布尔运算（BoolOps.cpp:135-142,423-438,480,564,582,598-606）。
- 剪除以最上层为底（BoolOps.cpp:586-606）。
- 拆分按 2^n 掩码，上限 8，含残余块（:534-566）。
- 结果 CompositeShapeItem + 一条撤销宏（:631-643）。
- 选区来自 selectedItems()，只有叠放顺序（scene/CanvasSceneItems.cpp:44-54）。


### 6.2 定稿口径

|项|定值|
|-|-|
|参与者|填充图形（含多边形）+ 文字；线条类全部排除（直线、直线箭头、曲线、括号、弧、纯描线多边形）|
|准入提示|有效参与者 < 2 时菜单与按钮置灰 + 状态栏说明|
|顺序|联合、异或 XOR、相交、剪除、拆分|
|异或|并集减重叠（= PPT"组合"语义，重叠区挖空）；tooltip 写明"不是奇偶 XOR"|
|剪除主体|先选中为底；框选多选时最底层为底|
|拆分|2^n，上限 8（即 ≤3 个填充参与者）|
|结果|仍为"多边形"CompositeShapeItem，一条撤销记录，样式取叠放最高参与者|
|Chord|Ctrl+M → U / X / I / S / F，1 秒超时，状态栏提示，不进快捷键覆盖表|
|图标|按钮主图标不变（UiMenuBuilder.cpp:205）|

关键洞察：新"异或"在算法层直接复用现有 Combine（并集减重叠），真正要改的是"参与集合（排除线条）"和"底的选取（先选中而非最上层）"。

### 6.3 实现落点

- BoolOps.cpp:135-142,423-438,480,564,582,598-606：删线条 band 分支；collect()（:233-282）过滤零面积图元。
- 底部选择：新增 SelectionOrder 支撑（§3.2），BoolOps 接收"底的图元指针"而非自行取最上层。
- 文字转轮廓：文字的 QPainterPath 由 QPainterPath::addText 生成（字号 pt 换算 px），因此合并会把文字固化为多边形轮廓，不可回写为文字；撤销可完整回退（一条宏）。
- 菜单与文案：UiMenuBuilder.cpp:201-229（五项名称）、MainWindowEditActionsController.cpp:118-140（命令文案）。
- Chord：src/ShortcutController.cpp 新增前缀状态机（见 §3.3）。
- 自检：src/selftest/AlignmentBoolGroupTests.cpp:309-386 的"直线可合并"用例必须反转。


### 6.4 兼容与迁移

- 行为变更（线条不可合并、剪除底改先选中、异或语义）需在 README 与状态栏文案写明；旧文档的多边形结果不受影响（已存的多边形仍是多边形）。
- 旧文档里由线条合并产生的多边形不做历史拆分。


### 6.5 自检

- 含线条的选区 → 菜单置灰（推翻"线条仍参与"）。
- 3 个填充图形做异或 → 结果 = 并集减所有重叠（推翻"奇偶 XOR"）。
- 剪除：先选中 A 后选 B → 保留 A 区域（推翻"最上层为底"）。
- 拆分 4 个参与者 → 提示超限且不执行；3 个 → 最多 8 块。
- 文字参与后结果为多边形、可直接编辑填充描线。


## 7. 需求 6：常见图形"直线"后新增"直线箭头"

### 7.1 定稿口径

|项|定值|
|-|-|
|菜单位置|常见图形"直线"之后|
|默认样式|末端开口箭头、起点无|
|默认尺寸|长边 64|
|显示名|"直线箭头"，底层 shapeType 仍为 line|
|图标|带开口箭头的直线图标|
|合并|不参与合并|

### 7.2 实现落点

作为预置项实现（§3.1），不新增 shapeType；objectName 建议 ShapeAction_LineArrow（与派生自 typeString 的既有命名并轨，需在 UiMenuBuilder.cpp:38-45 的生成逻辑里允许显式 objectName）。

注意：现有自检 MainWindowShapeMenuTests.cpp:113-115 断言"内置形状动作数 == shapeTypes.size()"，新增预置项后必然失败，必须改为"按 shapeType 去重后覆盖全部类型"。

## 8. 需求 7：自定义图形儿童故事模板

### 8.1 定稿口径

|项|定值|
|-|-|
|模板数|25|
|画风|单色线稿|
|填充|允许闭合面填充（描线归一化用中心线包围盒，预留 2% 内边距）|
|目录|四组子目录：小动物 / 自然 / 物件 / 人物与精灵|
|尺寸|长边 = 64|
|命名|显示名优先取 name 字段，缺省取文件名；小人（跑） 文件名用 小人-跑.sbcanvas|
|性能|按 mtime+size 缓存解析结果|
|新增命令|"从画布导出自定义图形"（把当前选区存成 .sbcanvas）|

25 项落位：

|组|数量|模板|
|-|-|-|
|小动物|7|兔子、小猫、小狗、小鸭、小鱼、小鸟、蝴蝶|
|自然|7|云、太阳、月亮、星星、山、树、小树苗|
|物件|7|小船、帆船、房子、火箭、爱心、瓶子、花朵|
|人物与精灵|4|小人、小人（跑）、精灵、星星精灵|

### 8.2 实现落点

CustomShapeTemplates.cpp:39-52,80-127（加载与归一化）、:120-123（名取文件名→改为 name 优先）、:138（扫描改分组）、:169-194（图标缓存）、MainWindowInsertController.cpp:149-176（插入长边 320→64）。

### 8.3 自检

CustomShapeDockTests.cpp:121-129 需按"长边 64 + 分组"改写；新增"分组不丢文件""同名覆盖取用户目录"用例。

## 9. 需求 8：三角形 A 边中点刻度线

### 9.1 定稿口径

|项|定值|
|-|-|
|显示条件|非查看模式 + 单选选中 + 非导出|
|绘制层|前景覆盖层（沿用 CanvasSceneHit.cpp:16,22-62 的 drawForeground 先例，导出直接 return）|
|尺寸|8px 屏幕恒定（用 CanvasItem::viewScale()，CanvasItem.cpp:208-215）|
|位置|A 边固定为内容矩形底边中点，随旋转与缩放跟随|
|语义|标记"可调底边与两侧内角"，纯视觉提示|
|排除|不进命中、不进遮挡计算、不参与合并、不被棋盘格影响|

### 9.2 实现落点

CanvasScene::drawForeground() 内遍历选中三角形（hasSingleSelection，CanvasItem.cpp:217-220），按 contentRect 底边中点绘制短垂线（长度 8px 屏幕恒定）。

### 9.3 自检

- 查看模式不画；导出 PNG 不出现刻度线；多选不画。
- 旋转 90° 后刻度线仍垂直于 A 边且在其中点。


## 10. 需求 9：shapes 目录自动分组 + 自定义图形标签页面板

### 10.1 现状

- 工具栏按钮 ToolBarCustomShapeButton 用 InstantPopup 挂 m_customShapeMenu（UiToolBarBuilder.cpp:121-129）。
- 同一 QMenu 被"常见图形"菜单 addMenu 成子菜单，两个 aboutToShow 挂重扫（UiMenuBuilder.cpp:49-54）。
- 三条自检写死该形态：MainWindowShapeMenuTests.cpp:113-120（子菜单存在）、:215-222（按钮挂菜单）、CustomShapeDockTests.cpp:364-383（菜单含 dock-probe）。


### 10.2 定稿口径

|项|定值|
|-|-|
|形态|独立浮动窗口 CustomShapePanel（Qt::Tool，带标题栏、可拖动、记住位置尺寸）；工具栏按钮与菜单项都只是触发器|
|数据|CustomShapeCatalog 单一数据源（分组扫描 + mtime/size 缓存 + 图标缓存）|
|内容|QTabWidget，每文件夹一页；页内滚动区 + 4 列网格，图标 40px、图标下一行截断名、tooltip 全名|
|页顺序|顺序.txt 优先，否则目录名 locale 排序；根目录散文件归"其它"页且固定最后|
|空/坏|空文件夹不出页；坏文件在该页底部灰色提示，不弹错误框|
|关闭|选中模板 → 插入并 hide()；ESC 可关闭；点击面板外部不关闭|
|键盘|方向键移动焦点，回车等价点击|
|页脚|"打开 shapes 目录…""刷新"|
|重扫|每次 showAt() 重扫目录、复用解析缓存；重建后回到原标签页|

### 10.3 实现落点

- 删除 m_customShapeMenu（MainWindow.h:234），新增 m_customShapePanel（惰性创建）。
- UiMenuBuilder.cpp:50-54：addMenu → addAction（objectName=CustomShapeAction），删两个 aboutToShow。
- UiToolBarBuilder.cpp:121-129：去掉 setPopupMode/setMenu，clicked → panel->showAt(button)。
- MainWindowInsertController.cpp:207-235：updateCustomShapeMenu() → rebuildCatalog() + refreshCustomShapePanel()；"打开目录""坏文件提示"移入面板。
- 新增 src/CustomShapePanel.{h,cpp}（建议再拆 CustomShapePanelGrid 以守住 500 行）、src/CustomShapeCatalog.{h,cpp}。


### 10.4 三个已定的实现默认

1. 1. 面板用 Qt::Tool 而非 Qt::Popup（点击外部不关闭，符合"独立窗口"）。
2. 从菜单项触发时延迟一拍（QTimer::singleShot(0, …)）再 show，避免 QMenu 关闭抢焦点导致面板闪退。
3. 重建后按页名恢复当前页，不跳回第一页。


### 10.5 自检（三条旧断言必须重写）

|旧|新|
|-|-|
|MainWindowShapeMenuTests.cpp:113-120 子菜单存在|ShapeMenu 含 CustomShapeAction；findChild<QMenu\*>("CustomShapeMenu") 为 null|
|:215-222 按钮挂了菜单|ToolBarCustomShapeButton->menu() == nullptr 且点击后面板可见|
|CustomShapeDockTests.cpp:382 菜单含 dock-probe|面板对应页内存在 dock-probe 条目|

## 11. 跨需求新机制

### 11.1 预置项机制

ShapePreset{ id, shapeType, displayName, iconId, defaultSize, params }，由 ShapeMeta 提供表；菜单生成时同时产出类型项与预置项，objectName 允许显式指定。受益需求：1（45°/60° 三角）、4（0 圆角/16% 双入口）、6（直线箭头）。

### 11.2 选择顺序记录

场景维护 QList<QPointer<CanvasItem>> m_selectionOrder：

- 单选点击 → 清空后追加；
- Ctrl 加选 → 追加；
- 框选 → 按叠放顺序（zValue 升序）追加，即"最底层在前"；
- 组选中 → 整体追加；
- 取消选择 / 删除 → 移除对应项。


消费者：剪除底（需求 5）、多选样式基准（需求 3）。

失效边界：图层重建、撤销恢复对象身份变化时顺序必须重建或清空——需在 scene/CanvasSceneItems.cpp 的选区分发与撤销恢复路径各加一处重置。

### 11.3 Chord 控制器

Ctrl+M 进入前缀态，等待 1 秒或下一次按键；U/X/I/S/F 触发；超时状态栏提示"已取消"；不写 Settings 覆盖表（ShortcutController.cpp:62-70 的键值机制不变）。已占用单键 H/V/B、Ctrl+C/X/V、Ctrl+=、[/] 不冲突。

### 11.4 CustomShapeCatalog + Panel

见 §10。

## 12. 数据与接口变更

|对象|新增/变更|位置|
|-|-|-|
|ShapeType 枚举|删 RoundedRectangle|ShapeItem.h:10-25|
|ShapeItemState|bottomAngle1/2（deg）、arrowStart/EndStyle、cornerRadius 语义不变（px）|ShapeItem.h:52-71|
|序列化|corner_radius、bottom_angle_1/2、arrow_start/end|ShapeItem.cpp:369-438|
|旧值映射|rounded_rectangle → rectangle + 圆角自动态|ShapeItem.cpp:170-184（setShapeType 复位逻辑需同步）|
|曲线|arrowStart/EndStyle|CurveItem 对应状态与序列化|
|文档 JSON|画布尺寸字段（实施时定位）|scene/CanvasSceneSerialization.cpp:40-107|
|剪贴板|新增 Excalidraw MIME 分支，自有 MIME 优先|MainWindowEditActionsController.h:14、:150-191|

## 13. UI 变更清单

|界面|变更|
|-|-|
|常见图形菜单|直线后加"直线箭头"；三角形后加 45°/60°；矩形/圆角矩形双入口；"自定义图形"由子菜单改为动作项|
|合并图元菜单|五项改名：联合 / 异或 XOR / 相交 / 剪除 / 拆分；主图标不变；≥2 有效参与者才启用|
|工具栏|自定义图形按钮去掉 InstantPopup，改触发器|
|编辑图元卡|三角形加"底边/角 A/角 B"三行；直线/曲线加"起点箭头/终点箭头"两行；宽度上限 320 → 360|
|属性窗|新增角度、箭头两组控件；圆角范围改为 0–短边一半|
|新建面板|CustomShapePanel（标签页 + 图标网格 + 页脚）|
|文件菜单|新增"导入 Excalidraw""导出 Excalidraw"|

## 14. 实施批次与工作树规划

按项目规则（按模块建工作树、合并 main 后自检与部署）：

|批次|内容|依赖|建议工作树|
|-|-|-|-|
|B1|常量层：画布/棋盘格/网格/默认尺寸表（需求 1 基础 + 需求 4 的枚举收敛前置）|无|wt-canvas-metrics|
|B2|预置项机制 + 三角形参数化 + 圆角矩形收敛 + 直线箭头预置项（需求 1/4/6）|B1|wt-shape-preset|
|B3|箭头字段与绘制（直线 + 曲线）+ 卡片/属性窗（需求 3）|B2|wt-arrow-style|
|B4|合并图元重做 + 选择顺序 + Chord（需求 5）|B2（预置项不影响，可与 B3 并行）|wt-boolops-ppt|
|B5|Excalidraw 导入导出与粘贴（需求 2）|B3（arrow 映射）|wt-excalidraw|
|B6|A 边刻度线 + 面板与分组扫描（需求 8/9）|B1（32px/64px 基准）|wt-customshape-panel|
|B7|25 个模板素材制作（需求 7）|B6（目录结构与面板）|与 B6 同树或独立素材目录|

若串行执行且一次只有一个任务，按项目规则可直接在主干逐批提交，不建工作树。

## 15. 兼容与迁移矩阵

|场景|行为|判定|
|-|-|-|
|旧文档（1920×1080）|尺寸不改写；棋盘格按 32px 锚定原点自适应，边缘允许残格|无破坏|
|旧三角形无角度|按 contentSize 反算角度，视觉不变|无破坏|
|旧 rounded_rectangle|读成 rectangle + 自动圆角|视觉无破坏，类型名变化|
|旧直线/曲线无箭头|两端"无"|无破坏|
|新文件在旧版本打开|rounded_rectangle 不写回，退化为直角矩形|明示接受|
|旧自定义图形模板|单层目录仍被识别（作为"其它"页/根层）|无破坏|
|已有合并结果（多边形）|保持原样，不回溯拆分|无破坏|
|快捷键|Chord 不进覆盖表，原有快捷键行为不变|无破坏|
|记号库/部署目录模板|用户目录同名优先（CustomShapeTemplates.cpp:151-167 语义保留组内）|升级不覆盖用户改动|

## 16. 测试矩阵（每条对应一种可推翻的错误机制）

|需求|用例|推翻的错误|
|-|-|-|
|1|32 倍数画布格线闭合；14 类默认尺寸逐条比对|尺寸漏改 / 棋盘格按等分算|
|1|改角 A 后底边不变、高变化|高度仍独立可拖|
|2|坏 JSON 导入后当前文档完好|导入失败破坏当前文档|
|2|2 点 arrow 往返位置误差 ≤1px|映射丢头或丢坐标|
|3|只改一端；缩放后头长=4×线宽|两端共用字段 / 头不随线宽|
|4|旧圆角矩形迁移后行为一致；卡片上限随短边|迁移丢圆角 / 上限固定 400|
|5|含线条选区置灰；3 图形异或；先选中为底；4 参与者拆分超限|线条仍参与 / 奇偶 XOR / 最上层为底 / 上限失效|
|6|菜单项计数按 shapeType 去重后覆盖全部|新增预置项打破旧断言|
|7|分组条数之和 == 可解析文件数；同名取用户目录；改文件后缓存刷新|递归漏扫 / 优先级反 / 缓存不刷新|
|8|查看模式不画、导出不画、多选不画、旋转后仍居中|刻度线泄漏到导出|
|9|面板可见、QMenu 为 null、dock-probe 在面板内|仍是 InstantPopup / 菜单残留|

## 17. 待校准参数（不得写入代码注释当定值）

|参数|现状|如何测量|
|-|-|-|
|Excalidraw 规模上限（elements ≤20000 / points ≤10000 / 单图 ≤20MB）|无来源，仅作初值|用真实导出文件与社区大文件实测解析耗时与内存，再定阈值|
|Excalidraw "proportional 圆角" ↔ px 换算系数|未定|用 Excalidraw 官方示例文件测量同图元渲染圆角半径|
|模板线宽与 2% 内边距的观感值|未定|先做 3 个样板（兔子/小船/太阳），在 64px 与放大 4 倍下目视验收|

## 18. 风险与实施注意

1. 1. 自检断言与新形态冲突：至少 5 处既有断言需要改写（MainWindowShapeMenuTests.cpp:113-120,215-222、CustomShapeDockTests.cpp:364-383、ShapeItemTests.cpp:101,126-127、SceneCanvasIoTests.cpp:314-315、AlignmentBoolGroupTests.cpp:309-386）。这些必须在同一次提交内改，否则合并 main 自检必挂。
2. 文字参与合并不可逆：合并把文字固化为多边形轮廓，需在 UI 上给明确提示（"合并后文字将转为轮廓"）。
3. 选择顺序易失效：撤销恢复会换对象身份，顺序表必须重建；建议在恢复后按新对象重新构造。
4. Chord 与输入焦点：文本编辑态下 Ctrl+M 不应进入前缀态，需判断 focusWidget 是否为文本控件。
5. 面板焦点时序：从 QMenu 触发必须延迟一拍，否则面板闪退。
6. 模板素材工作量：25 个 .sbcanvas 是主要工时，建议先 3 个样板验收线宽/内边距后再批量。
7. 500 行约束：CustomShapePanel、ExcalidrawImporter 都有超限风险，需提前拆分（建议 PanelGrid、ImporterMapping 独立文件）。


## 附录 A：文件级改动清单

改

- src/Settings.h、src/CanvasScene.h：画布默认 1536×1024、网格 32/160、棋盘格 32
- src/scene/CanvasSceneCanvas.cpp：棋盘格锚定、网格步长
- src/MainWindow.cpp：启动居中、updateCustomShapeMenu → refreshCustomShapePanel
- src/ShapeMeta.cpp / ShapeMeta.h：枚举收敛、默认尺寸表、预置项表、零面积表
- src/ShapeItem.h / ShapeItem.cpp：删枚举、角度与箭头字段、JSON、setShapeType 复位
- src/ShapePathBuilder.cpp：三角形角度驱动、圆角路径
- src/ShapeHitGeometry.cpp：箭头头部计入描带
- src/ShapePainter.cpp：箭头绘制、预置项图标
- src/UiMenuBuilder.cpp：预置项、五项合并改名、自定义图形改动作项
- src/UiToolBarBuilder.cpp：按钮改触发器
- src/MainWindowInsertController.cpp：预置插入、Catalog/面板、长边 64
- src/MainWindowEditActionsController.cpp：合并文案、Chord、Excalidraw 剪贴板
- src/ShortcutController.cpp：Chord 状态机
- src/BoolOps.h / BoolOps.cpp：参与集合、底的选择、文字轮廓
- src/scene/CanvasSceneItems.cpp、src/scene/CanvasSceneHit.cpp：选择顺序、A 边刻度线
- src/CurveItem.cpp：曲线箭头
- src/ItemDockPanel.cpp、src/ItemDockPanelValueCard.cpp：卡片新增行、宽度、圆角范围
- src/ItemPropertiesDialogStyleBlock.cpp、ItemPropertiesDialogGeometryBlock.cpp、ItemPropertiesDialog.cpp：角度/箭头/圆角
- src/DocumentController.cpp、src/ExportDialog.cpp、src/scene/CanvasSceneSerialization.cpp：Excalidraw 入口
- src/CustomShapeTemplates.h / .cpp：分组、name、缓存
- src/selftest/：见 §16 与 §18.1


新增

- src/CustomShapePanel.{h,cpp}（+ 建议 CustomShapePanelGrid.{h,cpp}）
- src/CustomShapeCatalog.{h,cpp}
- src/excalidraw/ExcalidrawImporter.{h,cpp}、ExcalidrawExporter.{h,cpp}、ExcalidrawClipboard.{h,cpp}
- src/SelectionOrder.{h,cpp}（或在 CanvasScene 内实现）


素材

- shapes/小动物/7、shapes/自然/7、shapes/物件/7、shapes/人物与精灵/4，共 25 个 .sbcanvas
