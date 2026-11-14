#pragma once

#include <QStringList>

class MainWindow;
class LayerPanel;
class ItemDockPanel;
class QDockWidget;
class QObject;
class QEvent;

// 右侧两张卡片（图层列表 / 编辑图元）的创建、上下顺序与显隐：
// 默认图层卡在上，用户拖拽调整后把顺序写进设置文件，下次启动按保存的顺序恢复。
class DockPanelController {
  public:
    explicit DockPanelController(MainWindow* window);

    void createLayerDock();
    void createItemDock();
    // 按设置里的顺序重排两张卡，并限定合适高度，避免占满右侧整列。
    void applyDockLayout();
    // 拖拽后读取实际上下位置并落盘。
    void rememberDockOrder();
    // 编辑图元卡跟随选中状态显示 / 隐藏。
    void updateItemDockVisibility();

    // 拖拽卡片标题栏换位时没有位置信号，用移动事件兜底。
    void handleMoveEvent(QObject* watched, QEvent* event);

    QDockWidget* layerDock() const { return m_layerDock; }
    QDockWidget* itemDock() const { return m_itemDock; }
    LayerPanel* layerPanel() const { return m_layerPanel; }
    ItemDockPanel* itemPanel() const { return m_itemPanel; }

    // 当前两张卡的实际上下顺序（对象名）。
    QStringList currentDockOrder() const;

  private:
    MainWindow* m_window = nullptr;
    QDockWidget* m_layerDock = nullptr;
    LayerPanel* m_layerPanel = nullptr;
    QDockWidget* m_itemDock = nullptr;
    ItemDockPanel* m_itemPanel = nullptr;
    // 两张卡的当前上下顺序（对象名），拖拽或布局变化后更新并写回设置。
    QStringList m_dockOrder;
    bool m_applyingDockLayout = false;
};
