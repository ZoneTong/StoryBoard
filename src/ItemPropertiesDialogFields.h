#pragma once

#include <QColor>
#include <QFont>
#include <QString>

class QDoubleSpinBox;
class QToolButton;
class QWidget;

// 属性对话框各区块共用的字段工厂：颜色按钮、数值框、成对数值框与文案格式化。
// 翻译上下文仍归属于 ItemPropertiesDialog，避免拆分类后中文译文丢失。
namespace ItemPropertiesFields {

// 颜色字段的图标语义：填充用色桶、边框用描边色板，其他文字等用通用色点。
enum class ColorIconRole { Generic, Fill, Border };

QString number(qreal value);
QString colorText(const QColor& color);
QString fontText(const QFont& font);
void refreshColorButton(QToolButton* button);
QToolButton* makeColorField(QWidget* parent, const QColor& color, const QString& objectName,
                            ColorIconRole iconRole = ColorIconRole::Generic);
QDoubleSpinBox* makeSpin(QWidget* parent, const QString& objectName, qreal minimum, qreal maximum);
QWidget* makePairField(QWidget* parent, const QString& objectName, QDoubleSpinBox* first, QDoubleSpinBox* second,
                       const QString& separator);

} // namespace ItemPropertiesFields
