#pragma once

#include "ShapeItem.h"

#include <QColor>
#include <QList>
#include <QWidget>

class QToolButton;

// 编辑图元卡的描边 / 填充颜色两行：每行 8 个预设色块加一个自定义颜色按钮，
// 选中图元后按图元当前颜色勾选；自定义颜色走颜色对话框后把颜色交回面板应用。
class ItemDockPanelColorsCard : public QWidget {
    Q_OBJECT

  public:
    explicit ItemDockPanelColorsCard(QWidget* parent = nullptr);

    void refresh(const QList<ShapeItem*>& shapes);

  signals:
    void strokeColorChosen(const QColor& color);
    void fillColorChosen(const QColor& color);

  private:
    QWidget* makeColorRow(bool stroke);

    QToolButton* m_strokeColorButton = nullptr;
    QToolButton* m_fillColorButton = nullptr;
    QList<QToolButton*> m_strokeSwatches;
    QList<QToolButton*> m_fillSwatches;
    QColor m_strokeFallback = QColor(QStringLiteral("#1E293B"));
    QColor m_fillFallback = QColor(QStringLiteral("#1E293B"));
};
