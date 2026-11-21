#include "ItemPropertiesDialog.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "ItemPropertiesDialogGeometryBlock.h"
#include "ItemPropertiesDialogIdentityBlock.h"
#include "ItemPropertiesDialogStyleBlock.h"
#include "ItemPropertiesDialogTextBlock.h"
#include "LineItemAbstract.h"
#include "ShapeItem.h"
#include "TextItem.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QTextCursor>
#include <QToolButton>
#include <QVBoxLayout>
#include <utility>

ItemPropertiesDialog::ItemPropertiesDialog(CanvasScene* scene, CanvasItem* item, QWidget* parent, bool editable)
    : QDialog(parent), m_scene(scene), m_item(item), m_editable(editable)
{
    setWindowTitle(tr("Item properties"));
    // 需求 42：属性窗统一改为模态；非编辑模式可打开查看，但字段只读。
    setModal(true);
    setMinimumWidth(480);

    // 打开属性窗时再兜底一次自动名称：旧画布、直接 addItem 或清空名称后，
    // 这里也能得到“类型 N”，不会出现看不到名称或名称为空的行。
    if (scene) {
        scene->AssignAutoItemNames();
    }

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 16);
    root->setSpacing(12);

    auto* heading = new QLabel(item ? item->Name() : tr("No item selected"), this);
    heading->setObjectName(QStringLiteral("ItemPropertiesHeading"));
    root->addWidget(heading);

    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    root->addWidget(line);

    if (!m_editable) {
        auto* readOnlyHint = new QLabel(tr("Read-only: switch to Edit mode to change properties."), this);
        readOnlyHint->setObjectName(QStringLiteral("ItemPropertiesReadOnlyHint"));
        readOnlyHint->setWordWrap(true);
        root->addWidget(readOnlyHint);
    }

    auto* form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(9);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    if (!item) {
        auto* typeValue = new QLabel(QStringLiteral("—"), this);
        typeValue->setObjectName(QStringLiteral("ItemPropertiesTypeValue"));
        form->addRow(tr("Item type"), typeValue);
        root->addLayout(form);
    } else {
        m_initialPos = item->pos();
        m_initialSize = item->ContentSize();
        m_initialRotation = item->RotationAngle();
        m_initialItemName = item->CustomName();
        m_initialDisplayName = item->Name();
        auto* typeValue = new QLabel(item->TypeDisplayName(), this);
        typeValue->setObjectName(QStringLiteral("ItemPropertiesTypeValue"));
        typeValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
        form->addRow(tr("Item type"), typeValue);

        m_identityBlock = new ItemPropertiesDialogIdentityBlock(scene, item, this);
        m_identityBlock->AddRows(form);

        if (dynamic_cast<ShapeItem*>(item) || dynamic_cast<LineItemAbstract*>(item)) {
            m_styleBlock = new ItemPropertiesDialogStyleBlock(item, this);
            m_styleBlock->AddRows(form);
            m_hasFill = m_styleBlock->HasFill();
            m_initialStrokeWidth = m_styleBlock->InitialStrokeWidth();
        } else if (item->HasTextFacet()) {
            m_textBlock = new ItemPropertiesDialogTextBlock(item, this);
            m_textBlock->AddRows(form);
            connect(m_textBlock, &ItemPropertiesDialogTextBlock::fontChanged, this,
                    &ItemPropertiesDialog::SyncTextNaturalSize);
        }

        // 图形在样式区已有透明度行；笔画、文字、图片等图元在这里统一补一行。
        if (!dynamic_cast<ShapeItem*>(item)) {
            m_opacitySpin = new QSpinBox(this);
            m_opacitySpin->setObjectName(QStringLiteral("ItemPropertiesOpacityValue"));
            m_opacitySpin->setRange(0, 100);
            m_opacitySpin->setSuffix(QStringLiteral(" %"));
            m_opacitySpin->setMinimumWidth(88);
            m_initialOpacity = item->ItemOpacity();
            m_opacitySpin->setValue(qRound(m_initialOpacity * 100.0));
            m_initialOpacityPercent = m_opacitySpin->value();
            form->addRow(tr("Opacity"), m_opacitySpin);
        }

        m_geometryBlock = new ItemPropertiesDialogGeometryBlock(item, this);
        m_geometryBlock->AddRows(form);
        if (m_styleBlock && m_styleBlock->CornerRadiusSpin()) {
            auto syncCornerRange = [this]() {
                if (!m_geometryBlock) {
                    return;
                }
                m_styleBlock->SetContentSize(
                    QSizeF(m_geometryBlock->WidthSpin()->value(), m_geometryBlock->HeightSizeSpin()->value()));
            };
            connect(m_geometryBlock->WidthSpin(), &QDoubleSpinBox::valueChanged, this, syncCornerRange);
            connect(m_geometryBlock->HeightSizeSpin(), &QDoubleSpinBox::valueChanged, this, syncCornerRange);
        }

        root->addLayout(form);
    }

    // 文字正文直接在属性窗里编辑；内容变化时同步到自然尺寸，确定后由
    // ItemPropertiesCommand 一次性提交文字、尺寸和撤销记录。
    if (m_textBlock && m_textBlock->TextEdit()) {
        connect(m_textBlock->TextEdit(), &QPlainTextEdit::textChanged, this,
                &ItemPropertiesDialog::SyncTextNaturalSize);
    }

    ApplyEditableState();

    auto* buttons = new QDialogButtonBox(
        m_editable ? (QDialogButtonBox::Ok | QDialogButtonBox::Cancel) : QDialogButtonBox::Ok, this);
    if (auto* ok = buttons->button(QDialogButtonBox::Ok)) {
        ok->setText(m_editable ? tr("OK") : tr("Close"));
    }
    if (auto* cancel = buttons->button(QDialogButtonBox::Cancel)) {
        cancel->setText(tr("Cancel"));
    }
    connect(buttons, &QDialogButtonBox::accepted, this, &ItemPropertiesDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    if (m_identityBlock && m_identityBlock->HasLayerControls()) {
        m_identityBlock->UpdateHeightRange(false);
        connect(m_identityBlock, &ItemPropertiesDialogIdentityBlock::layerChanged, this, [this]() {
            if (m_initializing) {
                return;
            }
            m_identityBlock->UpdateHeightRange(true);
        });
        m_initializing = false;
        m_identityBlock->SetInitialized(false);
    } else {
        m_initializing = false;
    }
}

void ItemPropertiesDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    // 只做一次，且只读模式不抢焦点；光标停在正文末尾，不整段选中。
    if (m_textFocusApplied || !m_editable || !m_textBlock || !m_textBlock->TextEdit()) {
        return;
    }
    m_textFocusApplied = true;
    QPlainTextEdit* edit = m_textBlock->TextEdit();
    edit->moveCursor(QTextCursor::End);
    edit->setFocus(Qt::OtherFocusReason);
}

void ItemPropertiesDialog::ApplyEditableState()
{
    if (m_editable) {
        return;
    }

    QList<QWidget*> widgets;
    if (m_identityBlock) {
        widgets << m_identityBlock->NameEdit() << m_identityBlock->LayerCombo() << m_identityBlock->HeightSpin();
    }
    if (m_styleBlock) {
        widgets << m_styleBlock->FillStyleCombo() << m_styleBlock->FillColorButton()
                << m_styleBlock->StrokeColorButton() << m_styleBlock->BorderStyleCombo()
                << m_styleBlock->StrokeWidthSpin() << m_styleBlock->OpacitySpin() << m_styleBlock->CornerRadiusSpin()
                << m_styleBlock->StartAngleSpin() << m_styleBlock->SweepAngleSpin();
    }
    if (m_opacitySpin) {
        widgets << m_opacitySpin;
    }
    if (m_textBlock) {
        widgets << m_textBlock->TextColorButton() << m_textBlock->FontButton() << m_textBlock->TextEdit();
    }
    if (m_geometryBlock) {
        widgets << m_geometryBlock->PositionXSpin() << m_geometryBlock->PositionYSpin() << m_geometryBlock->WidthSpin()
                << m_geometryBlock->HeightSizeSpin() << m_geometryBlock->MajorAxisSpin()
                << m_geometryBlock->MinorAxisSpin() << m_geometryBlock->RotationSpin();
    }
    for (QWidget* widget : widgets) {
        if (widget) {
            widget->setEnabled(false);
        }
    }
}

