#include "ui/panels/itemdialog/ItemPropertiesDialog.h"

#include "canvas/CanvasView.h"
#include "items/CanvasItem.h"
#include "items/geometry/TriangleGeometry.h"
#include "items/lines/LineAbstractItem.h"
#include "items/properties/ItemPropertyCatalog.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "items/text/TextItem.h"
#include "scene/CanvasScene.h"
#include "scene/Commands.h"
#include "ui/panels/itemdialog/ItemPropertiesDialogGeometryBlock.h"
#include "ui/panels/itemdialog/ItemPropertiesDialogIdentityBlock.h"
#include "ui/panels/itemdialog/ItemPropertiesDialogStyleBlock.h"
#include "ui/panels/itemdialog/ItemPropertiesDialogTextBlock.h"
#include "ui/panels/properties/PropertyCommitService.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPointer>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QTextCursor>
#include <QToolButton>
#include <QVBoxLayout>
#include <utility>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

#ifdef Q_OS_WIN
ItemPropertiesDialog* g_clickOutsideDialog = nullptr;
HHOOK g_clickOutsideHook = nullptr;
bool g_clickOutsideQueued = false;

bool IsClickOnDialogOwner(ItemPropertiesDialog* dialog, const QPoint& globalPoint)
{
    QWidget* owner = dialog ? dialog->parentWidget() : nullptr;
    if (!owner) {
        return false;
    }
    POINT nativePoint{globalPoint.x(), globalPoint.y()};
    HWND window = WindowFromPoint(nativePoint);
    if (!window) {
        return false;
    }
    // 只认主窗口自己的区域：点到别的应用（即使压在画布上）不关闭属性窗。
    return GetAncestor(window, GA_ROOT) == reinterpret_cast<HWND>(owner->winId());
}

LRESULT CALLBACK ClickOutsideHook(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION && (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN || wParam == WM_MBUTTONDOWN) &&
        g_clickOutsideDialog && g_clickOutsideDialog->isVisible() && !g_clickOutsideQueued) {
        // 子对话框（颜色/字体/文件）和下拉弹窗优先，不要把属性窗一起取消。
        QWidget* activeModal = QApplication::activeModalWidget();
        if ((!activeModal || activeModal == g_clickOutsideDialog) && !QApplication::activePopupWidget()) {
            const MSLLHOOKSTRUCT* mouse = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
            const QPoint globalPoint(mouse->pt.x, mouse->pt.y);
            if (!g_clickOutsideDialog->frameGeometry().contains(globalPoint) &&
                IsClickOnDialogOwner(g_clickOutsideDialog, globalPoint) &&
                g_clickOutsideDialog->IsGlobalPointInsideCanvas(globalPoint)) {
                g_clickOutsideQueued = true;
                QPointer<ItemPropertiesDialog> dialog(g_clickOutsideDialog);
                QMetaObject::invokeMethod(
                    g_clickOutsideDialog,
                    [dialog]() {
                        if (dialog) {
                            dialog->reject();
                        }
                    },
                    Qt::QueuedConnection);
                // 吞掉这次按下，避免属性窗关闭后同一下落又触发画布操作。
                return 1;
            }
        }
    }
    return CallNextHookEx(g_clickOutsideHook, code, wParam, lParam);
}

void InstallClickOutsideHook(ItemPropertiesDialog* dialog)
{
    if (!dialog || QGuiApplication::platformName() == QLatin1String("offscreen")) {
        return;
    }
    g_clickOutsideDialog = dialog;
    g_clickOutsideQueued = false;
    if (!g_clickOutsideHook) {
        g_clickOutsideHook = SetWindowsHookExW(WH_MOUSE_LL, ClickOutsideHook, GetModuleHandleW(nullptr), 0);
    }
}

void UninstallClickOutsideHook(ItemPropertiesDialog* dialog)
{
    if (g_clickOutsideDialog != dialog) {
        return;
    }
    g_clickOutsideDialog = nullptr;
    g_clickOutsideQueued = false;
    if (g_clickOutsideHook) {
        UnhookWindowsHookEx(g_clickOutsideHook);
        g_clickOutsideHook = nullptr;
    }
}
#endif

} // namespace

