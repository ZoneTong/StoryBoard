#pragma once

#include <QHash>
#include <QList>
#include <QString>

class CanvasItem;
class CanvasScene;

// 图层面板树中的一行：组行或图元行。depth 0 表示直接挂在图层名下。
// 只描述“该显示什么、排在第几层”，不持有任何控件。
struct LayerTreeRow {
    enum Kind { GROUP, ITEM };

    Kind kind = Kind::ITEM;
    CanvasItem* item = nullptr; // ITEM 行
    int groupId = 0;            // GROUP 行
    int depth = 0;
    int memberCount = 0; // GROUP 行：子树叶子图元数
};

// 只读的行规划：把场景的组与图元按图层排成面板行序。
// grouped=false（默认）：不出组行，整层图元按层内高度降序平铺。
// grouped=true：每个父节点先排子组（按组名），再排未分组图元；组内同样递归。
// 跨层 / 空组不出组行，其后代图元上提到最近可渲染的父级，保证整层图元不漏。
class LayerTreeModel {
  public:
    // 返回“图层 id → 该图层下的行（不含图层行本身）”。
    static QHash<int, QList<LayerTreeRow>> BuildAll(CanvasScene* scene, bool grouped);

    // 图元行的缩进前缀：3 + depth 个 em 空格，宽度随字号，近似对齐组行的 18 + 16*depth。
    static QString IndentText(int depth);
};
