#include "ItemPropertiesDialog.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "I18n.h"
#include "IconFactory.h"
#include "ImageItem.h"
#include "ShapeItem.h"
#include "StrokeItem.h"
#include "TextItem.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFontDialog>
#include <QFontInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
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

namespace {

QString number(qreal value)
{
    return QString::number(value, 'f', 1);
}

QString colorText(const QColor& color)
{
    return color.isValid() ? color.name(QColor::HexArgb).toUpper() : QStringLiteral("—");
}

QString fontText(const QFont& font)
{
    const QFontInfo info(font);
    const QString size = font.pointSizeF() > 0 ? ItemPropertiesDialog::tr("%1 pt").arg(number(font.pointSizeF()))
                                               : ItemPropertiesDialog::tr("%1 px").arg(font.pixelSize());
    return ItemPropertiesDialog::tr("%1 · %2").arg(info.family(), size);
}

void refreshColorButton(QToolButton* button)
{
    const QColor color = button->property("itemColor").value<QColor>();
    button->setIcon(makeColorIcon(color.isValid() ? color : QColor(Qt::transparent), 18));
    button->setText(colorText(color));
    button->setToolTip(colorText(color));
}

QToolButton* makeColorField(QWidget* parent, const QColor& color, const QString& objectName)
{
    auto* button = new QToolButton(parent);
    button->setObjectName(objectName);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setMinimumWidth(118);
    button->setProperty("itemColor", color);
    refreshColorButton(button);

    QObject::connect(button, &QToolButton::clicked, button, [button]() {
        const QColor initial = button->property("itemColor").value<QColor>();
        const QColor chosen = QColorDialog::getColor(initial, button, ItemPropertiesDialog::tr("Choose color"),
                                                     QColorDialog::ShowAlphaChannel);
        if (!chosen.isValid()) {
            return;
        }
        button->setProperty("itemColor", chosen);
        refreshColorButton(button);
    });
    return button;
}

QDoubleSpinBox* makeSpin(QWidget* parent, const QString& objectName, qreal minimum, qreal maximum)
{
    auto* spin = new QDoubleSpinBox(parent);
    spin->setObjectName(objectName);
    spin->setDecimals(1);
    spin->setRange(minimum, maximum);
    spin->setSingleStep(1.0);
    spin->setKeyboardTracking(false);
    spin->setMinimumWidth(88);
    return spin;
}

QWidget* makePairField(QWidget* parent, const QString& objectName, QDoubleSpinBox* first, QDoubleSpinBox* second,
                       const QString& separator)
{
    auto* field = new QWidget(parent);
    field->setObjectName(objectName);
    auto* layout = new QHBoxLayout(field);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(first);
    auto* label = new QLabel(separator, field);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);
    layout->addWidget(second);
    layout->addStretch(1);
    return field;
}

} // namespace

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
    heading->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: 600;"));
    root->addWidget(heading);

    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    root->addWidget(line);

    if (!m_editable) {
        auto* readOnlyHint = new QLabel(tr("Read-only: switch to Edit mode to change properties."), this);
        readOnlyHint->setObjectName(QStringLiteral("ItemPropertiesReadOnlyHint"));
        readOnlyHint->setWordWrap(true);
        readOnlyHint->setStyleSheet(QStringLiteral("color: #6B7280;"));
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

        // 需求 30：图元名称可编辑；空名称时显示并回退为类型名称。
        m_nameEdit = new QLineEdit(this);
        m_nameEdit->setObjectName(QStringLiteral("ItemPropertiesNameValue"));
        m_nameEdit->setText(m_initialDisplayName);
        m_nameEdit->setPlaceholderText(item->typeDisplayName());
        form->addRow(tr("Item name"), m_nameEdit);

        m_layerCombo = new QComboBox(this);
        m_layerCombo->setObjectName(QStringLiteral("ItemPropertiesLayerValue"));
        m_layerCombo->setMinimumWidth(220);
        if (scene) {
            for (const CanvasLayer& layer : scene->userLayers()) {
                m_layerCombo->addItem(layerDescription(scene, layer.id), layer.id);
            }
        }
        const int layerIndex = m_layerCombo->findData(item->layerId());
        if (layerIndex >= 0) {
            m_layerCombo->setCurrentIndex(layerIndex);
        }
        form->addRow(tr("Layer"), m_layerCombo);

        m_heightSpin = new QSpinBox(this);
        m_heightSpin->setObjectName(QStringLiteral("ItemPropertiesHeightValue"));
        m_heightSpin->setRange(0, 999999);
        m_heightSpin->setKeyboardTracking(false);
        m_heightSpin->setValue(qMax(0, item->height()));
        form->addRow(tr("Height"), m_heightSpin);

        if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
            const bool filled = shapeTypeIsFilled(shape->shapeType());
            m_hasFill = filled;

            m_fillStyleCombo = new QComboBox(this);
            m_fillStyleCombo->setObjectName(QStringLiteral("ItemPropertiesFillStyleValue"));
            m_fillStyleCombo->setMinimumWidth(150);
            if (filled) {
                for (FillStyle style : allFillStyles()) {
                    m_fillStyleCombo->addItem(fillStyleName(style), static_cast<int>(style));
                }
                const int index = m_fillStyleCombo->findData(static_cast<int>(shape->fillStyle()));
                if (index >= 0) {
                    m_fillStyleCombo->setCurrentIndex(index);
                }
            } else {
                m_fillStyleCombo->addItem(tr("Not applicable"), -1);
                m_fillStyleCombo->setEnabled(false);
            }
            form->addRow(tr("Fill style"), m_fillStyleCombo);

            m_fillColorButton =
                makeColorField(this, shape->fillColor(), QStringLiteral("ItemPropertiesFillColorValue"));
            m_fillColorButton->setEnabled(filled);
            form->addRow(tr("Fill color"), m_fillColorButton);

            m_strokeColorButton =
                makeColorField(this, shape->strokeColor(), QStringLiteral("ItemPropertiesStrokeColorValue"));
            form->addRow(tr("Stroke color"), m_strokeColorButton);

            m_strokeWidthSpin = new QDoubleSpinBox(this);
            m_strokeWidthSpin->setObjectName(QStringLiteral("ItemPropertiesStrokeWidthValue"));
            m_strokeWidthSpin->setDecimals(1);
            m_strokeWidthSpin->setRange(0.5, 64.0);
            m_strokeWidthSpin->setSingleStep(0.5);
            m_strokeWidthSpin->setValue(shape->strokeWidth());
            m_initialStrokeWidth = shape->strokeWidth();
            m_strokeWidthSpin->setMinimumWidth(88);
            form->addRow(tr("Stroke width"), m_strokeWidthSpin);
        } else if (auto* stroke = dynamic_cast<StrokeItem*>(item)) {
            m_strokeColorButton =
                makeColorField(this, stroke->strokeColor(), QStringLiteral("ItemPropertiesStrokeColorValue"));
            form->addRow(tr("Stroke color"), m_strokeColorButton);

            m_strokeWidthSpin = new QDoubleSpinBox(this);
            m_strokeWidthSpin->setObjectName(QStringLiteral("ItemPropertiesStrokeWidthValue"));
            m_strokeWidthSpin->setDecimals(1);
            m_strokeWidthSpin->setRange(0.5, 64.0);
            m_strokeWidthSpin->setSingleStep(0.5);
            m_strokeWidthSpin->setValue(stroke->strokeWidth());
            m_initialStrokeWidth = stroke->strokeWidth();
            m_strokeWidthSpin->setMinimumWidth(88);
            form->addRow(tr("Stroke width"), m_strokeWidthSpin);
        } else if (auto* image = dynamic_cast<ImageItem*>(item)) {
            m_imageNameEdit = new QLineEdit(this);
            m_imageNameEdit->setObjectName(QStringLiteral("ItemPropertiesImageValue"));
            m_imageNameEdit->setText(image->imageName());
            m_imageNameEdit->setPlaceholderText(tr("Image"));
            form->addRow(tr("Image name"), m_imageNameEdit);

            // 需求 39：图片本体也算可编辑属性，允许在属性窗内直接替换图片。
            m_imageButton = new QPushButton(tr("Replace image…"), this);
            m_imageButton->setObjectName(QStringLiteral("ItemPropertiesReplaceImageValue"));
            connect(m_imageButton, &QPushButton::clicked, this, &ItemPropertiesDialog::chooseImage);
            form->addRow(tr("Image"), m_imageButton);
        } else if (auto* text = dynamic_cast<TextItem*>(item)) {
            m_textFont = text->textFont();

            // 需求 39：文字正文也是可编辑属性，直接在属性窗里改，不必先离开再开文字对话框。
            m_textEdit = new QPlainTextEdit(this);
            m_textEdit->setObjectName(QStringLiteral("ItemPropertiesTextValue"));
            m_textEdit->setPlainText(text->text());
            m_textEdit->setMinimumHeight(88);
            form->addRow(tr("Text content"), m_textEdit);

            m_textColorButton = makeColorField(this, text->textColor(), QStringLiteral("ItemPropertiesTextColorValue"));
            form->addRow(tr("Text color"), m_textColorButton);

            m_fontButton = new QPushButton(this);
            m_fontButton->setObjectName(QStringLiteral("ItemPropertiesFontValue"));
            m_fontButton->setMinimumWidth(180);
            updateFontButton();
            form->addRow(tr("Font"), m_fontButton);
            connect(m_fontButton, &QPushButton::clicked, this, &ItemPropertiesDialog::chooseFont);
        }

        m_positionXSpin = makeSpin(this, QStringLiteral("ItemPropertiesPositionXValue"), -1000000.0, 1000000.0);
        m_positionYSpin = makeSpin(this, QStringLiteral("ItemPropertiesPositionYValue"), -1000000.0, 1000000.0);
        m_positionXSpin->setValue(item->pos().x());
        m_positionYSpin->setValue(item->pos().y());
        form->addRow(tr("Position"), makePairField(this, QStringLiteral("ItemPropertiesPositionValue"), m_positionXSpin,
                                                   m_positionYSpin, QStringLiteral(",")));

        m_widthSpin = makeSpin(this, QStringLiteral("ItemPropertiesWidthValue"), 1.0, 1000000.0);
        m_heightSizeSpin = makeSpin(this, QStringLiteral("ItemPropertiesHeightSizeValue"), 1.0, 1000000.0);
        m_widthSpin->setValue(item->contentSize().width());
        m_heightSizeSpin->setValue(item->contentSize().height());
        form->addRow(tr("Item size"), makePairField(this, QStringLiteral("ItemPropertiesSizeValue"), m_widthSpin,
                                                    m_heightSizeSpin, QStringLiteral("×")));

        m_rotationSpin = makeSpin(this, QStringLiteral("ItemPropertiesRotationValue"), -360.0, 360.0);
        m_rotationSpin->setSingleStep(5.0);
        m_rotationSpin->setSuffix(QStringLiteral("°"));
        m_rotationSpin->setValue(item->rotationAngle());
        form->addRow(tr("Rotation"), m_rotationSpin);

        root->addLayout(form);
    }

    // 文字正文直接在属性窗里编辑；内容变化时同步到自然尺寸，确定后由
    // ItemPropertiesCommand 一次性提交文字、尺寸和撤销记录。
    if (m_textEdit) {
        connect(m_textEdit, &QPlainTextEdit::textChanged, this, &ItemPropertiesDialog::syncTextNaturalSize);
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

    if (m_layerCombo && m_heightSpin) {
        updateHeightRange(false);
        connect(m_layerCombo, &QComboBox::currentIndexChanged, this, [this](int) {
            if (m_initializing) {
                return;
            }
            updateHeightRange(true);
        });
        m_initializing = false;
    } else {
        m_initializing = false;
    }
}

