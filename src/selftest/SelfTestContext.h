#pragma once

#include "CanvasScene.h"
#include "CanvasView.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QUndoStack>
#include <memory>

class ShapeItem;
class CurveItem;
class TextItem;

namespace selftest {

// 各测试入口之间共享的自检上下文：报告收集、工作目录，以及跨段复用的场景状态。
// 单个功能段的局部场景与图元仍留在各自入口函数内，这里只放被后续段继续使用的状态。
class Context {
  public:
    QString reportPath;
    QStringList lines;
    int failures = 0;
    QString workDirectory;

    // 记录一条断言：条件成立追加 PASS，否则计入失败并追加 FAIL（文案与历史自检保持一致）。
    void check(bool condition, const QString& name, const QString& detail = QString());

    // 图层模型段创建，可见性 / 锁定段继续复用。
    CanvasScene layerScene;
    QUndoStack layerUndo;
    CurveItem* lowerLine = nullptr;
    CurveItem* upperLine = nullptr;
    CurveItem* topLine = nullptr;
    int topLayerId = -1;

    // 视图交互段创建，属性对话框段与锁定图层段复用；锁定图层段末尾销毁视图。
    CanvasScene viewScene;
    QUndoStack viewUndo;
    std::unique_ptr<CanvasView> view;
    CurveItem* drawnStroke = nullptr;
    TextItem* doubleClickText = nullptr;
    int itemPropertyRequests = 0;

    // 图形段产出的整画布 JSON，主窗口图形菜单 / 载入段复用。
    QJsonObject shapeRoot;

    // 属性对话框段创建，后续图片 / 文字命名段继续复用同一编辑场景与首图形。
    CanvasScene propertyScene;
    QUndoStack propertyUndo;
    ShapeItem* propertyFirstShape = nullptr;
};

} // namespace selftest
