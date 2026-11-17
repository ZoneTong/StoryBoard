@APP_NAME@ @VERSION@ 便携版（Windows x64）

运行
1. 将整个文件夹解压到任意位置，路径可以包含中文。
2. 双击 @APP_NAME@.exe 运行，不需要安装 Qt 或额外运行库。
3. 「插入 → 插入图片…」把图片作为图元；「文件 → 导入图片线稿化…」把图片色块轮廓识别成笔画。

目录
- @APP_NAME@.exe：主程序。
- Qt6*.dll、libgcc_s_seh-1.dll、libstdc++-6.dll、libwinpthread-1.dll：Qt 与 MinGW 运行库。
- platforms、imageformats、styles、iconengines、tls：Qt 插件，请勿删除或移出程序目录。
- examples/demo-canvas.sbcanvas：示例画布。
- shapes/：自定义图形模板，展开「常见图形 → 自定义图形」即可看到。
- portable-selftest.txt：本包运行自检结果。
- LICENSE、LICENSE-LGPL-3.0、THIRD-PARTY-NOTICES.md：本项目 GPL-3.0 许可证、Qt 的 LGPLv3 全文与第三方组件归属，请勿删除。

说明
- 免安装包；设置（含界面语言）写入当前用户配置目录的 @SETTINGS_FILE@，不写注册表，也不写安装目录。
- 命令行自检：@APP_NAME@.exe --self-test selftest.txt
- 建议在 Windows 10/11 x64 上运行。
