#pragma once

#include "ShapeItem.h"

#include <QColor>
#include <QList>
#include <QWidget>

class QToolButton;
class LineItemAbstract;

// 属性编辑面板的描边 / 填充颜色两行：每行 8 个预设色块加一个自定义颜色按钮，
// 选中图元后按图元当前颜色勾选；自定义颜色走颜色对话框后把颜色交回面板应用。
// 两行由面板按“填充颜色在填充样式后、边框颜色紧邻边框样式后”的顺序插入布局。
class PropertiesPanelColorsCard : public QWidget {
    Q_OBJECT

  public:
    explicit PropertiesPanelColorsCard(QWidget* parent = nullptr);

    void refresh(const QList<ShapeItem*>& shapes, const QList<LineItemAbstract*>& strokes,
                 const QList<ShapeItem*>& composites = QList<ShapeItem*>());
    QWidget* fillColorRow() const { return m_fillColorRow; }
    QWidget* borderColorRow() const { return m_borderColorRow; }

  signals:
    void strokeColorChosen(const QColor& color);
    void fillColorChosen(const QColor& color);

  private:
    QWidget* makeColorRow(bool stroke);

    QWidget* m_fillColorRow = nullptr;
    QWidget* m_borderColorRow = nullptr;
    QToolButton* m_strokeColorButton = nullptr;
    QToolButton* m_fillColorButton = nullptr;
    QList<QToolButton*> m_strokeSwatches;
    QList<QToolButton*> m_fillSwatches;
    QColor m_strokeFallback = QColor(QStringLiteral("#1E293B"));
    QColor m_fillFallback = QColor(QStringLiteral("#1E293B"));
};
