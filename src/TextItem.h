#pragma once

#include "CanvasItem.h"

#include <QColor>
#include <QFont>
#include <QString>

class TextItem : public CanvasItem {
    Q_OBJECT
  public:
    explicit TextItem(QGraphicsItem* parent = nullptr);

    QString kind() const override { return QStringLiteral("text"); }
    QString displayName() const override;
    QString typeDisplayName() const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
    ItemPropertySet properties() const override;
    QVariant propertyValue(ItemProperty property) const override;
    void setPropertyValue(ItemProperty property, const QVariant& value) override;
    QPainterPath outlineShape() const override;
    QPainterPath paintedShape() const override;
    QPainterPath clippingShape() const override;

    bool hasTextFacet() const override { return true; }
    QString textContent() const override { return m_text; }
    QFont textFont() const override { return m_font; }
    QColor textColor() const override { return m_color; }
    QSizeF textNaturalSize() const override { return m_baseSize; }
    TextItemState textState() const override;
    void applyTextState(const TextItemState& state) override;

    void setTextFormat(const QString& text, const QFont& font, const QColor& color);
    QString text() const { return m_text; }

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
