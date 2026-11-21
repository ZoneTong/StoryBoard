#pragma once

#include "ShapeItem.h"

#include <QPainterPath>
#include <QRectF>

// 按 ShapeType 生成轮廓路径的纯几何单元：不持有图元状态，只根据类型与矩形
// （含圆角、扇形 / 弧线的起始角、扫过角与半径参数）计算 QPainterPath；
// 自定义模板路径的单位坐标铺开也在这里。
class ShapePathBuilder {
  public:
    // 图元类已各自持有几何（shapePath 直调对应方法）；pathFor 仅保留给
    // 图标绘制与测试这类“只知道 ShapeType”的场景。
    static QPainterPath PathFor(ShapeType type, const QRectF& rect, qreal cornerRadius);
    // 需求 6：扇形 / 弧线不再有独立半径，椭圆外接框即内容矩形（长短半轴 = 宽高一半）。
    static QPainterPath PathFor(ShapeType type, const QRectF& rect, qreal cornerRadius, qreal startAngle,
                                qreal sweepAngle);
    // 三角形按底边角度生成：底边 = 矩形宽，顶点横坐标 = 高 × cot(底角1)。
    // 角度是真相；调用方负责让矩形高与底边、角度保持自洽。
    static QPainterPath trianglePath(const QRectF& rect, qreal bottomAngle1, qreal bottomAngle2);
    // 需求 9：平行四边形左下内角（度）。上边相对底边的水平偏移 = 高 × cot(角度)，
    // 锐角向右移、钝角向左移，图形始终落在矩形内。
    static QPainterPath parallelogramPath(const QRectF& rect, qreal bottomLeftAngle);

    // 各图形的具体几何：由同名的图元子类在 ShapePath() 中直接调用。
    static QPainterPath roundedRectPath(const QRectF& rect, qreal cornerRadius);
    static QPainterPath ellipsePath(const QRectF& rect);
    static QPainterPath sectorPath(const QRectF& rect, qreal startAngle, qreal sweepAngle);
    static QPainterPath arrowPath(const QRectF& rect);
    static QPainterPath doubleArrowPath(const QRectF& rect);
    static QPainterPath starPath(const QRectF& rect);
    static QPainterPath fourPointStarPath(const QRectF& rect);
    static QPainterPath hexagramPath(const QRectF& rect);
    static QPainterPath heartPath(const QRectF& rect);
    static QPainterPath calloutPath(const QRectF& rect);
    static QPainterPath flagWavePath(const QRectF& rect);
    static QPainterPath scrollPath(const QRectF& rect);
    static QPainterPath cubePath(const QRectF& rect);
    static QPainterPath cylinderPath(const QRectF& rect);

    // 需求 13：花括号曲线化后不再是 ShapeType，插入采样与图标绘制共用这份几何。
    static QPainterPath bracePath(const QRectF& rect);

    // 模板存的是 0–1 单位坐标，按内容矩形铺开，路径本身可任意放大缩小。
    static QPainterPath mapUnitPath(const QPainterPath& unitPath, const QRectF& rect);
};
