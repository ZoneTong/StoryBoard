#pragma once

#include <QColor>
#include <QSizeF>
#include <QString>

class AppSettings
{
public:
    QColor brushColor = QColor(30, 41, 59);
    int brushWidth = 4;
    QSizeF boardSize = QSizeF(1920.0, 1080.0);
    bool showGrid = true;
    QColor canvasColor = QColor(43, 47, 54);
    QString lastExportDir;
    QString lastImageDir;
    QString lastTemplatePath;
    QString templatesDirectory;
    QByteArray windowGeometry;

    void load();
    void save() const;

    static QString defaultTemplatesDir();
    static QString templatesDir();
    static void ensureTemplatesDir();
};
