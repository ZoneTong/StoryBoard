#include "ItemPropertiesDialog.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "ImageItem.h"
#include "ItemPropertiesDialogGeometryBlock.h"
#include "ItemPropertiesDialogIdentityBlock.h"
#include "ItemPropertiesDialogImageBlock.h"
#include "ItemPropertiesDialogStyleBlock.h"
#include "ItemPropertiesDialogTextBlock.h"
#include "ShapeItem.h"
#include "StrokeItem.h"
#include "TextItem.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
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

    auto* heading = new QLabel(item ? item->name() : tr("No item selected"), this);
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
        m_initialRotation = item->rotationAngle();
        m_initialItemName = item->rawName();
        m_initialDisplayName = item->name();
        auto* typeValue = new QLabel(item->typeDisplayName(), this);
        typeValue->setObjectName(QStringLiteral("ItemPropertiesTypeValue"));
        typeValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
        form->addRow(tr("Item type"), typeValue);

        m_identityBlock = new ItemPropertiesDialogIdentityBlock(scene, item, this);
        m_identityBlock->addRows(form);

        if (dynamic_cast<ShapeItem*>(item) || dynamic_cast<StrokeItem*>(item)) {
            m_styleBlock = new ItemPropertiesDialogStyleBlock(item, this);
            m_styleBlock->addRows(form);
            m_hasFill = m_styleBlock->hasFill();
            m_initialStrokeWidth = m_styleBlock->initialStrokeWidth();
        } else if (dynamic_cast<ImageItem*>(item)) {
            m_imageBlock = new ItemPropertiesDialogImageBlock(item, this);
            m_imageBlock->addRows(form);
            connect(m_imageBlock, &ItemPropertiesDialogImageBlock::chooseImageRequested, this,
                    &ItemPropertiesDialog::chooseImage);
        } else if (dynamic_cast<TextItem*>(item)) {
            m_textBlock = new ItemPropertiesDialogTextBlock(item, this);
            m_textBlock->addRows(form);
            connect(m_textBlock, &ItemPropertiesDialogTextBlock::fontChanged, this,
                    &ItemPropertiesDialog::syncTextNaturalSize);
        }

        m_geometryBlock = new ItemPropertiesDialogGeometryBlock(item, this);
        m_geometryBlock->addRows(form);

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
                << m_styleBlock->strokeColorButton() << m_styleBlock->strokeWidthSpin();
    }
    if (m_textBlock) {
        widgets << m_textBlock->textColorButton() << m_textBlock->fontButton() << m_textBlock->textEdit();
    }
    if (m_imageBlock) {
        widgets << m_imageBlock->imageNameEdit() << m_imageBlock->imageButton();
    }
    if (m_geometryBlock) {
        widgets << m_geometryBlock->positionXSpin() << m_geometryBlock->positionYSpin() << m_geometryBlock->widthSpin()
                << m_geometryBlock->heightSizeSpin() << m_geometryBlock->rotationSpin();
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
    if (auto* text = dynamic_cast<TextItem*>(m_item.data())) {
        Q_UNUSED(text);
        const QSizeF natural = TextItem::measure(m_textBlock->textEdit()->toPlainText(), m_textBlock->textFont());
        if (m_geometryBlock) {
            m_geometryBlock->setNaturalSize(natural);
        }
    }
}

void ItemPropertiesDialog::chooseImage()
{
    if (!m_editable) {
        return;
    }

    const QString startDirectory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString path =
        QFileDialog::getOpenFileName(this, tr("Replace image"), startDirectory,
                                     tr("Image files (*.png *.jpg *.jpeg *.bmp *.gif *.webp);;All files (*.*)"));
    if (path.isEmpty()) {
        return;
    }

    QString error;
    if (!loadImageFromFile(path, &error)) {
        QMessageBox::warning(this, tr("Replace image"), tr("Could not read the image:\n%1").arg(error));
    }
}

bool ItemPropertiesDialog::loadImageFromFile(const QString& path, QString* errorMessage)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        if (errorMessage) {
            *errorMessage = reader.errorString();
        }
        return false;
    }

    m_newImage = QPixmap::fromImage(image);
    m_imageChanged = true;
    if (m_geometryBlock && m_geometryBlock->widthSpin() && m_geometryBlock->heightSizeSpin()) {
        m_geometryBlock->widthSpin()->setValue(m_newImage.width());
        m_geometryBlock->heightSizeSpin()->setValue(m_newImage.height());
    }
    return true;
}

