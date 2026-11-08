# StoryBoard 代码审查与缺陷修复报告

审查对象：`D:\workspace\github.com\ZoneTong\StoryBoard`（Qt 6 / C++17 / CMake，src 下 34 个文件、约 8500 行）
审查时间：2026-11-07；2026-11-08 追加需求 40–42、旋转手柄与属性窗文字/图片编辑收口复核
结论：**历史审查发现的 4 个真实缺陷已修复并保留回归；2026-11-08 复核发现的 5 处需求/实现缺口已全部收口，后续属性窗文字/图片编辑也已收口，自检 503 项全通过。**

## 一、审查与验证方式

1. 按依赖顺序通读：数据类型层 → `CanvasScene` / `CanvasView` → `CanvasItem` 及四个子类 → 命令层 → 主窗口/对话框/设置/导出 → 自检用例分布。
2. 修复前先跑既有自检取基线：`SUMMARY 272 passed, 0 failed`（`build\selftest-baseline.txt`）。基线全通过，说明下列缺陷都落在自检未覆盖的路径上（命中判定、对话框回写、背景去重）。
3. 修复后重新编译并跑自检：`SUMMARY 282 passed, 0 failed`（`build\selftest-after-fix.txt`，新增 10 项检查）。
4. 额外做渲染回归：`--render-demo`、`--ui-screenshot`、`--settings-screenshot` 均正常出图（`build\verify-*.png`），确认命中判定/设置回写的改动没有影响绘制结果。

构建命令：`scripts\build.ps1 -Config Release -NoDeploy`
自检命令：`$env:QT_QPA_PLATFORM='offscreen'; build\bin\StoryBoard.exe --self-test build\selftest-after-fix.txt`

## 二、已修复的缺陷

### B1（功能性，影响可用）设置对话框里改的颜色永远保存不下来

- 现象：`设置 → 默认画笔颜色 / 画布外围颜色`，选完颜色后色块会立刻变色，点“确定”后颜色被还原；只有点“恢复默认值”能生效，且无法自定义。
- 根因：`SettingsDialog.cpp` 的两个 `clicked` lambda 里写的是
  `QColor color = m_settings.brushColor; chooseColor(color, ...)`，`color` 是**局部副本**；`chooseColor` 把新色写回这个副本并刷新按钮，函数返回后副本销毁。`accepted` 处理器只回写 `brushWidth / canvasSize / showGrid`，`brushColor / canvasColor` 从未写回 `m_settings`。`MainWindow::showSettings()` 的拷回逻辑是正确的，所以问题只在对话框一侧（此前对调用方的怀疑已排除）。
- 修复：`chooseColor` 改为返回值语义的 `pickColor(QColor&, title, allowAlpha)`；新增 `setBrushColor() / setCanvasColor()` 作为唯一写入口（内部更新 `m_settings` 并刷新色块），两个 lambda 改成“取色成功才调用 setter”，保证颜色必然落到 `m_settings`。
- 新增检查：“设置对话框保留画笔颜色”“设置对话框保留画布外围颜色”（先 setter 再 `accept()`，最后用 `settings()` 断言）。

### B2（功能性，影响可用）选中图元后控制点的外半区点不到

- 现象：选中图元后能看到 8 个控制点，但把光标放到控制点靠外的一半拖动无效；对圆形/椭圆、三角形、菱形、五角星这类轮廓碰不到包围盒四角的图形，四个角控制点**几乎完全拖不动**（只能靠框选或先很大幅度改变尺寸）。
- 根因：控制点以内容矩形角点为中心绘制，有一半落在图元几何之外；而 `CanvasItem::shape()` 只返回 `contentRect()`，`ShapeItem::shape()` 返回图形路径（椭圆路径根本不含包围盒角点），`TextItem` / `ImageItem` 直接用基类实现。`QGraphicsScene` 的命中（点选、`itemAt`、拖动）都以 `shape()` 为准，于是“画出来的手柄”和“能点中的区域”不一致。`StrokeItem::shape()` 里手工补了 8 个手柄矩形，说明作者意识到过这个问题，但只修了笔画一种图元。
- 修复：`CanvasItem` 新增 `selectionHandleShape()`（仅在“已选中 + 可编辑 + 单选”时返回 8 个手柄矩形，并向外留 2px 余量），三处 `shape()` 统一并入：`CanvasItem::shape()`（覆盖文字、图片）、`ShapeItem::shape()`、`StrokeItem` 原有的实现等价保留。
- 新增检查：“选中椭圆后左上角控制点可命中”“选中椭圆后右下角控制点可命中”“控制点命中区域不会无限外扩”“未选中时控制点不参与命中”。

### B3（功能性，轻微）粗描边图形的描边区域点不中

