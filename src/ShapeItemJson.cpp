#include "ShapeItem.h"

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
#include <QJsonObject>
#include <QtMath>

namespace {
// 自定义图形轮廓随画布内嵌：QPainterPath 本身没有 JSON 表示，用 QDataStream
// 序列化再 base64，比手写点表更可靠，也兼容曲线与多子路径。
QString EncodePathData(const QPainterPath& path)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << path;
    return QString::fromLatin1(data.toBase64());
}

QPainterPath DecodePathData(const QString& text)
{
    QByteArray data = QByteArray::fromBase64(text.toLatin1());
    if (data.isEmpty()) {
        return QPainterPath();
    }
    QDataStream stream(&data, QIODevice::ReadOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    QPainterPath path;
    stream >> path;
    return path;
}
} // namespace

QJsonObject ShapeItem::ToJson() const
{
    QJsonObject json = BaseJson();
    json.insert(QStringLiteral("shapeType"), ShapeTypeToString(shapeType()));
    json.insert(QStringLiteral("strokeColor"), m_strokeColor.name(QColor::HexArgb));
    json.insert(QStringLiteral("fillColor"), m_fillColor.name(QColor::HexArgb));
    json.insert(QStringLiteral("fillStyle"), FillStyleToString(m_fillStyle));
    json.insert(QStringLiteral("strokeWidth"), m_strokeWidth);
    json.insert(QStringLiteral("strokeStyle"), static_cast<int>(m_strokeStyle));
    json.insert(QStringLiteral("opacity"), m_opacity);
    json.insert(QStringLiteral("cornerRadius"), m_cornerRadius);
    json.insert(QStringLiteral("startAngle"), m_startAngle);
    json.insert(QStringLiteral("sweepAngle"), m_sweepAngle);
    // 三角形以角度为真相，两个底角随画布保存；旧文件没有这两个字段时按 60°/60° 反算。
    if (shapeType() == ShapeType::TRIANGLE) {
        json.insert(QStringLiteral("bottomAngle1"), m_bottomAngle1);
        json.insert(QStringLiteral("bottomAngle2"), m_bottomAngle2);
    }
    // 需求 9：平行四边形的左下内角随画布保存；旧文件缺字段时按 60° 载入。
    if (shapeType() == ShapeType::PARALLELOGRAM) {
        json.insert(QStringLiteral("parallelogramAngle"), m_parallelogramAngle);
    }
    // 需求 15：图片填充独立属性，无图时不写键，旧版本忽略该键即退回纯色。
    // 需求 11：有内容哈希 id 时只写引用（与图片模式），图片本体由画布级 images 表承载；
    // 没有 id（模板 / 剪贴板这类自包含文件）仍写内联 data。
    if (!m_fillImage.isNull() && !m_fillImageId.isEmpty()) {
        json.insert(QStringLiteral("fillImageId"), m_fillImageId);
        json.insert(QStringLiteral("fillImageMode"), ImageFill::ModeToString(m_fillImageMode));
    } else {
        const QJsonObject fillImageJson = ImageFill::ToJson(m_fillImage, m_fillImageMode);
        if (!fillImageJson.isEmpty()) {
            json.insert(QStringLiteral("fillImage"), fillImageJson);
        }
    }
    // 需求 30：自定义轮廓不再一律写成 "custom"（旧版读回会退化成矩形），真实
    // 类型照常写进 shapeType；名字与轮廓单独存，只有名字的图元（立方体顶面）
    // 也能在重开后保留名称。
    if (!m_customName.isEmpty()) {
        json.insert(QStringLiteral("customName"), m_customName);
    }
    if (!m_customPath.isEmpty()) {
        json.insert(QStringLiteral("pathData"), EncodePathData(m_customPath));
    }
    WriteJsonExtra(json);
    return json;
}

