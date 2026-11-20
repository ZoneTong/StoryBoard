#pragma once

#include "IconFactory.h"

#include <QWidget>

class QToolButton;

// 属性编辑面板的图层与层内高度 ±1 一行：四个按钮分别调整上移 / 下移图层、
// 升高 / 降低层内高度，是否可用由面板按是否有选中图元刷新。
class PropertiesPanelArrangeCard : public QWidget {
    Q_OBJECT

  public:
    explicit PropertiesPanelArrangeCard(QWidget* parent = nullptr);

    void refresh(bool enabled);

  signals:
    void layerAdjusted(int delta);
    void heightAdjusted(int delta);

  private:
    QToolButton* makeButton(IconId id, const QString& toolTip, const QString& objectName, int delta, bool height);

    QToolButton* m_layerUpButton = nullptr;
    QToolButton* m_layerDownButton = nullptr;
    QToolButton* m_heightUpButton = nullptr;
    QToolButton* m_heightDownButton = nullptr;
};
