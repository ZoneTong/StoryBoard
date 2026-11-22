#include "ItemPropertiesDialogTextBlock.h"

#include "CanvasItem.h"
#include "ItemPropertiesDialog.h"
#include "ItemPropertiesDialogFields.h"

#include <QFontDialog>
#include <QFormLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>

ItemPropertiesDialogTextBlock::ItemPropertiesDialogTextBlock(QWidget* parent) : QWidget(parent)
{
    // 需求 39：文字正文也是可编辑属性，直接在属性窗里改，不必先离开再开文字对话框。
    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setObjectName(QStringLiteral("ItemPropertiesTextValue"));
    m_textEdit->setMinimumHeight(88);

    m_textColorButton =
        ItemPropertiesFields::MakeColorField(this, QColor(), QStringLiteral("ItemPropertiesTextColorValue"));

    m_fontButton = new QPushButton(this);
    m_fontButton->setObjectName(QStringLiteral("ItemPropertiesFontValue"));
    m_fontButton->setMinimumWidth(180);
    UpdateFontButton();
    connect(m_fontButton, &QPushButton::clicked, this, &ItemPropertiesDialogTextBlock::ChooseFont);
}

void ItemPropertiesDialogTextBlock::AddRows(QFormLayout* form)
{
    m_form = form;
    form->addRow(ItemPropertiesDialog::tr("Text content"), m_textEdit);
    form->addRow(ItemPropertiesDialog::tr("Text color"), m_textColorButton);
    form->addRow(ItemPropertiesDialog::tr("Font"), m_fontButton);
}

void ItemPropertiesDialogTextBlock::SetItem(CanvasItem* item)
{
    // 文字行的显隐按图元类别判断；正文 / 颜色 / 字体一律从属性协议读，基类不再有文字 facet。
    const bool showText = item && item->Category() == ItemCategory::TEXT;
    if (showText) {
        m_textFont = item->PropertyValue(ItemProperty::TEXT_FONT).value<QFont>();
        m_textEdit->setPlainText(item->PropertyValue(ItemProperty::TEXT_CONTENT).toString());
        m_textColorButton->setProperty("itemColor", item->PropertyValue(ItemProperty::TEXT_COLOR).value<QColor>());
        ItemPropertiesFields::RefreshColorButton(m_textColorButton);
        UpdateFontButton();
    }
    if (m_form) {
        m_form->setRowVisible(m_textEdit, showText);
        m_form->setRowVisible(m_textColorButton, showText);
        m_form->setRowVisible(m_fontButton, showText);
    }
}

void ItemPropertiesDialogTextBlock::UpdateFontButton()
{
    if (m_fontButton) {
        m_fontButton->setText(ItemPropertiesFields::FontText(m_textFont));
    }
}

void ItemPropertiesDialogTextBlock::ChooseFont()
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
    UpdateFontButton();
    emit fontChanged();
}
