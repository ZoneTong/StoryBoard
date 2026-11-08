StoryBoard @VERSION@ 便携版（Windows x64）

运行方法
1. 将整个文件夹解压到任意位置，路径可以包含中文。
2. 双击 StoryBoard.exe 即可运行，不需要安装 Qt 或额外的运行库。
3. 新建/打开/保存画布时，文件位置由程序内的文件对话框选择。

目录说明
- StoryBoard.exe：主程序。
- Qt6*.dll、libgcc_s_seh-1.dll、libstdc++-6.dll、libwinpthread-1.dll：运行所需的 Qt 与 MinGW 运行库。
- platforms、imageformats、styles、iconengines、tls 等目录：Qt 插件，请勿删除或移出程序目录。
- examples\demo-canvas.sbcanvas：示例画布，可在程序里通过“打开画布”载入。
- portable-selftest.txt：本包在无 Qt 环境下运行自检的结果。

说明
- 这是绿色免安装包，但界面语言设置会写入当前用户配置目录，不会写入安装目录。
- 命令行自检可用：StoryBoard.exe --self-test selftest.txt
- 建议在 Windows 10/11 x64 上运行。