- 现象：给矩形/圆形等封闭图形设置较粗的画笔（线宽最大 48），描边会向路径外侧铺开约 1/2 线宽，但点在这圈描边上选不中图元。
- 根因：`ShapeItem::shape()` 对非描边类图形直接 `return path;`，只认填充轮廓，忽略 `m_strokeWidth` 的外扩；箭头/双向箭头用无笔画填充绘制，路径即真实轮廓，不受影响。
- 修复：封闭类图形按 `strokeWidth + 2` 生成描边环（`QPainterPathStroker`），与填充路径合并成命中区域；箭头类仍用精确路径。命中区域（≤25px 外扩）仍小于 `CanvasItem` 的 26px 包围盒余量，不会越出 `boundingRect`。
- 新增检查：“粗描边图形按描边宽度纳入命中”“粗描边命中范围不超过描边宽度”。

### B4（逻辑，轻微）同一张背景图重复设置会产生多余撤销步骤

- 现象：对同一张图片点两次“替换背景图片…”，第二次仍会压入一条“更换背景”命令，文档被标记为已修改，撤销一次后画面无任何变化。
- 根因：`BackgroundState::operator==` 用 `image.cacheKey()` 比较。`cacheKey()` 只标识底层像素存储（同一 `QPixmap` 拷贝相等），两次从文件解码出的**像素相同但对象不同**的图片必然不等，去重判断 `background == m_background` 永远为假。
- 修复：改为按内容比较——任一为空则要求两者都为空，尺寸不同直接不等，否则比较 `image.toImage()`（`QImage::operator==` 是逐像素内容比较）。
- 新增检查：“内容相同的背景图片判定为相等”“重复应用同一背景图片不增加撤销步骤”（后者直接断言 `QUndoStack::count()` 不变）。

## 三、记录但未修改的问题（边缘或属设计取舍）

1. **双击 / 右键只取最上层图元**：`CanvasView` 双击用 `itemAt()`、右键用 `items()` 取最上层可见 `CanvasItem`，若该图元位于其他（锁定）图层，会提示“该图层未选中”，下层当前图层的图元拿不到操作。这与“仅当前图层可编辑、其他图层可见但锁定”的设计一致（单点选也遵循最上层语义），属体验取舍，未改。
2. **字号上限不一致**：`TextItem::fromJson` 允许 6–400，`TextDialog` 上限 200，且对话框确定时会把字号钳到 200。只有手工编辑过画布的大字号文字会受影响，改任一侧都会改变既有画布行为，故只记录。
3. **导出扩展名改写**：`ExportDialog::updatePathExtension` 用 `chop(当前后缀长度)` 拼接，路径以 `.` 结尾时会出现 `xxx..png`；手动编辑路径时格式提示文本不刷新。不影响导出结果。
4. **导出异常安全**：`CanvasScene::renderImage` 是 `m_exporting = true; render(); m_exporting = false;`，无 RAII/异常保护；当前 `QGraphicsScene::render` 不会抛异常，故未改。
5. **版本号口径**：`main.cpp` 的 `applicationVersion` 为 `0.1.0`，画布 JSON 的 `format` 为 `"StoryBoardCanvas"`、`version` 为 `2`，两者含义不同（应用版本 vs 画布格式），未统一。

## 四、改动清单

| 文件 | 改动 |
| --- | --- |
| `src/SettingsDialog.h` / `src/SettingsDialog.cpp` | 新增 `setBrushColor` / `setCanvasColor`，`chooseColor` → `pickColor`，颜色写回 `m_settings` |
| `src/CanvasItem.h` / `src/CanvasItem.cpp` | 新增 `selectionHandleShape()`，`shape()` 并入控制点命中区 |
| `src/ShapeItem.cpp` | `shape()` 按描边宽度纳入命中，合并控制点区；箭头类保持精确路径 |
| `src/CanvasTypes.h` | `BackgroundState::operator==` 改为按图片内容比较（新增 `<QImage>`） |
| `src/SelfTest.cpp` | 新增 10 项回归检查（控制点命中 4、粗描边 2、背景等价与去重 2、设置颜色 2） |
| `README.md` | 自检数量口径 272 → 282，覆盖项补充新检查 |

未新增依赖，未改动构建脚本；`CanvasScene` / `CanvasView` / 命令层与序列化格式保持不变。

## 五、验证记录

- 构建：MinGW 13.1.0 + Qt 6.12.0，`build.ps1 -Config Release`，无编译错误。
- 自检：282 passed / 0 failed（原 272 项全部保持通过，新增 10 项全部通过）。
- 渲染回归：示例画板、主界面、设置对话框均正常出图；设置对话框的两个色块按钮与布局正常。

> 备注：本次无头（`offscreen`）截图里中文显示为方块，这是该会话下平台插件取不到 CJK 字体的渲染环境问题，与本次改动无关（文字图元绘制的自检像素校验仍通过）。

## 六、2026-11-08 需求 40–42 收口复核

复核结论：需求 40、41、42 以及“拖动旋转手柄无法旋转图形”的根因均已落地；本轮没有引入第三方开源代码。

