#pragma once

#include "LineArtConverter.h"

#include <QString>
#include <QStringList>

class MainWindow;
class QRectF;

// 画布导出（PNG / JPEG / PDF，可按可见图层拆分）与图片 / 线稿图导入：
// 文件选择、渲染落盘、图元落位与导入后的状态栏反馈。
class ImportExportController {
  public:
    explicit ImportExportController(MainWindow* window);

    void exportCanvas();
    bool exportToFile(const QString& path, const QRectF& area, qreal scale, const QString& format,
                      bool showCompletion = true, int layerId = -1);
    void importImagesDialog();
    void importOutlineImageDialog();
    void importOutlineImageFile(const QString& file, const LineArtConverter::Options& options);
    void importImageFiles(const QStringList& files);

  private:
    MainWindow* m_window = nullptr;
};
