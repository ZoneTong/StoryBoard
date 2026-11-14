#include "ItemPropertiesDialogTextBlock.h"

#include "ItemPropertiesDialog.h"
#include "ItemPropertiesDialogFields.h"
#include "TextItem.h"

#include <QFontDialog>
#include <QFormLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>

ItemPropertiesDialogTextBlock::ItemPropertiesDialogTextBlock(CanvasItem* item, QWidget* parent) : QWidget(parent)
{
    auto* text = dynamic_cast<TextItem*>(item);
    if (!text) {
        return;
    }

    m_textFont = text->textFont();

    // 需求 39：文字正文也是可编辑属性，直接在属性窗里改，不必先离开再开文字对话框。
    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setObjectName(QStringLiteral("ItemPropertiesTextValue"));
    m_textEdit->setPlainText(text->text());
    m_textEdit->setMinimumHeight(88);

    m_textColorButton =
        ItemPropertiesFields::makeColorField(this, text->textColor(), QStringLiteral("ItemPropertiesTextColorValue"));

    m_fontButton = new QPushButton(this);
    m_fontButton->setObjectName(QStringLiteral("ItemPropertiesFontValue"));
    m_fontButton->setMinimumWidth(180);
    updateFontButton();
    connect(m_fontButton, &QPushButton::clicked, this, &ItemPropertiesDialogTextBlock::chooseFont);
}

void ItemPropertiesDialogTextBlock::addRows(QFormLayout* form)
{
    if (m_textEdit) {
        form->addRow(ItemPropertiesDialog::tr("Text content"), m_textEdit);
    }
    if (m_textColorButton) {
        form->addRow(ItemPropertiesDialog::tr("Text color"), m_textColorButton);
    }
    if (m_fontButton) {
        form->addRow(ItemPropertiesDialog::tr("Font"), m_fontButton);
    }
}

void ItemPropertiesDialogTextBlock::updateFontButton()
{
    if (m_fontButton) {
        m_fontButton->setText(ItemPropertiesFields::fontText(m_textFont));
    }
}

void ItemPropertiesDialogTextBlock::chooseFont()
{
    bool ok = false;
    // 使用 Qt 自绘字体窗，避免 Windows 原生字体对话框跟随系统 UI 语言而
    // 与应用内语言不一致；标准标签由嵌入的 qtbase_zh_CN.qm 提供中文。
    const QFont font = QFontDialog::getFont(&ok, m_textFont, this, ItemPropertiesDialog::tr("Choose font"),
                                            QFontDialog::DontUseNativeDialog);
    if (!ok) {
        return;
    }

    m_textFont = font;
    updateFontButton();
    emit fontChanged();
}
