// 线稿化离线实验台：直接调用 LineArtConverter，把结果落盘成 outlines.json + strokes.png，
// 供 metrics.py 算相似度，也方便批量扫参数。不链接主程序，只依赖 Qt6 Core/Gui。
//
// 用法：lineart_dump <image> <outDir> [levels=6 colors=24 merge=64 minarea=24
//                     minthick=1.2 tol=0.6 maxregions=800 side=900 width=1]

#include "LineArtConverter.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <cstdio>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = QCoreApplication::arguments();
    if (args.size() < 3) {
        std::fprintf(stderr, "usage: lineart_dump <image> <outDir> [key=value ...]\n");
        return 2;
    }

    LineArtConverter::Options options;
    for (int i = 3; i < args.size(); ++i) {
        const QString pair = args.at(i);
        const int eq = pair.indexOf(QLatin1Char('='));
        if (eq <= 0) {
            continue;
        }
        const QString key = pair.left(eq);
        const double value = pair.mid(eq + 1).toDouble();
        if (key == QStringLiteral("levels")) {
            options.colorLevels = static_cast<int>(value);
        } else if (key == QStringLiteral("colors")) {
            options.maxPaletteColors = static_cast<int>(value);
        } else if (key == QStringLiteral("merge")) {
            options.paletteMergeDistance = value;
        } else if (key == QStringLiteral("minarea")) {
            options.minRegionArea = static_cast<int>(value);
        } else if (key == QStringLiteral("adjmerge")) {
            options.adjacentMergeDistance = value;
        } else if (key == QStringLiteral("minthick")) {
            options.minRegionThickness = value;
        } else if (key == QStringLiteral("tol")) {
            options.simplifyTolerance = value;
        } else if (key == QStringLiteral("maxregions")) {
            options.maxRegions = static_cast<int>(value);
        } else if (key == QStringLiteral("side")) {
            options.maxProcessingSide = static_cast<int>(value);
        } else if (key == QStringLiteral("width")) {
            options.strokeWidth = value;
        }
    }

    QImageReader reader(args.at(1));
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        std::fprintf(stderr, "cannot read image: %s\n", qPrintable(args.at(1)));
        return 3;
    }

    QElapsedTimer timer;
    timer.start();
    const LineArtConverter::Result result = LineArtConverter::extract(image, options);
    const qint64 elapsed = timer.elapsed();

    const QString base = args.at(2);
    QDir().mkpath(base);

    QJsonObject root;
    root.insert(QStringLiteral("ok"), result.ok);
    root.insert(QStringLiteral("error"), result.error);
    root.insert(QStringLiteral("ms"), static_cast<double>(elapsed));
    root.insert(QStringLiteral("regions"), result.regionCount);
    root.insert(QStringLiteral("truncated"), result.truncated);
    root.insert(QStringLiteral("width"), result.imageSize.width());
    root.insert(QStringLiteral("height"), result.imageSize.height());

    QJsonArray list;
    int totalPoints = 0;
    for (const LineArtConverter::Outline& outline : result.outlines) {
        QJsonObject object;
        object.insert(QStringLiteral("area"), outline.area);
        object.insert(QStringLiteral("color"), outline.color.name());
        QJsonArray points;
        for (const QPointF& point : outline.points) {
            QJsonArray pair;
            pair.append(point.x());
            pair.append(point.y());
            points.append(pair);
        }
        totalPoints += outline.points.size();
        object.insert(QStringLiteral("points"), points);
        list.append(object);
    }
    root.insert(QStringLiteral("outlines"), list);
    root.insert(QStringLiteral("totalPoints"), totalPoints);

    QFile json(base + QStringLiteral("/outlines.json"));
    if (!json.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        std::fprintf(stderr, "cannot write %s\n", qPrintable(json.fileName()));
    } else {
        json.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        json.close();
    }

    // strokes.png 的绘制方式与主程序里的折线笔画一致：MiterJoin，端点不做圆头。
    QImage canvas(result.imageSize, QImage::Format_RGB32);
    canvas.fill(Qt::white);
    {
        QPainter painter(&canvas);
        painter.setRenderHint(QPainter::Antialiasing, true);
        for (const LineArtConverter::Outline& outline : result.outlines) {
            QPen pen(outline.color);
            pen.setWidthF(qMax<qreal>(0.5, options.strokeWidth));
            pen.setJoinStyle(Qt::MiterJoin);
            pen.setCapStyle(Qt::FlatCap);
            painter.setPen(pen);
            painter.drawPolyline(QPolygonF(outline.points));
        }
    }
    canvas.save(base + QStringLiteral("/strokes.png"));

    std::printf("ok=%d outlines=%d regions=%d points=%d ms=%lld truncated=%d error=%s\n", result.ok ? 1 : 0,
                int(result.outlines.size()), result.regionCount, totalPoints, static_cast<long long>(elapsed),
                result.truncated ? 1 : 0, qPrintable(result.error));
    return 0;
}
