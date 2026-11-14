#pragma once

#include "CanvasTypes.h"

#include <QWidget>

class QCheckBox;
class QLabel;
class QToolButton;

// 图层面板中的单行：可见性勾选框、名称标签、锁定按钮。行内控件的文案、对象名与
// 属性沿用原面板；行自身识别标签双击并发出重命名请求，不直接操作场景。
class LayerRow : public QWidget {
    Q_OBJECT
  public:
    LayerRow(const CanvasLayer& layer, int index, int itemCount, QWidget* parent = nullptr);

    // 选中态走 QSS 动态属性，颜色只在样式表里维护。
    void applySelectedStyle(bool selected);

  signals:
    void visibilityToggled(int layerId, bool visible);
    void lockToggled(int layerId, bool locked);
    void renameRequested(int layerId);

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    int m_layerId = 0;
    QCheckBox* m_visibleBox = nullptr;
    QLabel* m_nameLabel = nullptr;
    QToolButton* m_lockButton = nullptr;
};
