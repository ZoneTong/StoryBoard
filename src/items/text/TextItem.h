#pragma once

#include "items/CanvasItem.h"
#include "items/properties/TextAttributes.h"

#include <QColor>
#include <QFont>
#include <QSizeF>
#include <QString>
#include <QStringList>

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
    QPainterPath GeometryOutline() const override;
    QPainterPath GeometryPaintedShape() const override;
    QPainterPath ClippingShape() const override;

    QSizeF NaturalSize() const { return TextNaturalSize(); }
    QString Text() const { return TextContent(); }
    static QSizeF Measure(const QString& text, const QFont& font);
    // 容器文字：非空时表示这段文字归属该 uid 的图形，位置与换行由容器决定。
    QString ContainerUid() const { return m_containerUid; }
    void SetContainerUid(const QString& uid) { m_containerUid = uid.trimmed(); }
    // 容器文字的排版尺寸：宽度取容器内宽，高度按换行后的行数算，供容器同步使用。
    QSizeF ContainerLayoutSize(qreal width) const;
    // 按容器宽度折行后的正文行，容器文字绘制与字形轮廓共用同一份结果。
    QStringList WrappedLines(qreal width) const;

    // TextItemState 是文字专用的 Memento：正文 / 字体 / 颜色 / 自然尺寸一次快照恢复，
    // 不拆成逐属性命令（§3.6）。基类 CanvasItem 不再持有文字 facet。
    TextItemState State() const;
    void ApplyState(const TextItemState& state);

  protected:
    void PaintContent(QPainter* painter) override;
    qreal FramePadding() const override { return 20.0; }
    void TextChanged() override;

  private:
    QString m_containerUid;
};
