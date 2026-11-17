# 第三方组件与许可证

本项目（StoryBoard）自身代码按 GPL-3.0-only 发布，见仓库根目录 `LICENSE`。下列第三方组件的版权与许可证不属于本项目，分发时需一并保留。

## Qt 6

- 组件与版本：Qt 6.12.0（The Qt Company Ltd.）。本项目直接链接 Qt6Core / Qt6Gui / Qt6Widgets，便携包中的 Qt6Network、Qt6Svg 及 `platforms`、`imageformats`、`iconengines`、`styles`、`tls`、`networkinformation`、`generic` 插件由 windeployqt 按实际依赖自动部署。
- 许可证：Qt 6 开源版同时提供 LGPLv3 与 GPLv3 两种选项（另有商业授权）。本项目的 Windows / macOS / Linux 便携包按 **LGPLv3** 分发 Qt 库。
- 链接方式：动态链接。未静态链接 Qt，也未修改 Qt 源码；Qt 库以独立的 `.dll` / `.dylib` / `.so` 形式随包提供，你可以用自行编译的兼容版本替换后直接运行，无需重新链接本项目，因此满足 LGPLv3 关于「可替换、可重新链接」的要求。
- 源码获取：Qt 6.12.0 的对应源码可从 <https://download.qt.io/official_releases/qt/6.12/6.12.0/submodules/> 或 <https://code.qt.io/> 获取；也可通过 <https://github.com/ZoneTong/StoryBoard> 的 Issue 向维护者索取，我们会在收到请求后提供所用版本的完整源码副本或其下载地址。
- LGPLv3 全文：仓库根目录 `LICENSE-LGPL-3.0`（LGPLv3 正文及其引用的 GPLv3 正文，GPLv3 全文另见 `LICENSE`）。
- 兼容性说明：LGPLv3 与 GPLv3 兼容，本项目按 GPL-3.0-only 发布不冲突；若改按 Qt 的 GPLv3 选项使用，同样满足 GPLv3 的源码提供要求。

## MinGW 运行库（仅 Windows 包）

- 组件：`libgcc_s_seh-1.dll`、`libstdc++-6.dll`、`libwinpthread-1.dll`（GCC / MinGW-w64 运行时）。
- 许可证：`libgcc` 与 `libstdc++` 采用 GPLv3 加 GCC Runtime Library Exception，允许随程序二进制分发而不触发源码公开义务；`libwinpthread` 采用宽松许可（MIT 类）。

## 其他

- `docs/` 下的界面截图、`examples/` 下的示例画布与自定义图形模板均为本项目原创，随本项目许可证发布。
- 若后续引入新的第三方库或字体，请在本文件中追加对应的版权与许可证条目。
