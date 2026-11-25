#include "app/I18n.h"
#include "items/CanvasItem.h"
#include "items/shapes/ShapeFactory.h"
#include "scene/CanvasScene.h"
#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"
#include "ui/panels/layers/LayerPanel.h"

#include <QApplication>
#include <QCheckBox>
#include <QImage>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QUndoStack>

namespace {

QListWidgetItem* FindLayerRow(QListWidget* list, int layerId)
{
    if (!list) {
        return nullptr;
    }
    for (int row = 0; row < list->count(); ++row) {
        QListWidgetItem* entry = list->item(row);
        if (entry && entry->data(LayerPanel::ROLE_ROW_TYPE).toInt() == LayerPanel::ROW_LAYER &&
            entry->data(Qt::UserRole).toInt() == layerId) {
            return entry;
        }
    }
    return nullptr;
}

QListWidgetItem* FindItemRow(QListWidget* list, const CanvasItem* item)
{
    if (!list || !item) {
        return nullptr;
    }
    for (int row = 0; row < list->count(); ++row) {
        QListWidgetItem* entry = list->item(row);
        if (!entry || entry->data(LayerPanel::ROLE_ROW_TYPE).toInt() != LayerPanel::ROW_ITEM) {
            continue;
        }
        if (entry->text().trimmed() == item->Name()) {
            return entry;
        }
    }
    return nullptr;
}

QList<QListWidgetItem*> RowsOfType(QListWidget* list, int type, int layerId)
{
    QList<QListWidgetItem*> rows;
    if (!list) {
        return rows;
    }
    bool inLayer = layerId == CanvasScene::BackgroundLayerId;
    for (int row = 0; row < list->count(); ++row) {
        QListWidgetItem* entry = list->item(row);
        if (!entry) {
            continue;
        }
        const int entryType = entry->data(LayerPanel::ROLE_ROW_TYPE).toInt();
        if (entryType == LayerPanel::ROW_LAYER) {
            inLayer = entry->data(Qt::UserRole).toInt() == layerId;
            continue;
        }
        if (inLayer && entryType == type) {
            rows.append(entry);
        }
    }
    return rows;
}

int CountHighlightPixels(const QImage& image)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor color = image.pixelColor(x, y);
            // 亮青蓝 #00B4FF：红分量很低、绿与蓝都高。
            if (color.red() < 90 && color.green() > 140 && color.blue() > 200) {
                ++count;
            }
        }
    }
    return count;
}

} // namespace

