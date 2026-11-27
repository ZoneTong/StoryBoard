#pragma once

#include <QByteArray>
#include <QDockWidget>
#include <QStringList>

class CanvasScene;
class MainWindow;
class LayerPanel;
class PropertiesPanel;
class QObject;
class QEvent;
class QTimer;

// 主界面三张卡片：编辑图元默认停靠左侧，视图模式在右上、图层列表在右下。
// 右侧两张卡片的上下顺序可由用户拖拽调整并写进设置文件；编辑面板移动或悬浮后也会保存位置。
class DockPanelController {
  public:
    explicit DockPanelController(MainWindow* window);

    void CreateLayerDock();
    void CreatePropertiesPanelDock();
    void CreateViewModeDock();
    // 会话切换：把图层卡与编辑面板重绑到新的场景。
    void SetScene(CanvasScene* scene);
    // 把左侧工具条里的模式栏摘下，挂进右上角的视图模式卡片。
    void AttachViewModeToolBar();
    // 按设置恢复右侧卡片顺序和编辑面板位置；首次启动编辑面板默认停靠左侧。
    void ApplyDockLayout();
    // 拖拽后读取实际上下位置并落盘。
    void RememberDockOrder();
    // 把编辑面板当前的停靠侧 / 悬浮几何写进设置文件。
    void RememberPropertiesPanelDockState();
    // 编辑面板停靠时始终显示；只有悬浮窗口才跟随选中状态显隐。
    void UpdatePropertiesPanelDockVisibility();

    // 需求 15-3：全屏期把编辑面板与图层卡摘成悬浮窗（左侧 / 右下角贴底），
    // 退出时按进入前的快照还原停靠侧与几何。整个过程不写设置文件。
    void EnterFullScreenLayout();
    void ExitFullScreenLayout();
    // 窗口尺寸变化时重新摆位（全屏期内才生效）。
    void PlaceFullScreenPanels();
    bool IsFullScreenLayoutActive() const { return m_fullScreenLayout; }

    // 拖拽卡片标题栏换位时没有位置信号，用移动事件兜底。
    void HandleMoveEvent(QObject* watched, QEvent* event);

    QDockWidget* LayerDock() const { return m_layerDock; }
    QDockWidget* PropertiesDock() const { return m_propertiesDock; }
    QDockWidget* ViewModeDock() const { return m_viewModeDock; }
    LayerPanel* layerPanel() const { return m_layerPanel; }
    PropertiesPanel* propertiesPanel() const { return m_propertiesPanel; }

    // 右侧两张卡（视图模式 / 图层）的实际上下顺序（对象名）。
    QStringList CurrentDockOrder() const;

  private:
    void SchedulePropertiesPanelDockStateSave();
    void RestorePropertiesPanelDockState();
    // 编辑面板宽度：启动时按内容定一次，之后保持用户拖拽结果，只约束 260–360。
    void ApplyPropertiesPanelDockWidth(int width, bool resizeNow);
    // 按给定顺序把右侧两张卡恢复成上下切分。
    void SplitRightDocks(const QStringList& order);

    MainWindow* m_window = nullptr;
    QDockWidget* m_layerDock = nullptr;
    LayerPanel* m_layerPanel = nullptr;
    QDockWidget* m_propertiesDock = nullptr;
    PropertiesPanel* m_propertiesPanel = nullptr;
    QDockWidget* m_viewModeDock = nullptr;
    // 右侧两张卡的当前上下顺序（对象名），拖拽或布局变化后更新并写回设置。
    QStringList m_dockOrder;
    // 编辑面板移动后延迟落盘，避免拖动过程中频繁写设置文件。
    QTimer* m_propertiesDockStateTimer = nullptr;
    bool m_applyingDockLayout = false;
    // 全屏布局进行中：为真时不落盘，退出时按下面的快照还原。
    bool m_fullScreenLayout = false;
    bool m_propertiesFloatingBefore = false;
    bool m_layerFloatingBefore = false;
    bool m_propertiesVisibleBefore = true;
    bool m_layerVisibleBefore = true;
    Qt::DockWidgetArea m_propertiesAreaBefore = Qt::LeftDockWidgetArea;
    Qt::DockWidgetArea m_layerAreaBefore = Qt::RightDockWidgetArea;
    QByteArray m_propertiesGeometryBefore;
    QByteArray m_layerGeometryBefore;
};
