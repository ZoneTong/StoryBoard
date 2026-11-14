@APP_NAME@ @VERSION@ 便携版（Linux）

运行
1. 将整个目录解压到任意位置。
2. 在终端进入目录后执行：./@APP_NAME@
3. 若提示缺少图形库，按发行版安装 Qt/X11 相关运行库。

目录
- @APP_NAME@：主程序。
- libQt6*.so*：Qt 运行库。
- plugins：Qt 插件，请勿删除或移出程序目录。
- examples/demo-canvas.sbcanvas：示例画布。
- shapes/：自定义图形模板，展开「常见图形 → 自定义图形」即可看到。
- portable-selftest.txt：本包运行自检结果。

说明
- 免安装包；设置（含界面语言）写入当前用户配置目录的 @SETTINGS_FILE@，不使用注册表。
- 命令行自检：./@APP_NAME@ --self-test selftest.txt
