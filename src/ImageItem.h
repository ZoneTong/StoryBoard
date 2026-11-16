#pragma once

#include "CanvasItem.h"

#include <QPixmap>

class ImageItem : public CanvasItem {
    Q_OBJECT
  public:
    explicit ImageItem(QGraphicsItem* parent = nullptr);

    QString kind() const override { return QStringLiteral("image"); }
    QString displayName() const override;
    QString typeDisplayName() const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;

    void setImage(const QPixmap& pixmap, const QString& name = QString());
    QPixmap image() const { return m_pixmap; }
    QString imageName() const { return m_name; }
    void setImageName(const QString& name);

  protected:
    void paintContent(QPainter* painter) override;

  private:
    QPixmap m_pixmap;
    QString m_name;
    // 文件里的原始图片字节：解码失败（损坏/格式不支持）时仍保留，重新保存
    // 时原样写回，避免「打开→保存」把还没丢的原始数据彻底抹掉。
    QByteArray m_sourceBytes;
};
