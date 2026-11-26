#pragma once

#include "items/CanvasTypes.h"
#include "items/ItemSnapshot.h"
#include "items/ItemStack.h"
#include "scene/GroupDomain.h"
#include "scene/GroupState.h"
#include "scene/LayerDomain.h"

#include <QGraphicsScene>
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QPointer>
#include <QRectF>
#include <QSet>
#include <QSizeF>

class QUndoStack;
class CanvasItem;
class TextItem;
class QFont;
class QPainter;
class QStyleOptionGraphicsItem;
struct TextItemState;

class CanvasScene : public QGraphicsScene {
    Q_OBJECT
  public:
    explicit CanvasScene(QObject* parent = nullptr);
    ~CanvasScene() override;

    void SetUndoStack(QUndoStack* stack);
    QUndoStack* UndoStack() const { return m_undoStack; }

    ToolMode mode() const { return m_mode; }
    void SetMode(ToolMode mode);

    QSizeF CanvasSize() const { return m_canvasSize; }
    QPointF CanvasOrigin() const { return m_canvasOrigin; }
    QRectF CanvasRect() const;
    void ApplyCanvasSize(const QSizeF& size);
    void SetCanvasSizeWithUndo(const QSizeF& size);
    // Canvas geometry includes the origin so the canvas can be stretched from
    // any edge; apply* is live (drag) and commit* pushes one undo step.
    void ApplyCanvasRect(const QRectF& rect);
    void SetCanvasRectWithUndo(const QRectF& rect);
    void CommitCanvasRect(const QRectF& before);

    CanvasBackground canvasBackground() const { return m_background; }
    void SetCanvasBackground(const CanvasBackground& background);
    void ReplaceCanvasBackground(const CanvasBackground& background, const QString& text = QString());

    bool ShowGrid() const { return m_showGrid; }
    void SetShowGrid(bool enabled);

    QRectF ContentRect() const;
    // 单个图层上可见图元并集的最小包围框；图层不存在或没有图元时返回空矩形。
    QRectF LayerContentRect(int layerId) const;
    void UpdateSceneExtent();
    // 视图缩放后刷新图元的 boundingRect（旋转手柄等屏幕尺寸相关几何）。
    void RefreshViewDependentGeometry();

    QList<CanvasItem*> CanvasItems() const;
    int CanvasItemCount() const;
    // 绑定跟随：箭头端点在目标图形移动 / 缩放后重新贴回目标边缘，容器文字回到容器中心。
    // 只在交互提交后调用，不参与逐帧重绘，避免递归改几何。
    void SyncArrowBindings();
    void SyncContainerTexts();
    // 单段容器文字：按容器当前位置与宽度重排；没有容器时什么都不做。
    void SyncContainerText(TextItem* text);
    // 文字中心落在哪个图形上（自上而下取第一个可编辑图形），用于拖拽落入时自动绑定。
    CanvasItem* ContainerShapeForText(const TextItem* text) const;
    // 绑定 / 解绑容器文字，整体一条撤销记录。解绑保留文字当前位置与尺寸。
    bool SetTextContainer(TextItem* text, const QString& containerUid, const QString& label);
    // 拖拽提交用：显式给出拖拽前的 uid 与文字快照，撤销回到拖拽前的位置与归属。
    bool SetTextContainer(TextItem* text, const QString& containerUid, const QString& beforeUid,
                          const TextItemState& beforeState, const QString& label);
    CanvasItem* ItemByUid(const QString& uid) const;
    // 画箭头 / 拖端点时的吸附候选，只影响视图高亮，不写进文档。
    void SetBindingHighlight(CanvasItem* item);
    CanvasItem* BindingHighlight() const { return m_bindingHighlight; }
    // 绑定落点：从 from 朝目标图形中心，取图形边缘上的点，再沿外法线让开 gap。
    // focus >= 0 时按 Excalidraw 的归一化轮廓位置取点，保证缩放 / 移动后端点停在同一边。
    static QPointF AttachmentPoint(const CanvasItem* target, const QPointF& from, qreal gap,
                                   qreal focus = -1.0);
    // 轮廓参数：0 在左上角，顺时针绕一圈回到 1；用于 focus 与场景点互算。
    static qreal FocusForPoint(const QRectF& rect, const QPointF& point);
    static QPointF PointForFocus(const QRectF& rect, qreal focus);
    // 可绑定的图形：点落在图形内，或离内容框边缘不超过 maxDistance；按叠放自上而下取第一个。
    CanvasItem* BindableShapeAt(const QPointF& scenePos, qreal maxDistance) const;
    // 需求 32：给所有还没有自定义名称的图元补上“类型 1 / 类型 2 …”自动名称。
    // 同时修复载入文件里已有的重名条目，保证画布内名称唯一。
    void AssignAutoItemNames();
    QList<CanvasItem*> SelectedCanvasItems() const;
    // 图元析构时把自身从选中排序缓存清出（QPointer 惰性清理在接连析构下不安全）。
    void ForgetItem(CanvasItem* item);
    // 选择顺序：先选中的在前，框选时按场景叠放顺序（最底层在前）记录。
    // 合并图元的「剪除」以最先选中的图元为底，框选时以最底层为底。
    QList<CanvasItem*> SelectionOrder() const;
    // 名称只按画布内图元的 Name() 比较；空名称不占用唯一名额。
    bool IsItemNameAvailable(const QString& name, const CanvasItem* except = nullptr) const;
    // 按“类型 1 / 类型 2 …”生成类型专属的自动名称；用于新建图元与空名称补名。
    QString UniqueItemName(const QString& base, const CanvasItem* except = nullptr) const;
    // 在已有名称基础上生成唯一副本名称：Hero panel → Hero panel 2，矩形 1 → 矩形 2。
    QString UniqueItemNameFor(const QString& requested, const CanvasItem* except = nullptr) const;
    // 复制当前选中的可编辑图元（同层、偏移一点、名称自动取唯一值），整体一条撤销记录。
    // 返回实际生成的副本数量。
    int DuplicateSelectedItems();