ItemPropertiesDialog::ItemPropertiesDialog(CanvasScene* scene, CanvasItem* item, QWidget* parent, bool editable)
    : QDialog(parent)
{
    setWindowTitle(tr("Item properties"));
    // 需求 42：属性窗统一改为模态；非编辑模式可打开查看，但字段只读。
    setModal(true);
    setMinimumWidth(480);
    // 模态窗会先吃掉窗外的鼠标按下，必须由应用级过滤器在事件分发前识别。
    qApp->installEventFilter(this);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 16);
    root->setSpacing(12);

    m_heading = new QLabel(this);
    m_heading->setObjectName(QStringLiteral("ItemPropertiesHeading"));
    root->addWidget(m_heading);

    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    root->addWidget(line);

    m_readOnlyHint = new QLabel(tr("Read-only: switch to Edit mode to change properties."), this);
    m_readOnlyHint->setObjectName(QStringLiteral("ItemPropertiesReadOnlyHint"));
    m_readOnlyHint->setWordWrap(true);
    m_readOnlyHint->hide();
    root->addWidget(m_readOnlyHint);

    auto* form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(9);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_typeValue = new QLabel(this);
    m_typeValue->setObjectName(QStringLiteral("ItemPropertiesTypeValue"));
    m_typeValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(tr("Item type"), m_typeValue);

    // 所有行一次建齐、顺序固定（类型 → 名称 / 图层 / 高度 → 文字 → 填充 / 描线 /
    // 参数行 → 几何）；SetItem 按图元属性有无回填并显隐，全部类型共用同一实例。
    m_identityBlock = new ItemPropertiesDialogIdentityBlock(scene, this);
    m_identityBlock->AddRows(form, &m_rows);

    m_textBlock = new ItemPropertiesDialogTextBlock(this);
    m_textBlock->AddRows(form, &m_rows);
    connect(m_textBlock, &ItemPropertiesDialogTextBlock::fontChanged, this, &ItemPropertiesDialog::SyncTextNaturalSize);
    connect(m_textBlock->TextEdit(), &QPlainTextEdit::textChanged, this, &ItemPropertiesDialog::SyncTextNaturalSize);

    m_styleBlock = new ItemPropertiesDialogStyleBlock(this);
    m_styleBlock->AddRows(form, &m_rows);

    m_geometryBlock = new ItemPropertiesDialogGeometryBlock(this);
    m_geometryBlock->AddRows(form, &m_rows);
    // 圆角矩形：尺寸框变化时同步圆角上限（区块内部按当前图元是否适用自行判断）。
    // 回填期间跳过：逐值回填宽高时另一个尺寸框还是构造默认值，会把圆角上限钳小；
    // SetItem 末尾用真实内容尺寸统一同步一次。
    auto syncCornerRange = [this]() {
        if (m_initializing || !m_geometryBlock || !m_styleBlock) {
            return;
        }
        m_styleBlock->SetContentSize(
            QSizeF(m_geometryBlock->WidthSpin()->value(), m_geometryBlock->HeightSizeSpin()->value()));
    };
    connect(m_geometryBlock->WidthSpin(), &QDoubleSpinBox::valueChanged, this, syncCornerRange);
    connect(m_geometryBlock->HeightSizeSpin(), &QDoubleSpinBox::valueChanged, this, syncCornerRange);

    root->addLayout(form);
    m_form = form;

    m_dynamicHost = new QWidget(this);
    m_dynamicHost->setObjectName(QStringLiteral("ItemPropertiesDynamicHost"));
    m_dynamicForm = new QFormLayout(m_dynamicHost);
    m_dynamicForm->setContentsMargins(0, 0, 0, 0);
    m_dynamicForm->setHorizontalSpacing(form->horizontalSpacing());
    m_dynamicForm->setVerticalSpacing(form->verticalSpacing());
    m_dynamicForm->setLabelAlignment(form->labelAlignment());
    m_dynamicHost->setVisible(false);
    root->addWidget(m_dynamicHost);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &ItemPropertiesDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(m_buttons);

    if (m_identityBlock->HasLayerControls()) {
        connect(m_identityBlock, &ItemPropertiesDialogIdentityBlock::layerChanged, this, [this]() {
            if (m_initializing) {
                return;
            }
            m_identityBlock->UpdateHeightRange(true);
        });
    }

    SetItem(scene, item, editable);
}

