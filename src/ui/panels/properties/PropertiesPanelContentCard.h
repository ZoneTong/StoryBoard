#pragma once

#include <QColor>
#include <QFont>
#include <QList>
#include <QWidget>

class QFontComboBox;
class QLineEdit;
class QSpinBox;
class QToolButton;
class CanvasItem;
class TextItem;

// 编辑面板的内容属性行：文字正文 / 字体 / 文字颜色，以及图片名称与替换入口。
// 这些行只在当前选中存在对应图元时显示，避免给图形、笔画展示无效控件。
class PropertiesPanelContentCard : public QWidget {
    Q_OBJECT

  public:
    explicit PropertiesPanelContentCard(QWidget* parent = nullptr);

    void Refresh(const QList<TextItem*>& texts);
    QWidget* TextContentRow() const { return m_textContentRow; }
    QWidget* TextFontRow() const { return m_textFontRow; }
    QWidget* TextColorRow() const { return m_textColorRow; }

  signals:
    void textContentChosen(const QString& text);
    void textFontChosen(const QFont& font);
    void textColorChosen(const QColor& color);

  private:
    QWidget* MakeTextColorRow();
    QFont ComposedFont() const;
    void EmitFontChanged();

    QWidget* m_textContentRow = nullptr;
    QLineEdit* m_textEdit = nullptr;
    QWidget* m_textFontRow = nullptr;
    QFontComboBox* m_fontCombo = nullptr;
    QSpinBox* m_fontSizeSpin = nullptr;
    QToolButton* m_boldButton = nullptr;
    QToolButton* m_italicButton = nullptr;
    QWidget* m_textColorRow = nullptr;
    QToolButton* m_textColorButton = nullptr;
    QList<QToolButton*> m_textColorSwatches;
    QColor m_textColorFallback = QColor(30, 41, 59);
    QFont m_textFont;
    bool m_refreshing = false;
};
