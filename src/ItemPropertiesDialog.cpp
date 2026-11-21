#include "ItemPropertiesDialog.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "CanvasView.h"
#include "Commands.h"
#include "ItemPropertiesDialogGeometryBlock.h"
#include "ItemPropertiesDialogIdentityBlock.h"
#include "ItemPropertiesDialogStyleBlock.h"
#include "ItemPropertiesDialogTextBlock.h"
#include "LineItemAbstract.h"
#include "ShapeItem.h"
#include "TextItem.h"
#include "TriangleGeometry.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFileDialog>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QHideEvent>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPointer>
#include <QPolygon>
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
    : QDialog(parent), m_scene(scene), m_item(item), m_editable(editable)
{
    setWindowTitle(tr("Item properties"));
    // 需求 42：属性窗统一改为模态；非编辑模式可打开查看，但字段只读。
    setModal(true);
    setMinimumWidth(480);
    // 模态窗会先吃掉窗外的鼠标按下，必须由应用级过滤器在事件分发前识别。
    qApp->installEventFilter(this);

    // 打开属性窗时再兜底一次自动名称：旧画布、直接 addItem 或清空名称后，
    // 这里也能得到“类型 N”，不会出现看不到名称或名称为空的行。
    if (scene) {
        scene->AssignAutoItemNames();
    }

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 16);
    root->setSpacing(12);

    auto* heading = new QLabel(item ? item->Name() : tr("No item selected"), this);
    heading->setObjectName(QStringLiteral("ItemPropertiesHeading"));
    root->addWidget(heading);

    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    root->addWidget(line);

    if (!m_editable) {
        auto* readOnlyHint = new QLabel(tr("Read-only: switch to Edit mode to change properties."), this);
        readOnlyHint->setObjectName(QStringLiteral("ItemPropertiesReadOnlyHint"));
        readOnlyHint->setWordWrap(true);
        root->addWidget(readOnlyHint);
    }

    auto* form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(9);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    if (!item) {
        auto* typeValue = new QLabel(QStringLiteral("—"), this);
        typeValue->setObjectName(QStringLiteral("ItemPropertiesTypeValue"));
        form->addRow(tr("Item type"), typeValue);
        root->addLayout(form);
    } else {
        m_initialPos = item->pos();
        m_initialSize = item->ContentSize();
        m_initialRotation = item->RotationAngle();
        m_initialItemName = item->CustomName();
        m_initialDisplayName = item->Name();
        auto* typeValue = new QLabel(item->TypeDisplayName(), this);
        typeValue->setObjectName(QStringLiteral("ItemPropertiesTypeValue"));
        typeValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
        form->addRow(tr("Item type"), typeValue);

        m_identityBlock = new ItemPropertiesDialogIdentityBlock(scene, item, this);
        m_identityBlock->AddRows(form);

        if (dynamic_cast<ShapeItem*>(item) || dynamic_cast<LineItemAbstract*>(item)) {
            m_styleBlock = new ItemPropertiesDialogStyleBlock(item, this);
            m_styleBlock->AddRows(form);
            m_hasFill = m_styleBlock->HasFill();
            m_initialStrokeWidth = m_styleBlock->InitialStrokeWidth();
        } else if (item->HasTextFacet()) {
            m_textBlock = new ItemPropertiesDialogTextBlock(item, this);
            m_textBlock->AddRows(form);
            connect(m_textBlock, &ItemPropertiesDialogTextBlock::fontChanged, this,
                    &ItemPropertiesDialog::SyncTextNaturalSize);
        }

        // 图形在样式区已有透明度行；笔画、文字、图片等图元在这里统一补一行。
        if (!dynamic_cast<ShapeItem*>(item)) {
            m_opacitySpin = new QSpinBox(this);
            m_opacitySpin->setObjectName(QStringLiteral("ItemPropertiesOpacityValue"));
            m_opacitySpin->setRange(0, 100);
            m_opacitySpin->setSuffix(QStringLiteral(" %"));
            m_opacitySpin->setMinimumWidth(88);
            m_initialOpacity = item->ItemOpacity();
            m_opacitySpin->setValue(qRound(m_initialOpacity * 100.0));
            m_initialOpacityPercent = m_opacitySpin->value();
            form->addRow(tr("Opacity"), m_opacitySpin);
        }

        m_geometryBlock = new ItemPropertiesDialogGeometryBlock(item, this);
        m_geometryBlock->AddRows(form);
        if (m_styleBlock && m_styleBlock->CornerRadiusSpin()) {
            auto syncCornerRange = [this]() {
                if (!m_geometryBlock) {
                    return;
                }
                m_styleBlock->SetContentSize(
                    QSizeF(m_geometryBlock->WidthSpin()->value(), m_geometryBlock->HeightSizeSpin()->value()));
            };
            connect(m_geometryBlock->WidthSpin(), &QDoubleSpinBox::valueChanged, this, syncCornerRange);
            connect(m_geometryBlock->HeightSizeSpin(), &QDoubleSpinBox::valueChanged, this, syncCornerRange);
        }

        root->addLayout(form);
    }

    // 文字正文直接在属性窗里编辑；内容变化时同步到自然尺寸，确定后由
    // ItemPropertiesCommand 一次性提交文字、尺寸和撤销记录。
    if (m_textBlock && m_textBlock->TextEdit()) {
        connect(m_textBlock->TextEdit(), &QPlainTextEdit::textChanged, this,
                &ItemPropertiesDialog::SyncTextNaturalSize);
    }

    ApplyEditableState();

    auto* buttons = new QDialogButtonBox(
        m_editable ? (QDialogButtonBox::Ok | QDialogButtonBox::Cancel) : QDialogButtonBox::Ok, this);
    if (auto* ok = buttons->button(QDialogButtonBox::Ok)) {
        ok->setText(m_editable ? tr("OK") : tr("Close"));
    }
    if (auto* cancel = buttons->button(QDialogButtonBox::Cancel)) {
        cancel->setText(tr("Cancel"));
    }
    connect(buttons, &QDialogButtonBox::accepted, this, &ItemPropertiesDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    if (m_identityBlock && m_identityBlock->HasLayerControls()) {
        m_identityBlock->UpdateHeightRange(false);
        connect(m_identityBlock, &ItemPropertiesDialogIdentityBlock::layerChanged, this, [this]() {
            if (m_initializing) {
                return;
            }
            m_identityBlock->UpdateHeightRange(true);
        });
        m_initializing = false;
        m_identityBlock->SetInitialized(false);
    } else {
        m_initializing = false;
    }
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
    if (m_textFocusApplied || !m_editable || !m_textBlock || !m_textBlock->TextEdit()) {
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
    if (m_opacitySpin) {
        widgets << m_opacitySpin;
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
    if (!m_editable || !m_textBlock || !m_textBlock->TextEdit()) {
        return;
    }
    if (m_item->HasTextFacet()) {
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

    if (auto* shape = dynamic_cast<ShapeItem*>(m_item.data())) {
        before.hasShapeState = true;
        before.shapeState = shape->State();

        after.hasShapeState = true;
        after.shapeState = shape->State();
        after.shapeState.pos = after.pos;
        after.shapeState.size = after.size;
        if (style) {
            // 填充 / 描线两行直接绑定属性基类副本，确定时整份状态取回，不再逐控件拼值。
            after.shapeState.fill = style->FillState();
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
            after.shapeState.arc.startAngle =
                value == qRound(style->InitialStartAngle()) ? style->InitialStartAngle() : value;
        }
        if (style && style->SweepAngleSpin()) {
            const int value = style->SweepAngleSpin()->value();
            after.shapeState.arc.sweepAngle =
                value == qRound(style->InitialSweepAngle()) ? style->InitialSweepAngle() : value;
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
        if (m_hasFill && style) {
            after.shapeState.fill = style->FillState();
        }
    } else if (m_item->HasTextFacet()) {
        before.hasTextState = true;
        before.textState = m_item->TextState();

        after.hasTextState = true;
        after.textState = m_item->TextState();
        after.textState.pos = after.pos;
        after.textState.size = after.size;
        after.textState.font = text ? text->TextFont() : m_item->TextFont();
        if (text && text->TextEdit()) {
            after.textState.text = text->TextEdit()->toPlainText();
        }
        after.textState.baseSize = TextItem::Measure(after.textState.text, after.textState.font);
        // 需求追加：文字正文变化后直接回到文字自然尺寸，不再沿用旧图元的
        // 拉伸尺寸，避免新旧字形按不同比例叠在同一块内容区里。
        if (after.textState.text != before.textState.text) {
            after.size = after.textState.baseSize;
            after.textState.size = after.textState.baseSize;
        }
        if (text && text->TextColorButton()) {
            after.textState.color = text->TextColorButton()->property("itemColor").value<QColor>();
        }
    } else if (auto* stroke = dynamic_cast<LineItemAbstract*>(m_item.data())) {
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
        after.curveStartArrow = (style && style->StartArrowCombo())
                                    ? static_cast<ArrowStyle>(style->StartArrowCombo()->currentData().toInt())
                                    : before.curveStartArrow;
        after.curveEndArrow = (style && style->EndArrowCombo())
                                  ? static_cast<ArrowStyle>(style->EndArrowCombo()->currentData().toInt())
                                  : before.curveEndArrow;
        // 弧线角度 / 正弦周期振幅：改过才写入快照，未改动的行沿用原值。
        if (stroke->SupportsArcAngles()) {
            before.hasCurveParam = true;
            before.curveParam = stroke->paramState();
            after.hasCurveParam = true;
            after.curveParam = before.curveParam;
            if (style && style->ArcStartAngleSpin() && style->ArcSweepAngleSpin()) {
                after.curveParam.first = style->ArcStartAngleSpin()->value() == qRound(before.curveParam.first)
                                             ? before.curveParam.first
                                             : style->ArcStartAngleSpin()->value();
                after.curveParam.second = style->ArcSweepAngleSpin()->value() == qRound(before.curveParam.second)
                                              ? before.curveParam.second
                                              : style->ArcSweepAngleSpin()->value();
            }
        } else if (stroke->SupportsSineWave()) {
            before.hasCurveParam = true;
            before.curveParam = stroke->paramState();
            after.hasCurveParam = true;
            after.curveParam = before.curveParam;
            if (style && style->SineCyclesSpin() && style->SineAmplitudeSpin()) {
                after.curveParam.first = style->SineCyclesSpin()->value() == qRound(before.curveParam.first)
                                             ? before.curveParam.first
                                             : style->SineCyclesSpin()->value();
                after.curveParam.second = style->SineAmplitudeSpin()->value() == qRound(before.curveParam.second)
                                              ? before.curveParam.second
                                              : style->SineAmplitudeSpin()->value();
            }
        }
    }

    if (m_opacitySpin) {
        const int percent = m_opacitySpin->value();
        after.opacity = percent == m_initialOpacityPercent ? m_initialOpacity : percent / 100.0;
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
