StoryBoard @VERSION@ 便携版（macOS）

运行方法
1. 将整个目录解压到任意位置。
2. 双击 StoryBoard.app 运行；如果系统提示无法验证开发者，请右键选择“打开”。

目录说明
- StoryBoard.app：主程序。
- examples/demo-canvas.sbcanvas：示例画布。
- portable-selftest.txt：本包运行自检的结果。
- README.txt：本说明。

说明
- 这是免安装包，但所有设置（含界面语言）都写进当前用户配置目录的 StoryBoard.ini，不使用注册表。
- 命令行自检可用：
  ./StoryBoard.app/Contents/MacOS/StoryBoard --self-test selftest.txt