    // 剪贴板：把选中的可编辑图元序列化成 JSON 数组；粘贴时按数组重建并偏移选中。
    QJsonArray ClipboardItems() const;
    int PasteClipboardItems(const QJsonArray& items);
    // 方向键微调：按 delta 移动选中的可编辑图元，整体一条撤销记录，返回移动数量。
    int NudgeSelected(const QPointF& delta);

    // 分组：只允许同层、至少两个可编辑图元。移动任意组员时整组移动，
    // 但组员仍可单独选中并编辑属性。reason 非空时写出失败原因（§7.8 枚举）。
    bool CanGroupSelectedItems(GroupFailureReason* reason = nullptr) const;
    bool CanUngroupSelectedItems() const;
    int GroupSelectedItems(GroupFailureReason* reason = nullptr);
    int UngroupSelectedItems();
    QList<CanvasItem*> ItemsInGroup(int groupId) const;
    // 分组有层级：组本身还能被更外层的组包住。outermostGroup 沿父链找到最外层组，
    // itemsInGroupTree 返回该组及其所有嵌套子组里的图元。
    int ParentGroup(int groupId) const;
    int OutermostGroup(int groupId) const;
    QList<CanvasItem*> ItemsInGroupTree(int groupId) const;
    // §7.5 / D6：组内成员不允许单独换层。整棵组树的成员都被选中时视为“整组换层”，
    // 允许放行；同层内的层内高度调整不受此限制。
    bool CanMoveItemToLayer(const CanvasItem* item, int layerId) const;
    // 被拒绝的操作统一从这里提示，界面层决定展示时长。
    void NotifyGroupLayerBlocked();
    // 单击组内任意图元时选中整棵组树；返回 true 表示该图元属于某个组。
    bool SelectGroupOfItem(CanvasItem* item);
    QHash<int, int> GroupParents() const { return m_groups.Parents(); }
    void ApplyGroupParents(const QHash<int, int>& parents);
    void ApplyGroupIds(const QList<CanvasItem*>& items, const QList<int>& groupIds);
    // 插入即成组（立方体 / 圆柱体的面图元）：给 items 分配新组号并落组，
    // 不占撤销记录——插入本身的 addItemsWithUndo 已经是一条撤销。
    // reason 非空时写出失败原因；失败时返回 0 且不改动 items。
    // nameBase 非空时以其为组名 base 补后缀；为空时按“组 N”自动编号。
    int AssignNewGroup(const QList<CanvasItem*>& items, GroupFailureReason* reason = nullptr,
                       const QString& nameBase = QString());
    // 导入多图元自定义图形用：同样分配新组，但把成员归属 / 组名 / 父子关系推成一条
    // GroupCommand，调用方再用 beginMacro 与插入合并成一条撤销——撤销插入时组记录
    // 一并回退，重做时组名也随图元一起回来，不留在场景里的孤儿组。
    int AssignNewGroupWithUndo(const QList<CanvasItem*>& items, GroupFailureReason* reason, const QString& nameBase,
                               const QString& text);

