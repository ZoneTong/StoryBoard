#pragma once

#include "FillAttributes.h"
#include "StrokeAttributes.h"

#include <QList>
#include <QWidget>

class QComboBox;
class QSpinBox;
class QToolButton;

// 属性行的共用控件：每一行直接绑定一组属性基类对象（多选时是多个），
// 显示值从基类读、用户操作只发信号，写回与撤销由编辑面板 / 属性窗各自决定。
// 编辑面板与属性窗共用这些行，不再各写一套填充 / 描线控件。
class FillStyleRow : public QWidget {
    Q_OBJECT

  public:
    explicit FillStyleRow(QWidget* parent = nullptr);

    void SetTargets(const QList<FillAttributes*>& targets);
    void Refresh();

  signals:
    void fillStyleChosen(FillStyle style);

  private:
    QList<FillAttributes*> m_targets;
    QList<QToolButton*> m_buttons;
    bool m_refreshing = false;
};

class FillColorRow : public QWidget {
    Q_OBJECT

  public:
    explicit FillColorRow(QWidget* parent = nullptr);

    void SetTargets(const QList<FillAttributes*>& targets);
    void Refresh();

  signals:
    void fillColorChosen(const QColor& color);

  private:
    QList<FillAttributes*> m_targets;
    QList<QToolButton*> m_swatches;
    QToolButton* m_customButton = nullptr;
    QColor m_fallback = QColor(30, 41, 59);
    bool m_refreshing = false;
};

class FillImageRow : public QWidget {
    Q_OBJECT

  public:
    explicit FillImageRow(QWidget* parent = nullptr);

    void SetTargets(const QList<FillAttributes*>& targets);
    void Refresh();

  signals:
    void chooseFillImageClicked();
    void fillImageModeChosen(int mode);
    void clearFillImageClicked();

  private:
    QList<FillAttributes*> m_targets;
    QToolButton* m_chooseButton = nullptr;
    QComboBox* m_modeCombo = nullptr;
    QToolButton* m_clearButton = nullptr;
    bool m_refreshing = false;
};

class StrokeStyleRow : public QWidget {
    Q_OBJECT

  public:
    explicit StrokeStyleRow(QWidget* parent = nullptr);

    void SetTargets(const QList<StrokeAttributes*>& targets);
    void Refresh();

  signals:
    void borderStyleChosen(Qt::PenStyle style);

  private:
    QList<StrokeAttributes*> m_targets;
    QList<QToolButton*> m_buttons;
    bool m_refreshing = false;
};

class StrokeColorRow : public QWidget {
    Q_OBJECT

  public:
    explicit StrokeColorRow(QWidget* parent = nullptr);

    void SetTargets(const QList<StrokeAttributes*>& targets);
    void Refresh();

  signals:
    void strokeColorChosen(const QColor& color);

  private:
    QList<StrokeAttributes*> m_targets;
    QList<QToolButton*> m_swatches;
    QToolButton* m_customButton = nullptr;
    QColor m_fallback = QColor(30, 41, 59);
    bool m_refreshing = false;
};

class StrokeWidthRow : public QWidget {
    Q_OBJECT

  public:
    explicit StrokeWidthRow(QWidget* parent = nullptr);

    void SetTargets(const QList<StrokeAttributes*>& targets);
    void Refresh();

  signals:
    void borderWidthChosen(qreal width);

  private:
    QList<StrokeAttributes*> m_targets;
    QSpinBox* m_widthSpin = nullptr;
    bool m_refreshing = false;
};

QString AttributeRowText(const char* source);

// 行首图标显隐：编辑面板显示图标，属性窗（行标题已说明属性名）统一隐藏。
// StrokeWidthRow 的图标 objectName 被单独覆盖，这里一并兼容。
void SetRowIconVisible(QWidget* row, bool visible);
