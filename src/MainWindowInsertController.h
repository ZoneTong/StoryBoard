#pragma once

#include <QPointF>
#include <QString>

class MainWindow;
enum class ShapeType;

// 插入图元：常见图形 / 自定义模板 / 文字图元，以及自定义形状菜单的目录扫描
// 与 shapes 文件夹入口。图元创建后统一进入撤销栈，颜色与尺寸跟随当前画笔状态。
class MainWindowInsertController {
  public:
    explicit MainWindowInsertController(MainWindow* window);

    void insertTextAt(const QPointF& scenePos);
    void insertShape(ShapeType type, qreal triangleAngle1 = -1.0, qreal triangleAngle2 = -1.0);
    void insertCustomShape(const QString& filePath);
    void openShapesFolder();
    void updateCustomShapeMenu();

  private:
    MainWindow* m_window = nullptr;
};
