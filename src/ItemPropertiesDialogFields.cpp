#include "ItemPropertiesDialogFields.h"

#include "IconFactory.h"
#include "ItemPropertiesDialog.h"
#include "ItemStyleVisuals.h"

#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QFontInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

namespace ItemPropertiesFields {

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
    const ColorIconRole role = static_cast<ColorIconRole>(button->property("colorIconRole").toInt());
    QIcon icon;
    if (role == ColorIconRole::FILL) {
        icon = ItemStyleVisuals::fillColorIcon(color.isValid() ? color : QColor(Qt::transparent), 18);
    } else if (role == ColorIconRole::BORDER) {
        icon = ItemStyleVisuals::borderColorIcon(color.isValid() ? color : QColor(Qt::transparent), 18);
    } else {
        icon = makeColorIcon(color.isValid() ? color : QColor(Qt::transparent), 18);
    }
    button->setIcon(icon);
    button->setText(colorText(color));
    button->setToolTip(colorText(color));
}

QToolButton* makeColorField(QWidget* parent, const QColor& color, const QString& objectName, ColorIconRole iconRole)
{
    auto* button = new QToolButton(parent);
    button->setObjectName(objectName);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setMinimumWidth(118);
    button->setProperty("itemColor", color);
    button->setProperty("colorIconRole", static_cast<int>(iconRole));
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

} // namespace ItemPropertiesFields
