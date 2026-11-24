#pragma once

#include "items/properties/ArrowStyle.h"

#include <QPointF>
#include <QSizeF>
#include <QString>

class QAction;
class MainWindow;
enum class ShapeType;

// 插入图元：常见图形 / 自定义模板 / 文字图元，以及自定义形状菜单的目录扫描
// 与 shapes 文件夹入口。图元创建后统一进入撤销栈，颜色与尺寸跟随当前画笔状态。
class MainWindowInsertController {
  public:
    explicit MainWindowInsertController(MainWindow* window);

    void InsertTextAt(const QPointF& scenePos);
    // 需求 13：花括号按曲线插入（右括号 = 左括号旋转 180°）。
    void InsertBrace(bool left);
    void InsertShape(ShapeType type, qreal triangleAngle1 = -1.0, qreal triangleAngle2 = -1.0, qreal cornerRatio = 0.0);
    void InsertCustomShape(const QString& filePath);
    void OpenShapesFolder();
    void UpdateCustomShapeMenu();
    // 新图元落点：置为 true 时（全屏数字键等键盘入口）取鼠标处，否则取视口中心并按图元数量
    // 做 18px 级联；两种落点都夹回画布可用范围。鼠标点在菜单上时不会误落到菜单下方。
    QPointF PlacementCenter(const QSizeF& itemSize) const;
    void SetCursorPlacement(bool enabled) { m_cursorPlacement = enabled; }

  private:
    MainWindow* m_window = nullptr;
    // 立方体 = 3 面菱形图元组成的分组，圆柱体 = 顶椭圆 + 柱身组成的分组（需求 4/5）。
    void InsertCompositeShape(ShapeType type);
    // 「自定义图形…」这一项同时挂在常见图形菜单末尾与工具栏下拉里，
    // 每次重建前先删掉上一份，避免重复出现。
    QAction* m_customShapeTrigger = nullptr;
    // 只有键盘入口（全屏数字键）才把落点挪到鼠标处；按钮 / 菜单点击仍落视口中心。
    bool m_cursorPlacement = false;
};
