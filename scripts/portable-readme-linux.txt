StoryBoard @VERSION@ 便携版（Linux）

运行方法
1. 将整个目录解压到任意位置。
2. 打开终端，进入解压后的目录，执行：
   ./StoryBoard
3. 如果提示缺少系统图形库，请按发行版安装 Qt/X11 相关运行库。

目录说明
- StoryBoard：主程序。
- libQt6*.so*：Qt 运行库。
- plugins：Qt 插件，请勿删除或移出程序目录。
- examples/demo-canvas.sbcanvas：示例画布。
- portable-selftest.txt：本包运行自检的结果。
- README.txt：本说明。

说明
- 这是免安装包，但所有设置（含界面语言）都写进当前用户配置目录的 StoryBoard.ini，不使用注册表。
- 命令行自检可用：./StoryBoard --self-test selftest.txt
