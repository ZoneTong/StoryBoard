#pragma once

#include <QColor>
#include <QList>
#include <QWidget>

class FillAttributes;
class StrokeAttributes;
class FillColorRow;
class StrokeColorRow;

// 编辑面板的描线 / 填充颜色两行：复用属性行控件，颜色直接取自对应属性基类。
class PropertiesPanelColorsCard : public QWidget {
    Q_OBJECT

  public:
    explicit PropertiesPanelColorsCard(QWidget* parent = nullptr);

    void Refresh(const QList<FillAttributes*>& fills, const QList<StrokeAttributes*>& strokes);
    QWidget* FillColorRow() const;
    QWidget* BorderColorRow() const;

  signals:
    void strokeColorChosen(const QColor& color);
    void fillColorChosen(const QColor& color);

  private:
    class FillColorRow* m_fillColorRow = nullptr;
    class StrokeColorRow* m_borderColorRow = nullptr;
};
