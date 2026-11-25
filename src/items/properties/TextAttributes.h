#pragma once

#include <QColor>
#include <QFont>
#include <QJsonObject>
#include <QPointF>
#include <QSizeF>
#include <QString>

// 文字属性基类：正文、字体、颜色、自然尺寸的唯一存处。
// TextItem 只保留 CanvasItem 的几何，文字状态通过本类多继承获得；
// 编辑面板 / 属性窗 / 撤销都按这一份状态读写，未来图形标签或多边形文字层可直接复用。
struct TextAttributesState {
    QString text;
    QFont font;
    QColor color;
    QSizeF baseSize;

    bool operator==(const TextAttributesState& other) const
    {
        return text == other.text && font == other.font && color == other.color && baseSize == other.baseSize;
    }
    bool operator!=(const TextAttributesState& other) const { return !(*this == other); }
};

// 文字图元的状态快照（撤销 / 属性窗共用）；放在文字属性头里，
// 上层不必为了读写文字而强转到 TextItem。
struct TextItemState {
    QString text;
    QFont font;
    QColor color;
    QPointF pos;
    QSizeF size;
    QSizeF baseSize;

    bool operator==(const TextItemState& other) const
    {
        return text == other.text && font == other.font && color == other.color && pos == other.pos &&
               size == other.size && baseSize == other.baseSize;
    }
    bool operator!=(const TextItemState& other) const { return !(*this == other); }
};

class TextAttributes {
  public:
    TextAttributes();
    virtual ~TextAttributes() = default;

    QString TextContent() const { return m_text; }
    QFont TextFont() const { return m_font; }
    QColor TextColor() const { return m_color; }
    QSizeF TextNaturalSize() const { return m_baseSize; }

    void SetTextContent(const QString& text);
    void SetTextFont(const QFont& font);
    void SetTextColor(const QColor& color);
    void SetTextFormat(const QString& text, const QFont& font, const QColor& color);

    TextAttributesState TextAttributeState() const;
    void ApplyTextAttributeState(const TextAttributesState& state);

    // JSON 键沿用 text / color / fontFamily / fontSize / fontWeight / italic /
    // underline / strikeOut / baseWidth / baseHeight，旧画布不需要迁移。
    void WriteTextJson(QJsonObject& json) const;
    void ReadTextJson(const QJsonObject& json);

    static QSizeF Measure(const QString& text, const QFont& font);
    // 画板文字的默认字族（新建、导入同源），保留字族便于按指定字号量自然尺寸。
    static QFont DefaultFont(qreal pointSize = 32.0);

  protected:
    // 文字内容或字体变化后的钩子：文字图元据此重算自然尺寸并刷新。
    virtual void TextChanged() {}

    QString m_text;
    QFont m_font;
    QColor m_color = QColor(30, 41, 59);
    QSizeF m_baseSize;
};
