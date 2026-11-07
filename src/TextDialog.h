#pragma once

#include <QColor>
#include <QDialog>
#include <QFont>

class QCheckBox;
class QDialogButtonBox;
class QFontComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

class TextDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TextDialog(QWidget* parent = nullptr);

    void setHeading(const QString& heading);
    void setTextValue(const QString& text);
    void setTextFont(const QFont& font);
    void setTextColor(const QColor& color);

    QString textValue() const;
    QFont textFont() const;
    QColor textColor() const { return m_color; }

private:
    void refreshColorButton();
    void updateAcceptState();
    void chooseColor();

    QLabel* m_heading = nullptr;
    QPlainTextEdit* m_edit = nullptr;
    QFontComboBox* m_fontCombo = nullptr;
    QSpinBox* m_sizeSpin = nullptr;
    QPushButton* m_colorButton = nullptr;
    QCheckBox* m_boldCheck = nullptr;
    QCheckBox* m_italicCheck = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
    QColor m_color = QColor(30, 41, 59);
};
