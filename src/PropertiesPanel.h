#pragma once

#include "ItemProperties.h"
#include "ShapeItem.h"

#include <QList>
#include <QMap>
#include <QWidget>
#include <functional>

class CanvasScene;
class CanvasItem;
class ArcItem;
class SineItem;
class LineItemAbstract;
class FillAttributes;
class StrokeAttributes;
class FillImageRow;
struct TextItemState;
class QPixmap;
class QFont;
class QAction;
class QComboBox;
class QToolButton;
class QLabel;
class QWidget;
class QLayout;

class PropertiesPanelSelectionCard;
class PropertiesPanelColorsCard;
class PropertiesPanelFillBorderCard;
class PropertiesPanelValueCard;
class PropertiesPanelArrangeCard;
class PropertiesPanelContentCard;
class PropertiesPanelIdentityCard;
class PropertiesPanelGeometryCard;

// 选中图元时出现在右侧的「编辑图元」dock 卡片。外壳只负责整体布局、选中联动与
// 撤销命令提交；图形、颜色、填充 / 描边、透明度 / 圆角、图层 / 高度各行由对应
// 子部件卡片构建与刷新，没有选中图元时由 MainWindow 隐藏整张卡片。
class PropertiesPanel : public QWidget {
    Q_OBJECT

  public:
    explicit PropertiesPanel(CanvasScene* scene, QWidget* parent = nullptr);

    void Refresh();
    // 按卡片内最宽的一行（包括当前隐藏的行）计算启动宽度；之后不再随选中变化。
    int PreferredWidth() const;
    // 多选时复用主窗口的对齐 / 合并图元 / 复制 / 删除 / 旋转 / 组合动作，保证菜单、工具栏与卡片行为一致。
    void SetSelectionActions(const QList<QAction*>& alignActions, const QList<QAction*>& boolActions,
                             QAction* copyAction, QAction* removeAction, QAction* rotateCwAction,
                             QAction* rotateCcwAction, QAction* groupAction, QAction* ungroupAction);
    // 笔画平滑与图元属性窗也是选中项可用的操作，卡片沿用主窗口同一批 QAction。
    void SetItemActions(QAction* smoothAction, QAction* propertiesAction);
    // 替换当前唯一选中的图片，保留原位置与图元名称；图片本体、尺寸和名称一起进入撤销记录。

  signals:
    void statusMessage(const QString& message);
    // 卡片里的描边颜色 / 宽度同时是画笔的当前颜色与宽度，改动后交给主窗口同步。
    void strokeColorPicked(const QColor& color);
    void borderWidthPicked(qreal width);

  private:
    void ScheduleRefresh();
    void UpdateSummary();
    // 选中的是否还值得再套一层组合：正好选中的是一整棵组树时返回 false。
    bool SelectionGroupUseful(const QList<CanvasItem*>& items) const;

    QList<CanvasItem*> SelectedItems() const;
    QList<ShapeItem*> SelectedShapes() const;
    QList<LineItemAbstract*> SelectedStrokes() const;
    QList<CanvasItem*> SelectedTexts() const;

    void ChangeSelectedShapes(const QString& text, const std::function<void(ShapeItem*)>& mutate);
    // 弧线角度与正弦参数：几何由参数派生，改动按参数快照入撤销栈。
    void ChangeSelectedArcs(const QString& text, const std::function<void(LineItemAbstract*)>& mutate);
    void ChangeSelectedSines(const QString& text, const std::function<void(LineItemAbstract*)>& mutate);
    void ChangeSelectedTexts(const QString& text, const std::function<void(TextItemState&)>& mutate);
    void ApplyTextContent(const QString& text);
    void ApplyTextFont(const QFont& font);
    void ApplyTextColor(const QColor& color);
    // 需求 13：给选中的所有可填充图元批量替换背景图片（多选也生效）。
    void ChooseFillImage();
    void ClearFillImage();
    // 图片填充方式（拉伸 / 适应 / 平铺）与图元属性窗一致，多选批量写回。
    void ApplyFillImageMode(int mode);
    // 属性表通道：同一组属性值写回所有目标图元，只占一条撤销记录。
    void ApplyPropertyValues(const QMap<ItemProperty, QVariant>& values, const QString& text,
                             const QList<CanvasItem*>& targets = QList<CanvasItem*>());
    // 按属性基类批量修改：选中集合里能解析出填充 / 描线基类的图元统一套用一次，
    // 具体叶子类的特殊语义（多边形按图层取色）由基类虚函数分派。
    void ApplyFillMutation(const QString& text, const std::function<void(FillAttributes*)>& mutate);
    void ApplyStrokeMutation(const QString& text, const std::function<void(StrokeAttributes*)>& mutate);
    void ApplyStrokeColor(const QColor& color);
    void ApplyFillColor(const QColor& color);
    void ApplyFillStyle(FillStyle style);
    void ApplyBorderStyle(Qt::PenStyle style);
    void ApplyBorderWidth(qreal width);
    void ApplyOpacity(int percent);
    void ApplyCornerRadius(int radius);
    void ApplyStartAngle(int angle);
    void ApplySweepAngle(int angle);
    // 正弦波形：周期数（1–12）与振幅（相对中心线的像素高度，1–200）。
    void ApplySineCycles(int cycles);
    void ApplySineAmplitude(int percent);
    // 三角形：两个底角；角度优先，高度随底边派生。
    void ApplyTriangleAngle1(int degrees);
    void ApplyTriangleAngle2(int degrees);
    // 直线两端箭头样式（ArrowStyle 整数值）。
    void ApplyArrowStart(int style);
    void ApplyArrowEnd(int style);
    // 属性窗里可直接编辑的身份 / 几何字段，在编辑面板里同样可改：
    // 名称、目标图层、层内高度、位置与尺寸、旋转、平行四边形内角。
    void ApplyItemName(const QString& name);
    void ApplyItemLayer(int layerId);
    void ApplyItemHeight(int height);
    void ApplyItemGeometry(const QPointF& position, const QSizeF& size);
    void ApplyItemRotation(qreal angle);
    // 长短半轴：椭圆 / 扇形与弧线共用 MAJOR_AXIS / MINOR_AXIS 属性写入路径。
    void ApplyItemAxes(qreal majorAxis, qreal minorAxis);
    void ApplyParallelogramAngle(qreal angle);
    // 图层 / 高度一次改到目标值：与属性窗一样，同层同高度时互换两条记录。
    void ApplyItemStack(int layerId, int height, const QString& text);
    void AdjustLayer(int delta);
    void AdjustHeight(int delta);

    CanvasScene* m_scene = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_metaLabel = nullptr;
    QWidget* m_contentWidget = nullptr;
    // 需求 13：背景图片行（填充颜色之后），与属性窗共用同一个 FillImageRow。
    FillImageRow* m_fillImageRow = nullptr;

    PropertiesPanelSelectionCard* m_selectionCard = nullptr;
    PropertiesPanelContentCard* m_contentCard = nullptr;
    PropertiesPanelColorsCard* m_colorsCard = nullptr;
    PropertiesPanelFillBorderCard* m_fillBorderCard = nullptr;
    PropertiesPanelValueCard* m_valueCard = nullptr;
    PropertiesPanelArrangeCard* m_arrangeCard = nullptr;
    PropertiesPanelIdentityCard* m_identityCard = nullptr;
    PropertiesPanelGeometryCard* m_geometryCard = nullptr;

    bool m_refreshing = false;
    bool m_refreshPending = false;
};
