#include "ui/panels/itemdialog/ItemPropertiesDialogFields.h"

#include "items/properties/ItemStyleVisuals.h"
#include "ui/icons/IconFactory.h"
#include "ui/panels/itemdialog/ItemPropertiesDialog.h"

#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QFontInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

namespace ItemPropertiesFields {

QString Number(qreal value)
{
    return QString::number(value, 'f', 1);
}

QString ColorText(const QColor& color)
{
    return color.isValid() ? color.name(QColor::HexArgb).toUpper() : QStringLiteral("—");
}

QString FontText(const QFont& font)
{
    const QFontInfo info(font);
    const QString size = font.pointSizeF() > 0 ? ItemPropertiesDialog::tr("%1 pt").arg(Number(font.pointSizeF()))
                                               : ItemPropertiesDialog::tr("%1 px").arg(font.pixelSize());
    return ItemPropertiesDialog::tr("%1 · %2").arg(info.family(), size);
}

void RefreshColorButton(QToolButton* button)
{
    const QColor color = button->property("itemColor").value<QColor>();
    const ColorIconRole role = static_cast<ColorIconRole>(button->property("colorIconRole").toInt());
    QIcon icon;
    if (role == ColorIconRole::FILL) {
        icon = ItemStyleVisuals::FillColorIcon(color.isValid() ? color : QColor(Qt::transparent), 18);
    } else if (role == ColorIconRole::BORDER) {
        icon = ItemStyleVisuals::BorderColorIcon(color.isValid() ? color : QColor(Qt::transparent), 18);
    } else {
        icon = IconFactory::MakeColor(color.isValid() ? color : QColor(Qt::transparent), 18);
    }
    button->setIcon(icon);
    button->setText(ColorText(color));
    button->setToolTip(ColorText(color));
}

QToolButton* MakeColorField(QWidget* parent, const QColor& color, const QString& objectName, ColorIconRole iconRole)
{
    auto* button = new QToolButton(parent);
    button->setObjectName(objectName);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setMinimumWidth(118);
    button->setProperty("itemColor", color);
    button->setProperty("colorIconRole", static_cast<int>(iconRole));
    RefreshColorButton(button);

    QObject::connect(button, &QToolButton::clicked, button, [button]() {
        const QColor initial = button->property("itemColor").value<QColor>();
        const QColor chosen = QColorDialog::getColor(initial, button, ItemPropertiesDialog::tr("Choose color"),
                                                     QColorDialog::ShowAlphaChannel);
        if (!chosen.isValid()) {
            return;
        }
        button->setProperty("itemColor", chosen);
        RefreshColorButton(button);
    });
    return button;
}

QDoubleSpinBox* MakeSpin(QWidget* parent, const QString& objectName, qreal minimum, qreal maximum)
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

QWidget* MakePairField(QWidget* parent, const QString& objectName, QDoubleSpinBox* first, QDoubleSpinBox* second,
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

} // namespace ItemPropertiesFields
