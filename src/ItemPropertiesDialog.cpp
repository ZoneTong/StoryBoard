#include "ItemPropertiesDialog.h"

#include "BoardItem.h"
#include "BoardScene.h"
#include "I18n.h"
#include "ImageItem.h"
#include "ShapeItem.h"
#include "StrokeItem.h"
#include "TextItem.h"

#include <QDialogButtonBox>
#include <QFontInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QString number(qreal value)
{
    return QString::number(value, 'f', 1);
}

QString colorText(const QColor& color)
{
    return color.isValid() ? color.name(QColor::HexArgb) : QStringLiteral("—");
}

QWidget* makeColorValue(const QColor& color)
{
    auto* field = new QWidget;
    auto* layout = new QHBoxLayout(field);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* swatch = new QLabel(field);
    QPixmap pixmap(16, 16);
    pixmap.fill(color.isValid() ? color : QColor(Qt::transparent));
    swatch->setPixmap(pixmap);
    swatch->setFixedSize(16, 16);
    swatch->setStyleSheet(QStringLiteral("border: 1px solid rgb(180, 186, 196);"));
    layout->addWidget(swatch);

    auto* text = new QLabel(colorText(color), field);
    layout->addWidget(text);
    layout->addStretch(1);
    return field;
}

} // namespace

ItemPropertiesDialog::ItemPropertiesDialog(BoardScene* scene, BoardItem* item, QWidget* parent)
    : QDialog(parent)
    , m_scene(scene)
    , m_item(item)
{
    setWindowTitle(uiText(QStringLiteral("Item properties")));
    setModal(true);
    setMinimumWidth(430);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 16);
    root->setSpacing(12);

    auto* heading = new QLabel(item ? item->displayName()
                                    : uiText(QStringLiteral("No item selected")), this);
    heading->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: 600;"));
    root->addWidget(heading);

    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    root->addWidget(line);

    auto* form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(9);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    if (!item) {
        addRow(form, uiText(QStringLiteral("Item type")), QStringLiteral("—"));
        root->addLayout(form);
    } else {
        addRow(form, uiText(QStringLiteral("Item type")), item->displayName());
        addRow(form, uiText(QStringLiteral("Layer")), layerDescription(scene, item->layerId()),
               QStringLiteral("ItemPropertiesLayerValue"));
        addRow(form, uiText(QStringLiteral("Height")), QString::number(item->height()),
               QStringLiteral("ItemPropertiesHeightValue"));

        if (const auto* shape = dynamic_cast<const ShapeItem*>(item)) {
            const bool filled = shapeTypeIsFilled(shape->shapeType());
            addRow(form, uiText(QStringLiteral("Fill style")),
                   filled ? fillStyleName(shape->fillStyle()) : uiText(QStringLiteral("Not applicable")),
                   QStringLiteral("ItemPropertiesFillStyleValue"));
            if (filled)
                addColorRow(form, uiText(QStringLiteral("Fill color")), shape->fillColor(),
                            QStringLiteral("ItemPropertiesFillColorValue"));
            else
                addRow(form, uiText(QStringLiteral("Fill color")),
                       uiText(QStringLiteral("Not applicable")),
                       QStringLiteral("ItemPropertiesFillColorValue"));
            addColorRow(form, uiText(QStringLiteral("Stroke color")), shape->strokeColor());
            addRow(form, uiText(QStringLiteral("Stroke width")),
                   uiText(QStringLiteral("%1 px")).arg(number(shape->strokeWidth())));
        } else if (const auto* stroke = dynamic_cast<const StrokeItem*>(item)) {
            addColorRow(form, uiText(QStringLiteral("Stroke color")), stroke->strokeColor());
            addRow(form, uiText(QStringLiteral("Stroke width")),
                   uiText(QStringLiteral("%1 px")).arg(number(stroke->strokeWidth())));
        } else if (const auto* image = dynamic_cast<const ImageItem*>(item)) {
            if (!image->imageName().isEmpty())
                addRow(form, uiText(QStringLiteral("Image")), image->imageName());
        } else if (const auto* text = dynamic_cast<const TextItem*>(item)) {
            addColorRow(form, uiText(QStringLiteral("Text color")), text->textColor());
            QFont font = text->textFont();
            const QFontInfo info(font);
            const QString sizeText = font.pointSizeF() > 0
                ? uiText(QStringLiteral("%1 pt")).arg(number(font.pointSizeF()))
                : uiText(QStringLiteral("%1 px")).arg(font.pixelSize());
            addRow(form, uiText(QStringLiteral("Font")),
                   uiText(QStringLiteral("%1 · %2")).arg(info.family(), sizeText));
        }

        addRow(form, uiText(QStringLiteral("Position")),
               uiText(QStringLiteral("%1, %2")).arg(number(item->pos().x()), number(item->pos().y())));
        addRow(form, uiText(QStringLiteral("Item size")),
               uiText(QStringLiteral("%1 × %2"))
                   .arg(number(item->contentSize().width()), number(item->contentSize().height())));
        root->addLayout(form);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    if (auto* close = buttons->button(QDialogButtonBox::Close))
        close->setText(uiText(QStringLiteral("Close")));
    if (dynamic_cast<TextItem*>(item)) {
        auto* edit = buttons->addButton(uiText(QStringLiteral("Edit text…")),
                                        QDialogButtonBox::ActionRole);
        connect(edit, &QPushButton::clicked, this, [this]() { done(EditTextRequested); });
    }
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

QString ItemPropertiesDialog::layerDescription(const BoardScene* scene, int layerId)
{
    if (layerId == BoardScene::BackgroundLayerId)
        return uiText(QStringLiteral("Background layer"));

    const int index = scene ? scene->layerIndex(layerId) : layerId;
    QString layerName;
    if (scene) {
        for (const BoardLayer& layer : scene->layers()) {
            if (layer.id == layerId) {
                layerName = layer.name;
                break;
            }
        }
    }
    const QString defaultName = uiText(QStringLiteral("Layer %1")).arg(index);
    if (layerName.isEmpty() || layerName == defaultName)
        return defaultName;
    return uiText(QStringLiteral("Layer %1 · %2")).arg(index).arg(layerName);
}

void ItemPropertiesDialog::addRow(QFormLayout* form, const QString& label,
                                  const QString& value, const QString& objectName)
{
    auto* field = new QLabel(value, this);
    field->setTextInteractionFlags(Qt::TextSelectableByMouse);
    if (!objectName.isEmpty())
        field->setObjectName(objectName);
    form->addRow(label, field);
}

void ItemPropertiesDialog::addColorRow(QFormLayout* form, const QString& label,
                                       const QColor& color, const QString& objectName)
{
    QWidget* field = makeColorValue(color);
    if (!objectName.isEmpty())
        field->setObjectName(objectName);
    form->addRow(label, field);
}
