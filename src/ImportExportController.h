#pragma once

#include "LineArtConverter.h"

#include <QString>
#include <QStringList>

class MainWindow;
class QRectF;
class QDropEvent;
class QMimeData;

// 画布导出（PNG / JPEG / PDF，可按可见图层拆分）与图片 / 线稿图导入：
// 文件选择、渲染落盘、图元落位与导入后的状态栏反馈。
class ImportExportController {
  public:
    explicit ImportExportController(MainWindow* window);

    void ExportCanvas();
    bool ExportToFile(const QString& path, const QRectF& area, qreal scale, const QString& format,
                      bool showCompletion = true, int layerId = -1);
    void ImportImagesDialog();
    void ImportOutlineImageDialog();
    void ImportOutlineImageFile(const QString& file, const LineArtConverter::Options& options);
    void ImportImageFiles(const QStringList& files);

    // 拖拽落盘：判断拖入的是否为画板文件或图片，按类型载入或插入。
    bool AcceptsDropUrls(const QMimeData* mimeData) const;
    void HandleDrop(QDropEvent* event);

  private:
    MainWindow* m_window = nullptr;
};
