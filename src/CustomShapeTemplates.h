#pragma once

#include <QIcon>
#include <QList>
#include <QPainterPath>
#include <QString>

// 用户自定义图形：把一张画布文件（.sbcanvas）里的图形 / 笔画轮廓合并成一条路径，
// 归一化到 0–1 的单位坐标后作为可缩放的图元模板。文件放在配置文件所在目录的
// shapes 子目录里，按文件名排序后追加到「常见图形 → 自定义图形」下拉菜单。
struct CustomShapeTemplate {
    QString name;
    QString filePath;
    // 单位坐标轮廓（0–1），宽高比保存在 aspectRatio 里，插入时保持原始比例。
    QPainterPath unitPath;
    qreal aspectRatio = 1.0;
};

// 扫描目录里的全部 .sbcanvas，跳过无法解析或没有可用轮廓的文件。
// 解析失败的条目会追加到 errors（一行一个文件名 + 原因），供界面提示。
QList<CustomShapeTemplate> loadCustomShapeTemplates(const QString& directory, QStringList* errors = nullptr);

// 按给出的目录顺序扫描并合并结果：同名模板只保留先扫到的那份（前面的目录优先）。
QList<CustomShapeTemplate> loadCustomShapeTemplatesFromDirs(const QStringList& directories,
                                                            QStringList* errors = nullptr);

// 读取单个模板文件；失败时通过 error 返回原因。
bool loadCustomShapeTemplate(const QString& filePath, CustomShapeTemplate* result, QString* error = nullptr);

// 用模板轮廓画一个菜单图标（单位路径按原比例铺进图标框）。
QIcon makeCustomShapeIcon(const QPainterPath& unitPath, const QColor& color = QColor(58, 64, 80), int logicalSize = 18);
