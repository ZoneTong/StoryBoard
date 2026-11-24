#include "app/FullScreenBar.h"

#include "canvas/CanvasView.h"
#include "ui/icons/IconFactory.h"
#include "ui/mainwindow/MainWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QMenu>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>

namespace {

// 9 个图形按钮的键位 → 菜单动作对象名。数字键只在全屏期绑定，不进改键表。
struct ShapeBinding {
    const char* keyText;
    const char* actionName;
};

const ShapeBinding SHAPE_BINDINGS[] = {
    {"0", "ShapeAction_ellipse"},     {"1", "ShapeAction_line"},        {"2", "ShapePreset_lineArrow"},
    {"3", "ShapeAction_triangle"},    {"4", "ShapePreset_roundedRect"}, {"5", "ShapeAction_star"},
    {"6", "ShapeAction_diamond"},     {"7", "ShapeAction_heart"},       {"8", "ShapePreset_braceLeft"},
};

} // namespace

FullScreenBar::FullScreenBar(MainWindow* window) : QWidget(window), m_window(window)
{
    setObjectName(QStringLiteral("FullScreenBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral("#FullScreenBar { background: rgba(255, 255, 255, 235);"
                                 " border: 1px solid rgba(15, 23, 42, 40); border-radius: 10px; }"));
    setFixedHeight(BAR_HEIGHT);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(4);

    for (const ShapeBinding& binding : SHAPE_BINDINGS) {
        QToolButton* button =
            AddShapeButton(QString::fromLatin1(binding.actionName), QString::fromLatin1(binding.keyText));
        m_digitButtons.append(button);
        layout->addWidget(button);
    }

    // 自定义图形：点按钮 / 按数字键 9 直接打开自定义图形面板，不再展开模板子菜单。
    m_customShapeButton = new QToolButton(this);
    m_customShapeButton->setObjectName(QStringLiteral("FullScreenCustomShapeButton"));
    m_customShapeButton->setIconSize(QSize(BAR_ICON_SIZE, BAR_ICON_SIZE));
    m_customShapeButton->setFixedSize(46, BAR_HEIGHT - 8);
    m_customShapeButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    m_customShapeButton->setText(QStringLiteral("9"));
    m_customShapeButton->setToolTip(MainWindow::tr("Custom shapes (9)"));
    m_customShapeButton->setIcon(IconFactory::Make(IconId::SHAPES, QColor(58, 64, 80), BAR_ICON_SIZE));
    QObject::connect(m_customShapeButton, &QToolButton::clicked, m_window,
                     [this]() { m_window->OpenCustomShapePanel(m_customShapeButton); });
    layout->addWidget(m_customShapeButton);
    m_digitButtons.append(m_customShapeButton);

    // 行序与主工具栏一致：填充样式 → 填充颜色 → 描线样式 → 描线颜色 → 描线宽度，
    // 描线颜色紧贴宽度输入框，全屏下改描线不用跨过别的按钮。
    layout->addSpacing(6);
    m_fillStyleButton = AddStyleButton(QStringLiteral("FullScreenFillStyleButton"));
    layout->addWidget(m_fillStyleButton);
    m_fillColorButton = AddStyleButton(QStringLiteral("FullScreenFillColorButton"));
    layout->addWidget(m_fillColorButton);
    m_strokeStyleButton = AddStyleButton(QStringLiteral("FullScreenStrokeStyleButton"));
    layout->addWidget(m_strokeStyleButton);
    m_strokeColorButton = AddStyleButton(QStringLiteral("FullScreenStrokeColorButton"));
    layout->addWidget(m_strokeColorButton);

    // 样式按钮复用主工具栏的同一批菜单与动作，全屏里改的就是主工具栏那份状态。
    auto* fillMenu = new QMenu(m_fillStyleButton);
    if (m_window->m_fillStyleGroup) {
        fillMenu->addActions(m_window->m_fillStyleGroup->actions());
    }
    m_fillStyleButton->setMenu(fillMenu);
    m_fillStyleButton->setPopupMode(QToolButton::InstantPopup);
    if (m_window->m_strokeStyleMenu) {
        m_strokeStyleButton->setMenu(m_window->m_strokeStyleMenu);
        m_strokeStyleButton->setPopupMode(QToolButton::InstantPopup);
    }
    QObject::connect(m_fillColorButton, &QToolButton::clicked, m_window, [this]() { m_window->ChooseFillColor(); });
    QObject::connect(m_strokeColorButton, &QToolButton::clicked, m_window, [this]() { m_window->ChooseStrokeColor(); });

    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setObjectName(QStringLiteral("FullScreenStrokeWidthSpin"));
    m_widthSpin->setRange(1, 64);
    m_widthSpin->setFixedSize(74, 30);
    m_widthSpin->setToolTip(MainWindow::tr("Stroke width"));
    layout->addWidget(m_widthSpin);

    layout->addSpacing(6);
    m_modeBadge = AddStyleButton(QStringLiteral("FullScreenModeBadge"));
    m_modeBadge->setFixedSize(34, 30);
    layout->addWidget(m_modeBadge);

    adjustSize();
    hide();

    QObject::connect(m_widthSpin, &QSpinBox::valueChanged, m_window,
                     [this](int value) { m_window->SetBrushWidthUi(value); });
    if (m_window->m_widthSpin) {
        QObject::connect(m_window->m_widthSpin, &QSpinBox::valueChanged, this, [this](int value) {
            const QSignalBlocker blocker(m_widthSpin);
            m_widthSpin->setValue(value);
        });
    }
    if (m_window->m_view) {
        QObject::connect(m_window->m_view, &CanvasView::modeChanged, this, [this](ToolMode) { UpdateModeBadge(); });
    }
}

