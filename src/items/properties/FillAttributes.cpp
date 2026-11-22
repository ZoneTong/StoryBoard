#include "items/properties/FillAttributes.h"

#include <QtGlobal>

FillAttributesState FillAttributes::FillState() const
{
    FillAttributesState state;
    state.color = FillColor();
    state.style = fillStyle();
    state.hasImage = HasFillImage();
    state.image = FillImage();
    state.imageId = FillImageId();
    state.imageMode = FillImageMode();
    return state;
}

void FillAttributes::ApplyFillState(const FillAttributesState& state)
{
    SetFill(state.color, state.style);
    if (state.hasImage && !state.image.isNull()) {
        SetFillImage(state.image, state.imageMode);
        SetFillImageId(state.imageId);
    } else {
        ClearFillImage();
    }
    SetFillStyle(state.style);
    FillChanged();
}

void FillAttributes::SetFillColor(const QColor& color)
{
    SetFill(color, fillStyle());
}

void FillAttributes::SetFill(const QColor& color, FillStyle style)
{
    m_fillColor = color.isValid() ? color : QColor(Qt::transparent);
    m_fillStyle = style;
    FillChanged();
}

void FillAttributes::SetFillStyle(FillStyle style)
{
    if (m_fillStyle == style) {
        return;
    }
    m_fillStyle = style;
    FillChanged();
}

void FillAttributes::SetFillImage(const QPixmap& image, ImageFill::Mode mode)
{
    if (image.isNull()) {
        ClearFillImage();
        return;
    }
    m_fillImage = image;
    m_fillImageMode = mode;
    // 图片内容变了，旧的内容哈希 id 失效，由保存时按新内容重新签发。
    m_fillImageId.clear();
    FillChanged();
}

void FillAttributes::ClearFillImage()
{
    if (m_fillImage.isNull() && m_fillImageId.isEmpty()) {
        return;
    }
    m_fillImage = QPixmap();
    m_fillImageId.clear();
    m_fillImageMode = ImageFill::Mode::STRETCH;
    FillChanged();
}

void FillAttributes::WriteFillJson(QJsonObject& json) const
{
    json.insert(QStringLiteral("fillColor"), FillColor().name(QColor::HexArgb));
    json.insert(QStringLiteral("fillStyle"), FillStyleToString(fillStyle()));
    // 有内容哈希 id 时只写引用（与图片模式），图片本体由画布级 images 表承载；
    // 没有 id（模板 / 剪贴板这类自包含文件）仍写内联 data。
    if (!FillImage().isNull() && !FillImageId().isEmpty()) {
        json.insert(QStringLiteral("fillImageId"), FillImageId());
        json.insert(QStringLiteral("fillImageMode"), ImageFill::ModeToString(FillImageMode()));
        return;
    }
    const QJsonObject fillImageJson = ImageFill::ToJson(FillImage(), FillImageMode());
    if (!fillImageJson.isEmpty()) {
        json.insert(QStringLiteral("fillImage"), fillImageJson);
    }
}

void FillAttributes::ReadFillJson(const QJsonObject& json, const QColor& fallbackColor)
{
    if (json.contains(QStringLiteral("fillColor"))) {
        m_fillColor = QColor(json.value(QStringLiteral("fillColor")).toString());
        if (!m_fillColor.isValid()) {
            m_fillColor = QColor(Qt::transparent);
        }
    } else {
        m_fillColor = fallbackColor.isValid() ? fallbackColor : QColor(30, 41, 59);
        m_fillColor.setAlpha(36);
    }

    if (json.contains(QStringLiteral("fillStyle"))) {
        m_fillStyle = FillStyleFromString(json.value(QStringLiteral("fillStyle")).toString());
    } else {
        m_fillStyle = m_fillColor.alpha() > 0 ? FillStyle::SOLID : FillStyle::NONE;
    }

    if (json.contains(QStringLiteral("fillImage"))) {
        m_fillImage = ImageFill::PixmapFromJson(json.value(QStringLiteral("fillImage")).toObject(), &m_fillImageMode);
        m_fillImageId.clear();
        return;
    }
    if (json.contains(QStringLiteral("fillImageId"))) {
        // 图片本体在画布级 images 表里，先记下 id，由加载层解析成 pixmap。
        m_fillImageId = json.value(QStringLiteral("fillImageId")).toString();
        m_fillImageMode = ImageFill::ModeFromString(json.value(QStringLiteral("fillImageMode")).toString());
        m_fillImage = QPixmap();
        return;
    }
    m_fillImage = QPixmap();
    m_fillImageId.clear();
    m_fillImageMode = ImageFill::Mode::STRETCH;
}
