#pragma once

#include "CanvasItem.h"

#include <QFont>
#include <QPointer>
#include <QWidget>

class QFormLayout;
class QPlainTextEdit;
class QPushButton;
class QToolButton;

// 属性窗的文字区块：正文直接在窗内编辑，文字颜色与字体随文字图元初始化；
// 字体改动后发出信号，由外壳按自然尺寸同步位置尺寸框。
class ItemPropertiesDialogTextBlock : public QWidget {
    Q_OBJECT

  public:
    ItemPropertiesDialogTextBlock(CanvasItem* item, QWidget* parent = nullptr);

    void addRows(QFormLayout* form);

    QPlainTextEdit* textEdit() const { return m_textEdit; }
    QToolButton* textColorButton() const { return m_textColorButton; }
    QPushButton* fontButton() const { return m_fontButton; }
    QFont TextFont() const { return m_textFont; }

    void chooseFont();

  signals:
    void fontChanged();

  private:
    void updateFontButton();

    QFont m_textFont;
    QPlainTextEdit* m_textEdit = nullptr;
    QToolButton* m_textColorButton = nullptr;
    QPushButton* m_fontButton = nullptr;
};
