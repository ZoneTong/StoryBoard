#pragma once

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

    static constexpr int BAR_HEIGHT = 58;
    static constexpr int BAR_ICON_SIZE = 26;

  private:
    QToolButton* AddShapeButton(const QString& actionName, const QString& keyText);
    QToolButton* AddStyleButton(const QString& objectName);
    void UpdateModeBadge();

    MainWindow* m_window = nullptr;
    QWidget* m_shapeGroup = nullptr;
    QWidget* m_styleGroup = nullptr;
    QToolButton* m_customShapeButton = nullptr;
    QToolButton* m_fillStyleButton = nullptr;
    QToolButton* m_fillColorButton = nullptr;
    QToolButton* m_strokeStyleButton = nullptr;
    QToolButton* m_strokeColorButton = nullptr;
    QToolButton* m_modeBadge = nullptr;
    QSpinBox* m_widthSpin = nullptr;
};
