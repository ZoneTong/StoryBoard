#pragma once

#include <QStringList>

class MainWindow;
class LayerPanel;
class ItemDockPanel;
class QDockWidget;
class QObject;
class QEvent;

// 主界面三张卡片：编辑图元在左侧，视图模式在右上、图层列表在右下。
// 右侧两张卡片的上下顺序可由用户拖拽调整并写进设置文件；编辑图元卡默认悬浮，可拖回左侧停靠。
class DockPanelController {
  public:
    explicit DockPanelController(MainWindow* window);

    void createLayerDock();
    void createItemDock();
    void createViewModeDock();
    // 把左侧工具条里的模式栏摘下，挂进右上角的视图模式卡片。
    void attachViewModeToolBar();
    // 按设置里的顺序重排右侧两张卡，并限定合适高度，避免占满右侧整列。
    void applyDockLayout();
    // 拖拽后读取实际上下位置并落盘。
    void rememberDockOrder();
    // 编辑图元卡停靠时始终显示；只有悬浮窗口才跟随选中状态显隐。
    void updateItemDockVisibility();

    // 拖拽卡片标题栏换位时没有位置信号，用移动事件兜底。
    void handleMoveEvent(QObject* watched, QEvent* event);

    QDockWidget* layerDock() const { return m_layerDock; }
    QDockWidget* itemDock() const { return m_itemDock; }
    QDockWidget* viewModeDock() const { return m_viewModeDock; }
    LayerPanel* layerPanel() const { return m_layerPanel; }
    ItemDockPanel* itemPanel() const { return m_itemPanel; }

    // 右侧两张卡（视图模式 / 图层）的实际上下顺序（对象名）。
    QStringList currentDockOrder() const;

  private:
    MainWindow* m_window = nullptr;
    QDockWidget* m_layerDock = nullptr;
    LayerPanel* m_layerPanel = nullptr;
    QDockWidget* m_itemDock = nullptr;
    ItemDockPanel* m_itemPanel = nullptr;
    QDockWidget* m_viewModeDock = nullptr;
    // 右侧两张卡的当前上下顺序（对象名），拖拽或布局变化后更新并写回设置。
    QStringList m_dockOrder;
    bool m_applyingDockLayout = false;
};