void ShapeItem::FromJson(const QJsonObject& json)
{
    const QString typeName = json.value(QStringLiteral("shapeType")).toString();
    bool forceRoundedCorner = false;
    if (typeName == QStringLiteral("custom")) {
        m_customName = json.value(QStringLiteral("customName")).toString();
        m_customPath = DecodePathData(json.value(QStringLiteral("pathData")).toString());
    } else {
        // 需求 5：旧的 rounded_rectangle 读成「矩形 + 自动圆角」，不向后写兼容。
        const QString normalizedType = typeName.trimmed().toLower();
        const bool legacyRounded = normalizedType == QStringLiteral("rounded_rectangle") ||
                                   normalizedType == QStringLiteral("roundedrect") ||
                                   normalizedType == QStringLiteral("roundrect");
        m_customName.clear();
        m_customPath = QPainterPath();
        if (legacyRounded) {
            forceRoundedCorner = true;
        }
        // 需求 30：新文件把自定义轮廓挂在真实类型上（立方体的菱形面），
        // 旧文件仍是 "custom" + 矩形底层。
        const QString customName = json.value(QStringLiteral("customName")).toString();
        if (!customName.isEmpty()) {
            m_customName = customName;
        }
        const QString pathData = json.value(QStringLiteral("pathData")).toString();
        if (!pathData.isEmpty()) {
            m_customPath = DecodePathData(pathData);
        }
    }

    m_strokeColor = QColor(json.value(QStringLiteral("strokeColor")).toString(QStringLiteral("#ff1e293b")));
    if (!m_strokeColor.isValid()) {
        m_strokeColor = QColor(30, 41, 59);
    }

    if (json.contains(QStringLiteral("fillColor"))) {
        m_fillColor = QColor(json.value(QStringLiteral("fillColor")).toString());
        if (!m_fillColor.isValid()) {
            m_fillColor = QColor(Qt::transparent);
        }
    } else {
        m_fillColor = m_strokeColor;
        m_fillColor.setAlpha(36);
    }

    if (json.contains(QStringLiteral("fillStyle"))) {
        m_fillStyle = FillStyleFromString(json.value(QStringLiteral("fillStyle")).toString());
    } else {
        m_fillStyle = m_fillColor.alpha() > 0 ? FillStyle::SOLID : FillStyle::NONE;
    }

    m_strokeWidth = qBound<qreal>(0.5, json.value(QStringLiteral("strokeWidth")).toDouble(4.0), 48.0);
    m_strokeStyle = static_cast<Qt::PenStyle>(qBound(0, json.value(QStringLiteral("strokeStyle")).toInt(1), 5));
    m_opacity = qBound<qreal>(0.0, json.value(QStringLiteral("opacity")).toDouble(1.0), 1.0);
    m_cornerRadius = qMax<qreal>(0.0, json.value(QStringLiteral("cornerRadius")).toDouble(0.0));
    m_startAngle = json.value(QStringLiteral("startAngle")).toDouble(30.0);
    m_sweepAngle = json.value(QStringLiteral("sweepAngle")).toDouble(120.0);
    // 需求 9：平行四边形内角；旧文件没有这个键时按默认 60°。
    m_parallelogramAngle = qBound<qreal>(15.0, json.value(QStringLiteral("parallelogramAngle")).toDouble(60.0), 165.0);
    // 需求 6：旧文件可能还带 shapeRadius；先记下原值，等尺寸恢复后再折算（见下方迁移）。
    const qreal legacyRadius = json.value(QStringLiteral("shapeRadius")).toDouble(-1.0);
    if (json.contains(QStringLiteral("fillImage"))) {
        m_fillImage = ImageFill::PixmapFromJson(json.value(QStringLiteral("fillImage")).toObject(), &m_fillImageMode);
        m_fillImageId.clear();
    } else if (json.contains(QStringLiteral("fillImageId"))) {
        // 需求 11：图片本体在画布级 images 表里，先记下 id，由加载层解析成 pixmap。
        m_fillImageId = json.value(QStringLiteral("fillImageId")).toString();
        m_fillImageMode = ImageFill::ModeFromString(json.value(QStringLiteral("fillImageMode")).toString());
        m_fillImage = QPixmap();
    } else {
        m_fillImage = QPixmap();
        m_fillImageId.clear();
        m_fillImageMode = ImageFill::Mode::STRETCH;
    }
    const bool hasAngles = json.contains(QStringLiteral("bottomAngle1"));

    // 载入期间先屏蔽三角形尺寸联动，避免 baseJson 恢复尺寸时反推高度、改动旧文件外观。
    m_syncingTriangle = true;
    ApplyBaseJson(json);
    if (ContentSize().isEmpty()) {
        SetContentSize(DefaultContentSize());
    }
    m_syncingTriangle = false;

    // 需求 6：扇形 / 弧线旧半径 < 内容宽的一半时，把椭圆外接框改成新内容矩形：
    // k = 2r、ry = r·h/w，位置按椭圆中心不动补偿，轮廓逐点不变；r<0 或 r≥w/2 无需迁移。
    if (shapeType() == ShapeType::SECTOR && legacyRadius >= 0.0 && !IsCustomShape()) {
        const QSizeF size = ContentSize();
        if (size.width() > 0.0 && legacyRadius < size.width() / 2.0) {
            const qreal ry = legacyRadius * size.height() / size.width();
            SetContentSize(QSizeF(legacyRadius * 2.0, ry * 2.0));
            setPos(pos() + QPointF(size.width() / 2.0 - legacyRadius, size.height() / 2.0 - ry));
        }
    }

    // 旧文件没有角度字段：按原三角形的顶点位置（内容矩形上边中点）反算两个底角，
    // 使角度与恢复后的尺寸自洽，外观零变化。
    if (shapeType() == ShapeType::TRIANGLE && !IsCustomShape()) {
        if (hasAngles) {
            m_bottomAngle1 = qBound<qreal>(1.0, json.value(QStringLiteral("bottomAngle1")).toDouble(60.0), 174.0);
            m_bottomAngle2 =
                qBound<qreal>(1.0, json.value(QStringLiteral("bottomAngle2")).toDouble(60.0), 175.0 - m_bottomAngle1);
        } else {
            const QSizeF size = ContentSize();
            const qreal halfWidth = qMax<qreal>(1.0, size.width() / 2.0);
            const qreal legacyAngle = qRadiansToDegrees(qAtan2(qMax<qreal>(1.0, size.height()), halfWidth));
            m_bottomAngle1 = qBound<qreal>(1.0, legacyAngle, 174.0);
            m_bottomAngle2 = qBound<qreal>(1.0, legacyAngle, 175.0 - m_bottomAngle1);
        }
    }

    ReadJsonExtra(json);
    if (forceRoundedCorner && !IsCustomShape()) {
        m_cornerRadius = qMin(ContentSize().width(), ContentSize().height()) * 0.16;
    }
    m_triangleSyncSize = ContentSize();
    update();
}
