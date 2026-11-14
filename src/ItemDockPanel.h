#pragma once

#include "ShapeItem.h"

#include <QList>
#include <QWidget>
#include <functional>

class CanvasScene;
class CanvasItem;
class StrokeItem;
class QAction;
class QLabel;

class ItemDockPanelSelectionCard;
class ItemDockPanelColorsCard;
class ItemDockPanelFillBorderCard;
class ItemDockPanelValueCard;
class ItemDockPanelArrangeCard;

// 选中图元时出现在右侧的「编辑图元」dock 卡片。外壳只负责整体布局、选中联动与
// 撤销命令提交；图形、颜色、填充 / 描边、透明度 / 圆角、图层 / 高度各行由对应
// 子部件卡片构建与刷新，没有选中图元时由 MainWindow 隐藏整张卡片。
class ItemDockPanel : public QWidget {
    Q_OBJECT

  public:
    explicit ItemDockPanel(CanvasScene* scene, QWidget* parent = nullptr);

    void refresh();
    // 多选时复用主窗口的对齐 / 化形 / 复制 / 删除 / 旋转 / 组合动作，保证菜单、工具栏与卡片行为一致。
    void setSelectionActions(const QList<QAction*>& alignActions, const QList<QAction*>& boolActions,
                             QAction* copyAction, QAction* removeAction, QAction* rotateCwAction,
                             QAction* rotateCcwAction, QAction* groupAction, QAction* ungroupAction);

  signals:
    void statusMessage(const QString& message);
    // 卡片里的描边颜色 / 宽度同时是画笔的当前颜色与宽度，改动后交给主窗口同步。
    void strokeColorPicked(const QColor& color);
    void borderWidthPicked(qreal width);

  private:
    void scheduleRefresh();
    void updateSummary();
    // 选中的是否还值得再套一层组合：正好选中的是一整棵组树时返回 false。
    bool selectionGroupUseful(const QList<CanvasItem*>& items) const;

    QList<CanvasItem*> selectedItems() const;
    QList<ShapeItem*> selectedShapes() const;
    QList<StrokeItem*> selectedStrokes() const;
    // 描线颜色 / 宽度可作用的对象：图形与笔画。
    QList<CanvasItem*> selectedStrokeTargets() const;
    void changeSelectedShapes(const QString& text, const std::function<void(ShapeItem*)>& mutate);
    void applyStrokeColor(const QColor& color);
    void applyFillColor(const QColor& color);
    void applyFillStyle(FillStyle style);
    void applyBorderStyle(Qt::PenStyle style);
    void applyBorderWidth(qreal width);
    void applyOpacity(int percent);
    void applyCornerRadius(int radius);
    void applySweepAngle(int angle);
    void applyShapeRadius(int radius);
    void adjustLayer(int delta);
    void adjustHeight(int delta);

    CanvasScene* m_scene = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_metaLabel = nullptr;

    ItemDockPanelSelectionCard* m_selectionCard = nullptr;
    ItemDockPanelColorsCard* m_colorsCard = nullptr;
    ItemDockPanelFillBorderCard* m_fillBorderCard = nullptr;
    ItemDockPanelValueCard* m_valueCard = nullptr;
    ItemDockPanelArrangeCard* m_arrangeCard = nullptr;

    bool m_refreshing = false;
    bool m_refreshPending = false;
};
