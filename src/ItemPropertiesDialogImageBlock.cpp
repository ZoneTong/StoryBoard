#include "ItemPropertiesDialogImageBlock.h"

#include "ImageItem.h"
#include "ItemPropertiesDialog.h"

#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>

ItemPropertiesDialogImageBlock::ItemPropertiesDialogImageBlock(CanvasItem* item, QWidget* parent) : QWidget(parent)
{
    auto* image = dynamic_cast<ImageItem*>(item);
    if (!image) {
        return;
    }

    m_imageNameEdit = new QLineEdit(this);
    m_imageNameEdit->setObjectName(QStringLiteral("ItemPropertiesImageValue"));
    m_imageNameEdit->setText(image->imageName());
    m_imageNameEdit->setPlaceholderText(ItemPropertiesDialog::tr("Image"));

    // 需求 39：图片本体也算可编辑属性，允许在属性窗内直接替换图片。
    m_imageButton = new QPushButton(ItemPropertiesDialog::tr("Replace image…"), this);
    m_imageButton->setObjectName(QStringLiteral("ItemPropertiesReplaceImageValue"));
    connect(m_imageButton, &QPushButton::clicked, this, &ItemPropertiesDialogImageBlock::chooseImageRequested);
}

void ItemPropertiesDialogImageBlock::addRows(QFormLayout* form)
{
    if (m_imageNameEdit) {
        form->addRow(ItemPropertiesDialog::tr("Image name"), m_imageNameEdit);
    }
    if (m_imageButton) {
        form->addRow(ItemPropertiesDialog::tr("Image"), m_imageButton);
    }
}
