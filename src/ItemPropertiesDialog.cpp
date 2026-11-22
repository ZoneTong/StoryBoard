#include "ItemPropertiesDialog.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "CanvasView.h"
#include "Commands.h"
#include "ItemPropertiesDialogGeometryBlock.h"
#include "ItemPropertiesDialogIdentityBlock.h"
#include "ItemPropertiesDialogStyleBlock.h"
#include "ItemPropertiesDialogTextBlock.h"
#include "LineAbstractItem.h"
#include "ShapeAbstractItem.h"
#include "TextItem.h"
#include "TriangleGeometry.h"

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
    m_identityBlock->AddRows(form);

    m_textBlock = new ItemPropertiesDialogTextBlock(this);
    m_textBlock->AddRows(form);
    connect(m_textBlock, &ItemPropertiesDialogTextBlock::fontChanged, this, &ItemPropertiesDialog::SyncTextNaturalSize);
    connect(m_textBlock->TextEdit(), &QPlainTextEdit::textChanged, this, &ItemPropertiesDialog::SyncTextNaturalSize);

    m_styleBlock = new ItemPropertiesDialogStyleBlock(this);
    m_styleBlock->AddRows(form);

    m_geometryBlock = new ItemPropertiesDialogGeometryBlock(this);
    m_geometryBlock->AddRows(form);
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
    const int targetLayer =
        (identity && identity->LayerCombo()) ? identity->LayerCombo()->currentData().toInt() : oldLayer;
    const int targetHeight = (identity && identity->HeightSpin()) ? identity->HeightSpin()->value() : oldHeight;

    QList<CanvasItem*> swapped;
    if (targetLayer != oldLayer || targetHeight != oldHeight) {
        for (CanvasItem* peer : m_scene->LayerItems(targetLayer)) {
            if (peer != m_item && peer->ZOrder() == targetHeight) {
                swapped.append(peer);
            }
        }
    }

    ItemPropertiesState before;
    before.pos = m_item->pos();
    before.size = m_item->ContentSize();
    before.rotation = m_initialRotation;
    before.opacity = m_item->ItemOpacity();
    before.itemName = m_initialItemName;
    before.stack.append({m_item, oldLayer, oldHeight});
    for (CanvasItem* peer : std::as_const(swapped)) {
        before.stack.append({peer, peer->LayerId(), peer->ZOrder()});
    }

    ItemPropertiesState after;
    auto spinValue = [](auto* spin, qreal initial) {
        if (!spin) {
            return initial;
        }
        const qreal value = spin->value();
        // 数值框按控件自身精度显示；用户没动过时保留原始精度，避免“打开就点确定”也产生修改。
        return qFuzzyCompare(value, qRound(initial * 10.0) / 10.0) ? initial : value;
    };
    after.pos = QPointF(spinValue(geometry->PositionXSpin(), m_initialPos.x()),
                        spinValue(geometry->PositionYSpin(), m_initialPos.y()));
    after.size = QSizeF(spinValue(geometry->WidthSpin(), m_initialSize.width()),
                        spinValue(geometry->HeightSizeSpin(), m_initialSize.height()));
    after.rotation = spinValue(geometry->RotationSpin(), m_initialRotation);
    after.opacity = before.opacity;
    const QString typedName = (identity && identity->NameEdit()) ? identity->NameEdit()->text().trimmed() : QString();
    // 清空名称不能退回“多个同名类型名”的状态；这时补一个本画布内唯一的自动名称。
    const QString effectiveName =
        typedName.isEmpty() ? m_scene->UniqueItemName(m_item->TypeDisplayName(), m_item) : typedName;
    after.itemName = (m_initialItemName.isEmpty() && typedName == m_initialDisplayName) ? QString() : effectiveName;
    after.stack.append({m_item, targetLayer, targetHeight});
    for (CanvasItem* peer : std::as_const(swapped)) {
        after.stack.append({peer, oldLayer, oldHeight});
    }

    if (auto* shape = dynamic_cast<ShapeAbstractItem*>(m_item.data())) {
        before.hasShapeState = true;
        before.shapeState = shape->State();

        after.hasShapeState = true;
        after.shapeState = shape->State();
        after.shapeState.pos = after.pos;
        after.shapeState.size = after.size;
        if (style) {
            // 填充 / 描线两行直接绑定属性基类副本，确定时整份状态取回，不再逐控件拼值。
            // 只描线的图形没有填充行：副本保持图元原值，不产生额外修改。
            if (m_hasFill) {
                after.shapeState.fill = style->FillState();
            }
            after.shapeState.stroke = style->StrokeState();
        }
        if (style && style->OpacitySpin()) {
            const int percent = style->OpacitySpin()->value();
            after.shapeState.opacity =
                percent == style->InitialOpacityPercent() ? style->InitialOpacity() : percent / 100.0;
            after.opacity = after.shapeState.opacity;
        }
        if (style && style->StartAngleSpin()) {
            const int value = style->StartAngleSpin()->value();
            after.shapeState.propertyExtras.insert(
                ItemProperty::START_ANGLE,
                value == qRound(style->InitialStartAngle()) ? style->InitialStartAngle() : static_cast<qreal>(value));
        }
        if (style && style->SweepAngleSpin()) {
            const int value = style->SweepAngleSpin()->value();
            after.shapeState.propertyExtras.insert(
                ItemProperty::SWEEP_ANGLE,
                value == qRound(style->InitialSweepAngle()) ? style->InitialSweepAngle() : static_cast<qreal>(value));
        }
        if (style && style->CornerRadiusSpin()) {
            const int value = style->CornerRadiusSpin()->value();
            // 圆角矩形默认值来自短边的 16%，对话框里显示为非 0；
            // 未改动时保留“按类型取默认”的 -1，避免打开属性窗点确定就写回显式值。
            after.shapeState.cornerRadius =
                value == style->InitialCornerRadius() ? style->InitialStoredCornerRadius() : value;
        }
        // 需求 1：三角形的两个底角为真相，未改动时保留原值（含非整数角度）。
        if (style && style->BottomAngle1Spin()) {
            const int value = style->BottomAngle1Spin()->value();
            after.shapeState.bottomAngle1 =
                value == qRound(style->InitialBottomAngle1()) ? style->InitialBottomAngle1() : value;
        }
        if (style && style->BottomAngle2Spin()) {
            const int value = style->BottomAngle2Spin()->value();
            after.shapeState.bottomAngle2 =
                value == qRound(style->InitialBottomAngle2()) ? style->InitialBottomAngle2() : value;
        }
        // 需求 9：平行四边形左下内角，未改动时保留原值。
        if (style && style->ParallelogramAngleSpin()) {
            const int value = style->ParallelogramAngleSpin()->value();
            after.shapeState.parallelogramAngle =
                value == qRound(style->InitialParallelogramAngle()) ? style->InitialParallelogramAngle() : value;
        }
        // 只改底角、不动尺寸时保持底边长度：按新角度重算包围盒，而不是沿用旧盒宽。
        const bool triangleAnglesChanged =
            !qFuzzyCompare(before.shapeState.bottomAngle1 + 2.0, after.shapeState.bottomAngle1 + 2.0) ||
            !qFuzzyCompare(before.shapeState.bottomAngle2 + 2.0, after.shapeState.bottomAngle2 + 2.0);
        if (shape->shapeType() == ShapeType::TRIANGLE && !shape->IsCustomShape() && triangleAnglesChanged &&
            after.size == before.size) {
            after.size = TriangleGeometry::SizeForAngles(before.size, before.shapeState.bottomAngle1,
                                                         before.shapeState.bottomAngle2, after.shapeState.bottomAngle1,
                                                         after.shapeState.bottomAngle2);
            after.shapeState.size = after.size;
        }
    } else if (m_item->Category() == ItemCategory::TEXT) {
        // 文字属性走属性协议：正文 / 颜色 / 字体各按 ItemProperty 提交，
        // 尺寸与位置仍由同一份快照回放，整次编辑只占一条撤销记录。
        before.textParams.insert(ItemProperty::TEXT_CONTENT, m_item->PropertyValue(ItemProperty::TEXT_CONTENT));
        before.textParams.insert(ItemProperty::TEXT_COLOR, m_item->PropertyValue(ItemProperty::TEXT_COLOR));
        before.textParams.insert(ItemProperty::TEXT_FONT, m_item->PropertyValue(ItemProperty::TEXT_FONT));
        after.textParams = before.textParams;

        const QFont font = text ? text->TextFont() : m_item->PropertyValue(ItemProperty::TEXT_FONT).value<QFont>();
        after.textParams.insert(ItemProperty::TEXT_FONT, font);
        if (text && text->TextEdit()) {
            after.textParams.insert(ItemProperty::TEXT_CONTENT, text->TextEdit()->toPlainText());
        }
        const QString textBefore = before.textParams.value(ItemProperty::TEXT_CONTENT).toString();
        const QString textAfter = after.textParams.value(ItemProperty::TEXT_CONTENT).toString();
        // 需求追加：文字正文变化后直接回到文字自然尺寸，不再沿用旧图元的
        // 拉伸尺寸，避免新旧字形按不同比例叠在同一块内容区里。
        if (textAfter != textBefore) {
            after.size = TextItem::Measure(textAfter, font);
        }
        if (text && text->TextColorButton()) {
            after.textParams.insert(ItemProperty::TEXT_COLOR,
                                    text->TextColorButton()->property("itemColor").value<QColor>());
        }
        // 不透明度是所有图元共有的属性：文字在这里与图形共用同一行提交流程。
        if (style && style->OpacitySpin()) {
            const int percent = style->OpacitySpin()->value();
            after.opacity = percent == style->InitialOpacityPercent() ? style->InitialOpacity() : percent / 100.0;
        }
    } else if (auto* stroke = dynamic_cast<LineAbstractItem*>(m_item.data())) {
        before.hasStrokeStyle = true;
        before.strokeColor = stroke->StrokeColor();
        before.strokeWidth = stroke->StrokeWidth();
        before.strokeStyle = stroke->strokePattern();
        // 需求 27：曲线两端箭头与描线一起进入同一条撤销记录；箭头对全部曲线子型开放。
        before.curveStartArrow = stroke->StartArrow();
        before.curveEndArrow = stroke->EndArrow();

        const StrokeAttributesState beforeStroke{before.strokeColor, before.strokeWidth, before.strokeStyle};
        const StrokeAttributesState strokeState = style ? style->StrokeState() : beforeStroke;
        after.hasStrokeStyle = true;
        after.strokeColor = strokeState.color;
        after.strokeWidth = strokeState.width;
        after.strokeStyle = strokeState.style;
        // 不透明度行在样式区（紧跟描线宽度），与图形同一份提交流程。
        if (style && style->OpacitySpin()) {
            const int percent = style->OpacitySpin()->value();
            after.opacity = percent == style->InitialOpacityPercent() ? style->InitialOpacity() : percent / 100.0;
        }
        after.curveStartArrow = (style && style->StartArrowCombo())
                                    ? static_cast<ArrowStyle>(style->StartArrowCombo()->currentData().toInt())
                                    : before.curveStartArrow;
        after.curveEndArrow = (style && style->EndArrowCombo())
                                  ? static_cast<ArrowStyle>(style->EndArrowCombo()->currentData().toInt())
                                  : before.curveEndArrow;
        // 弧线角度 / 正弦周期振幅：改过才写入快照，未改动的行沿用原值。
        // 参数一律按 ItemProperty 键读写，基类不再提供叶子类型的参数快照。
        const ItemPropertySet strokeProperties = stroke->Properties();
        if (strokeProperties.Contains(ItemProperty::START_ANGLE) &&
            strokeProperties.Contains(ItemProperty::SWEEP_ANGLE)) {
            before.curveParams.insert(ItemProperty::START_ANGLE, stroke->PropertyValue(ItemProperty::START_ANGLE));
            before.curveParams.insert(ItemProperty::SWEEP_ANGLE, stroke->PropertyValue(ItemProperty::SWEEP_ANGLE));
            after.curveParams = before.curveParams;
            if (style && style->ArcStartAngleSpin() && style->ArcSweepAngleSpin()) {
                const qreal startBefore = before.curveParams.value(ItemProperty::START_ANGLE).toDouble();
                const qreal sweepBefore = before.curveParams.value(ItemProperty::SWEEP_ANGLE).toDouble();
                after.curveParams.insert(ItemProperty::START_ANGLE,
                                         style->ArcStartAngleSpin()->value() == qRound(startBefore)
                                             ? startBefore
                                             : style->ArcStartAngleSpin()->value());
                after.curveParams.insert(ItemProperty::SWEEP_ANGLE,
                                         style->ArcSweepAngleSpin()->value() == qRound(sweepBefore)
                                             ? sweepBefore
                                             : style->ArcSweepAngleSpin()->value());
            }
            // 弧线长短半轴：与角度同一份快照，未改动时 before == after 自动跳过。
            if (geometry && geometry->MajorAxisSpin()) {
                before.hasArcAxes = true;
                before.arcMajor = geometry->InitialArcMajor();
                before.arcMinor = geometry->InitialArcMinor();
                after.hasArcAxes = true;
                after.arcMajor = geometry->MajorAxisSpin()->value();
                after.arcMinor = geometry->MinorAxisSpin()->value();
            }
        } else if (strokeProperties.Contains(ItemProperty::SINE_CYCLES) &&
                   strokeProperties.Contains(ItemProperty::SINE_AMPLITUDE)) {
            before.curveParams.insert(ItemProperty::SINE_CYCLES, stroke->PropertyValue(ItemProperty::SINE_CYCLES));
            before.curveParams.insert(ItemProperty::SINE_AMPLITUDE,
                                      stroke->PropertyValue(ItemProperty::SINE_AMPLITUDE));
            after.curveParams = before.curveParams;
            if (style && style->SineCyclesSpin() && style->SineAmplitudeSpin()) {
                const int cyclesBefore = before.curveParams.value(ItemProperty::SINE_CYCLES).toInt();
                const qreal amplitudeBefore = before.curveParams.value(ItemProperty::SINE_AMPLITUDE).toDouble();
                after.curveParams.insert(ItemProperty::SINE_CYCLES, style->SineCyclesSpin()->value() == cyclesBefore
                                                                        ? cyclesBefore
                                                                        : style->SineCyclesSpin()->value());
                after.curveParams.insert(ItemProperty::SINE_AMPLITUDE,
                                         style->SineAmplitudeSpin()->value() == qRound(amplitudeBefore)
                                             ? amplitudeBefore
                                             : style->SineAmplitudeSpin()->value());
            }
        }
    }

    if (before == after) {
        return;
    }

    auto* command = new ItemPropertiesCommand(m_scene, m_item, before, after, tr("Edit item properties"));
    if (auto* undo = m_scene->UndoStack()) {
        undo->push(command);
    } else {
        command->redo();
        delete command;
    }
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
