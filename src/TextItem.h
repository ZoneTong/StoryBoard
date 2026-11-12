#pragma once

#include "CanvasItem.h"

#include <QColor>
#include <QFont>
#include <QString>

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

class TextItem : public CanvasItem {
    Q_OBJECT
  public:
    explicit TextItem(QGraphicsItem* parent = nullptr);

    QString kind() const override { return QStringLiteral("text"); }
    QString displayName() const override;
    QString typeDisplayName() const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
    QPainterPath outlineShape() const override;
    QPainterPath paintedShape() const override;

    void setTextFormat(const QString& text, const QFont& font, const QColor& color);
    QString text() const { return m_text; }
    QFont textFont() const { return m_font; }
    QColor textColor() const { return m_color; }

    QSizeF naturalSize() const { return m_baseSize; }
    static QSizeF measure(const QString& text, const QFont& font);

    TextItemState state() const;
    void applyState(const TextItemState& state);

  protected:
    void paintContent(QPainter* painter) override;
    qreal framePadding() const override { return 20.0; }

  private:
    QString m_text;
    QFont m_font;
    QColor m_color = QColor(30, 41, 59);
    QSizeF m_baseSize{120.0, 44.0};
};
