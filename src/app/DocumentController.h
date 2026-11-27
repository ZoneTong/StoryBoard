#pragma once

#include <QString>
#include <QStringList>

class CanvasSession;
class MainWindow;

// 画布文档：新建 / 打开 / 保存 / 另存为，最近画布菜单与画布文件夹入口，
// 以及当前文件路径与窗口标题。图元数据本身仍由 CanvasScene 与撤销栈持有。
class DocumentController {
  public:
    explicit DocumentController(MainWindow* window);

    void NewCanvas();
    void OpenCanvasDialog();
    // 一次选多个文件：逐个开进标签，重复路径切到已有标签，坏文件汇总成一次提示。
    int OpenCanvasFiles(const QStringList& paths);
    bool OpenCanvasFile(const QString& path);
    bool LoadCanvasFile(const QString& path, bool showErrors, QString* errorOut = nullptr);
    bool SaveCanvas();
    bool SaveCanvasAs();
    void ExportExcalidraw();
    void ExportCustomShape();
    bool SaveToFile(const QString& path);
    bool MaybeSave();
    // 按会话询问未保存：保存会先切到该会话再落盘。
    bool MaybeSaveSession(CanvasSession* session);
    bool CloseCanvasSession(CanvasSession* session);
    // 关窗前逐页询问，任一取消返回 false。
    bool CloseAllCanvases();
    void UpdateCanvasesMenu();
    void OpenCanvasesFolder();
    void UpdateTitle();
    QString DocumentName() const;

  private:
    MainWindow* m_window = nullptr;
};
