#pragma once

#include "ShapeItem.h"

#include <QList>
#include <QWidget>
#include <functional>

class CanvasScene;
class CanvasItem;
class QLabel;
class QMenu;
class QSpinBox;
class QDoubleSpinBox;
class QToolButton;

// 选中图元时出现在右侧的「编辑图元」dock 卡片。紧凑布局：去掉了折叠分组
// 小标题，行内用小图标标注各行用途，常见图形、常用颜色、填充、边框、
// 透明度、圆角、图层与层内高度全部平铺，没有选中图元时由 MainWindow
// 隐藏整张卡片，行为与图层面板一致。
class ItemDockPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ItemDockPanel(CanvasScene* scene, QWidget* parent = nullptr);

    void refresh();

  signals:
    // 没有选中图形时，点常见图形按“插入”处理，交给主窗口沿用当前画笔与填充设置。
    void insertShapeRequested(int shapeType);
    void insertCustomShapeRequested(const QString& filePath);
    void openShapesFolderRequested();
    void statusMessage(const QString& message);

  private:
    void buildShapesSection(QWidget* body);
    void buildColorsSection(QWidget* body);
    void buildFillSection(QWidget* body);
    void buildBorderSection(QWidget* body);
    void buildOpacitySection(QWidget* body);
    void buildCornerSection(QWidget* body);
    void buildArrangeSection(QWidget* body);
    void rebuildCustomShapeMenu();

    void scheduleRefresh();
    void updateSummary();
    void updateSections();
    void updateShapeButtons();
    void updateColorSwatches();
    void updateFillButtons();
    void updateBorderButtons();
    void updateValueEditors();
    void updateArrangeButtons();

    QList<CanvasItem*> selectedItems() const;
    QList<class ShapeItem*> selectedShapes() const;
    void changeSelectedShapes(const QString& text, const std::function<void(class ShapeItem*)>& mutate);
    void applyStrokeColor(const QColor& color);
    void applyFillColor(const QColor& color);
    void applyFillStyle(FillStyle style);
    void applyBorderStyle(Qt::PenStyle style);
    void applyBorderWidth(qreal width);
    void applyOpacity(int percent);
    void applyCornerRadius(int radius);
    void insertOrChangeShape(ShapeType type);
    void adjustLayer(int delta);
    void adjustHeight(int delta);

    CanvasScene* m_scene = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_metaLabel = nullptr;
    QWidget* m_cornerSection = nullptr;

    QList<QToolButton*> m_shapeButtons;
    QToolButton* m_customShapeButton = nullptr;
    QMenu* m_customShapeMenu = nullptr;

    QToolButton* m_strokeColorButton = nullptr;
    QToolButton* m_fillColorButton = nullptr;
    QList<QToolButton*> m_strokeSwatches;
    QList<QToolButton*> m_fillSwatches;

    QList<QToolButton*> m_fillStyleButtons;
    QList<QToolButton*> m_borderStyleButtons;
    QDoubleSpinBox* m_borderWidthSpin = nullptr;
    QSpinBox* m_opacitySpin = nullptr;
    QSpinBox* m_cornerSpin = nullptr;

    QToolButton* m_layerDownButton = nullptr;
    QToolButton* m_layerUpButton = nullptr;
    QToolButton* m_heightDownButton = nullptr;
    QToolButton* m_heightUpButton = nullptr;

    bool m_refreshing = false;
    bool m_refreshPending = false;
};
