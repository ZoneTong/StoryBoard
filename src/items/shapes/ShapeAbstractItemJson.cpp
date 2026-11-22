#include "items/shapes/ShapeAbstractItem.h"

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

QJsonObject ShapeAbstractItem::ToJson() const
{
    QJsonObject json = BaseJson();
    json.insert(QStringLiteral("shapeType"), ShapeTypeToString(shapeType()));
    // 填充与描线两组属性由各自的属性基类序列化，图形只拼接几何与其它字段。
    WriteStrokeJson(json);
    WriteFillJson(json);
    json.insert(QStringLiteral("cornerRadius"), m_cornerRadius);
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

void ShapeAbstractItem::FromJson(const QJsonObject& json)
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

    ReadStrokeJson(json);
    // 旧文件缺 fillColor 时按描线色派生 36/255 透明度的纯色填充，保持原外观。
    ReadFillJson(json, StrokeColor());
    m_cornerRadius = qMax<qreal>(0.0, json.value(QStringLiteral("cornerRadius")).toDouble(0.0));
    // 需求 6：旧文件可能还带 shapeRadius；先记下原值，等尺寸恢复后再折算（见下方迁移）。
    const qreal legacyRadius = json.value(QStringLiteral("shapeRadius")).toDouble(-1.0);

    ApplyBaseJson(json);
    if (ContentSize().isEmpty()) {
        SetContentSize(DefaultContentSize());
    }

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

    // 旧文件没有角度字段时的兼容反算由 TriangleItem::ReadJsonExtra 处理。
    ReadJsonExtra(json);
    if (forceRoundedCorner && !IsCustomShape()) {
        m_cornerRadius = qMin(ContentSize().width(), ContentSize().height()) * 0.16;
    }
    update();
}
