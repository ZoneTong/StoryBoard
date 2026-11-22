#pragma once

#include "items/CanvasItem.h"
#include "ui/panels/properties/PropertyRowRegistry.h"

#include <QColor>
#include <QDialog>
#include <QPixmap>
#include <QPoint>
#include <QPointF>
#include <QPointer>
#include <QSizeF>
#include <QString>

class CanvasScene;
class QDialogButtonBox;
class QFormLayout;
class QLabel;
class ItemPropertiesDialogGeometryBlock;
class ItemPropertiesDialogIdentityBlock;
class ItemPropertiesDialogStyleBlock;
class ItemPropertiesDialogTextBlock;
class QEvent;
class QHideEvent;
class QMouseEvent;
class QObject;
class QShowEvent;

// 编辑单个图元的属性。图元类型只读，其余字段（名称、图层、层内高度、填充、
// 描边、位置尺寸、文字字体颜色、图片名称）由对应区块类构建与收集，确认后
// 作为一条撤销记录提交。文字正文在属性窗内直接编辑，不再弹出额外的文字对话框。
// 所有图元类型复用同一实例：全部行在构造时一次建齐、顺序固定，SetItem 按图元
// 属性有无回填数值并显隐对应行，不再按类型重建控件。
class ItemPropertiesDialog : public QDialog {
    Q_OBJECT

  public:
    explicit ItemPropertiesDialog(CanvasScene* scene, CanvasItem* item, QWidget* parent = nullptr,
                                  bool editable = true);
    ~ItemPropertiesDialog() override;

    CanvasItem* Item() const { return m_item.data(); }
    bool IsEditable() const { return m_editable; }
    bool IsItemInScene() const;
    // 属性窗总行数（含固定的图元类型行）：§4.1 定稿为 28 行。
    int PropertyRowCount() const { return m_rows.RowCount() + 1; }
    // 只有画布本身的点击才触发“取消并关闭”，工具栏、停靠面板等区域不关窗。
    bool IsGlobalPointInsideCanvas(const QPoint& globalPoint) const;
    static QString LayerDescription(const CanvasScene* scene, int layerId);
    // 换绑目标图元：回填全部行控件并按属性显隐；未改动的隐藏行在提交时原样跳过。
    void SetItem(CanvasScene* scene, CanvasItem* item, bool editable);
    void accept() override;

  protected:
    // 需求 3：属性窗首次显示时，可编辑文字图元的输入焦点直接落到正文尾部。
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    // 模态窗会吞掉画布上的鼠标事件，这里在应用级过滤到“窗外的按下”后按取消处理。
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    void ApplyChanges();
    void SyncTextNaturalSize();
    void ApplyEditableState();

    CanvasScene* m_scene = nullptr;
    QPointer<CanvasItem> m_item;
    bool m_editable = true;

    QLabel* m_heading = nullptr;
    QLabel* m_typeValue = nullptr;
    QLabel* m_readOnlyHint = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
    // 全部属性行的注册表：显隐只看图元上报的 Properties()（§4.3）。
    PropertyRowRegistry m_rows;
    QFormLayout* m_form = nullptr;
    ItemPropertiesDialogIdentityBlock* m_identityBlock = nullptr;
    ItemPropertiesDialogStyleBlock* m_styleBlock = nullptr;
    ItemPropertiesDialogTextBlock* m_textBlock = nullptr;
    ItemPropertiesDialogGeometryBlock* m_geometryBlock = nullptr;
    qreal m_initialStrokeWidth = 4.0;
    bool m_hasFill = false;
    bool m_textFocusApplied = false;

    QPointF m_initialPos;
    QSizeF m_initialSize;
    qreal m_initialRotation = 0.0;
    QString m_initialItemName;
    QString m_initialDisplayName;
    bool m_initializing = true;
    bool m_outsideClickClosing = false;
};