    // ---- 组模型（§7.2）：组不是 QGraphicsItem，只在场景里登记组名 / 父子关系 ----
    // 组名在文档范围内唯一（trimmed 后大小写不敏感）；groupId <= 0 返回空名。
    QString GroupName(int groupId) const;
    bool IsGroupNameAvailable(const QString& name, int exceptGroupId = 0) const;
    // 按 base 生成唯一组名：base 可用即用 base，否则补 2、3… 后缀。
    QString UniqueGroupName(const QString& base) const;
    QList<int> GroupIds() const;
    QList<int> ChildGroups(int groupId) const;
    QList<CanvasItem*> DirectMembers(int groupId) const;
    QList<CanvasItem*> AllLeafItems(int groupId) const;
    int MemberCount(int groupId) const;
    // 组框：所有成员 mapRectToScene(FrameRect()) 的并集；空组返回空矩形。
    QRectF GroupBounds(int groupId) const;
    bool IsGroupEmpty(int groupId) const;
    // 组所属图层：取组内成员唯一 LayerId；跨层或空组返回 BackgroundLayerId。
    int GroupLayerId(int groupId) const;
    bool IsOutermostGroup(int groupId) const;
    // 删除 / 剪切 / 解散 / 载入后清理没有叶子图元的空组（含其子组记录与组名）。
    void PruneEmptyGroups();
    // 重命名组：空名 / 重名不提交；合法时压一条撤销记录。
    bool RenameGroupWithUndo(int groupId, const QString& name);
    // 选中该组树下全部可编辑叶子图元，不改变当前活动图层。
    void SelectGroupById(int groupId);
    // 组名 / 父子关系快照：供删除命令与分组命令在撤销 / 重做时整体恢复。
    QHash<int, QString> GroupNames() const { return m_groups.Names(); }
    GroupState GroupSnapshot() const { return m_groups.Snapshot(); }
    void ApplyGroupNames(const QHash<int, QString>& names);
    void ApplyGroupRecords(const GroupState& groups);
    // 旧签名保留为薄转发，调用方逐步改用 GroupState。
    void ApplyGroupRecords(const QHash<int, QString>& names, const QHash<int, int>& parents);
    // 供组选框控制器判断：选中集合是否恰好是一个完整的最外层组。
    bool SelectionIsCompleteGroup(int* outGroupId = nullptr) const;
    // 完整整组选中时，成员自身的虚线框 / 手柄隐藏，由组选框统一绘制（§7.6）。
    bool HideMemberOverlay(CanvasItem* item) const;

    // 手动平滑：只处理编辑模式下选中的自由笔画，一次操作只占一条撤销记录。
    bool CanSmoothSelectedStrokes() const;
    int SmoothSelectedStrokes(qreal tolerance);

    static constexpr int BackgroundLayerId = 0;

    const QList<CanvasLayer>& Layers() const { return m_layerDomain.Layers(); }
    QList<CanvasLayer> UserLayers() const;
    int LayerCount() const { return m_layerDomain.Count(); }
    int UserLayerCount() const { return qMax(0, m_layerDomain.Count() - 1); }
    bool HasLayer(int layerId) const;
    CanvasLayer layer(int layerId) const;
    int LayerIndex(int layerId) const;
    int LayerItemCount(int layerId) const;
    QList<CanvasItem*> LayerItems(int layerId) const;
    bool IsLayerVisible(int layerId) const;
    bool SetLayerVisible(int layerId, bool visible);
    bool SetAllLayersVisible(bool visible);
    bool IsLayerLocked(int layerId) const;
    bool SetLayerLocked(int layerId, bool locked);
    bool IsItemEditable(const CanvasItem* item) const;
    // True when the active layer accepts new items (exists, visible, unlocked).
    bool IsActiveLayerWritable() const;
    int ActiveLayerId() const { return m_layerDomain.ActiveLayerId(); }
    void SetActiveLayer(int layerId);

