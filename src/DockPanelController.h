#pragma once

#include <QStringList>

class MainWindow;
class LayerPanel;
class PropertiesPanel;
class QDockWidget;
class QObject;
class QEvent;
class QTimer;

// 主界面三张卡片：编辑图元默认停靠左侧，视图模式在右上、图层列表在右下。
// 右侧两张卡片的上下顺序可由用户拖拽调整并写进设置文件；属性编辑面板移动或悬浮后也会保存位置。
class DockPanelController {
  public:
    explicit DockPanelController(MainWindow* window);

    void createLayerDock();
    void createPropertiesPanelDock();
    void createViewModeDock();
    // 把左侧工具条里的模式栏摘下，挂进右上角的视图模式卡片。
    void attachViewModeToolBar();
    // 按设置恢复右侧卡片顺序和属性编辑面板位置；首次启动属性编辑面板默认停靠左侧。
    void applyDockLayout();
    // 拖拽后读取实际上下位置并落盘。
    void rememberDockOrder();
    // 把属性编辑面板当前的停靠侧 / 悬浮几何写进设置文件。
    void rememberPropertiesPanelDockState();
    // 属性编辑面板停靠时始终显示；只有悬浮窗口才跟随选中状态显隐。
    void updatePropertiesPanelDockVisibility();

    // 拖拽卡片标题栏换位时没有位置信号，用移动事件兜底。
    void handleMoveEvent(QObject* watched, QEvent* event);

    QDockWidget* layerDock() const { return m_layerDock; }
    QDockWidget* propertiesDock() const { return m_propertiesDock; }
    QDockWidget* viewModeDock() const { return m_viewModeDock; }
    LayerPanel* layerPanel() const { return m_layerPanel; }
    PropertiesPanel* propertiesPanel() const { return m_propertiesPanel; }

    // 右侧两张卡（视图模式 / 图层）的实际上下顺序（对象名）。
    QStringList currentDockOrder() const;

  private:
    void schedulePropertiesPanelDockStateSave();
    void restorePropertiesPanelDockState();
    // 属性编辑面板宽度：启动时按内容定一次，之后保持用户拖拽结果，只约束 260–360。
    void applyPropertiesPanelDockWidth(int width, bool resizeNow);

    MainWindow* m_window = nullptr;
    QDockWidget* m_layerDock = nullptr;
    LayerPanel* m_layerPanel = nullptr;
    QDockWidget* m_propertiesDock = nullptr;
    PropertiesPanel* m_propertiesPanel = nullptr;
    QDockWidget* m_viewModeDock = nullptr;
    // 右侧两张卡的当前上下顺序（对象名），拖拽或布局变化后更新并写回设置。
    QStringList m_dockOrder;
    // 属性编辑面板移动后延迟落盘，避免拖动过程中频繁写设置文件。
    QTimer* m_propertiesDockStateTimer = nullptr;
    bool m_applyingDockLayout = false;
};