void ItemPropertiesDialog::SetItem(CanvasScene* scene, CanvasItem* item, bool editable)
{
    m_scene = scene;
    m_item = item;
    m_editable = editable;
    m_textFocusApplied = false;
    m_outsideClickClosing = false;
    // 回填期间屏蔽信号联动（文字正文变化同步自然尺寸、图层切换重算高度等），
    // 避免“打开属性窗”被误当成用户修改。
    m_initializing = true;

    // 打开属性窗时再兜底一次自动名称：旧画布、直接 addItem 或清空名称后，
    // 这里也能得到“类型 N”，不会出现看不到名称或名称为空的行。
    if (scene) {
        scene->AssignAutoItemNames();
    }

    m_heading->setText(item ? item->Name() : tr("No item selected"));
    m_typeValue->setText(item ? item->TypeDisplayName() : QStringLiteral("—"));
    m_readOnlyHint->setVisible(item && !m_editable);

    if (item) {
        m_initialPos = item->pos();
        m_initialSize = item->ContentSize();
        m_initialRotation = item->RotationAngle();
        m_initialItemName = item->CustomName();
        m_initialDisplayName = item->Name();
    } else {
        m_initialPos = QPointF();
        m_initialSize = QSizeF();
        m_initialRotation = 0.0;
        m_initialItemName.clear();
        m_initialDisplayName.clear();
    }

    m_identityBlock->SetItem(scene, item);
    m_textBlock->SetItem(item);
    m_styleBlock->SetItem(item);
    m_hasFill = m_styleBlock->HasFill();
    m_initialStrokeWidth = m_styleBlock->InitialStrokeWidth();
    m_geometryBlock->SetItem(item);
    // 宽高回填完成后用真实内容尺寸统一同步圆角上限（含未存储圆角的图元按比例推默认值）。
    if (item) {
        m_styleBlock->SetContentSize(m_initialSize);
    }
    // 行显隐只看图元上报的属性集合（§4.3）：换绑图元时整表重算一次。
    m_rows.Apply(m_form, item ? item->Properties() : ItemPropertySet());
    RefreshDynamicRows(item);

    ApplyEditableState();

    if (auto* ok = m_buttons->button(QDialogButtonBox::Ok)) {
        ok->setText(m_editable ? tr("OK") : tr("Close"));
    }
    if (auto* cancel = m_buttons->button(QDialogButtonBox::Cancel)) {
        cancel->setVisible(m_editable);
    }

    if (m_identityBlock->HasLayerControls()) {
        m_identityBlock->UpdateHeightRange(false);
        m_identityBlock->SetInitialized(false);
    }
    m_initializing = false;
}

ItemPropertiesDialog::~ItemPropertiesDialog()
{
#ifdef Q_OS_WIN
    UninstallClickOutsideHook(this);
#endif
    if (qApp) {
        qApp->removeEventFilter(this);
    }
}

bool ItemPropertiesDialog::IsGlobalPointInsideCanvas(const QPoint& globalPoint) const
{
    if (!m_scene) {
        return false;
    }
    QWidget* owner = parentWidget();
    CanvasView* view = qobject_cast<CanvasView*>(owner);
    if (!view && owner) {
        view = owner->findChild<CanvasView*>();
    }
    if (!view || !view->viewport()) {
        return false;
    }
    // 必须真正落在画布视图上：被浮动面板挡住时，即使屏幕坐标在画布矩形内也不触发。
    // 无桌面平台（自检 offscreen）拿不到 widgetAt 时，只按画布矩形判断。
    QWidget* hit = QApplication::widgetAt(globalPoint);
    if (hit && hit != view->viewport()) {
        return false;
    }
    const QPoint viewportPoint = view->viewport()->mapFromGlobal(globalPoint);
    return view->mapFromScene(m_scene->CanvasRect()).containsPoint(viewportPoint, Qt::OddEvenFill);
}

bool ItemPropertiesDialog::eventFilter(QObject* watched, QEvent* event)
{
    // 只处理模态属性窗自己的“窗外按下”：子对话框（颜色/字体/文件）和
    // 下拉弹窗上的点击不应把属性窗一起取消。
    if (!event || event->type() != QEvent::MouseButtonPress || !isVisible() || m_outsideClickClosing ||
        QApplication::activeModalWidget() != this || QApplication::activePopupWidget()) {
        return QDialog::eventFilter(watched, event);
    }

    const QMouseEvent* mouseEvent = static_cast<const QMouseEvent*>(event);
    const QPoint globalPoint = mouseEvent->globalPosition().toPoint();
    if (mouseEvent->button() == Qt::NoButton || frameGeometry().contains(globalPoint) ||
        !IsGlobalPointInsideCanvas(globalPoint)) {
        return QDialog::eventFilter(watched, event);
    }

    m_outsideClickClosing = true;
    // 窗外点击按取消处理：丢弃尚未确认的修改，和按 Cancel 一致。
    reject();
    // 吞掉这次按下，避免关闭后同一次点击又落到画布上。
    return true;
}

void ItemPropertiesDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
#ifdef Q_OS_WIN
    InstallClickOutsideHook(this);
