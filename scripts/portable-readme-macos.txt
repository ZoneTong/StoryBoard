@APP_NAME@ @VERSION@ 便携版（macOS）

运行
1. 将整个目录解压到任意位置。
2. 双击 @APP_NAME@.app；若提示无法验证开发者，右键选择“打开”。

目录
- @APP_NAME@.app：主程序。
- examples/demo-canvas.sbcanvas：示例画布。
- shapes/：自定义图形模板，展开「常见图形 → 自定义图形」即可看到。
- portable-selftest.txt：本包运行自检结果。
- README.txt：本说明。
- LICENSE、LICENSE-LGPL-3.0、THIRD-PARTY-NOTICES.md：本项目 GPL-3.0 许可证、Qt 的 LGPLv3 全文与第三方组件归属，请勿删除。

说明
- 免安装包；设置（含界面语言）写入当前用户配置目录的 @SETTINGS_FILE@，不使用注册表。
- 命令行自检：./@APP_NAME@.app/Contents/MacOS/@APP_NAME@ --self-test selftest.txt
