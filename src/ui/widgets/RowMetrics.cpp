#include "ui/widgets/RowMetrics.h"

#include <QAbstractSpinBox>
#include <QLineEdit>
#include <QSizePolicy>
#include <QWidget>

namespace RowMetrics {

namespace {

qreal MeasureFieldHeight()
{
    static qreal cached = 0.0;
    if (cached > 0.0) {
        return cached;
    }
    // 不挂父窗口，只借样式算出输入框的自然高度：字体、DPI、样式表都会影响这个值。
    // 基准取 QLineEdit 而不是 QSpinBox：数值框内部的上下按钮会把它撑高，量出来不是行里输入框的可见高度。
    QLineEdit sample;
    cached = qMax<qreal>(20.0, sample.sizeHint().height());
    return cached;
}

} // namespace

int FieldHeight()
{
    return static_cast<int>(MeasureFieldHeight());
}

int RowHeight()
{
    return FieldHeight() + 7;
}

int GlyphSize()
{
    const int candidate = FieldHeight() - 9;
    return qBound(16, candidate, 20);
}

QSize IconButtonSize()
{
    const int side = FieldHeight();
    return QSize(side, side);
}

int SwatchSize()
{
    // 色块比字段高（30）小一档：一排 8 个预设色时不会把整行撑得比其它行宽，也不显笨重。
    return 20;
}

void ApplyRow(QWidget* row)
{
    if (!row) {
        return;
    }
    row->setMinimumHeight(RowHeight());
    row->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void ApplyFieldHeight(QWidget* widget)
{
    if (!widget) {
        return;
    }
    widget->ensurePolished();
    // 数值框的 sizeHint 会被内部上下按钮撑高（比可见输入框高 3px），这类控件直接取标准字段高。
    const bool isSpin = qobject_cast<QAbstractSpinBox*>(widget) != nullptr;
    widget->setFixedHeight(isSpin ? FieldHeight() : qMax(FieldHeight(), widget->sizeHint().height()));
}

} // namespace RowMetrics
