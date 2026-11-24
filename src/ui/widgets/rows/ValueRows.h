#pragma once

#include "items/properties/ItemProperties.h"
#include "ui/icons/IconFactory.h"

#include <QWidget>

class CanvasItem;
class CanvasScene;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;
class QToolButton;

// 把描述符里的范围 / 步长 / 单位灌进已有的整数输入框，供不走通用行的属性卡片复用。
void ApplyItemPropertyRange(QSpinBox* spin, ItemProperty property);

// 编辑面板与属性窗共用的基础属性行：名称、图层、层内高度、单个数值与成对数值。
// 行控件只负责回显与发信号，写回与撤销由宿主决定；图标用于编辑面板的紧凑行，
// 属性窗把图标关掉，仍套用同一套控件与文案。
class TextValueRow : public QWidget {
    Q_OBJECT

  public:
    explicit TextValueRow(const QString& placeholder, QWidget* parent = nullptr);

    void Refresh(const QString& text);
    QString Text() const;
    QLineEdit* Edit() const { return m_edit; }

  signals:
    void textChosen(const QString& text);

  private:
    QLineEdit* m_edit = nullptr;
};

// 图层与层内高度合成一行：左边选目标图层，右边填该层内的叠放高度，
// 两者同属“图元在哪个层面”这一件事，分两行会把面板拉长。
class LayerHeightRow : public QWidget {
    Q_OBJECT

  public:
    explicit LayerHeightRow(QWidget* parent = nullptr, bool showIcon = true);

    // 按画布的图层列表重建下拉，同时回显层内高度与其上限。
    void Refresh(CanvasScene* scene, int layerId, int height, int maximum);
    int LayerId() const;
    int Height() const;
    QComboBox* Combo() const { return m_combo; }
    QSpinBox* Spin() const { return m_spin; }

  signals:
    void layerChosen(int layerId);
    void heightChosen(int height);

  private:
    QComboBox* m_combo = nullptr;
    QSpinBox* m_spin = nullptr;
    bool m_refreshing = false;
};

// 一行一个数值框：透明度 / 圆角 / 角度 / 正弦参数 / 旋转等共用。
class NumberValueRow : public QWidget {
    Q_OBJECT

  public:
    NumberValueRow(IconId icon, bool showIcon, int decimals, qreal minimum, qreal maximum, const QString& suffix,
                   const QString& objectName, QWidget* parent = nullptr);
    // 范围 / 步长 / 单位 / 小数位从属性描述符取，避免面板与属性窗各写一份常量。
    NumberValueRow(IconId icon, bool showIcon, ItemProperty property, const QString& objectName,
                   QWidget* parent = nullptr);

    void Refresh(qreal value);
    qreal Value() const;
    QDoubleSpinBox* Spin() const { return m_spin; }
    void SetSingleStep(qreal step);
    // 行图标（编辑面板显示）的悬停提示：由宿主传入翻译后的属性行名称。
    void SetIconToolTip(const QString& toolTip);

  signals:
    void valueChosen(qreal value);

  private:
    QToolButton* m_icon = nullptr;
    QDoubleSpinBox* m_spin = nullptr;
    bool m_refreshing = false;
};

// 一行两个数值框：位置 X/Y、图元尺寸 W×H 共用，两个框之间不放分隔符。
class PairValueRow : public QWidget {
    Q_OBJECT

  public:
    PairValueRow(IconId icon, bool showIcon, const QString& firstName, const QString& secondName, int decimals,
                 qreal minimum, qreal maximum, const QString& suffix, QWidget* parent = nullptr);
    PairValueRow(IconId icon, bool showIcon, const QString& firstName, const QString& secondName, ItemProperty property,
                 QWidget* parent = nullptr);

    void Refresh(qreal first, qreal second);
    qreal First() const;
    qreal Second() const;
    QDoubleSpinBox* FirstSpin() const { return m_first; }
    QDoubleSpinBox* SecondSpin() const { return m_second; }
    void SetIconToolTip(const QString& toolTip);

  signals:
    void valuesChosen(qreal first, qreal second);

  private:
    QToolButton* m_icon = nullptr;
    QDoubleSpinBox* m_first = nullptr;
    QDoubleSpinBox* m_second = nullptr;
    bool m_refreshing = false;
};