void ItemPropertiesDialog::accept()
{
    if (!m_editable) {
        QDialog::accept();
        return;
    }
    if (m_identityBlock && m_identityBlock->NameEdit() && m_scene && m_item) {
        const QString typedName = m_identityBlock->NameEdit()->text().trimmed();
        if (!typedName.isEmpty() && !m_scene->IsItemNameAvailable(typedName, m_item)) {
            QMessageBox::warning(this, tr("Duplicate item name"),
                                 tr("Another item in this canvas already uses this name. Choose a different name."));
            m_identityBlock->NameEdit()->setFocus();
            m_identityBlock->NameEdit()->selectAll();
            return;
        }
    }
    ApplyChanges();
    QDialog::accept();
}

void ItemPropertiesDialog::SyncTextNaturalSize()
{
    if (!m_editable || !m_textBlock || !m_textBlock->TextEdit()) {
        return;
    }
    if (m_item->HasTextFacet()) {
        const QSizeF natural = TextItem::Measure(m_textBlock->TextEdit()->toPlainText(), m_textBlock->TextFont());
        if (m_geometryBlock) {
            m_geometryBlock->SetNaturalSize(natural);
        }
    }
}

bool ItemPropertiesDialog::IsItemInScene() const
{
    return m_scene && m_item && m_item->scene() == m_scene;
}