#endif
    // 只做一次，且只读模式不抢焦点；光标停在正文末尾，不整段选中。
    if (m_textFocusApplied || !m_editable || !m_item || m_item->Category() != ItemCategory::TEXT) {
        return;
    }
    m_textFocusApplied = true;
    QPlainTextEdit* edit = m_textBlock->TextEdit();
    edit->moveCursor(QTextCursor::End);
    edit->setFocus(Qt::OtherFocusReason);
}

void ItemPropertiesDialog::hideEvent(QHideEvent* event)
{
#ifdef Q_OS_WIN
    UninstallClickOutsideHook(this);
#endif
    QDialog::hideEvent(event);
}

void ItemPropertiesDialog::ApplyEditableState()
{
    if (m_editable) {
        return;
    }

    QList<QWidget*> widgets;
    if (m_identityBlock) {
        widgets << m_identityBlock->NameEdit() << m_identityBlock->LayerCombo() << m_identityBlock->HeightSpin();
    }
    if (m_styleBlock) {
        widgets << m_styleBlock->StyleRows();
    }
    if (m_textBlock) {
        widgets << m_textBlock->TextColorButton() << m_textBlock->FontButton() << m_textBlock->TextEdit();
    }
    if (m_geometryBlock) {
        widgets << m_geometryBlock->PositionXSpin() << m_geometryBlock->PositionYSpin() << m_geometryBlock->WidthSpin()
                << m_geometryBlock->HeightSizeSpin() << m_geometryBlock->MajorAxisSpin()
                << m_geometryBlock->MinorAxisSpin() << m_geometryBlock->RotationSpin();
    }
    widgets << m_dynamicEditors.values();
    for (QWidget* widget : widgets) {
        if (widget) {
            widget->setEnabled(false);
        }
    }
}

void ItemPropertiesDialog::accept()
{
    if (!m_editable) {
        QDialog::accept();
        return;
    }
    if (m_identityBlock && m_identityBlock->NameEdit() && m_scene && m_item) {
        const QString typedName = m_identityBlock->NameEdit()->text().trimmed();
        if (!typedName.isEmpty() && !m_scene->IsItemNameAvailable(typedName, m_item)) {
            QMessageBox::warning(this, tr("Duplicate item name"),
                                 tr("Another item in this canvas already uses this name. Choose a different name."));
            m_identityBlock->NameEdit()->setFocus();
            m_identityBlock->NameEdit()->selectAll();
            return;
        }
    }
    ApplyChanges();
    QDialog::accept();
}

void ItemPropertiesDialog::SyncTextNaturalSize()
{
    if (m_initializing || !m_editable || !m_textBlock || !m_textBlock->TextEdit()) {
        return;
    }
    if (m_item && m_item->Category() == ItemCategory::TEXT) {
        const QSizeF natural = TextItem::Measure(m_textBlock->TextEdit()->toPlainText(), m_textBlock->TextFont());
        if (m_geometryBlock) {
            m_geometryBlock->SetNaturalSize(natural);
        }
    }
}

bool ItemPropertiesDialog::IsItemInScene() const
{
    return m_scene && m_item && m_item->scene() == m_scene;
}

void ItemPropertiesDialog::RefreshDynamicRows(CanvasItem* item)
{
    if (!m_dynamicForm || !m_dynamicHost) {
        return;
    }
    while (m_dynamicForm->rowCount() > 0) {
        auto row = m_dynamicForm->takeRow(0);
        delete row.labelItem;
        if (row.fieldItem) {
            delete row.fieldItem->widget();
            delete row.fieldItem;
        }
    }
    m_dynamicEditors.clear();
    if (!item) {
        m_dynamicHost->setVisible(false);
        return;
    }

    for (PropertyKey property : item->Properties().Keys()) {
        if (property.IsCommon()) {
            continue;
        }
        const ItemPropertyDescriptor* descriptor = ItemPropertyCatalog::Find(property);
        if (!descriptor) {
            continue;
        }
        const QVariant current = item->PropertyValueByKey(property);
        QWidget* editor = nullptr;
        if (descriptor->kind == ItemPropertyKind::TEXT) {
            auto* line = new QLineEdit(m_dynamicHost);
            line->setText(current.toString());
            editor = line;
        } else if (descriptor->kind == ItemPropertyKind::INTEGER) {
            auto* spin = new QSpinBox(m_dynamicHost);
            spin->setRange(qRound(descriptor->minimum), qRound(descriptor->maximum));
            spin->setSingleStep(qMax(1, qRound(descriptor->step)));
            spin->setSuffix(descriptor->suffix);
            spin->setValue(current.toInt());
            editor = spin;
        } else if (descriptor->kind == ItemPropertyKind::REAL) {
            auto* spin = new QDoubleSpinBox(m_dynamicHost);
            spin->setRange(descriptor->minimum, descriptor->maximum);
            spin->setSingleStep(descriptor->step);
            spin->setDecimals(descriptor->decimals);
            spin->setSuffix(descriptor->suffix);
            spin->setValue(current.toDouble());
            editor = spin;
        } else {
            editor = new QLabel(current.toString(), m_dynamicHost);
        }
        m_dynamicEditors.insert(property.Id(), editor);
        m_dynamicForm->addRow(ItemPropertyCatalog::Label(property), editor);
    }
    m_dynamicHost->setVisible(!m_dynamicEditors.isEmpty());
}

