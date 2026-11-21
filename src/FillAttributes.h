#pragma once

#include "ImageFill.h"

#include <QColor>
#include <QJsonObject>
#include <QPixmap>
#include <QString>

// 填充属性基类：把「怎么填」这一组属性从具体图元里独立出来。
// 有面积的图元多继承本类与描线属性基类；纯描线图元只继承描线基类。
// 属性编辑面板按本类的虚函数批量改选中图元，不必再按叶子类强转。
enum class FillStyle {
    NONE = 0,
    SOLID,
    HACHURE,
    REVERSE_HACHURE,
    CROSS_HATCH,
    GRID_HATCH,
    HONEYCOMB,
    HORIZONTAL_HATCH,
    VERTICAL_HATCH,
    BRICK,
    DOTS
};

QString FillStyleToString(FillStyle style);
FillStyle FillStyleFromString(const QString& value);

// 一次填充属性变化的完整快照（撤销 / 属性窗共用）。
struct FillAttributesState {
    QColor color = QColor(30, 41, 59, 36);
    FillStyle style = FillStyle::SOLID;
    bool hasImage = false;
    QPixmap image;
    QString imageId;
    ImageFill::Mode imageMode = ImageFill::Mode::STRETCH;

    bool operator==(const FillAttributesState& other) const
    {
        return color == other.color && style == other.style && hasImage == other.hasImage &&
               image.cacheKey() == other.image.cacheKey() && imageId == other.imageId && imageMode == other.imageMode;
    }
    bool operator!=(const FillAttributesState& other) const { return !(*this == other); }
};

class FillAttributes {
  public:
    FillAttributes() = default;
    virtual ~FillAttributes() = default;

    virtual QColor FillColor() const { return m_fillColor; }
    // 沿用既有命名：属性面板、布尔运算与序列化都按这个接口读取填充样式。
    virtual FillStyle fillStyle() const { return m_fillStyle; }
    virtual bool HasFillImage() const { return !m_fillImage.isNull(); }
    virtual QPixmap FillImage() const { return m_fillImage; }
    virtual QString FillImageId() const { return m_fillImageId; }
    virtual ImageFill::Mode FillImageMode() const { return m_fillImageMode; }

    virtual void SetFillColor(const QColor& color);
    virtual void SetFill(const QColor& color, FillStyle style);
    virtual void SetFillStyle(FillStyle style);
    virtual void SetFillImageId(const QString& id) { m_fillImageId = id; }
    virtual void SetFillImage(const QPixmap& image, ImageFill::Mode mode);
    virtual void ClearFillImage();

    FillAttributesState FillState() const;
    void ApplyFillState(const FillAttributesState& state);

    // 只写本组属性，图元的整体 JSON 负责拼接其他字段。
    void WriteFillJson(QJsonObject& json) const;
    // fallbackColor 供旧文件缺少 fillColor 时按描线色派生半透明填充色。
    void ReadFillJson(const QJsonObject& json, const QColor& fallbackColor);

  protected:
    // 具体图元在本组属性变化后刷新自己；默认什么都不做。
    virtual void FillChanged() {}

    QColor m_fillColor = QColor(30, 41, 59, 36);
    FillStyle m_fillStyle = FillStyle::SOLID;
    QPixmap m_fillImage;
    QString m_fillImageId;
    ImageFill::Mode m_fillImageMode = ImageFill::Mode::STRETCH;
};