void ItemPropertiesDialog::ApplyChanges()
{
    if (!m_editable || !m_scene || !m_item) {
        return;
    }

    auto* identity = m_identityBlock;
    auto* style = m_styleBlock;
    auto* text = m_textBlock;
    auto* geometry = m_geometryBlock;

    const int oldLayer = m_item->LayerId();
    const int oldHeight = m_item->ZOrder();
    const int targetLayer =
        (identity && identity->LayerCombo()) ? identity->LayerCombo()->currentData().toInt() : oldLayer;
    const int targetHeight = (identity && identity->HeightSpin()) ? identity->HeightSpin()->value() : oldHeight;

    QList<CanvasItem*> swapped;
    if (targetLayer != oldLayer || targetHeight != oldHeight) {
        for (CanvasItem* peer : m_scene->LayerItems(targetLayer)) {
            if (peer != m_item && peer->ZOrder() == targetHeight) {
                swapped.append(peer);
            }
        }
    }

    ItemPropertiesState before;
    before.pos = m_item->pos();
    before.size = m_item->ContentSize();
    before.rotation = m_initialRotation;
    before.opacity = m_item->ItemOpacity();
    before.itemName = m_initialItemName;
    before.stack.append({m_item, oldLayer, oldHeight});
    for (CanvasItem* peer : std::as_const(swapped)) {
        before.stack.append({peer, peer->LayerId(), peer->ZOrder()});
    }

    ItemPropertiesState after;
    auto spinValue = [](auto* spin, qreal initial) {
        if (!spin) {
            return initial;
        }
        const qreal value = spin->value();
        // 数值框按控件自身精度显示；用户没动过时保留原始精度，避免“打开就点确定”也产生修改。
        return qFuzzyCompare(value, qRound(initial * 10.0) / 10.0) ? initial : value;
    };
    after.pos = QPointF(spinValue(geometry->PositionXSpin(), m_initialPos.x()),
                        spinValue(geometry->PositionYSpin(), m_initialPos.y()));
    after.size = QSizeF(spinValue(geometry->WidthSpin(), m_initialSize.width()),
                        spinValue(geometry->HeightSizeSpin(), m_initialSize.height()));
    after.rotation = spinValue(geometry->RotationSpin(), m_initialRotation);
    after.opacity = before.opacity;
    const QString typedName = (identity && identity->NameEdit()) ? identity->NameEdit()->text().trimmed() : QString();
    // 清空名称不能退回“多个同名类型名”的状态；这时补一个本画布内唯一的自动名称。
    const QString effectiveName =
        typedName.isEmpty() ? m_scene->UniqueItemName(m_item->TypeDisplayName(), m_item) : typedName;
    after.itemName = (m_initialItemName.isEmpty() && typedName == m_initialDisplayName) ? QString() : effectiveName;
    after.stack.append({m_item, targetLayer, targetHeight});
    for (CanvasItem* peer : std::as_const(swapped)) {
        after.stack.append({peer, oldLayer, oldHeight});
    }

    if (auto* shape = dynamic_cast<ShapeItem*>(m_item.data())) {
        before.hasShapeState = true;
        before.shapeState = shape->State();

        after.hasShapeState = true;
        after.shapeState = shape->State();
        after.shapeState.pos = after.pos;
        after.shapeState.size = after.size;
        if (style && style->StrokeColorButton()) {
            after.shapeState.stroke.color = style->StrokeColorButton()->property("itemColor").value<QColor>();
        }
        if (style && style->StrokeWidthSpin()) {
            after.shapeState.stroke.width = spinValue(style->StrokeWidthSpin(), m_initialStrokeWidth);
        }
        if (style && style->BorderStyleCombo() && style->BorderStyleCombo()->currentData().toInt() >= 0) {
            after.shapeState.stroke.style = static_cast<Qt::PenStyle>(style->BorderStyleCombo()->currentData().toInt());
        }
        if (style && style->OpacitySpin()) {
            const int percent = style->OpacitySpin()->value();
            after.shapeState.opacity =
                percent == style->InitialOpacityPercent() ? style->InitialOpacity() : percent / 100.0;
            after.opacity = after.shapeState.opacity;
        }
        if (style && style->StartAngleSpin()) {
            const int value = style->StartAngleSpin()->value();
            after.shapeState.startAngle =
                value == qRound(style->InitialStartAngle()) ? style->InitialStartAngle() : value;
        }
        if (style && style->SweepAngleSpin()) {
            const int value = style->SweepAngleSpin()->value();
            after.shapeState.sweepAngle =
                value == qRound(style->InitialSweepAngle()) ? style->InitialSweepAngle() : value;
        }
        if (style && style->CornerRadiusSpin()) {
            const int value = style->CornerRadiusSpin()->value();
            // 圆角矩形默认值来自短边的 16%，对话框里显示为非 0；
            // 未改动时保留“按类型取默认”的 -1，避免打开属性窗点确定就写回显式值。
            after.shapeState.cornerRadius =
                value == style->InitialCornerRadius() ? style->InitialStoredCornerRadius() : value;
        }
        // 需求 1：三角形的两个底角为真相，未改动时保留原值（含非整数角度）。
        if (style && style->BottomAngle1Spin()) {
            const int value = style->BottomAngle1Spin()->value();
            after.shapeState.bottomAngle1 =
                value == qRound(style->InitialBottomAngle1()) ? style->InitialBottomAngle1() : value;
        }
        if (style && style->BottomAngle2Spin()) {
            const int value = style->BottomAngle2Spin()->value();
            after.shapeState.bottomAngle2 =
                value == qRound(style->InitialBottomAngle2()) ? style->InitialBottomAngle2() : value;
        }
        // 需求 9：平行四边形左下内角，未改动时保留原值。
        if (style && style->ParallelogramAngleSpin()) {
            const int value = style->ParallelogramAngleSpin()->value();
            after.shapeState.parallelogramAngle =
                value == qRound(style->InitialParallelogramAngle()) ? style->InitialParallelogramAngle() : value;
        }
        if (m_hasFill) {
            if (style && style->FillStyleCombo() && style->FillStyleCombo()->currentData().toInt() >= 0) {
                after.shapeState.fill.style = static_cast<FillStyle>(style->FillStyleCombo()->currentData().toInt());
            }
            if (style && style->FillColorButton()) {
                after.shapeState.fill.color = style->FillColorButton()->property("itemColor").value<QColor>();
            }
            // 需求 15：图片填充是独立属性，FillAttributesState 直接快照。
            if (style && style->HasFillImageRow()) {
                after.shapeState.fill.hasImage = style->FillImagePresent();
                after.shapeState.fill.image = style->FillImage();
                after.shapeState.fill.imageMode = style->FillImageMode();
            }
        }
    } else if (m_item->HasTextFacet()) {
        before.hasTextState = true;
        before.textState = m_item->TextState();

        after.hasTextState = true;
        after.textState = m_item->TextState();
        after.textState.pos = after.pos;
        after.textState.size = after.size;
        after.textState.font = text ? text->TextFont() : m_item->TextFont();
        if (text && text->TextEdit()) {
            after.textState.text = text->TextEdit()->toPlainText();
        }
        after.textState.baseSize = TextItem::Measure(after.textState.text, after.textState.font);
        // 需求追加：文字正文变化后直接回到文字自然尺寸，不再沿用旧图元的
        // 拉伸尺寸，避免新旧字形按不同比例叠在同一块内容区里。
        if (after.textState.text != before.textState.text) {
            after.size = after.textState.baseSize;
            after.textState.size = after.textState.baseSize;
        }
        if (text && text->TextColorButton()) {
            after.textState.color = text->TextColorButton()->property("itemColor").value<QColor>();
        }
    } else if (auto* stroke = dynamic_cast<LineItemAbstract*>(m_item.data())) {
        before.hasStrokeStyle = true;
        before.strokeColor = stroke->StrokeColor();
        before.strokeWidth = stroke->StrokeWidth();
        before.strokeStyle = stroke->strokePattern();
        // 需求 27：曲线两端箭头与描线一起进入同一条撤销记录；箭头对全部曲线子型开放。
        before.curveStartArrow = stroke->StartArrow();
        before.curveEndArrow = stroke->EndArrow();

        after.hasStrokeStyle = true;
        after.strokeColor = (style && style->StrokeColorButton())
                                ? style->StrokeColorButton()->property("itemColor").value<QColor>()
                                : before.strokeColor;
        after.strokeWidth = (style && style->StrokeWidthSpin())
                                ? spinValue(style->StrokeWidthSpin(), m_initialStrokeWidth)
                                : before.strokeWidth;
        after.strokeStyle = (style && style->BorderStyleCombo())
                                ? static_cast<Qt::PenStyle>(style->BorderStyleCombo()->currentData().toInt())
                                : before.strokeStyle;
        after.curveStartArrow = (style && style->StartArrowCombo())
                                    ? static_cast<ArrowStyle>(style->StartArrowCombo()->currentData().toInt())
                                    : before.curveStartArrow;
        after.curveEndArrow = (style && style->EndArrowCombo())
                                  ? static_cast<ArrowStyle>(style->EndArrowCombo()->currentData().toInt())
                                  : before.curveEndArrow;
    }

    if (m_opacitySpin) {
        const int percent = m_opacitySpin->value();
        after.opacity = percent == m_initialOpacityPercent ? m_initialOpacity : percent / 100.0;
    }

    if (before == after) {
        return;
    }

    auto* command = new ItemPropertiesCommand(m_scene, m_item, before, after, tr("Edit item properties"));
    if (auto* undo = m_scene->UndoStack()) {
        undo->push(command);
    } else {
        command->redo();
        delete command;
    }
}

QString ItemPropertiesDialog::LayerDescription(const CanvasScene* scene, int layerId)
{
    if (layerId == CanvasScene::BackgroundLayerId) {
        return tr("Background layer");
    }

    const int index = scene ? scene->LayerIndex(layerId) : layerId;
    QString layerName;
    if (scene) {
        for (const CanvasLayer& layer : scene->Layers()) {
            if (layer.id == layerId) {
                layerName = layer.name;
                break;
            }
        }
    }
    const QString defaultName = tr("Layer %1").arg(index);
    if (layerName.isEmpty() || layerName == defaultName) {
        return defaultName;
    }
    return tr("Layer %1 · %2").arg(index).arg(layerName);
}
