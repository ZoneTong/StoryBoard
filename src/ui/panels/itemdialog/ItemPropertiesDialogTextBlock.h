#pragma once

#include "items/CanvasItem.h"

#include <QFont>
#include <QPointer>
#include <QWidget>

class QFormLayout;
class QPlainTextEdit;
class QPushButton;
class QToolButton;
class PropertyRowRegistry;

// 属性窗的文字区块：正文直接在窗内编辑，文字颜色与字体随文字图元初始化；
// 字体改动后发出信号，由外壳按自然尺寸同步位置尺寸框。
// 三行构造时一次建齐；SetItem 按图元回填，仅文字图元显示本块。
class ItemPropertiesDialogTextBlock : public QWidget {
    Q_OBJECT

  public:
    explicit ItemPropertiesDialogTextBlock(QWidget* parent = nullptr);

    void AddRows(QFormLayout* form, PropertyRowRegistry* rows);

    void SetItem(CanvasItem* item);

    QPlainTextEdit* TextEdit() const { return m_textEdit; }
    QToolButton* TextColorButton() const { return m_textColorButton; }
    QPushButton* FontButton() const { return m_fontButton; }
    QFont TextFont() const { return m_textFont; }

    void ChooseFont();

  signals:
    void fontChanged();

  private:
    void UpdateFontButton();

    QFormLayout* m_form = nullptr;
    QFont m_textFont;
    QPlainTextEdit* m_textEdit = nullptr;
    QToolButton* m_textColorButton = nullptr;
    QPushButton* m_fontButton = nullptr;
};
