#pragma once

#include "ShapeItem.h"

#include <QList>
#include <QWidget>
#include <functional>

class CanvasScene;
class CanvasItem;
class QAction;
class QLabel;

class ItemDockPanelShapesCard;
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
    // 多选时复用主窗口的对齐 / 合并形状动作，保证菜单、工具栏与卡片行为一致。
    void setSelectionActions(const QList<QAction*>& alignActions, const QList<QAction*>& boolActions);

  signals:
    // 没有选中图形时，点常见图形按“插入”处理，交给主窗口沿用当前画笔与填充设置。
    void insertShapeRequested(int shapeType);
    void insertCustomShapeRequested(const QString& filePath);
    void openShapesFolderRequested();
    void statusMessage(const QString& message);

  private:
    void scheduleRefresh();
    void updateSummary();

    QList<CanvasItem*> selectedItems() const;
    QList<ShapeItem*> selectedShapes() const;
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
    void insertOrChangeShape(ShapeType type);
    void adjustLayer(int delta);
    void adjustHeight(int delta);

    void onShapeTriggered(int type);

    CanvasScene* m_scene = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_metaLabel = nullptr;

    ItemDockPanelShapesCard* m_shapesCard = nullptr;
    ItemDockPanelSelectionCard* m_selectionCard = nullptr;
    ItemDockPanelColorsCard* m_colorsCard = nullptr;
    ItemDockPanelFillBorderCard* m_fillBorderCard = nullptr;
    ItemDockPanelValueCard* m_valueCard = nullptr;
    ItemDockPanelArrangeCard* m_arrangeCard = nullptr;

    bool m_refreshing = false;
    bool m_refreshPending = false;
};
