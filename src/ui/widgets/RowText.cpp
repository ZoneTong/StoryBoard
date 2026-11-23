#include "ui/widgets/RowText.h"

#include <QCoreApplication>
#include <QToolButton>

QString RowText(const char* source)
{
    // 编辑面板与属性窗共用同一批行控件，译文沿用 PropertiesPanel 上下文。
    return QCoreApplication::translate("PropertiesPanel", source);
}

void SetRowIconVisible(QWidget* row, bool visible)
{
    if (!row) {
        return;
    }
    for (const char* name : {"ItemRowIcon", "ItemBorderWidthIcon"}) {
        for (QToolButton* icon : row->findChildren<QToolButton*>(QString::fromLatin1(name))) {
            icon->setVisible(visible);
        }
    }
}
