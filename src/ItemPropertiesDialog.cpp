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
        scene->assignAutoItemNames();
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
        m_initialSize = item->contentSize();
        m_initialRotation = item->RotationAngle();
        m_initialItemName = item->CustomName();
        m_initialDisplayName = item->Name();
        auto* typeValue = new QLabel(item->TypeDisplayName(), this);
        typeValue->setObjectName(QStringLiteral("ItemPropertiesTypeValue"));
        typeValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
        form->addRow(tr("Item type"), typeValue);

        m_identityBlock = new ItemPropertiesDialogIdentityBlock(scene, item, this);
        m_identityBlock->addRows(form);

        if (dynamic_cast<ShapeItem*>(item) || dynamic_cast<LineItemAbstract*>(item)) {
            m_styleBlock = new ItemPropertiesDialogStyleBlock(item, this);
            m_styleBlock->addRows(form);
            m_hasFill = m_styleBlock->hasFill();
            m_initialStrokeWidth = m_styleBlock->initialStrokeWidth();
        } else if (item->HasTextFacet()) {
            m_textBlock = new ItemPropertiesDialogTextBlock(item, this);
            m_textBlock->addRows(form);
            connect(m_textBlock, &ItemPropertiesDialogTextBlock::fontChanged, this,
                    &ItemPropertiesDialog::syncTextNaturalSize);
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
        m_geometryBlock->addRows(form);
        if (m_styleBlock && m_styleBlock->cornerRadiusSpin()) {
            auto syncCornerRange = [this]() {
                if (!m_geometryBlock) {
                    return;
                }
                m_styleBlock->SetContentSize(
                    QSizeF(m_geometryBlock->widthSpin()->value(), m_geometryBlock->heightSizeSpin()->value()));
            };
            connect(m_geometryBlock->widthSpin(), &QDoubleSpinBox::valueChanged, this, syncCornerRange);
            connect(m_geometryBlock->heightSizeSpin(), &QDoubleSpinBox::valueChanged, this, syncCornerRange);
        }

        root->addLayout(form);
    }

    // 文字正文直接在属性窗里编辑；内容变化时同步到自然尺寸，确定后由
    // ItemPropertiesCommand 一次性提交文字、尺寸和撤销记录。
    if (m_textBlock && m_textBlock->textEdit()) {
        connect(m_textBlock->textEdit(), &QPlainTextEdit::textChanged, this,
                &ItemPropertiesDialog::syncTextNaturalSize);
    }

    applyEditableState();

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

    if (m_identityBlock && m_identityBlock->hasLayerControls()) {
        m_identityBlock->updateHeightRange(false);
        connect(m_identityBlock, &ItemPropertiesDialogIdentityBlock::layerChanged, this, [this]() {
            if (m_initializing) {
                return;
            }
            m_identityBlock->updateHeightRange(true);
        });
        m_initializing = false;
        m_identityBlock->setInitialized(false);
    } else {
        m_initializing = false;
    }
}

void ItemPropertiesDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    // 只做一次，且只读模式不抢焦点；光标停在正文末尾，不整段选中。
    if (m_textFocusApplied || !m_editable || !m_textBlock || !m_textBlock->textEdit()) {
        return;
    }
    m_textFocusApplied = true;
    QPlainTextEdit* edit = m_textBlock->textEdit();
    edit->moveCursor(QTextCursor::End);
    edit->setFocus(Qt::OtherFocusReason);
}

