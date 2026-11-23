#pragma once

#include "items/properties/FillAttributes.h"

#include <QList>
#include <QWidget>

class QToolButton;
class FillAttributes;
class StrokeAttributes;
class FillStyleRow;
class StrokeStyleRow;
class StrokeWidthRow;

// 编辑面板的填充样式、描线样式与描线宽度三行：内部直接复用属性行控件，
// 行控件绑定 FillAttributes / StrokeAttributes，改动交回面板走撤销命令。
class PropertiesPanelFillBorderCard : public QWidget {
    Q_OBJECT

  public:
    explicit PropertiesPanelFillBorderCard(QWidget* parent = nullptr);

    void Refresh(const QList<FillAttributes*>& fills, const QList<StrokeAttributes*>& strokes);
    QWidget* FillStyleWidget() const;
    QWidget* BorderStyleWidget() const;
    QWidget* BorderWidthRow() const;

  signals:
    void fillStyleChosen(FillStyle style);
    void borderStyleChosen(Qt::PenStyle style);
    void borderWidthChosen(qreal width);

  private:
    class FillStyleRow* m_fillStyleRow = nullptr;
    class StrokeStyleRow* m_borderStyleRow = nullptr;
    class StrokeWidthRow* m_borderWidthRow = nullptr;
};
