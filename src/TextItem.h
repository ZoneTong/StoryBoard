#pragma once

#include "CanvasItem.h"

#include <QColor>
#include <QFont>
#include <QString>

class TextItem : public CanvasItem {
    Q_OBJECT
  public:
    explicit TextItem(QGraphicsItem* parent = nullptr);

    QString Kind() const override { return QStringLiteral("text"); }
    QString DisplayName() const override;
    QString TypeDisplayName() const override;
    QJsonObject ToJson() const override;
    void FromJson(const QJsonObject& json) override;
    ItemPropertySet Properties() const override;
    QVariant PropertyValue(ItemProperty property) const override;
    void SetPropertyValue(ItemProperty property, const QVariant& value) override;
    QPainterPath OutlineShape() const override;
    QPainterPath PaintedShape() const override;
    QPainterPath ClippingShape() const override;

    bool HasTextFacet() const override { return true; }
    QString TextContent() const override { return m_text; }
    QFont TextFont() const override { return m_font; }
    QColor TextColor() const override { return m_color; }
    QSizeF TextNaturalSize() const override { return m_baseSize; }
    TextItemState TextState() const override;
    void ApplyTextState(const TextItemState& state) override;

    void setTextFormat(const QString& text, const QFont& font, const QColor& color);
    QString text() const { return m_text; }

    QSizeF naturalSize() const { return m_baseSize; }
    static QSizeF measure(const QString& text, const QFont& font);

    TextItemState State() const;
    void ApplyState(const TextItemState& state);

  protected:
    void PaintContent(QPainter* painter) override;
    qreal FramePadding() const override { return 20.0; }

  private:
    QString m_text;
    QFont m_font;
    QColor m_color = QColor(30, 41, 59);
    QSizeF m_baseSize{120.0, 44.0};
};
