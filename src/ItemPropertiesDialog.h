#pragma once

#include "CanvasItem.h"

#include <QColor>
#include <QDialog>
#include <QPixmap>
#include <QPointF>
#include <QPointer>
#include <QSizeF>
#include <QString>

class CanvasScene;
class ItemPropertiesDialogGeometryBlock;
class ItemPropertiesDialogIdentityBlock;
class ItemPropertiesDialogImageBlock;
class ItemPropertiesDialogStyleBlock;
class ItemPropertiesDialogTextBlock;
class QSpinBox;
class QShowEvent;

// 编辑单个图元的属性。图元类型只读，其余字段（名称、图层、层内高度、填充、
// 描边、位置尺寸、文字字体颜色、图片名称）由对应区块类构建与收集，确认后
// 作为一条撤销记录提交。文字正文在属性窗内直接编辑，不再弹出额外的文字对话框。
class ItemPropertiesDialog : public QDialog {
    Q_OBJECT

  public:
    explicit ItemPropertiesDialog(CanvasScene* scene, CanvasItem* item, QWidget* parent = nullptr,
                                  bool editable = true);

    CanvasItem* item() const { return m_item.data(); }
    bool isEditable() const { return m_editable; }
    bool isItemInScene() const;
    // 需求 39：从文件载入替换图片；返回 false 时通过 errorMessage 给出原因。
    bool loadImageFromFile(const QString& path, QString* errorMessage = nullptr);
    void setReplacementImage(const QPixmap& image);
    static QString layerDescription(const CanvasScene* scene, int layerId);
    void accept() override;

  protected:
    // 需求 3：属性窗首次显示时，可编辑文字图元的输入焦点直接落到正文尾部。
    void showEvent(QShowEvent* event) override;

  private:
    void applyChanges();
    void syncTextNaturalSize();
    void chooseImage();
    void applyEditableState();

    CanvasScene* m_scene = nullptr;
    QPointer<CanvasItem> m_item;
    bool m_editable = true;

    ItemPropertiesDialogIdentityBlock* m_identityBlock = nullptr;
    ItemPropertiesDialogStyleBlock* m_styleBlock = nullptr;
    ItemPropertiesDialogTextBlock* m_textBlock = nullptr;
    ItemPropertiesDialogImageBlock* m_imageBlock = nullptr;
    ItemPropertiesDialogGeometryBlock* m_geometryBlock = nullptr;
    // 图形沿用样式区里的透明度行，其余图元由属性窗统一提供。
    QSpinBox* m_opacitySpin = nullptr;
    qreal m_initialOpacity = 1.0;
    int m_initialOpacityPercent = 100;

    QPixmap m_newImage;
    bool m_textFocusApplied = false;
    bool m_imageChanged = false;

    QPointF m_initialPos;
    QSizeF m_initialSize;
    qreal m_initialRotation = 0.0;
    QString m_initialItemName;
    QString m_initialDisplayName;
    qreal m_initialStrokeWidth = 4.0;
    bool m_hasFill = false;
    bool m_initializing = true;
};