void selftest::RunLayerTreePanelTests(Context& ctx)
{
    CanvasScene scene;
    QUndoStack undo;
    scene.SetUndoStack(&undo);
    scene.SetMode(ToolMode::EDIT);

    auto makeRect = [&scene](QPointF pos, QColor color) {
        auto* item = ShapeFactory::Create(ShapeType::RECTANGLE);
        item->SetStyle(color, QColor(Qt::transparent), 4.0);
        item->setPos(pos);
        item->SetContentSize(QSizeF(120, 90));
        return item;
    };
    auto* low = makeRect(QPointF(40, 40), QColor(QStringLiteral("#2563EB")));
    auto* mid = makeRect(QPointF(200, 40), QColor(QStringLiteral("#16A34A")));
    auto* high = makeRect(QPointF(360, 40), QColor(QStringLiteral("#DC2626")));
    scene.AddItemsWithUndo({low, mid, high}, QStringLiteral("层树测试图元"));
    scene.clearSelection();
    const int layerId = low->LayerId();
    QApplication::processEvents();

    LayerPanel panel(&scene);
    panel.resize(260, 480);
    auto* list = panel.findChild<QListWidget*>(QStringLiteral("LayerList"));
    auto* groupBox = panel.findChild<QCheckBox*>(QStringLiteral("LayerGroupModeBox"));
    ctx.Check(list != nullptr, QStringLiteral("图层面板创建列表"));
    ctx.Check(groupBox != nullptr && !groupBox->isChecked(), QStringLiteral("图层面板默认不勾选按组显示"));

    if (!list || !groupBox) {
        return;
    }

    // 不勾选：整层图元按层内高度降序平铺，不带组名。
    const QList<QListWidgetItem*> flatRows = RowsOfType(list, LayerPanel::ROW_ITEM, layerId);
    ctx.Check(flatRows.size() == 3, QStringLiteral("不勾选按组显示时图层下列出全部图元"),
              QStringLiteral("rows=%1").arg(flatRows.size()));
    ctx.Check(flatRows.size() == 3 && flatRows.at(0)->text().trimmed() == high->Name() &&
                  flatRows.at(1)->text().trimmed() == mid->Name() && flatRows.at(2)->text().trimmed() == low->Name(),
              QStringLiteral("图层内图元行按 height 从大到小排列"));
    ctx.Check(!flatRows.isEmpty() && flatRows.at(0)->data(Qt::UserRole).toInt() == -1,
              QStringLiteral("图元行的 UserRole 不占用图层 id"));

    // 建组：mid + high 成组。
    scene.clearSelection();
    mid->setSelected(true);
    high->setSelected(true);
    GroupFailureReason reason = GroupFailureReason::NONE;
    scene.GroupSelectedItems(&reason);
    QApplication::processEvents();
    const int groupId = mid->GroupId();
    ctx.Check(groupId > 0 && scene.ItemsInGroupTree(groupId).size() == 2, QStringLiteral("层树测试建组成功"),
              QStringLiteral("groupId=%1 members=%2").arg(groupId).arg(scene.ItemsInGroupTree(groupId).size()));

    const QList<QListWidgetItem*> groupedOffRows = RowsOfType(list, LayerPanel::ROW_ITEM, layerId);
    ctx.Check(groupedOffRows.size() == 3 && RowsOfType(list, LayerPanel::ROW_GROUP, layerId).isEmpty(),
              QStringLiteral("不勾选按组显示时组名不出行"));

    groupBox->setChecked(true);
    QApplication::processEvents();
    QListWidgetItem* layerRow = FindLayerRow(list, layerId);
    const QList<QListWidgetItem*> groupRows = RowsOfType(list, LayerPanel::ROW_GROUP, layerId);
    const QList<QListWidgetItem*> groupedRows = RowsOfType(list, LayerPanel::ROW_ITEM, layerId);
    ctx.Check(groupRows.size() == 1 && groupedRows.size() == 3, QStringLiteral("勾选按组显示后图层下先出组名再出图元"),
              QStringLiteral("groups=%1 items=%2").arg(groupRows.size()).arg(groupedRows.size()));
    const int layerRowIndex = list->row(layerRow);
    ctx.Check(layerRowIndex >= 0 && list->row(groupRows.isEmpty() ? nullptr : groupRows.first()) == layerRowIndex + 1,
              QStringLiteral("组名紧跟图层行、在图元名之前"));
    ctx.Check(groupedRows.size() == 3 && groupedRows.at(0)->text().trimmed() == high->Name() &&
                  groupedRows.at(1)->text().trimmed() == mid->Name() &&
                  groupedRows.at(2)->text().trimmed() == low->Name(),
              QStringLiteral("组内成员按 height 降序、未分组图元排在组之后"));
    ctx.Check(groupedRows.size() == 3 && groupedRows.at(2)->text().size() < groupedRows.at(0)->text().size(),
              QStringLiteral("组内图元名比未分组图元名多一层缩进"));

    // 面板 → 画布：选中图元行等于选中该图元。
    list->clearSelection();
    QListWidgetItem* highRowFirst = FindItemRow(list, high);
    if (highRowFirst) {
        highRowFirst->setSelected(true);
    }
    QApplication::processEvents();
    const QList<CanvasItem*> panelSelection = scene.SelectedCanvasItems();
    ctx.Check(panelSelection.size() == 1 && panelSelection.first() == high,
              QStringLiteral("图层面板选中图元行同步到画布选择"),
              QStringLiteral("selected=%1").arg(panelSelection.size()));

    // 画布 → 面板：画布选中图元时对应图元行自动选中。
    scene.clearSelection();
    QApplication::processEvents();
    mid->setSelected(true);
    QApplication::processEvents();
    QListWidgetItem* midRow = FindItemRow(list, mid);
    QListWidgetItem* highRow = FindItemRow(list, high);
    QListWidgetItem* lowRow = FindItemRow(list, low);
    ctx.Check(midRow && highRow && lowRow && midRow->isSelected() && !highRow->isSelected() && !lowRow->isSelected(),
              QStringLiteral("画布选中图元时图层面板对应图元名自动选中"));
    QListWidgetItem* layerRowForSelection = FindLayerRow(list, layerId);
    ctx.Check(layerRowForSelection && layerRowForSelection->isSelected(),
              QStringLiteral("画布选中图元时其图层行同时选中"));

    // 画布整组选中：勾选态高亮组行与全部成员，且不切换当前图层。
    const int activeBefore = scene.ActiveLayerId();
    ctx.Check(scene.ItemsInGroupTree(groupId).size() == 2, QStringLiteral("整组仍包含两个成员图元"),
              QStringLiteral("members=%1").arg(scene.ItemsInGroupTree(groupId).size()));
    scene.SelectGroupById(groupId);
    ctx.Check(scene.SelectedCanvasItems().size() == 2, QStringLiteral("画布整组选中包含两个成员图元"),
              QStringLiteral("selected=%1").arg(scene.SelectedCanvasItems().size()));
    QApplication::processEvents();
    const QList<QListWidgetItem*> completeGroupRows = RowsOfType(list, LayerPanel::ROW_GROUP, layerId);
    ctx.Check(completeGroupRows.size() == 1 && completeGroupRows.first()->isSelected(),
              QStringLiteral("画布选中整组时组行高亮"));
    const bool membersLit = FindItemRow(list, mid) && FindItemRow(list, mid)->isSelected() && FindItemRow(list, high) &&
                            FindItemRow(list, high)->isSelected();
    ctx.Check(membersLit, QStringLiteral("画布选中整组时组内成员图元行同时高亮"));
    ctx.Check(scene.ActiveLayerId() == activeBefore, QStringLiteral("整组选中不切换当前图层"));

    // 取消勾选后组行消失，成员行仍保持高亮。
    groupBox->setChecked(false);
    QApplication::processEvents();
    ctx.Check(RowsOfType(list, LayerPanel::ROW_GROUP, layerId).isEmpty(), QStringLiteral("取消勾选后组行从图层树移除"));
    const bool membersStillLit = FindItemRow(list, mid) && FindItemRow(list, mid)->isSelected() &&
                                 FindItemRow(list, high) && FindItemRow(list, high)->isSelected();
    ctx.Check(membersStillLit, QStringLiteral("取消勾选后整组成员仍保持图元行高亮"));

    // 选中亮青蓝虚线轮廓：编辑模式画、查看模式不画。
    groupBox->setChecked(true);
    scene.clearSelection();
    low->setSelected(true);
    QApplication::processEvents();
    const QRectF area = low->sceneBoundingRect().adjusted(-30, -30, 30, 30);
    auto renderSelection = [&scene, &area]() {
        QImage image(200, 200, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        QPainter painter(&image);
        scene.render(&painter, QRectF(image.rect()), area);
        painter.end();
        return image;
    };
    const int editHighlight = CountHighlightPixels(renderSelection());
    scene.SetMode(ToolMode::VIEW);
    low->setSelected(true);
    QApplication::processEvents();
    const int viewHighlight = CountHighlightPixels(renderSelection());
    scene.SetMode(ToolMode::EDIT);
    ctx.Check(editHighlight > 0, QStringLiteral("编辑模式选中图元叠加亮青蓝虚线轮廓"),
              QStringLiteral("highlight=%1").arg(editHighlight));
    ctx.Check(viewHighlight == 0, QStringLiteral("查看模式不绘制选中轮廓"),
              QStringLiteral("highlight=%1").arg(viewHighlight));
}
