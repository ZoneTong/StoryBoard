#pragma once

#include <QString>

class MainWindow;

// 画布文档：新建 / 打开 / 保存 / 另存为，最近画布菜单与画布文件夹入口，
// 以及当前文件路径与窗口标题。图元数据本身仍由 CanvasScene 与撤销栈持有。
class DocumentController {
  public:
    explicit DocumentController(MainWindow* window);

    void NewCanvas();
    void OpenCanvasDialog();
    bool OpenCanvasFile(const QString& path);
    bool LoadCanvasFile(const QString& path, bool showErrors);
    bool SaveCanvas();
    bool SaveCanvasAs();
    void ExportExcalidraw();
    void ExportCustomShape();
    bool SaveToFile(const QString& path);
    bool MaybeSave();
    void UpdateCanvasesMenu();
    void OpenCanvasesFolder();
    void UpdateTitle();
    QString DocumentName() const;

  private:
    MainWindow* m_window = nullptr;
};
