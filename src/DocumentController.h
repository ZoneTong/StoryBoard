#pragma once

#include <QString>

class MainWindow;

// 画布文档：新建 / 打开 / 保存 / 另存为，最近画布菜单与画布文件夹入口，
// 以及当前文件路径与窗口标题。图元数据本身仍由 CanvasScene 与撤销栈持有。
class DocumentController {
  public:
    explicit DocumentController(MainWindow* window);

    void newCanvas();
    void openCanvasDialog();
    bool openCanvasFile(const QString& path);
    bool loadCanvasFile(const QString& path, bool showErrors);
    bool saveCanvas();
    bool saveCanvasAs();
    void exportExcalidraw();
    void exportCustomShape();
    bool saveToFile(const QString& path);
    bool maybeSave();
    void updateCanvasesMenu();
    void openCanvasesFolder();
    void updateTitle();
    QString documentName() const;

  private:
    MainWindow* m_window = nullptr;
};
