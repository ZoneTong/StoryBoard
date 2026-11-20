#pragma once

#include "I18n.h"
#include "LineItemAbstract.h"

#include <QColor>
#include <QRectF>

// 花括号图元：几何是采样路径 + 折线模式。右括号 = 左括号旋转 180°，
// 不复制第二套几何；采样来自 ShapePathBuilder::bracePath()，插入与图标共用同一份。
class BraceItem : public LineItemAbstract {
    Q_OBJECT
  public:
    explicit BraceItem(QGraphicsItem* parent = nullptr);

    QString kind() const override { return QStringLiteral("brace"); }
    QString displayName() const override { return uiText(QStringLiteral("Brace")); }

    // 插入入口：括号外接框（场景坐标）+ 描线颜色与宽度；采样下沉到本类，
    // 插入端不再自己拼点。
    void setBraceRect(const QRectF& rectScene, const QColor& color, qreal width);

    ItemPropertySet properties() const override;
    QVariant propertyValue(ItemProperty property) const override;
    void setPropertyValue(ItemProperty property, const QVariant& value) override;
};
