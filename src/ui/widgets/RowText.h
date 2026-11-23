#pragma once

#include <QString>

class QWidget;

// 属性行共用文案与可见性小工具：行控件散落在编辑面板与属性窗两处，
// 这里只放跨行复用的两个入口，避免每批行控件各自复刻一份。
// 译文沿用 PropertiesPanel 上下文（与 PanelCatalog 的既有译文对齐）。
QString RowText(const char* source);

// 属性窗的行不需要行首图标：按 objectName 把两类图标按钮一并隐藏。
void SetRowIconVisible(QWidget* row, bool visible);