    int CreateLayer(const QString& name = QString());
    bool AddLayerWithUndo(const QString& name = QString());
    bool RemoveLayerWithUndo(int layerId, QString* errorMessage = nullptr);
    bool MoveLayerWithUndo(int layerId, int delta);
    // 拖拽排序：把图层移动到目标下标（用户图层区间，背景层固定在 0）；同下标不动作。
    bool MoveLayerToIndexWithUndo(int layerId, int targetIndex);
    bool RenameLayerWithUndo(int layerId, const QString& name);
    LayerStackState LayerState() const;
    void ApplyLayerState(const LayerStackState& state);

    int NextHeight(int layerId) const;
    bool LayerHeightsUnique(int layerId) const;
    bool AdjustItemHeight(CanvasItem* item, int delta, const QString& text = QString());
    bool MoveItemToAdjacentLayer(CanvasItem* item, int delta, const QString& text = QString());
    // 对当前选中的所有图元批量调整高度 / 图层，整体只占一条撤销记录。
    int AdjustSelectedHeights(int delta);
    int MoveSelectedItemsToAdjacentLayer(int delta);

    // 图层面板中被选中的图层。重叠图元命中时优先选择这些图层里的图元。
    void SetPreferredLayerIds(const QList<int>& ids);
    QList<int> PreferredLayerIds() const { return m_preferredLayerIds; }
    void ApplyItemStack(const ItemStackList& stack);

    bool ChangeTextWithUndo(TextItem* item, const QString& text, const QFont& font, const QColor& color,
                            const QString& commandText = QString());
    void ApplyTextState(TextItem* item, const TextItemState& state);

    void AddItemsWithUndo(const QList<CanvasItem*>& items, const QString& text);
    void CommitNewItem(CanvasItem* item, const QString& text);
    void AttachItem(CanvasItem* item);
    void DetachItem(CanvasItem* item);
    void DeleteSelected();
    void SelectAllItems();
    // mergeId 非空时命令标记为可合并（目前用于 nudge：连续方向键微调并成一条撤销记录）。
    void PushGeometry(const ItemPlacementList& before, const ItemPlacementList& after, const QString& text,
                      const QString& mergeId = QString());
    // 需求 35：旋转手柄与快捷键的撤销入口。
    void PushRotation(CanvasItem* item, qreal before, qreal after, const QString& text);
    // 组旋转：一条撤销记录同时覆盖成员角度与绕组心旋转后的位置（§7.7）。
    void PushGroupRotation(const QList<CanvasItem*>& items, const QList<qreal>& anglesBefore,
                           const QList<qreal>& anglesAfter, const ItemPlacementList& geometryBefore,
                           const ItemPlacementList& geometryAfter, const QString& text);
    int RotateSelected(qreal degrees, const QString& commandText = QString());
    // 左右（horizontal=true）/ 上下镜像选中的图元：绕选区并集框的中线翻转，
    // 位置沿轴镜像、旋转角取反、对应翻转位取反，一条撤销记录。
    int MirrorSelected(bool horizontal, const QString& commandText = QString());
    // 扇形 / 弧线 / 角度线三型互换：返回新图元（已挂进场景并选中），失败返回 nullptr。
    CanvasItem* ConvertItemType(CanvasItem* item, const QString& targetKind);

    void ResetDocument();
    QJsonObject ToJson() const;
    bool FromJson(const QJsonObject& root, QString* errorMessage = nullptr);

    QImage RenderImage(const QRectF& area, qreal scale);
    // 只渲染指定图层上的图元；画板背景仍按背景层的显示状态保留。
    QImage RenderLayerImage(const QRectF& area, qreal scale, int layerId);
    bool IsExporting() const { return m_exporting; }

