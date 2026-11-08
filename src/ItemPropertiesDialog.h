#pragma once

#include <QColor>
#include <QDialog>
#include <QFont>
#include <QPointF>
#include <QSizeF>

class BoardItem;
class BoardScene;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QToolButton;

// 编辑单个图元的属性。图元类型只读，其余字段（图层、层内高度、填充、
// 描边、位置尺寸、文字字体颜色、图片名称）都可以修改，确认后作为一条
// 撤销记录提交。
class ItemPropertiesDialog : public QDialog
{
    Q_OBJECT
public:
    enum ResultCode {
        EditTextRequested = 2
    };

    explicit ItemPropertiesDialog(BoardScene* scene, BoardItem* item, QWidget* parent = nullptr);

    static QString layerDescription(const BoardScene* scene, int layerId);
    void accept() override;

private:
    void applyChanges();
    void updateHeightRange(bool selectTop);
    void updateFontButton();
    void chooseFont();

    BoardScene* m_scene = nullptr;
    BoardItem* m_item = nullptr;

    QComboBox* m_layerCombo = nullptr;
    QSpinBox* m_heightSpin = nullptr;
    QComboBox* m_fillStyleCombo = nullptr;
    QToolButton* m_fillColorButton = nullptr;
    QToolButton* m_strokeColorButton = nullptr;
    QDoubleSpinBox* m_strokeWidthSpin = nullptr;
    QDoubleSpinBox* m_positionXSpin = nullptr;
    QDoubleSpinBox* m_positionYSpin = nullptr;
    QDoubleSpinBox* m_widthSpin = nullptr;
    QDoubleSpinBox* m_heightSizeSpin = nullptr;
    QToolButton* m_textColorButton = nullptr;
    QPushButton* m_fontButton = nullptr;
    QLineEdit* m_imageNameEdit = nullptr;

    QFont m_textFont;
    QPointF m_initialPos;
    QSizeF m_initialSize;
    qreal m_initialStrokeWidth = 4.0;
    bool m_hasFill = false;
    bool m_initializing = true;
};
