#include "LineArtPreprocessor.h"

#include <QPainter>

namespace LineArtInternal {

QImage ComposeOnWhite(const QImage& source)
{
    QImage base(source.size(), QImage::Format_RGB32);
    base.fill(Qt::white);
    {
        QPainter painter(&base);
        painter.drawImage(0, 0, source);
    }
    return base;
}

QImage FitToSide(const QImage& image, int side)
{
    if (qMax(image.width(), image.height()) <= side) {
        return image;
    }
    return image.scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation)
        .convertToFormat(QImage::Format_RGB32);
}

} // namespace LineArtInternal