void ItemPropertiesDialog::applyEditableState()
{
    if (m_editable) {
        return;
    }

    for (QWidget* widget :
         {static_cast<QWidget*>(m_nameEdit), static_cast<QWidget*>(m_layerCombo), static_cast<QWidget*>(m_heightSpin),
          static_cast<QWidget*>(m_fillStyleCombo), static_cast<QWidget*>(m_fillColorButton),
          static_cast<QWidget*>(m_strokeColorButton), static_cast<QWidget*>(m_strokeWidthSpin),
          static_cast<QWidget*>(m_positionXSpin), static_cast<QWidget*>(m_positionYSpin),
          static_cast<QWidget*>(m_widthSpin), static_cast<QWidget*>(m_heightSizeSpin),
          static_cast<QWidget*>(m_rotationSpin), static_cast<QWidget*>(m_textColorButton),
          static_cast<QWidget*>(m_fontButton), static_cast<QWidget*>(m_textEdit),
          static_cast<QWidget*>(m_imageNameEdit), static_cast<QWidget*>(m_imageButton)}) {
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
    if (m_nameEdit && m_scene && m_item) {
        const QString typedName = m_nameEdit->text().trimmed();
        if (!typedName.isEmpty() && !m_scene->isItemNameAvailable(typedName, m_item)) {
            QMessageBox::warning(this, tr("Duplicate item name"),
                                 tr("Another item in this canvas already uses this name. Choose a different name."));
            m_nameEdit->setFocus();
            m_nameEdit->selectAll();
            return;
        }
    }
    applyChanges();
    QDialog::accept();
}

void ItemPropertiesDialog::updateHeightRange(bool selectTop)
{
    if (!m_layerCombo || !m_heightSpin || !m_item) {
        return;
    }

    const int targetLayer = m_layerCombo->currentData().toInt();
    int maxHeight = 0;
    if (m_scene) {
        for (CanvasItem* peer : m_scene->layerItems(targetLayer)) {
            maxHeight = qMax(maxHeight, peer->height());
        }
    }
    // Height 0 is valid and deletion can leave gaps. Allow one slot above the
    // highest existing height, and never clamp the current value on open.
    maxHeight = qMax(0, maxHeight + 1);
    if (!selectTop) {
        maxHeight = qMax(maxHeight, m_item->height());
    }
    maxHeight = qBound(0, maxHeight, 999999);
    m_heightSpin->setMaximum(maxHeight);
    if (selectTop) {
        m_heightSpin->setValue(maxHeight);
    }
}

void ItemPropertiesDialog::updateFontButton()
{
    if (!m_fontButton) {
        return;
    }
    m_fontButton->setText(fontText(m_textFont));
}

void ItemPropertiesDialog::syncTextNaturalSize()
{
    if (!m_editable || !m_textEdit) {
        return;
    }
    if (auto* text = dynamic_cast<TextItem*>(m_item.data())) {
        Q_UNUSED(text);
        const QSizeF natural = TextItem::measure(m_textEdit->toPlainText(), m_textFont);
        if (m_widthSpin) {
            m_widthSpin->setValue(natural.width());
        }
        if (m_heightSizeSpin) {
            m_heightSizeSpin->setValue(natural.height());
        }
    }
}

void ItemPropertiesDialog::chooseFont()
{
    if (!m_editable) {
        return;
    }

    bool ok = false;
    // 使用 Qt 自绘字体窗，避免 Windows 原生字体对话框跟随系统 UI 语言而
    // 与应用内语言不一致；标准标签由嵌入的 qtbase_zh_CN.qm 提供中文。
    const QFont font = QFontDialog::getFont(&ok, m_textFont, this, tr("Choose font"), QFontDialog::DontUseNativeDialog);
    if (!ok) {
        return;
    }

    m_textFont = font;
    updateFontButton();
    syncTextNaturalSize();
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
    if (m_widthSpin && m_heightSizeSpin) {
        m_widthSpin->setValue(m_newImage.width());
        m_heightSizeSpin->setValue(m_newImage.height());
    }
    return true;
}

void ItemPropertiesDialog::setReplacementImage(const QPixmap& image)
{
    m_newImage = image;
    m_imageChanged = !image.isNull();
    if (m_imageChanged && m_widthSpin && m_heightSizeSpin) {
        m_widthSpin->setValue(image.width());
        m_heightSizeSpin->setValue(image.height());
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

    const int oldLayer = m_item->layerId();
    const int oldHeight = m_item->height();
    const int targetLayer = m_layerCombo ? m_layerCombo->currentData().toInt() : oldLayer;
    const int targetHeight = m_heightSpin ? m_heightSpin->value() : oldHeight;

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
    after.pos = QPointF(spinValue(m_positionXSpin, m_initialPos.x()), spinValue(m_positionYSpin, m_initialPos.y()));
    after.size =
        QSizeF(spinValue(m_widthSpin, m_initialSize.width()), spinValue(m_heightSizeSpin, m_initialSize.height()));
    after.rotation = spinValue(m_rotationSpin, m_initialRotation);
    const QString typedName = m_nameEdit ? m_nameEdit->text().trimmed() : QString();
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
        if (m_strokeColorButton) {
            after.shapeState.strokeColor = m_strokeColorButton->property("itemColor").value<QColor>();
        }
        if (m_strokeWidthSpin) {
            after.shapeState.strokeWidth = spinValue(m_strokeWidthSpin, m_initialStrokeWidth);
        }
        if (m_hasFill) {
            if (m_fillStyleCombo && m_fillStyleCombo->currentData().toInt() >= 0) {
                after.shapeState.fillStyle = static_cast<FillStyle>(m_fillStyleCombo->currentData().toInt());
            }
            if (m_fillColorButton) {
                after.shapeState.fillColor = m_fillColorButton->property("itemColor").value<QColor>();
            }
        }
    } else if (auto* text = dynamic_cast<TextItem*>(m_item.data())) {
        before.hasTextState = true;
        before.textState = text->state();

        after.hasTextState = true;
        after.textState = text->state();
        after.textState.pos = after.pos;
        after.textState.size = after.size;
        after.textState.font = m_textFont;
        if (m_textEdit) {
            after.textState.text = m_textEdit->toPlainText();
        }
        after.textState.baseSize = TextItem::measure(after.textState.text, m_textFont);
        // 需求追加：文字正文变化后直接回到文字自然尺寸，不再沿用旧图元的
        // 拉伸尺寸，避免新旧字形按不同比例叠在同一块内容区里。
        if (after.textState.text != before.textState.text) {
            after.size = after.textState.baseSize;
            after.textState.size = after.textState.baseSize;
        }
        if (m_textColorButton) {
            after.textState.color = m_textColorButton->property("itemColor").value<QColor>();
        }
    } else if (auto* stroke = dynamic_cast<StrokeItem*>(m_item.data())) {
        before.hasStrokeStyle = true;
        before.strokeColor = stroke->strokeColor();
        before.strokeWidth = stroke->strokeWidth();

        after.hasStrokeStyle = true;
        after.strokeColor =
            m_strokeColorButton ? m_strokeColorButton->property("itemColor").value<QColor>() : before.strokeColor;
        after.strokeWidth = m_strokeWidthSpin ? spinValue(m_strokeWidthSpin, m_initialStrokeWidth) : before.strokeWidth;
    } else if (auto* image = dynamic_cast<ImageItem*>(m_item.data())) {
        before.hasImageState = true;
        before.image = image->image();
        before.hasImageName = true;
        before.imageName = image->imageName();

        after.hasImageState = true;
        after.image = m_imageChanged ? m_newImage : before.image;
        // 需求追加：替换图片本体后直接使用新图片的原始像素尺寸。
        if (m_imageChanged && !after.image.isNull()) {
            after.size = QSizeF(after.image.size());
        }
        after.hasImageName = true;
        after.imageName = m_imageNameEdit ? m_imageNameEdit->text() : before.imageName;
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
