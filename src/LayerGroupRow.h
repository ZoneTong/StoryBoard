#pragma once

#include <QWidget>

class QLabel;

// 图层面板里的组行：组图标 + 组名 + 成员数；双击组名触发重命名（§7.9）。
class LayerGroupRow : public QWidget {
    Q_OBJECT
  public:
    LayerGroupRow(int groupId, const QString& name, int memberCount, int depth, QWidget* parent = nullptr);

    void ApplySelectedStyle(bool selected);

  signals:
    void renameRequested(int groupId);

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    int m_groupId = 0;
    QLabel* m_nameLabel = nullptr;
};