void ItemPropertiesDialog::applyEditableState()
{
    if (m_editable) {
        return;
    }

    QList<QWidget*> widgets;
    if (m_identityBlock) {
        widgets << m_identityBlock->nameEdit() << m_identityBlock->layerCombo() << m_identityBlock->heightSpin();
    }
    if (m_styleBlock) {
        widgets << m_styleBlock->fillStyleCombo() << m_styleBlock->fillColorButton()
                << m_styleBlock->strokeColorButton() << m_styleBlock->borderStyleCombo()
                << m_styleBlock->strokeWidthSpin() << m_styleBlock->opacitySpin() << m_styleBlock->cornerRadiusSpin()
                << m_styleBlock->startAngleSpin() << m_styleBlock->sweepAngleSpin();
    }
    if (m_opacitySpin) {
        widgets << m_opacitySpin;
    }
    if (m_textBlock) {
        widgets << m_textBlock->textColorButton() << m_textBlock->fontButton() << m_textBlock->textEdit();
    }
    if (m_geometryBlock) {
        widgets << m_geometryBlock->positionXSpin() << m_geometryBlock->positionYSpin() << m_geometryBlock->widthSpin()
                << m_geometryBlock->heightSizeSpin() << m_geometryBlock->majorAxisSpin()
                << m_geometryBlock->minorAxisSpin() << m_geometryBlock->rotationSpin();
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
    if (m_identityBlock && m_identityBlock->nameEdit() && m_scene && m_item) {
        const QString typedName = m_identityBlock->nameEdit()->text().trimmed();
        if (!typedName.isEmpty() && !m_scene->isItemNameAvailable(typedName, m_item)) {
            QMessageBox::warning(this, tr("Duplicate item name"),
                                 tr("Another item in this canvas already uses this name. Choose a different name."));
            m_identityBlock->nameEdit()->setFocus();
            m_identityBlock->nameEdit()->selectAll();
            return;
        }
    }
    applyChanges();
    QDialog::accept();
}

void ItemPropertiesDialog::syncTextNaturalSize()
{
    if (!m_editable || !m_textBlock || !m_textBlock->textEdit()) {
        return;
    }
    if (m_item->HasTextFacet()) {
        const QSizeF natural = TextItem::measure(m_textBlock->textEdit()->toPlainText(), m_textBlock->TextFont());
        if (m_geometryBlock) {
            m_geometryBlock->setNaturalSize(natural);
        }
    }
}

bool ItemPropertiesDialog::isItemInScene() const
{
    return m_scene && m_item && m_item->scene() == m_scene;
}

void ItemPropertiesDialog::applyChanges()
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
        (identity && identity->layerCombo()) ? identity->layerCombo()->currentData().toInt() : oldLayer;
    const int targetHeight = (identity && identity->heightSpin()) ? identity->heightSpin()->value() : oldHeight;

    QList<CanvasItem*> swapped;
    if (targetLayer != oldLayer || targetHeight != oldHeight) {
        for (CanvasItem* peer : m_scene->layerItems(targetLayer)) {
            if (peer != m_item && peer->ZOrder() == targetHeight) {
                swapped.append(peer);
            }
        }
    }

    ItemPropertiesState before;
    before.pos = m_item->pos();
    before.size = m_item->contentSize();
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
    after.pos = QPointF(spinValue(geometry->positionXSpin(), m_initialPos.x()),
                        spinValue(geometry->positionYSpin(), m_initialPos.y()));
    after.size = QSizeF(spinValue(geometry->widthSpin(), m_initialSize.width()),
                        spinValue(geometry->heightSizeSpin(), m_initialSize.height()));
    after.rotation = spinValue(geometry->rotationSpin(), m_initialRotation);
    after.opacity = before.opacity;
    const QString typedName = (identity && identity->nameEdit()) ? identity->nameEdit()->text().trimmed() : QString();
    // 清空名称不能退回“多个同名类型名”的状态；这时补一个本画布内唯一的自动名称。
    const QString effectiveName =
        typedName.isEmpty() ? m_scene->uniqueItemName(m_item->TypeDisplayName(), m_item) : typedName;
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
        if (style && style->strokeColorButton()) {
            after.shapeState.strokeColor = style->strokeColorButton()->property("itemColor").value<QColor>();
        }
        if (style && style->strokeWidthSpin()) {
            after.shapeState.strokeWidth = spinValue(style->strokeWidthSpin(), m_initialStrokeWidth);
        }
        if (style && style->borderStyleCombo() && style->borderStyleCombo()->currentData().toInt() >= 0) {
            after.shapeState.strokeStyle = static_cast<Qt::PenStyle>(style->borderStyleCombo()->currentData().toInt());
        }
        if (style && style->opacitySpin()) {
            const int percent = style->opacitySpin()->value();
            after.shapeState.opacity =
                percent == style->initialOpacityPercent() ? style->initialOpacity() : percent / 100.0;
            after.opacity = after.shapeState.opacity;
        }
        if (style && style->startAngleSpin()) {
            const int value = style->startAngleSpin()->value();
            after.shapeState.startAngle =
                value == qRound(style->initialStartAngle()) ? style->initialStartAngle() : value;
        }
        if (style && style->sweepAngleSpin()) {
            const int value = style->sweepAngleSpin()->value();
            after.shapeState.sweepAngle =
                value == qRound(style->initialSweepAngle()) ? style->initialSweepAngle() : value;
        }
        if (style && style->cornerRadiusSpin()) {
            const int value = style->cornerRadiusSpin()->value();
            // 圆角矩形默认值来自短边的 16%，对话框里显示为非 0；
            // 未改动时保留“按类型取默认”的 -1，避免打开属性窗点确定就写回显式值。
            after.shapeState.cornerRadius =
                value == style->initialCornerRadius() ? style->initialStoredCornerRadius() : value;
        }
        // 需求 1：三角形的两个底角为真相，未改动时保留原值（含非整数角度）。
        if (style && style->bottomAngle1Spin()) {
            const int value = style->bottomAngle1Spin()->value();
            after.shapeState.bottomAngle1 =
                value == qRound(style->initialBottomAngle1()) ? style->initialBottomAngle1() : value;
        }
        if (style && style->bottomAngle2Spin()) {
            const int value = style->bottomAngle2Spin()->value();
            after.shapeState.bottomAngle2 =
                value == qRound(style->initialBottomAngle2()) ? style->initialBottomAngle2() : value;
        }
        // 需求 1：底边长度独立字段，写入内容矩形宽度（高度由角度派生）。
        if (style && style->triangleBaseSpin()) {
            const int value = style->triangleBaseSpin()->value();
            if (value != style->initialTriangleBase()) {
                after.size.setWidth(qMax<qreal>(8.0, static_cast<qreal>(value)));
            }
        }
        // 需求 9：平行四边形左下内角，未改动时保留原值。
        if (style && style->parallelogramAngleSpin()) {
            const int value = style->parallelogramAngleSpin()->value();
            after.shapeState.parallelogramAngle =
                value == qRound(style->initialParallelogramAngle()) ? style->initialParallelogramAngle() : value;
        }
        if (m_hasFill) {
            if (style && style->fillStyleCombo() && style->fillStyleCombo()->currentData().toInt() >= 0) {
                after.shapeState.fillStyle = static_cast<FillStyle>(style->fillStyleCombo()->currentData().toInt());
            }
            if (style && style->fillColorButton()) {
                after.shapeState.fillColor = style->fillColorButton()->property("itemColor").value<QColor>();
            }
            // 需求 15：图片填充是独立属性，ShapeItemState 直接快照。
            if (style && style->hasFillImageRow()) {
                after.shapeState.hasFillImage = style->fillImagePresent();
                after.shapeState.fillImage = style->FillImage();
                after.shapeState.fillImageMode = style->FillImageMode();
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
        if (text && text->textEdit()) {
            after.textState.text = text->textEdit()->toPlainText();
        }
        after.textState.baseSize = TextItem::measure(after.textState.text, after.textState.font);
        // 需求追加：文字正文变化后直接回到文字自然尺寸，不再沿用旧图元的
        // 拉伸尺寸，避免新旧字形按不同比例叠在同一块内容区里。
        if (after.textState.text != before.textState.text) {
            after.size = after.textState.baseSize;
            after.textState.size = after.textState.baseSize;
        }
        if (text && text->textColorButton()) {
            after.textState.color = text->textColorButton()->property("itemColor").value<QColor>();
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
        after.strokeColor = (style && style->strokeColorButton())
                                ? style->strokeColorButton()->property("itemColor").value<QColor>()
                                : before.strokeColor;
        after.strokeWidth = (style && style->strokeWidthSpin())
                                ? spinValue(style->strokeWidthSpin(), m_initialStrokeWidth)
                                : before.strokeWidth;
        after.strokeStyle = (style && style->borderStyleCombo())
                                ? static_cast<Qt::PenStyle>(style->borderStyleCombo()->currentData().toInt())
                                : before.strokeStyle;
        after.curveStartArrow = (style && style->startArrowCombo())
                                    ? static_cast<ArrowStyle>(style->startArrowCombo()->currentData().toInt())
                                    : before.curveStartArrow;
        after.curveEndArrow = (style && style->endArrowCombo())
                                  ? static_cast<ArrowStyle>(style->endArrowCombo()->currentData().toInt())
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
    if (auto* undo = m_scene->undoStack()) {
        undo->push(command);
    } else {
        command->redo();
        delete command;
    }
}

QString ItemPropertiesDialog::layerDescription(const CanvasScene* scene, int layerId)
{
    if (layerId == CanvasScene::BackgroundLayerId) {
        return tr("Background layer");
    }

    const int index = scene ? scene->layerIndex(layerId) : layerId;
    QString layerName;
    if (scene) {
        for (const CanvasLayer& layer : scene->layers()) {
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
