#pragma once

#include "CanvasItem.h"
#include "TextAttributes.h"

#include <QColor>
#include <QFont>
#include <QString>

// 文字图元：几何来自 CanvasItem，正文 / 字体 / 颜色 / 自然尺寸来自 TextAttributes。
// 基类只保留虚文字 facet 接口，实际状态读写都委托给属性基类。
class TextItem : public CanvasItem, public TextAttributes {
    Q_OBJECT
  public:
    explicit TextItem(QGraphicsItem* parent = nullptr);

    QString Kind() const override { return QStringLiteral("text"); }
    ItemCategory Category() const override { return ItemCategory::TEXT; }
    void Accept(CanvasItemVisitor& visitor) override;
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
    QString TextContent() const override { return TextAttributes::TextContent(); }
    QFont TextFont() const override { return TextAttributes::TextFont(); }
    QColor TextColor() const override { return TextAttributes::TextColor(); }
    QSizeF TextNaturalSize() const override { return TextAttributes::TextNaturalSize(); }
    TextItemState TextState() const override;
    void ApplyTextState(const TextItemState& state) override;

    QSizeF NaturalSize() const { return TextNaturalSize(); }
    QString Text() const { return TextContent(); }
    static QSizeF Measure(const QString& text, const QFont& font);

    TextItemState State() const;
    void ApplyState(const TextItemState& state);

  protected:
    void PaintContent(QPainter* painter) override;
    qreal FramePadding() const override { return 20.0; }
    void TextChanged() override;
};
