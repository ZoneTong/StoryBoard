# StoryBoard 故事板

一个使用 **Qt 6 / C++17 / CMake** 编写的轻量故事板应用。英文名 **StoryBoard**，中文名「故事板」。

一个故事板可以包含多张**画布**；每张画布由多个**图层**组成，每个图层包含笔画、图片、文字和常见图形图元。画布以 UTF-8 JSON 保存为 `.sbcanvas`，可继续编辑，也可导出为图片或 PDF。

![StoryBoard 界面](docs/screenshot.png)

## 功能

- **四种模式**：查看、编辑、画笔、文字。查看模式只改变视图，不修改图元坐标；编辑模式支持选择、移动、缩放、旋转和框选；画笔模式绘制自由曲线；文字模式点击画布插入文字。
- **图元**：自由笔画、图片、文字，以及矩形、圆角矩形、椭圆、三角形、菱形、五角星、箭头、双向箭头、直线、左右大括号、对话气泡共 12 种常见图形。
- **图元属性**：选中图元后按 `Q` 或双击打开模态属性窗。任意模式可查看，仅编辑模式可编辑；名称、图层、层内高度、填充、描边、位置尺寸、旋转角度、文字正文/颜色/字体、图片名称与图片本体均可修改，确认后只生成一条撤销记录。
- **图层与叠放**：背景固定为第 0 层；用户图层可新建、删除空图层、上移/下移、重命名、显示/隐藏、锁定。同一图层内的 `height` 唯一，`]` / `[` 或右键可调整层内高度，也可跨图层移动。
- **背景与画布**：纯色/图片背景（拉伸、适应、平铺），辅助网格，画布外围颜色；画布四边四角可拖拽拉伸或裁剪，越界图元不会被删除。
- **撤销 / 重做**：图元增删、移动缩放旋转、文字与图片编辑、图形插入、背景替换、画布尺寸、图层增删/排序、层内高度与跨图层移动均进入撤销栈，上限 300 步。
- **画布文件**：图片与背景以 PNG 内嵌，文字保存字体、颜色、粗体/斜体/下划线/删除线字段，图形保存类型、描边、填充与线宽。
- **导出**：画布区域或全部图元，PNG / JPEG / PDF，0.25×–4×；可合并导出，也可按每个可见图层分别导出。
- **界面语言**：简体中文 / English，可跟随系统；切换后当前窗口立即生效，不需要重启。

![常见图形快速插入菜单](docs/shape-menu.png)

## 构建与运行

要求：Qt 6.5+、C++17、CMake 3.21+；编译器可用 MinGW、MSVC、Clang 或 GCC。

项目版本号只在 `CMakeLists.txt` 的 `project(StoryBoard VERSION ...)` 维护；Windows 文件版本资源、关于对话框和便携包命名都从它派生，不要再单独写死版本号。

### Linux / macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.0/gcc_64"
cmake --build build -j
```

### Windows（Git Bash / MSYS2）

```bash
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="D:/Qt/6.12.0/mingw_64"
cmake --build build -j
```

### 运行

```bash
# Linux / macOS
./build/bin/StoryBoard

# Windows（Git Bash / MSYS2）
./build/bin/StoryBoard.exe

# 打开示例画布
./build/bin/StoryBoard examples/demo-canvas.sbcanvas
```

也可以直接用 Qt Creator 打开根目录的 `CMakeLists.txt`，选择本机对应的 Qt 6 kit。

## 便携打包

`scripts/package-portable.sh` 是跨平台打包入口，可在 Windows 的 Git Bash / MSYS2 / Cygwin、macOS 和 Linux 的 bash 中执行。脚本会定位 Qt、调用平台部署工具、补齐 offscreen 插件、运行包内自检，然后生成压缩包。

```bash
bash scripts/package-portable.sh --help

# 使用已有构建产物打包
bash scripts/package-portable.sh

# 先构建再打包
bash scripts/package-portable.sh --build

