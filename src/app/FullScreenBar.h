#pragma once

#include "ui/widgets/LayoutMetrics.h"

#include <QVector>
#include <QWidget>

class MainWindow;
class QActionGroup;
class QToolButton;
class QSpinBox;

// 全屏态的悬浮工具栏：只列需求里的 9 个图形 + 自定义图形按钮，后面跟填充 / 描线样式、
// 描线宽度与模式徽标。按钮复用主窗口同一批 QAction，图标放大到 26px，键位字符画在图标下方。
class FullScreenBar : public QWidget {
  public:
    explicit FullScreenBar(MainWindow* window);

    // 按主窗口当前状态刷新图标、描线宽度与模式徽标（进入全屏时调一次即可）。
    void SyncFromWindow();
    // 底部居中放置：中心点与顶部留白都由调用方算好。
    void PlaceAt(int centerX, int topInset);
    // 数字键 0–9 触发对应图形按钮（9 直接打开自定义图形面板）；返回是否确有对应按钮。
    bool TriggerDigit(int digit);

    // 栏高 66px，图标边长 26px；按钮高度 = 栏高 − 8，保证键位字符不被下沿切掉。
    // 取值在 LayoutMetrics 统一维护；这里保留 FullScreenBar:: 别名供既有调用点使用。
    static constexpr int BAR_HEIGHT = LayoutMetrics::FULLSCREEN_BAR_HEIGHT;
    static constexpr int BAR_ICON_SIZE = LayoutMetrics::FULLSCREEN_BAR_ICON_SIZE;

  private:
    QToolButton* AddShapeButton(const QString& actionName, const QString& keyText);
    QToolButton* AddStyleButton(const QString& objectName);
    void UpdateModeBadge();

    MainWindow* m_window = nullptr;
    QWidget* m_shapeGroup = nullptr;
    QWidget* m_styleGroup = nullptr;
    // 键位 0–9 对应的按钮，索引即键位字符（9 是自定义图形）。
    QVector<QToolButton*> m_digitButtons;
    QToolButton* m_customShapeButton = nullptr;
    QToolButton* m_fillStyleButton = nullptr;
    QToolButton* m_fillColorButton = nullptr;
    QToolButton* m_strokeStyleButton = nullptr;
    QToolButton* m_strokeColorButton = nullptr;
    QToolButton* m_modeBadge = nullptr;
    QSpinBox* m_widthSpin = nullptr;
};
