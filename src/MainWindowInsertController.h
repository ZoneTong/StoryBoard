#pragma once

#include "ArrowStyle.h"

#include <QPointF>
#include <QString>

class QAction;
class MainWindow;
enum class ShapeType;

// 插入图元：常见图形 / 自定义模板 / 文字图元，以及自定义形状菜单的目录扫描
// 与 shapes 文件夹入口。图元创建后统一进入撤销栈，颜色与尺寸跟随当前画笔状态。
class MainWindowInsertController {
  public:
    explicit MainWindowInsertController(MainWindow* window);

    void insertTextAt(const QPointF& scenePos);
    void insertShape(ShapeType type, qreal triangleAngle1 = -1.0, qreal triangleAngle2 = -1.0,
                     ArrowStyle startArrow = ArrowStyle::None, ArrowStyle endArrow = ArrowStyle::None,
                     bool autoCorner = false);
    void insertCustomShape(const QString& filePath);
    void openShapesFolder();
    void updateCustomShapeMenu();

  private:
    MainWindow* m_window = nullptr;
    // 「自定义图形…」这一项同时挂在常见图形菜单末尾与工具栏下拉里，
    // 每次重建前先删掉上一份，避免重复出现。
    QAction* m_customShapeTrigger = nullptr;
};