# 指定 Qt
bash scripts/package-portable.sh --build --qt-dir "$HOME/Qt/6.8.0/gcc_64"
```

| 主机 | 产物 | 部署方式 |
| --- | --- | --- |
| Windows（Git Bash / MSYS2 / Cygwin） | `StoryBoard-<ver>-win64-portable.zip` | `windeployqt` + 补 `qoffscreen.dll` |
| macOS | `StoryBoard.app` 及 `.zip` | `macdeployqt` + 补 `libqoffscreen.dylib` |
| Linux | `StoryBoard-<ver>-linux-<arch>-portable.tar.gz` | 拷贝 `libQt6*`、插件与 `qt.conf` |

Windows 分支已在 Windows 11 + MinGW + Qt 6.12 实机验证；macOS / Linux 分支按 Qt 官方部署方式提供，尚未在对应主机实机验证。自检默认要求 Windows 504 项，其他平台接受当前通过数；可用 `--expected-passed 0` 放宽，`--skip-self-test` 跳过（不推荐）。

### Windows 内部签名

公开分发需要购买受信任的代码签名证书；只在内部机器使用时，统一使用本机共享的通用自签名证书：

```text
主题: CN=ZoneTong Internal Code Signing, O=ZoneTong, C=CN
指纹: E1622A2E4F44689BF02B8F6415A79A88C509D77F
有效期: 2026-11-09 至 2031-11-09
```

私钥只保存在 `Cert:\CurrentUser\My`，不进入仓库和便携包。公钥与复用脚本放在本机共享目录：

```text
D:\workspace\github.com\ZoneTong\.code-signing\ZoneTong-CodeSigning.cer
D:\workspace\github.com\ZoneTong\.code-signing\sign-internal.ps1
```

本机已把公钥安装到 `Cert:\CurrentUser\Root`。其他内部项目可以直接用共享脚本签名：

```powershell
powershell -ExecutionPolicy Bypass -File `
  D:\workspace\github.com\ZoneTong\.code-signing\sign-internal.ps1 `
  D:\work\OtherApp\dist\OtherApp.exe
