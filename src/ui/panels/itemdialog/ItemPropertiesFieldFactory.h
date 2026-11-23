#pragma once

#include <QColor>
#include <QFont>
#include <QString>

class QDoubleSpinBox;
class QToolButton;
class QWidget;

// 属性对话框各区块共用的字段工厂：颜色按钮、数值框、成对数值框与文案格式化。
// 翻译上下文仍归属于 ItemPropertiesDialog，避免拆分类后中文译文丢失。
namespace ItemPropertiesFieldFactory {

// 颜色字段的图标语义：填充用色桶、边框用描边色板，其他文字等用通用色点。
enum class ColorIconRole { GENERIC, FILL, BORDER };

QString Number(qreal value);
QString ColorText(const QColor& color);
QString FontText(const QFont& font);
void RefreshColorButton(QToolButton* button);
QToolButton* MakeColorField(QWidget* parent, const QColor& color, const QString& objectName,
                            ColorIconRole iconRole = ColorIconRole::GENERIC);
QDoubleSpinBox* MakeSpin(QWidget* parent, const QString& objectName, qreal minimum, qreal maximum);
QWidget* MakePairField(QWidget* parent, const QString& objectName, QDoubleSpinBox* first, QDoubleSpinBox* second,
                       const QString& separator);

} // namespace ItemPropertiesFieldFactory
