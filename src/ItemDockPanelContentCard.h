#pragma once

#include <QColor>
#include <QFont>
#include <QList>
#include <QWidget>

class QFontComboBox;
class QLineEdit;
class QSpinBox;
class QToolButton;
class TextItem;
class ImageItem;

// 编辑图元卡的内容属性行：文字正文 / 字体 / 文字颜色，以及图片名称与替换入口。
// 这些行只在当前选中存在对应图元时显示，避免给图形、笔画展示无效控件。
class ItemDockPanelContentCard : public QWidget {
    Q_OBJECT

  public:
    explicit ItemDockPanelContentCard(QWidget* parent = nullptr);

    void refresh(const QList<TextItem*>& texts, const QList<ImageItem*>& images);
    QWidget* textContentRow() const { return m_textContentRow; }
    QWidget* textFontRow() const { return m_textFontRow; }
    QWidget* textColorRow() const { return m_textColorRow; }
    QWidget* imageRow() const { return m_imageRow; }

  signals:
    void textContentChosen(const QString& text);
    void textFontChosen(const QFont& font);
    void textColorChosen(const QColor& color);
    void imageNameChosen(const QString& name);
    void replaceImageRequested();

  private:
    QWidget* makeTextColorRow();
    QFont composedFont() const;
    void emitFontChanged();

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
    QWidget* m_imageRow = nullptr;
    QLineEdit* m_imageNameEdit = nullptr;
    QToolButton* m_replaceImageButton = nullptr;
    QFont m_textFont;
    bool m_refreshing = false;
};
