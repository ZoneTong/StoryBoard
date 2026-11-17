#pragma once

#include "ShapeItem.h"

#include <QList>
#include <QWidget>
#include <functional>

class CanvasScene;
class CanvasItem;
class CompositeShapeItem;
class CurveItem;
class TextItem;
class ImageItem;
struct TextItemState;
class QPixmap;
class QFont;
class QAction;
class QLabel;
class QWidget;
class QLayout;

class ItemDockPanelSelectionCard;
class ItemDockPanelColorsCard;
class ItemDockPanelFillBorderCard;
class ItemDockPanelValueCard;
class ItemDockPanelArrangeCard;
class ItemDockPanelContentCard;

// 选中图元时出现在右侧的「编辑图元」dock 卡片。外壳只负责整体布局、选中联动与
// 撤销命令提交；图形、颜色、填充 / 描边、透明度 / 圆角、图层 / 高度各行由对应
// 子部件卡片构建与刷新，没有选中图元时由 MainWindow 隐藏整张卡片。
class ItemDockPanel : public QWidget {
    Q_OBJECT

  public:
    explicit ItemDockPanel(CanvasScene* scene, QWidget* parent = nullptr);

    void refresh();
    // 按卡片内最宽的一行（包括当前隐藏的行）计算启动宽度；之后不再随选中变化。
    int preferredWidth() const;
    // 多选时复用主窗口的对齐 / 合并图元 / 复制 / 删除 / 旋转 / 组合动作，保证菜单、工具栏与卡片行为一致。
    void setSelectionActions(const QList<QAction*>& alignActions, const QList<QAction*>& boolActions,
                             QAction* copyAction, QAction* removeAction, QAction* rotateCwAction,
                             QAction* rotateCcwAction, QAction* groupAction, QAction* ungroupAction);
    // 笔画平滑与图元属性窗也是选中项可用的操作，卡片沿用主窗口同一批 QAction。
    void setItemActions(QAction* smoothAction, QAction* propertiesAction);
    // 替换当前唯一选中的图片，保留原位置与图元名称；图片本体、尺寸和名称一起进入撤销记录。
    void replaceSelectedImageWith(const QPixmap& pixmap, const QString& name = QString());

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
    QList<CompositeShapeItem*> selectedComposites() const;
    QList<CurveItem*> selectedStrokes() const;
    QList<TextItem*> selectedTexts() const;
    QList<ImageItem*> selectedImages() const;
    // 描线颜色 / 宽度可作用的对象：图形、笔画与多边形。
    QList<CanvasItem*> selectedStrokeTargets() const;
    void changeSelectedShapes(const QString& text, const std::function<void(ShapeItem*)>& mutate);
    void changeSelectedFills(const QString& text, const std::function<void(ShapeItem*)>& mutateShape,
                             const std::function<void(CompositeShapeItem*)>& mutateComposite);
    void changeSelectedTexts(const QString& text, const std::function<void(TextItemState&)>& mutate);
    void applyTextContent(const QString& text);
    void applyTextFont(const QFont& font);
    void applyTextColor(const QColor& color);
    void applyImageName(const QString& name);
    void chooseReplacementImage();
    void applyStrokeColor(const QColor& color);
    void applyFillColor(const QColor& color);
    void applyFillStyle(FillStyle style);
    void applyBorderStyle(Qt::PenStyle style);
    void applyBorderWidth(qreal width);
    void applyOpacity(int percent);
    void applyCornerRadius(int radius);
    void applySweepAngle(int angle);
    void applyShapeRadius(int radius);
    // 三角形：底边长度与两个底角；角度优先，高度随底边派生。
    void applyTriangleBase(int base);
    void applyTriangleAngle1(int degrees);
    void applyTriangleAngle2(int degrees);
    // 直线两端箭头样式（ArrowStyle 整数值）。
    void applyArrowStart(int style);
    void applyArrowEnd(int style);
    void adjustLayer(int delta);
    void adjustHeight(int delta);

    CanvasScene* m_scene = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_metaLabel = nullptr;
    QWidget* m_contentWidget = nullptr;

    ItemDockPanelSelectionCard* m_selectionCard = nullptr;
    ItemDockPanelContentCard* m_contentCard = nullptr;
    ItemDockPanelColorsCard* m_colorsCard = nullptr;
    ItemDockPanelFillBorderCard* m_fillBorderCard = nullptr;
    ItemDockPanelValueCard* m_valueCard = nullptr;
    ItemDockPanelArrangeCard* m_arrangeCard = nullptr;

    bool m_refreshing = false;
    bool m_refreshPending = false;
};