QVariant ItemPropertiesDialog::ReadDynamicValue(PropertyKey property) const
{
    QWidget* editor = m_dynamicEditors.value(property.Id(), nullptr);
    if (auto* line = qobject_cast<QLineEdit*>(editor)) {
        return line->text();
    }
    if (auto* spin = qobject_cast<QSpinBox*>(editor)) {
        return spin->value();
    }
    if (auto* spin = qobject_cast<QDoubleSpinBox*>(editor)) {
        return spin->value();
    }
    if (auto* label = qobject_cast<QLabel*>(editor)) {
        return label->text();
    }
    return QVariant();
}

void ItemPropertiesDialog::ApplyChanges()
{
    if (!m_editable || !m_scene || !m_item) {
        return;
    }

    auto* identity = m_identityBlock;
    auto* style = m_styleBlock;
    auto* text = m_textBlock;
    auto* geometry = m_geometryBlock;

    const int oldLayer = m_item->LayerId();
    const int oldHeight = m_item->ZOrder();
    int targetLayer = (identity && identity->LayerCombo()) ? identity->LayerCombo()->currentData().toInt() : oldLayer;
    // D6：组内成员不许单独换层。属性窗把图层夹回原值，并给一次状态栏提示。
    if (!m_scene->CanMoveItemToLayer(m_item, targetLayer)) {
        targetLayer = oldLayer;
        m_scene->NotifyGroupLayerBlocked();
    }
    const int targetHeight = (identity && identity->HeightSpin()) ? identity->HeightSpin()->value() : oldHeight;

    QList<CanvasItem*> swapped;
    if (targetLayer != oldLayer || targetHeight != oldHeight) {
        for (CanvasItem* peer : m_scene->LayerItems(targetLayer)) {
            if (peer != m_item && peer->ZOrder() == targetHeight) {
                swapped.append(peer);
            }
        }
    }

    auto spinValue = [](auto* spin, qreal initial) {
        if (!spin) {
            return initial;
        }
        const qreal value = spin->value();
        // 数值框按控件自身精度显示；用户没动过时保留原始精度，避免“打开就点确定”也产生修改。
        return qFuzzyCompare(value, qRound(initial * 10.0) / 10.0) ? initial : value;
    };

    QPointF afterPos = QPointF(spinValue(geometry->PositionXSpin(), m_initialPos.x()),
                               spinValue(geometry->PositionYSpin(), m_initialPos.y()));
    QSizeF afterSize = QSizeF(spinValue(geometry->WidthSpin(), m_initialSize.width()),
                              spinValue(geometry->HeightSizeSpin(), m_initialSize.height()));
    qreal afterRotation = spinValue(geometry->RotationSpin(), m_initialRotation);
    // §3.4 中心点是派生属性：用户改过中心点时按“初始位置 + 中心增量”折算位置，
    // 尺寸与旋转不受影响；未改动时与位置行的结果完全一致。
    const QPointF centerValue(geometry->CenterXSpin()->value(), geometry->CenterYSpin()->value());
    const QPointF initialCenter = geometry->InitialCenter();
    if (!qFuzzyCompare(centerValue.x() + 1.0, initialCenter.x() + 1.0) ||
        !qFuzzyCompare(centerValue.y() + 1.0, initialCenter.y() + 1.0)) {
        afterPos = m_initialPos + (centerValue - initialCenter);
    }

    const QString typedName = (identity && identity->NameEdit()) ? identity->NameEdit()->text().trimmed() : QString();
    // 清空名称不能退回“多个同名类型名”的状态；这时补一个本画布内唯一的自动名称。
    const QString effectiveName =
        typedName.isEmpty() ? m_scene->UniqueItemName(m_item->TypeDisplayName(), m_item) : typedName;
    const QString afterName =
        (m_initialItemName.isEmpty() && typedName == m_initialDisplayName) ? QString() : effectiveName;

    // 属性表命令直接携带 before/after 两个属性映射；CENTER 是派生属性，不写回以免重复平移。
    QMap<PropertyKey, QVariant> before;
    for (PropertyKey property : m_item->Properties().Keys()) {
        before.insert(property, m_item->PropertyValueByKey(property));
    }
    before.remove(ItemProperty::CENTER);
    QMap<PropertyKey, QVariant> after = before;
    after.insert(ItemProperty::NAME, afterName);
    after.insert(ItemProperty::LAYER, targetLayer);
    after.insert(ItemProperty::HEIGHT, targetHeight);
    after.insert(ItemProperty::POSITION, afterPos);
    after.insert(ItemProperty::SIZE, afterSize);
    after.insert(ItemProperty::ROTATION, afterRotation);
    for (auto it = m_dynamicEditors.constBegin(); it != m_dynamicEditors.constEnd(); ++it) {
        const PropertyKey property(it.key());
        after.insert(property, ReadDynamicValue(property));
    }

    if (auto* shape = dynamic_cast<ShapeAbstractItem*>(m_item.data())) {
        if (style) {
            // 填充 / 描线两行直接绑定属性基类副本，确定时整份状态取回，不再逐控件拼值。
            // 只描线的图形没有填充行：副本保持图元原值，不产生额外修改。
            if (m_hasFill) {
                const FillAttributesState fill = style->FillState();
                after.insert(ItemProperty::FILL_STYLE, static_cast<int>(fill.style));
                after.insert(ItemProperty::FILL_COLOR, fill.color);
                after.insert(ItemProperty::FILL_IMAGE, QVariant::fromValue(fill.hasImage ? fill.image : QPixmap()));
                after.insert(ItemProperty::FILL_IMAGE_MODE, static_cast<int>(fill.imageMode));
            }
            const StrokeAttributesState strokeState = style->StrokeState();
            after.insert(ItemProperty::STROKE_STYLE, static_cast<int>(strokeState.style));
            after.insert(ItemProperty::STROKE_COLOR, strokeState.color);
            after.insert(ItemProperty::STROKE_WIDTH, strokeState.width);
        }
        if (style && style->OpacitySpin()) {
            const int percent = style->OpacitySpin()->value();
            after.insert(ItemProperty::OPACITY,
                         percent == style->InitialOpacityPercent() ? style->InitialOpacity() : percent / 100.0);
        }
        if (style && style->StartAngleSpin() && before.contains(ItemProperty::START_ANGLE)) {
            const int value = style->StartAngleSpin()->value();
            after.insert(ItemProperty::START_ANGLE, value == qRound(style->InitialStartAngle())
                                                        ? style->InitialStartAngle()
                                                        : static_cast<qreal>(value));
        }
        if (style && style->SweepAngleSpin() && before.contains(ItemProperty::SWEEP_ANGLE)) {
            const int value = style->SweepAngleSpin()->value();
            after.insert(ItemProperty::SWEEP_ANGLE, value == qRound(style->InitialSweepAngle())
                                                        ? style->InitialSweepAngle()
                                                        : static_cast<qreal>(value));
        }
        if (geometry && geometry->MajorAxisSpin() && before.contains(ItemProperty::MAJOR_AXIS)) {
            after.insert(ItemProperty::MAJOR_AXIS, geometry->MajorAxisSpin()->value());
            after.insert(ItemProperty::MINOR_AXIS, geometry->MinorAxisSpin()->value());
        }
        if (style && style->CornerRadiusSpin() && before.contains(ItemProperty::CORNER_RADIUS)) {
            const int value = style->CornerRadiusSpin()->value();
            // 圆角矩形默认值来自短边的 16%，对话框里显示为非 0；
            // 未改动时保留“按类型取默认”的 -1，避免打开属性窗点确定就写回显式值。
            after.insert(ItemProperty::CORNER_RADIUS, value == style->InitialCornerRadius()
                                                          ? style->InitialStoredCornerRadius()
                                                          : static_cast<qreal>(value));
        }
        // 内角 A / B 两行由三角形与平行四边形共用：同一控件按目标回填与提交，
        // 换绑图元后不会把旧值脏写到另一类属性上（§4.2）。
        if (style && style->InteriorAngleASpin()) {
            const int value = style->InteriorAngleASpin()->value();
            const qreal initial = style->InitialInteriorAngleA();
            const bool unchanged = value == qRound(initial);
            if (style->Target() == ItemPropertiesDialogStyleBlock::InteriorAngleTarget::TRIANGLE_A &&
                before.contains(ItemProperty::BOTTOM_ANGLE1)) {
                after.insert(ItemProperty::BOTTOM_ANGLE1, unchanged ? initial : static_cast<qreal>(value));
            } else if (style->Target() == ItemPropertiesDialogStyleBlock::InteriorAngleTarget::PARALLELOGRAM &&
                       before.contains(ItemProperty::PARALLELOGRAM_ANGLE)) {
                after.insert(ItemProperty::PARALLELOGRAM_ANGLE, unchanged ? initial : static_cast<qreal>(value));
            }
        }
        if (style && style->InteriorAngleBSpin() && before.contains(ItemProperty::BOTTOM_ANGLE2)) {
            const int value = style->InteriorAngleBSpin()->value();
            const qreal initial = style->InitialInteriorAngleB();
            after.insert(ItemProperty::BOTTOM_ANGLE2, value == qRound(initial) ? initial : static_cast<qreal>(value));
        }
        // 只改底角、不动尺寸时保持底边长度：按新角度重算包围盒，而不是沿用旧盒宽。
        const qreal angle1Before = before.value(ItemProperty::BOTTOM_ANGLE1).toDouble();
        const qreal angle2Before = before.value(ItemProperty::BOTTOM_ANGLE2).toDouble();
        const qreal angle1After = after.value(ItemProperty::BOTTOM_ANGLE1).toDouble();
        const qreal angle2After = after.value(ItemProperty::BOTTOM_ANGLE2).toDouble();
        const bool triangleAnglesChanged = !qFuzzyCompare(angle1Before + 2.0, angle1After + 2.0) ||
                                           !qFuzzyCompare(angle2Before + 2.0, angle2After + 2.0);
        if (shape->shapeType() == ShapeType::TRIANGLE && !shape->IsCustomShape() && triangleAnglesChanged &&
            afterSize == before.value(ItemProperty::SIZE).toSizeF()) {
            afterSize = TriangleGeometry::SizeForAngles(before.value(ItemProperty::SIZE).toSizeF(), angle1Before,
                                                        angle2Before, angle1After, angle2After);
            after.insert(ItemProperty::SIZE, afterSize);
        }
    } else if (m_item->Category() == ItemCategory::TEXT) {
        // 文字属性走属性协议：正文 / 颜色 / 字体各按 ItemProperty 提交，
        // 尺寸与位置仍由同一份映射回放，整次编辑只占一条撤销记录。
        const QFont font = text ? text->TextFont() : m_item->PropertyValue(ItemProperty::TEXT_FONT).value<QFont>();
        after.insert(ItemProperty::TEXT_FONT, font);
        if (text && text->TextEdit()) {
            after.insert(ItemProperty::TEXT_CONTENT, text->TextEdit()->toPlainText());
        }
        const QString textBefore = before.value(ItemProperty::TEXT_CONTENT).toString();
        const QString textAfter = after.value(ItemProperty::TEXT_CONTENT).toString();
        // 需求追加：文字正文变化后直接回到文字自然尺寸，不再沿用旧图元的
        // 拉伸尺寸，避免新旧字形按不同比例叠在同一块内容区里。
        if (textAfter != textBefore) {
            afterSize = TextItem::Measure(textAfter, font);
            after.insert(ItemProperty::SIZE, afterSize);
        }
        if (text && text->TextColorButton()) {
            after.insert(ItemProperty::TEXT_COLOR, text->TextColorButton()->property("itemColor").value<QColor>());
        }
        // 不透明度是所有图元共有的属性：文字在这里与图形共用同一行提交流程。
        if (style && style->OpacitySpin()) {
            const int percent = style->OpacitySpin()->value();
            after.insert(ItemProperty::OPACITY,
                         percent == style->InitialOpacityPercent() ? style->InitialOpacity() : percent / 100.0);
        }
    } else if (dynamic_cast<LineAbstractItem*>(m_item.data())) {
        const StrokeAttributesState beforeStroke{
            before.value(ItemProperty::STROKE_COLOR).value<QColor>(),
            before.value(ItemProperty::STROKE_WIDTH).toDouble(),
            static_cast<Qt::PenStyle>(before.value(ItemProperty::STROKE_STYLE).toInt())};
        const StrokeAttributesState strokeState = style ? style->StrokeState() : beforeStroke;
        after.insert(ItemProperty::STROKE_STYLE, static_cast<int>(strokeState.style));
        after.insert(ItemProperty::STROKE_COLOR, strokeState.color);
        after.insert(ItemProperty::STROKE_WIDTH, strokeState.width);
        // 不透明度行在样式区（紧跟描线宽度），与图形同一份提交流程。
        if (style && style->OpacitySpin()) {
            const int percent = style->OpacitySpin()->value();
            after.insert(ItemProperty::OPACITY,
                         percent == style->InitialOpacityPercent() ? style->InitialOpacity() : percent / 100.0);
        }
        if (style && style->StartArrowCombo()) {
            after.insert(ItemProperty::ARROW_START,
                         static_cast<int>(static_cast<ArrowStyle>(style->StartArrowCombo()->currentData().toInt())));
        }
        if (style && style->EndArrowCombo()) {
            after.insert(ItemProperty::ARROW_END,
                         static_cast<int>(static_cast<ArrowStyle>(style->EndArrowCombo()->currentData().toInt())));
        }
        // 弧线角度 / 正弦周期振幅：改过才写入映射，未改动的行沿用原值。
        if (before.contains(ItemProperty::START_ANGLE) && before.contains(ItemProperty::SWEEP_ANGLE)) {
            if (style && style->StartAngleSpin() && style->SweepAngleSpin()) {
                const qreal startBefore = before.value(ItemProperty::START_ANGLE).toDouble();
                const qreal sweepBefore = before.value(ItemProperty::SWEEP_ANGLE).toDouble();
                after.insert(ItemProperty::START_ANGLE, style->StartAngleSpin()->value() == qRound(startBefore)
                                                            ? startBefore
                                                            : style->StartAngleSpin()->value());
                after.insert(ItemProperty::SWEEP_ANGLE, style->SweepAngleSpin()->value() == qRound(sweepBefore)
                                                            ? sweepBefore
                                                            : style->SweepAngleSpin()->value());
            }
            // 弧线长短半轴：与角度同一份映射，未改动时 before == after 自动跳过。
            if (geometry && geometry->MajorAxisSpin()) {
                after.insert(ItemProperty::MAJOR_AXIS, geometry->MajorAxisSpin()->value());
                after.insert(ItemProperty::MINOR_AXIS, geometry->MinorAxisSpin()->value());
            }
        } else if (before.contains(ItemProperty::SINE_CYCLES) && before.contains(ItemProperty::SINE_AMPLITUDE)) {
            if (style && style->SineCyclesSpin() && style->SineAmplitudeSpin()) {
                const int cyclesBefore = before.value(ItemProperty::SINE_CYCLES).toInt();
                const qreal amplitudeBefore = before.value(ItemProperty::SINE_AMPLITUDE).toDouble();
                after.insert(ItemProperty::SINE_CYCLES, style->SineCyclesSpin()->value() == cyclesBefore
                                                            ? cyclesBefore
                                                            : style->SineCyclesSpin()->value());
                after.insert(ItemProperty::SINE_AMPLITUDE,
                             style->SineAmplitudeSpin()->value() == qRound(amplitudeBefore)
                                 ? amplitudeBefore
                                 : style->SineAmplitudeSpin()->value());
            }
        }
    }

    if (before == after) {
        return;
    }

    QList<CanvasItem*> targets{m_item};
    QList<QMap<PropertyKey, QVariant>> valuesBefore{before};
    QList<QMap<PropertyKey, QVariant>> valuesAfter{after};
    for (CanvasItem* peer : std::as_const(swapped)) {
        QMap<PropertyKey, QVariant> peerBefore;
        peerBefore.insert(ItemProperty::LAYER, peer->LayerId());
        peerBefore.insert(ItemProperty::HEIGHT, peer->ZOrder());
        QMap<PropertyKey, QVariant> peerAfter;
        peerAfter.insert(ItemProperty::LAYER, oldLayer);
        peerAfter.insert(ItemProperty::HEIGHT, oldHeight);
        targets.append(peer);
        valuesBefore.append(peerBefore);
        valuesAfter.append(peerAfter);
    }

    PropertyCommitService::Push(m_scene, targets, valuesBefore, valuesAfter, tr("Edit item properties"));
}

QString ItemPropertiesDialog::LayerDescription(const CanvasScene* scene, int layerId)
{
    if (layerId == CanvasScene::BackgroundLayerId) {
        return tr("Background layer");
    }

    const int index = scene ? scene->LayerIndex(layerId) : layerId;
    QString layerName;
    if (scene) {
        for (const CanvasLayer& layer : scene->Layers()) {
            if (layer.id == layerId) {
                layerName = layer.name;
                break;
            }
        }
    }
    const QString defaultName = tr("Layer %1").arg(index);
    if (layerName.isEmpty() || layerName == defaultName) {
        return defaultName;
    }
    return tr("Layer %1 · %2").arg(index).arg(layerName);
}
