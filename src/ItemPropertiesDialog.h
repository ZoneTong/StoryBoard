#pragma once

#include <QColor>
#include <QDialog>
#include <QFont>
#include <QPointF>
#include <QPixmap>
#include <QPointer>
#include <QSizeF>
#include <QString>

class CanvasItem;
class CanvasScene;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QToolButton;

// 编辑单个图元的属性。图元类型只读，其余字段（名称、图层、层内高度、填充、
// 描边、位置尺寸、文字字体颜色、图片名称）都可以修改，确认后作为一条
// 撤销记录提交。文字正文在属性窗内直接编辑，不再弹出额外的文字对话框。
class ItemPropertiesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ItemPropertiesDialog(CanvasScene* scene, CanvasItem* item,
                                  QWidget* parent = nullptr, bool editable = true);

    CanvasItem* item() const { return m_item.data(); }
    bool isEditable() const { return m_editable; }
    bool isItemInScene() const;
    // 需求 39：从文件载入替换图片；返回 false 时通过 errorMessage 给出原因。
    bool loadImageFromFile(const QString& path, QString* errorMessage = nullptr);
    void setReplacementImage(const QPixmap& image);
    static QString layerDescription(const CanvasScene* scene, int layerId);
    void accept() override;

private:
    void applyChanges();
    void updateHeightRange(bool selectTop);
    void updateFontButton();
    void chooseFont();
    void syncTextNaturalSize();
    void chooseImage();
    void applyEditableState();

    CanvasScene* m_scene = nullptr;
    QPointer<CanvasItem> m_item;
    bool m_editable = true;

    QComboBox* m_layerCombo = nullptr;
    QSpinBox* m_heightSpin = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QComboBox* m_fillStyleCombo = nullptr;
    QToolButton* m_fillColorButton = nullptr;
    QToolButton* m_strokeColorButton = nullptr;
    QDoubleSpinBox* m_strokeWidthSpin = nullptr;
    QDoubleSpinBox* m_positionXSpin = nullptr;
    QDoubleSpinBox* m_positionYSpin = nullptr;
    QDoubleSpinBox* m_widthSpin = nullptr;
    QDoubleSpinBox* m_heightSizeSpin = nullptr;
    QDoubleSpinBox* m_rotationSpin = nullptr;
    QToolButton* m_textColorButton = nullptr;
    QPushButton* m_fontButton = nullptr;
    QPlainTextEdit* m_textEdit = nullptr;
    QLineEdit* m_imageNameEdit = nullptr;
    QPushButton* m_imageButton = nullptr;
    QPixmap m_newImage;
    bool m_imageChanged = false;

    QFont m_textFont;
    QString m_initialItemName;
    QString m_initialDisplayName;
    QPointF m_initialPos;
    QSizeF m_initialSize;
    qreal m_initialRotation = 0.0;
    qreal m_initialStrokeWidth = 4.0;
    bool m_hasFill = false;
    bool m_initializing = true;
};
