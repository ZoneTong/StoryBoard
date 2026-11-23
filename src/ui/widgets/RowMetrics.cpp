#include "ui/widgets/RowMetrics.h"

#include <QDoubleSpinBox>
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
    QDoubleSpinBox sample;
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
    return FieldHeight() + 6;
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

void ApplyRow(QWidget* row)
{
    if (!row) {
        return;
    }
    row->setMinimumHeight(RowHeight());
    row->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

} // namespace RowMetrics