```

打包 StoryBoard 时传入证书指纹和公钥文件，脚本会在 `windeployqt` 之后、包内自检之前签 `StoryBoard.exe`，并把 `.cer` 和安装说明放进包内：

```bash
STORYBOARD_SIGN_CERT_SHA1="E1622A2E4F44689BF02B8F6415A79A88C509D77F" \
STORYBOARD_SIGN_CERT_FILE="/d/workspace/github.com/ZoneTong/.code-signing/ZoneTong-CodeSigning.cer" \
bash scripts/package-portable.sh --qt-dir "/d/Qt/6.12.0/mingw_64"
```

验证签名：

```powershell
Get-AuthenticodeSignature .\StoryBoard.exe | Format-List Status,StatusMessage,SignerCertificate
```

目标机器首次使用前，右键包内 `ZoneTong-CodeSigning.cer` → 安装证书 → 当前用户 → 受信任的根证书颁发机构；或执行：

```powershell
certutil -addstore -user Root .\ZoneTong-CodeSigning.cer
```

需要所有用户生效时，用管理员 PowerShell：

```powershell
certutil -addstore Root .\ZoneTong-CodeSigning.cer
certutil -addstore TrustedPublisher .\ZoneTong-CodeSigning.cer
```

机器级根存储是让 UAC 和资源管理器都识别发布者的关键；只装当前用户根时，部分 UAC/SmartScreen 场景仍会按机器级存储判定为未知发布者。

自签名只在安装了该根证书的机器上被信任；对外分发仍会提示未知发布者，SmartScreen 信誉也不由自签名解决。若文件是从网络或聊天工具下载的，先对 zip 或 `StoryBoard.exe` 执行 `Unblock-File`，再解压/运行，可去掉“来自 Internet”标记。若签名已显示 `Valid`、证书也已装入机器级根，但仍出现 SmartScreen 的“来自 Internet/无法识别的应用”拦截，那属于 SmartScreen 信誉机制，只能选择“更多信息 → 仍要运行”或关闭系统的“检查应用和文件”。

## 使用说明

### 四种模式

| 模式 | 快捷键 | 说明 |
| --- | --- | --- |
| 查看模式 | `V` | 默认模式。左键拖拽平移，滚轮缩放；进入时自动全景显示所有可见图元。 |
| 编辑模式 | `E` | 选择/拖动图元，拖动 8 个控制点缩放；顶边上方旋转手柄按 5° 吸附旋转；空白处框选；`Delete` 删除。 |
| 画笔模式 | `B` | 左键绘制自由曲线；工具栏选择颜色与粗细。 |
| 文字模式 | `T` | 点击画布插入默认文字图元并打开属性窗，确定后回到编辑模式并选中新文字。 |

### 图元与属性

- 选中单个图元后，四角与四边共 8 个控制点可缩放；按住 `Shift` 等比缩放。选中多个图元后拖动任意一个，全部一起移动。
- 旋转手柄按 5° 步长旋转；`Ctrl+R` / `Ctrl+Shift+R` 顺/逆时针旋转 45°。
- 双击图元或按 `Q` 打开属性窗。文字正文、字体、字号、颜色、图片名称与图片本体都在属性窗内编辑，不再提供独立文字对话框。
- 文字正文变化后，图元尺寸自动重置为文字自然尺寸；替换图片后，尺寸重置为新图片原始像素尺寸。
- 右键图元可调整层内高度、跨图层移动或删除；多选时批量作用于全部选中图元。

### 图层与叠放

- 背景固定为第 0 层，不能接收图元、不能删除/上移/解锁。
- 用户图层按上层在前显示；新建图层放在最上层。新图元进入当前图层。
- 同一图层内按 `height` 从小到大绘制，`height` 越大越靠上；新建图元取当前层最大 `height + 1`，同层内唯一。
- 图层顺序优先级高于层内 `height`；高优先级纯色填充图元会完整遮挡下层。
- 可见且未锁定的图层才可编辑；隐藏或锁定图层上的图元不响应点选、框选、拖动、双击和右键菜单。
- 图层面板表头三态勾选框可全部显示/全部隐藏；单行勾选框控制显示，锁形按钮控制锁定。选中隐藏图层会自动显示，隐藏当前图层会自动切换到最近的可见图层。

### 背景、设置与语言

- **画布 → 替换背景图片…**：纯色、图片（拉伸/适应/平铺）或清除背景。
- **画布 → 设置…**：默认画笔颜色/粗细、画布尺寸、辅助网格、画布外围颜色、界面语言。

![设置对话框](docs/settings.png)

- 界面支持跟随系统 / 简体中文 / English；`STORYBOARD_LANG=zh|en` 可临时覆盖。
- 应用内颜色选择使用 Qt 自绘对话框，字体选择也强制使用 Qt 自绘对话框，避免原生窗口语言不受应用控制。

### 画布文件

- 画布菜单会列出画布目录中的 `.sbcanvas` 文件；默认目录为 `%APPDATA%\ZoneTong\StoryBoard\canvases`。
- 示例画布：[examples/demo-canvas.sbcanvas](examples/demo-canvas.sbcanvas)

![示例画布](docs/sample-canvas.png)

- 画布 JSON 保存画布范围、图层顺序/显示/锁定、当前图层，以及每个图元的 `layer` / `height`。
- 文字额外保存 `fontFamily`、`fontSize`、`fontWeight`、`italic`、`underline`、`strikeOut`、`color`。
- 图形额外保存 `shapeType`、`strokeColor`、`fillColor`、`strokeWidth`、`fillStyle`。

### 导出

- 范围：画布区域或全部图元；格式：PNG / JPEG / PDF；比例 0.25×–4×。
- 可按每个可见图层分别导出，文件名格式为 `<基础名>_L<图层 id>_<图层名>.<扩展名>`。
- 导出不包含网格、选中框和手柄；隐藏图层不导出。

![导出对话框](docs/export-dialog.png)

### 快捷键

| 操作 | 快捷键 |
| --- | --- |
| 新建 / 打开 / 保存 / 另存为 | `Ctrl+N` / `Ctrl+O` / `Ctrl+S` / `Ctrl+Shift+S` |
| 导入图片 / 导出画布 | `Ctrl+I` / `Ctrl+E` |
| 撤销 / 重做 | `Ctrl+Z` / `Ctrl+Y`（也支持 `Ctrl+Shift+Z`） |
| 全选 / 删除 | `Ctrl+A` / `Delete` |
| 顺时针 / 逆时针旋转 45° | `Ctrl+R` / `Ctrl+Shift+R` |
| 查看 / 画笔 / 编辑 / 文字模式 | `V` / `B` / `E` / `T` |
| 回到全景 / 放大 / 缩小 / 实际大小 | `Ctrl+0` 或 `Home` / `Ctrl+=` / `Ctrl+-` / `Ctrl+1` |
| 设置 | `Ctrl+,` |
| 取消当前笔画 | `Esc` |
| 调整层内高度 | `]` / `[` |

## 自检

```bash
# Linux / macOS
QT_QPA_PLATFORM=offscreen ./build/bin/StoryBoard --self-test build/selftest.txt
cat build/selftest.txt

# Windows 的 Git Bash / MSYS2
QT_QPA_PLATFORM=offscreen ./build/bin/StoryBoard.exe --self-test build/selftest.txt
cat build/selftest.txt
```

当前 Windows 基线为 **504 passed, 0 failed**；其他平台以实际 `SUMMARY` 为准。

`--self-test` 需要平台插件目录下的 offscreen 插件（Windows `platforms/qoffscreen.dll`、macOS `platforms/libqoffscreen.dylib`、Linux `platforms/libqoffscreen.so`）。打包脚本会自动补齐。

开发用截图/渲染参数：

```bash
./build/bin/StoryBoard --render-demo build/demo.png
./build/bin/StoryBoard --demo-canvas examples/demo-canvas.sbcanvas
./build/bin/StoryBoard --ui-screenshot build/ui.png
./build/bin/StoryBoard --item-properties-screenshot build/props.png
```

> Windows 下把 `./build/bin/StoryBoard` 换成 `./build/bin/StoryBoard.exe`。

## 项目结构

```text
StoryBoard/
├─ CMakeLists.txt
├─ src/                 主窗口、画布场景/视图、图元、命令、对话框、设置、i18n、自检
├─ resources/           图标、下拉箭头、qrc、Windows rc 模板
├─ translations/        storyboard_zh.ts 与 qtbase_zh_CN.qm
├─ scripts/             package-portable.sh 与各平台包内 README 模板
├─ examples/            示例画布
└─ docs/                README 截图
```

核心实现：

- 图元只存在于场景坐标；查看模式只改变 `QGraphicsView` 视口变换。
- 绘制顺序由 `QGraphicsItem::zValue = 图层序号 × 1000000 + height` 推导。
- 图元命中以 `shape()` 为准，选中手柄、粗描边区域都并入命中范围。
- 文字用 `QFontMetricsF` 计算自然尺寸，缩放时按比例调整字号与行距。
- 图形用 `QPainterPath` 描述，支持纯色与多种纹理填充；纯色填充参与完全遮挡。
- 画布载入时修复重复 `height`，未知图元类型会被拒绝，避免半加载。

## 已知限制

- 暂无图元分组；文字不支持同一图元内混排不同字号/颜色。
- 图形不支持逐图元独立设置描边线宽或虚线。
- 图层删除仅支持空图层；图层拖拽排序尚未提供，可用面板按钮上移/下移。
- 画布内嵌图片，包含大图时 `.sbcanvas` 会相应变大。
- 撤销历史上限 300 步；删除的图元在文档重置前保留在内存中以便重做。
- 应用运行验证目前在 Windows 11 + MinGW 完成；macOS / Linux 的构建与打包分支已提供，但尚未实机验证。
