#pragma once

#include "items/properties/ImageFill.h"

#include <QDialog>
#include <QPixmap>
#include <QVector>

class QListWidget;

// 需求 13：图片资源面板。替换背景图片时先弹这个面板，列出当前画布已用到的图片，
// 也可以从这里挑一个本地文件加进来；选定后返回图片，调用方负责批量套到选中图元。
class ImagePickerDialog : public QDialog {
    Q_OBJECT
  public:
    explicit ImagePickerDialog(const QVector<QPixmap>& images, QWidget* parent = nullptr);

    QPixmap SelectedImage() const;

    // 常用入口：直接以模态方式打开，取消时返回空图。
    static QPixmap Pick(QWidget* parent, const QVector<QPixmap>& images, const QString& title);

  private:
    void AddImage(const QPixmap& image);
    void ChooseLocalFile();

    QListWidget* m_list = nullptr;
    QString m_localName;
};

// 需求 13：把画布上已经用到的背景图片收集成去重列表（按像素内容去重），
// 供资源面板展示，也让「同一图片共用一份」在交互层就成立。
QVector<QPixmap> CollectCanvasImages(const class CanvasScene* scene);