void ItemPropertiesDialog::setReplacementImage(const QPixmap& image)
{
    m_newImage = image;
    m_imageChanged = !image.isNull();
    if (m_imageChanged && m_geometryBlock && m_geometryBlock->widthSpin() && m_geometryBlock->heightSizeSpin()) {
        m_geometryBlock->widthSpin()->setValue(image.width());
        m_geometryBlock->heightSizeSpin()->setValue(image.height());
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
    auto* image = m_imageBlock;
    auto* geometry = m_geometryBlock;

    const int oldLayer = m_item->layerId();
    const int oldHeight = m_item->height();
    const int targetLayer =
        (identity && identity->layerCombo()) ? identity->layerCombo()->currentData().toInt() : oldLayer;
    const int targetHeight = (identity && identity->heightSpin()) ? identity->heightSpin()->value() : oldHeight;

    QList<CanvasItem*> swapped;
    if (targetLayer != oldLayer || targetHeight != oldHeight) {
        for (CanvasItem* peer : m_scene->layerItems(targetLayer)) {
            if (peer != m_item && peer->height() == targetHeight) {
                swapped.append(peer);
            }
        }
    }

    ItemPropertiesState before;
    before.pos = m_item->pos();
    before.size = m_item->contentSize();
    before.rotation = m_initialRotation;
    before.itemName = m_initialItemName;
    before.stack.append({m_item, oldLayer, oldHeight});
    for (CanvasItem* peer : std::as_const(swapped)) {
        before.stack.append({peer, peer->layerId(), peer->height()});
    }

    ItemPropertiesState after;
    auto spinValue = [](QDoubleSpinBox* spin, qreal initial) {
        if (!spin) {
            return initial;
        }
        const qreal value = spin->value();
        // 数值框只显示 1 位小数；用户没动过时保留原始精度，避免“打开就点确定”也产生修改。
        return qFuzzyCompare(value, qRound(initial * 10.0) / 10.0) ? initial : value;
    };
    after.pos = QPointF(spinValue(geometry->positionXSpin(), m_initialPos.x()),
                        spinValue(geometry->positionYSpin(), m_initialPos.y()));
    after.size = QSizeF(spinValue(geometry->widthSpin(), m_initialSize.width()),
                        spinValue(geometry->heightSizeSpin(), m_initialSize.height()));
    after.rotation = spinValue(geometry->rotationSpin(), m_initialRotation);
    const QString typedName = (identity && identity->nameEdit()) ? identity->nameEdit()->text().trimmed() : QString();
    // 清空名称不能退回“多个同名类型名”的状态；这时补一个本画布内唯一的自动名称。
    const QString effectiveName =
        typedName.isEmpty() ? m_scene->uniqueItemName(m_item->typeDisplayName(), m_item) : typedName;
    after.itemName = (m_initialItemName.isEmpty() && typedName == m_initialDisplayName) ? QString() : effectiveName;
    after.stack.append({m_item, targetLayer, targetHeight});
    for (CanvasItem* peer : std::as_const(swapped)) {
        after.stack.append({peer, oldLayer, oldHeight});
    }

    if (auto* shape = dynamic_cast<ShapeItem*>(m_item.data())) {
        before.hasShapeState = true;
        before.shapeState = shape->state();

        after.hasShapeState = true;
        after.shapeState = shape->state();
        after.shapeState.pos = after.pos;
        after.shapeState.size = after.size;
        if (style && style->strokeColorButton()) {
            after.shapeState.strokeColor = style->strokeColorButton()->property("itemColor").value<QColor>();
        }
        if (style && style->strokeWidthSpin()) {
            after.shapeState.strokeWidth = spinValue(style->strokeWidthSpin(), m_initialStrokeWidth);
        }
        if (m_hasFill) {
            if (style && style->fillStyleCombo() && style->fillStyleCombo()->currentData().toInt() >= 0) {
                after.shapeState.fillStyle = static_cast<FillStyle>(style->fillStyleCombo()->currentData().toInt());
            }
            if (style && style->fillColorButton()) {
                after.shapeState.fillColor = style->fillColorButton()->property("itemColor").value<QColor>();
            }
        }
    } else if (auto* textItem = dynamic_cast<TextItem*>(m_item.data())) {
        before.hasTextState = true;
        before.textState = textItem->state();

        after.hasTextState = true;
        after.textState = textItem->state();
        after.textState.pos = after.pos;
        after.textState.size = after.size;
        after.textState.font = text ? text->textFont() : QFont();
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
    } else if (auto* stroke = dynamic_cast<StrokeItem*>(m_item.data())) {
        before.hasStrokeStyle = true;
        before.strokeColor = stroke->strokeColor();
        before.strokeWidth = stroke->strokeWidth();

        after.hasStrokeStyle = true;
        after.strokeColor = (style && style->strokeColorButton())
                                ? style->strokeColorButton()->property("itemColor").value<QColor>()
                                : before.strokeColor;
        after.strokeWidth = (style && style->strokeWidthSpin())
                                ? spinValue(style->strokeWidthSpin(), m_initialStrokeWidth)
                                : before.strokeWidth;
    } else if (auto* imageItem = dynamic_cast<ImageItem*>(m_item.data())) {
        before.hasImageState = true;
        before.image = imageItem->image();
        before.hasImageName = true;
        before.imageName = imageItem->imageName();

        after.hasImageState = true;
        after.image = m_imageChanged ? m_newImage : before.image;
        // 需求追加：替换图片本体后直接使用新图片的原始像素尺寸。
        if (m_imageChanged && !after.image.isNull()) {
            after.size = QSizeF(after.image.size());
        }
        after.hasImageName = true;
        after.imageName = (image && image->imageNameEdit()) ? image->imageNameEdit()->text() : before.imageName;
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
