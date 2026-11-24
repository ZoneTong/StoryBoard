// 属性面板的文字与填充图片应用方法：文字正文 / 字体 / 颜色的 Memento 写回，
// 以及背景图片的选择、清除与摆放方式。与填充 / 描线基类批量修改同属 PropertiesPanel，
// 按职责拆到本翻译单元，控制单文件行数。
#include "items/CanvasItem.h"
#include "items/geometry/ShapeMeta.h"
#include "items/properties/FillAttributes.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "items/text/TextItem.h"
#include "scene/CanvasScene.h"
#include "scene/Commands.h"
#include "ui/dialogs/ImagePickerDialog.h"
#include "ui/panels/properties/PropertiesPanel.h"

#include <QFont>
#include <QPixmap>
#include <QUndoStack>

void PropertiesPanel::ChangeSelectedTexts(const QString& text, const std::function<void(TextItemState&)>& mutate)
{
    const QList<TextItem*> texts = SelectedTexts();
    if (texts.isEmpty() || !m_scene || !m_scene->UndoStack()) {
        return;
    }

    QList<TextItemState> beforeStates;
    QList<TextItemState> afterStates;
    bool changed = false;
    for (TextItem* item : texts) {
        const TextItemState before = item->State();
        TextItemState after = before;
        mutate(after);
        beforeStates.append(before);
        afterStates.append(after);
        changed = changed || after != before;
    }
    if (!changed) {
        return;
    }

    QUndoStack* undo = m_scene->UndoStack();
    undo->beginMacro(text);
    for (int i = 0; i < texts.size(); ++i) {
        undo->push(new TextItemCommand(m_scene, texts.at(i), beforeStates.at(i), afterStates.at(i), text));
    }
    undo->endMacro();
    Refresh();
}

void PropertiesPanel::ApplyTextContent(const QString& text)
{
    ChangeSelectedTexts(tr("Change text content"), [&text](TextItemState& state) {
        if (state.text == text) {
            return;
        }
        state.text = text;
        state.baseSize = TextItem::Measure(state.text, state.font);
        state.size = state.baseSize;
    });
}

void PropertiesPanel::ApplyTextFont(const QFont& font)
{
    ChangeSelectedTexts(tr("Change text font"), [&font](TextItemState& state) {
        if (state.font == font) {
            return;
        }
        state.font = font;
        state.baseSize = TextItem::Measure(state.text, state.font);
        state.size = state.baseSize;
    });
}

void PropertiesPanel::ApplyTextColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    ChangeSelectedTexts(tr("Change text color"), [&color](TextItemState& state) { state.color = color; });
}

void PropertiesPanel::ChooseFillImage()
{
    // 需求 13：选中多个可填充图元时一次替换；图片从资源面板里选（也可以现场从本地加一张）。
    bool hasTarget = false;
    for (CanvasItem* item : SelectedItems()) {
        auto* fill = dynamic_cast<FillAttributes*>(item);
        auto* shape = dynamic_cast<ShapeAbstractItem*>(item);
        if (fill && shape && !shape->IsCustomShape() && ShapeMeta::IsFilled(shape->shapeType())) {
            hasTarget = true;
            break;
        }
    }
    if (!hasTarget || !m_scene || !m_scene->UndoStack()) {
        return;
    }
    const QPixmap pixmap = ImagePickerDialog::Pick(this, CollectCanvasImages(m_scene), tr("Fill image"));
    if (pixmap.isNull()) {
        return;
    }
    ApplyFillMutation(tr("Replace fill image"), [pixmap](FillAttributes* fill) {
        if (auto* shape = dynamic_cast<ShapeAbstractItem*>(fill); shape && shape->IsCustomShape()) {
            return;
        }
        // 替换图片同时重置为拉伸模式，与原整份状态命令语义一致。
        fill->SetFillImage(pixmap, ImageFill::Mode::STRETCH);
    });
}

void PropertiesPanel::ClearFillImage()
{
    ApplyFillMutation(tr("Clear fill image"), [](FillAttributes* fill) { fill->ClearFillImage(); });
}

void PropertiesPanel::ApplyFillImageMode(int mode)
{
    // 与属性窗同一套摆放方式；无图时组合框已禁用，这里再兜一次底。
    ApplyFillMutation(tr("Change fill image mode"), [mode](FillAttributes* fill) {
        if (!fill->HasFillImage()) {
            return;
        }
        fill->SetFillImage(fill->FillImage(), static_cast<ImageFill::Mode>(mode));
    });
}