    // 非查看模式下被更高优先级图元遮住、需要额外描出轮廓的图元。
    // 优先级：图层越高越优先；同一图层内 height 越大越优先。
    // area 非空时只统计与该区域相交的图元（绘制时用它避免整场景重算）。
    QList<CanvasItem*> ObscuredItems(const QRectF& area = QRectF()) const;

  signals:
    void modeChanged(ToolMode mode);
    void contentChanged();
    void backgroundChanged();
    void canvasSizeChanged(const QSizeF& size);
    void layersChanged();
    void activeLayerChanged(int layerId);
    // 分组动作失败时带出原因，视图层转成状态栏文案；成功不发此信号。
    void groupOperationFailed(GroupFailureReason reason);
    // 被规则拒绝的编辑操作（目前是组内成员单独换层）的提示文案。
    void statusMessage(const QString& message);
    // 吸附候选变化：视图据此重画高亮框。
    void bindingHighlightChanged();

  protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;
    // 逐图元裁剪：被更高优先级图元盖住的部分不画下层颜色（不是半透明混合）。
    void drawItems(QPainter* painter, int numItems, QGraphicsItem* items[], const QStyleOptionGraphicsItem* options,
                   QWidget* widget = nullptr) override;

  private:
    CanvasItem* CreateItem(const QString& kind, const QString& shapeType = QString()) const;
    // 图元类型标识只按 JSON 里的 kind 分派：线条族五个形态各自一个类，
    // 图形 / 多边形 / 文字各一个族。
    CanvasItem* CreateItemForJson(const QJsonObject& json) const;
    // reserved 里是尚未挂到场景、但已经保留给其它副本的名称。
    QString UniqueItemNameFor(const QString& requested, const CanvasItem* except, const QSet<QString>& reserved) const;
    void DrawCanvasBackground(QPainter* painter, const QRectF& area, bool includeGrid) const;
    // 透明背景（None）在视图里画浅灰 / 白棋盘格作为透明标识，每格固定 64 逻辑像素；导出图不画。
    void DrawCheckerboard(QPainter* painter, const QRectF& canvas) const;
    void UpdateItemFlags();
    void ApplyItemInteraction(CanvasItem* item);
    void ClearOrphans();
    // 维护选择顺序：新增的选中项追加到末尾，撤销选中或删除的被移除。
    void RefreshSelectionOrder();
    // 选中高亮：直接从场景当前选择集绘制，不再另存一份图元快照（销毁期不重入）。
    void DrawSelectionHighlight(QPainter* painter, const QRectF& rect) const;

    void EnsureLayerState();
    LayerStackState SanitizedLayerState(const LayerStackState& state) const;
    // 组名建议：base 为空按“组 N”编号，否则取 base 并在重名时补数字后缀。
    QString SuggestGroupName(const QString& base) const;
    void ApplyLayerStateInternal(const LayerStackState& state, bool preserveLock = true);
    void AssignNewItemPlacement(CanvasItem* item);
    void ApplyItemStacking(CanvasItem* item);
    void ApplyAllItemStacking();
    bool NormalizeLayerHeights(int layerId);
    void NormalizeAllHeights();
    bool HeightUsedByOther(int layerId, int height, const CanvasItem* except) const;
    qreal LayerBaseZ(int layerId) const;
    ItemStackList ItemStackSnapshot(const QList<CanvasItem*>& items) const;

    QUndoStack* m_undoStack = nullptr;
    // 只在画箭头 / 拖端点的过程里活着，结束即清空，不跨事件循环持有。
    CanvasItem* m_bindingHighlight = nullptr;
    ToolMode m_mode = ToolMode::VIEW;
    QSizeF m_canvasSize{1536.0, 1024.0};
    QPointF m_canvasOrigin{0.0, 0.0};
    CanvasBackground m_background;
    bool m_showGrid = false;
    bool m_exporting = false;
    // fromJson 加载重建期间为 true：这期间的归一化修复不算用户操作，
    // 不往撤销栈压命令（否则打开文件就会产生一条脏的历史记录）。
    bool m_restoringDocument = false;
    LayerDomain m_layerDomain;
    GroupDomain m_groups;
    QList<QPointer<CanvasItem>> m_selectionOrder;
    QList<int> m_preferredLayerIds;
    QList<CanvasItem*> m_orphans;
};
