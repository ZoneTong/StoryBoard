#pragma once

#include <QColor>
#include <QDialog>

class BoardItem;
class BoardScene;
class QFormLayout;
class QVBoxLayout;

// 只读展示单个图元的属性：类型、图层、层内高度、填充样式与颜色、位置和尺寸等。
// 文字图元额外提供“编辑文字…”入口，保持双击图元后的文字编辑路径可达。
class ItemPropertiesDialog : public QDialog
{
    Q_OBJECT
public:
    enum ResultCode {
        EditTextRequested = 2
    };

    explicit ItemPropertiesDialog(BoardScene* scene, BoardItem* item, QWidget* parent = nullptr);

    static QString layerDescription(const BoardScene* scene, int layerId);

private:
    void addRow(QFormLayout* form, const QString& label, const QString& value,
                const QString& objectName = QString());
    void addColorRow(QFormLayout* form, const QString& label, const QColor& color,
                     const QString& objectName = QString());

    BoardScene* m_scene = nullptr;
    BoardItem* m_item = nullptr;
};
