#pragma once

#include <QIcon>
#include <QJsonArray>
#include <QList>
#include <QPainterPath>
#include <QRectF>
#include <QString>

class CanvasItem;

// 用户自定义图形：把一张画布文件（.sbcanvas）里的图形 / 笔画轮廓拆成若干子路径，
// 归一化到 0–1 的单位坐标后作为可缩放的图元模板。文件放在配置文件所在目录的
// shapes 子目录里，按文件名排序后追加到「常见图形 → 自定义图形」下拉菜单。
// 模板按 0–1 单位坐标存储；插入时再按目标矩形映射。合并路径保留给面板图标 /
// 宽高比计算，parts 按子路径拆分；但插入优先按模板里的原始图元 JSON 重建同类图元，
// 只有旧模板缺少可用条目时才退回 parts 的轮廓近似（闭合走多边形、开放走曲线）。
struct CustomShapePart {
    QPainterPath unitPath;
    bool closed = false;
};

struct CustomShapeTemplate {
    QString name;
    QString filePath;
    // 单位坐标轮廓（0–1），宽高比保存在 aspectRatio 里，插入时保持原始比例。
    QPainterPath unitPath;
    qreal aspectRatio = 1.0;
    QList<CustomShapePart> parts;
    // 原始图元 JSON（保类型、保样式）：插入时按类型重建，不再一律压成多边形 / 曲线。
    // 旧模板或纯轮廓模板可能没有可用条目，这时为空，插入侧回退到 parts 轮廓。
    QJsonArray items;
    // items 在模板坐标里的包围盒；插入时按它映射到目标矩形。没有 items 时为无效矩形。
    QRectF itemsRect;
};

// 把模板里的原始图元按类型重建并缩放到 targetRect（返回的新图元由调用方接管）。
// 只处理能建出类的条目；全部条目都不可用时返回空列表，调用方可回退到 parts 轮廓。
QList<CanvasItem*> CreateTemplateItems(const CustomShapeTemplate& shape, const QRectF& targetRect);

// 扫描目录里的全部 .sbcanvas，跳过无法解析或没有可用轮廓的文件。
// 解析失败的条目会追加到 errors（一行一个文件名 + 原因），供界面提示。
QList<CustomShapeTemplate> LoadCustomShapeTemplates(const QString& directory, QStringList* errors = nullptr);

// 按给出的目录顺序扫描并合并结果：同名模板只保留先扫到的那份（前面的目录优先）。
QList<CustomShapeTemplate> LoadCustomShapeTemplatesFromDirs(const QStringList& directories,
                                                            QStringList* errors = nullptr);

// 读取单个模板文件；失败时通过 error 返回原因。
bool LoadCustomShapeTemplate(const QString& filePath, CustomShapeTemplate* result, QString* error = nullptr);

// 用模板轮廓画一个菜单图标（单位路径按原比例铺进图标框）。
QIcon MakeCustomShapeIcon(const QPainterPath& unitPath, const QColor& color = QColor(58, 64, 80), int logicalSize = 18);
