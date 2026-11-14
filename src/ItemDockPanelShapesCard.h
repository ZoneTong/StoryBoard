#pragma once

#include "ShapeItem.h"

#include <QList>
#include <QWidget>

class QMenu;
class QToolButton;

// 编辑图元卡的「常见图形 + 自定义图形」一行：普通图形按网格平铺，
// 自定义图形收进下拉菜单；选中图形后高亮当前类型，无选中时由面板按插入处理。
class ItemDockPanelShapesCard : public QWidget {
    Q_OBJECT

  public:
    explicit ItemDockPanelShapesCard(QWidget* parent = nullptr);

    // 按当前选中图元刷新按钮勾选态与自定义图形提示。
    void refresh(const QList<ShapeItem*>& shapes);

  signals:
    // 点击普通图形按钮；是否插入新图元由面板结合选中状态判断。
    void shapeTriggered(int shapeType);
    void insertCustomShapeRequested(const QString& filePath);
    void openShapesFolderRequested();

  private:
    void rebuildCustomShapeMenu();

    QList<QToolButton*> m_shapeButtons;
    QToolButton* m_customShapeButton = nullptr;
    QMenu* m_customShapeMenu = nullptr;
};