QToolButton* FullScreenBar::AddShapeButton(const QString& actionName, const QString& keyText)
{
    auto* button = new QToolButton(this);
    button->setObjectName(QStringLiteral("FullScreenShapeButton") + keyText);
    button->setIconSize(QSize(BAR_ICON_SIZE, BAR_ICON_SIZE));
    button->setFixedSize(46, BAR_HEIGHT - 8);
    // 文本画在图标下方：全屏栏里只放键位字符，动作名走悬停提示。
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setText(keyText);

    QAction* action = m_window->m_shapeMenu ? m_window->m_shapeMenu->findChild<QAction*>(actionName) : nullptr;
    if (action) {
        button->setIcon(action->icon());
        button->setToolTip(MainWindow::tr("%1 (key %2)").arg(action->text(), keyText));
        // 不挂 setDefaultAction：action 的 16px 图标会覆盖这里的 26px。
        QObject::connect(button, &QToolButton::clicked, action, [action]() { action->trigger(); });
    }
    return button;
}

QToolButton* FullScreenBar::AddStyleButton(const QString& objectName)
{
    auto* button = new QToolButton(this);
    button->setObjectName(objectName);
    button->setIconSize(QSize(20, 20));
    button->setFixedSize(34, 30);
    button->setAutoRaise(true);
    return button;
}

void FullScreenBar::SyncFromWindow()
{
    if (m_window->m_fillButton && m_fillStyleButton) {
        m_fillStyleButton->setIcon(m_window->m_fillButton->icon());
        m_fillStyleButton->setToolTip(m_window->m_fillButton->toolTip());
    }
    if (m_fillColorButton) {
        m_fillColorButton->setIcon(IconFactory::MakeColor(m_window->m_fillColor, 20));
        m_fillColorButton->setToolTip(MainWindow::tr("Fill color: %1 (click to choose)").arg(m_window->m_fillColor.name()));
    }
    if (m_window->m_strokeStyleButton && m_strokeStyleButton) {
        m_strokeStyleButton->setIcon(m_window->m_strokeStyleButton->icon());
        m_strokeStyleButton->setToolTip(m_window->m_strokeStyleButton->toolTip());
    }
    if (m_strokeColorButton) {
        m_strokeColorButton->setIcon(IconFactory::MakeColor(m_window->m_brushColor, 20));
        m_strokeColorButton->setToolTip(MainWindow::tr("Stroke color: %1").arg(m_window->m_brushColor.name()));
    }
    if (m_widthSpin) {
        const QSignalBlocker blocker(m_widthSpin);
        m_widthSpin->setValue(m_window->m_brushWidth);
    }
    UpdateModeBadge();
    adjustSize();
}

void FullScreenBar::UpdateModeBadge()
{
    if (!m_modeBadge || !m_window->m_view) {
        return;
    }
    QAction* modeAction = nullptr;
    if (m_window->m_modeGroup) {
        for (QAction* action : m_window->m_modeGroup->actions()) {
            if (action->isChecked()) {
                modeAction = action;
                break;
            }
        }
    }
    m_modeBadge->setText(modeAction ? modeAction->shortcut().toString(QKeySequence::NativeText) : QString());
    if (modeAction) {
        m_modeBadge->setToolTip(MainWindow::tr("%1 — click to switch mode").arg(modeAction->toolTip()));
    }
    // 徽标点开就是模式菜单，全屏下照样能换模式。
    if (m_window->m_modeGroup) {
        auto* menu = new QMenu(m_modeBadge);
        menu->addActions(m_window->m_modeGroup->actions());
        m_modeBadge->setMenu(menu);
        m_modeBadge->setPopupMode(QToolButton::InstantPopup);
    }
}

void FullScreenBar::PlaceAt(int centerX, int topInset)
{
    const int width = sizeHint().width();
    setGeometry(centerX - width / 2, topInset, width, BAR_HEIGHT);
}

bool FullScreenBar::TriggerDigit(int digit)
{
    if (digit < 0 || digit >= m_digitButtons.size()) {
        return false;
    }
    QToolButton* button = m_digitButtons.at(digit);
    if (!button) {
        return false;
    }
    // 自定义图形按钮点击即打开面板；其余按钮触发对应动作。
    button->click();
    return true;
}