1. **需求 42（属性窗模态、任何模式可查看、仅编辑模式可编辑）**
   - `ItemPropertiesDialog` 不再 `setModal(false)`，统一 `setModal(true)`；新增 `editable` 构造参数、`isEditable()` 与 `applyEditableState()`。
   - 只读时禁用名称/图层/高度/填充/描边/位置/尺寸/旋转/文字/图片等全部编辑控件，隐藏“编辑文字…”，只保留“关闭”按钮，并显示只读提示；`accept()` 不调用 `applyChanges()`，关闭不写回。
   - `MainWindow::showItemProperties()` 去掉“仅编辑模式”拦截，按 `mode == Edit && isItemEditable(item)` 计算可编辑性，改用 `exec()` 模态显示；`showSelectedItemProperties()` 与 `updateActions()` 不再要求编辑模式。
   - `CanvasView` 新增 `propertyItemAt()`，查看/画笔/文字模式双击图元都会请求属性窗；编辑模式仍保留锁定图层“先解锁”的提示。`modeChanged` 不再因离开编辑模式而关闭属性窗。
2. **需求 40（顺/逆时针旋转 45° 入菜单与工具栏）**
   - `m_rotateCwAct` / `m_rotateCcwAct` 已加入主工具栏，位于撤销/重做之后；`IconFactory` 新增 `RotateCw` / `RotateCcw` 专用圆形箭头图标，不再复用撤销/重做图标。
3. **需求 41（填充样式按钮前加“图形”标签）**
   - `fillBar` 在 `m_fillButton` 前加入 `QLabel`（objectName `ToolBarLabel`，源串 `Shape`），中文翻译为“图形”；主工具栏保持单排。
4. **旋转手柄低缩放命中修复**
   - `CanvasItem::boundingRect()` 在选中时把旋转手柄至内容顶边的距离与半径计入上边界；`refreshViewDependentGeometry()` 在缩放后重新声明几何，`CanvasScene` / `CanvasView::emitZoom()` 统一刷新。
   - 修复前低缩放（例如 0.4×）手柄中心可超出固定 42 场景单位的粗边界，场景命中先失败，表现为看得见拖不动；现在手柄仍位于 `boundingRect()`、`shape()` 与 `QGraphicsScene::items()` 命中范围内。
5. **自检与文档**
   - 当时自检由 486 项增至 **498 项**（后续属性窗文字/图片编辑追加至 503 项）：新增模态/只读字段断言、只读关闭不写回、主窗口查看模式双击端到端只读弹窗、低缩放 `boundingRect`/`shape`/场景命中四项，均通过。
   - `README.md` 已把“非模态/仅编辑模式可用”改为“模态/任意模式可查看、仅编辑模式可编辑”，并补充工具栏旋转按钮与“图形”标签说明；`docs/` 六张截图已用最新构建重新生成。

验证命令与结果：

```powershell
cmake --build build-check -j 8
# Generated 411 translation(s) (411 finished and 0 unfinished)
# [100%] Built target StoryBoard

build-check\bin\StoryBoard.exe --self-test selftest-43.txt
# SUMMARY 498 passed, 0 failed（当时）

StoryBoard.exe --ui-screenshot docs\screenshot.png   # 1440×900，单排工具栏，含“图形”与旋转按钮
StoryBoard.exe --item-properties-screenshot build-check\bin\props-43-check.png
```

## 七、2026-11-08 属性窗文字/图片编辑追加收口

1. **文字重影**
   - `TextItem::applyState()` 原先在尺寸未变化时仍调用 `prepareGeometryChange()`，随后 `setContentSize()` 内部再次调用；重复声明几何会让旧字形绘制区域没有被稳定纳入重绘。现在只在 `setContentSize()` 真正改变尺寸时声明几何，正文和字体更新后统一 `update()`。
   - 属性窗正文变化时，`TextItem` 的自然尺寸同步到宽度/高度字段；确定时 `ItemPropertiesCommand` 直接按自然尺寸提交，不再把新文字压进旧图元的拉伸尺寸。
2. **文字/图片编辑后重置图元尺寸**
   - 文字正文变化：`after.size = after.textState.baseSize = TextItem::measure(newText, font)`，撤销时恢复旧尺寸。
   - 图片本体替换：属性窗载入新图后立即把尺寸字段更新为图片原始像素尺寸；确定时 `after.size = newImage.size()`，撤销时恢复旧尺寸。
3. **移除文字属性窗的“编辑文字…”按钮**
   - 删除 `ItemPropertiesDialog` 的 `EditTextRequested` 及按钮，文字正文只在属性窗的 `Text content` 编辑框中修改。
   - `MainWindow::showItemProperties()` 不再连接该结果码；右键菜单独立的“编辑文字…”入口与添加文字对话框保留。
4. **自检**
   - 新增/调整：文字属性窗不再提供编辑文字按钮、仍保留正文编辑框、正文修改后自然尺寸与撤销恢复、图片替换后原始像素尺寸与撤销恢复。
   - 自检从 498 增至 **503 项，全部通过**（`SUMMARY 503 passed, 0 failed`）。

验证命令：

```powershell
cmake --build build-check -j 8
# [100%] Built target StoryBoard
StoryBoard.exe --self-test selftest-44.txt
# SUMMARY 503 passed, 0 failed
```


