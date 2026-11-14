#pragma once

#include "CanvasItem.h"

#include <QPointer>
#include <QWidget>

class QFormLayout;
class QLineEdit;
class QPushButton;

// 属性窗的图片区块：图片名称可改名，「替换图片…」按钮把选图动作交回外壳，
// 由外壳读取文件并把新图纳入同一条撤销记录。
class ItemPropertiesDialogImageBlock : public QWidget {
    Q_OBJECT

  public:
    ItemPropertiesDialogImageBlock(CanvasItem* item, QWidget* parent = nullptr);

    void addRows(QFormLayout* form);

    QLineEdit* imageNameEdit() const { return m_imageNameEdit; }
    QPushButton* imageButton() const { return m_imageButton; }

  signals:
    void chooseImageRequested();

  private:
    QLineEdit* m_imageNameEdit = nullptr;
    QPushButton* m_imageButton = nullptr;
};
