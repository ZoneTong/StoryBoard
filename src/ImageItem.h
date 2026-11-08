#pragma once

#include "BoardItem.h"

#include <QPixmap>

class ImageItem : public BoardItem
{
    Q_OBJECT
public:
    explicit ImageItem(QGraphicsItem* parent = nullptr);

    QString kind() const override { return QStringLiteral("image"); }
    QString displayName() const override;
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
};
