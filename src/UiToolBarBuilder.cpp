#include "UiToolBarBuilder.h"

#include "CanvasView.h"
#include "IconFactory.h"
#include "MainWindow.h"
#include "StyleController.h"

#include <QActionGroup>
#include <QColor>
#include <QComboBox>
#include <QLabel>
#include <QMenu>
#include <QSignalBlocker>
#include <QSize>
#include <QSpinBox>
#include <QToolBar>
#include <QToolButton>
#include <utility>

namespace {

QList<int> brushWidthPresets()
{
    return {1, 2, 3, 4, 6, 8, 10, 12, 16, 20, 24, 32, 40, 48, 64};
}

} // namespace

UiToolBarBuilder::UiToolBarBuilder(MainWindow* window) : m_window(window) {}

void UiToolBarBuilder::buildToolBars()
{
    auto* mainBar = new QToolBar(MainWindow::tr("Main toolbar"), m_window);
    mainBar->setObjectName(QStringLiteral("MainToolBar"));
    mainBar->setMovable(false);
    mainBar->setFloatable(false);
    mainBar->setIconSize(QSize(MainWindow::kToolBarIconSize, MainWindow::kToolBarIconSize));
    mainBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    mainBar->setContextMenuPolicy(Qt::PreventContextMenu);
    m_window->addToolBar(Qt::TopToolBarArea, mainBar);

    mainBar->addAction(m_window->m_newAct);
    mainBar->addAction(m_window->m_openAct);
    // 需求：线稿化入口放在“保存画布”按钮前面，和文件菜单里的位置对应。
    mainBar->addAction(m_window->m_lineArtAct);
    mainBar->addAction(m_window->m_saveAct);
    mainBar->addAction(m_window->m_exportAct);
    mainBar->addSeparator();

    // 图形区：常见图形直接平铺成一排，后面接填充 / 描线颜色与描线宽度。
    createShapeToolbar(mainBar);
    mainBar->addSeparator();

    // 对齐与化形留在菜单栏与编辑图元卡里，主工具栏不再重复放置入口。
    // 画笔区只留下平滑笔画按钮和容差输入；画笔颜色 / 粗细改由图形描线区统一设置。
    createSmoothingToolbar(mainBar);
    // 布尔运算的右键菜单入口照旧保留，只是不再作为工具栏下拉按钮。
    if (m_window->m_view) {
        m_window->m_view->addContextMenuGroup(MainWindow::tr("Morph"), m_window->m_boolActions);
    }

    // 统一主工具栏里所有纯图标按钮的尺寸：QToolBar 为 action 自动创建的按钮、
    // 以及 addWidget 加入的色板/填充按钮，都钉成同一个正方形，避免按各自
    // sizeHint 算出宽窄不一的方块。带文字或下拉菜单的按钮保持自身宽度。
    for (QToolButton* button : mainBar->findChildren<QToolButton*>()) {
        if (button->toolButtonStyle() != Qt::ToolButtonIconOnly) {
            continue;
        }
        button->setFixedSize(MainWindow::kToolBarButtonSize, MainWindow::kToolBarButtonSize);
    }

    auto* modeBar = new QToolBar(MainWindow::tr("Mode"), m_window);
    modeBar->setObjectName(QStringLiteral("ModeToolBar"));
    modeBar->setMovable(false);
    modeBar->setFloatable(false);
    modeBar->setIconSize(QSize(MainWindow::kToolBarIconSize, MainWindow::kToolBarIconSize));
    modeBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    modeBar->setContextMenuPolicy(Qt::PreventContextMenu);
    modeBar->setMinimumWidth(80);
    m_window->addToolBar(Qt::LeftToolBarArea, modeBar);
    modeBar->addAction(m_window->m_viewModeAct);
    modeBar->addAction(m_window->m_editModeAct);
    modeBar->addAction(m_window->m_brushModeAct);
    modeBar->addAction(m_window->m_textModeAct);
}

void UiToolBarBuilder::createSmoothingToolbar(QToolBar* mainBar)
{
    auto* smoothButton = new QToolButton(mainBar);
    smoothButton->setObjectName(QStringLiteral("SmoothStrokeButton"));
    smoothButton->setDefaultAction(m_window->m_smoothAct);
    // 这个按钮由工具栏 addWidget 加入，不会自动继承 QToolBar::iconSize，
    // 必须显式设置，否则图标会按样式默认尺寸绘制、比旁边按钮大一圈。
    smoothButton->setIconSize(QSize(MainWindow::kToolBarIconSize, MainWindow::kToolBarIconSize));
    smoothButton->setFixedHeight(MainWindow::kToolBarButtonSize);
    // 只显示图标，与相邻按钮等宽；功能靠悬停提示说明。
    smoothButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    smoothButton->setToolTip(MainWindow::tr("Smooth strokes once"));
    mainBar->addWidget(smoothButton);

    auto* smoothingLabel = new QLabel(MainWindow::tr("Tolerance"), mainBar);
    // 紧跟图标按钮的标签，留白收到最小，避免和按钮内边距叠成一条缝。
    smoothingLabel->setObjectName(QStringLiteral("ToolBarLabelTight"));
    mainBar->addWidget(smoothingLabel);

    m_window->m_smoothingSpin = new QSpinBox(mainBar);
    m_window->m_smoothingSpin->setObjectName(QStringLiteral("BrushSmoothingSpin"));
    m_window->m_smoothingSpin->setRange(0, 30);
    m_window->m_smoothingSpin->setSuffix(QStringLiteral(" px"));
    m_window->m_smoothingSpin->setFixedWidth(64);
    m_window->m_smoothingSpin->setFixedHeight(MainWindow::kToolBarControlHeight);
    m_window->m_smoothingSpin->setValue(m_window->m_settings.strokeSmoothing);
    m_window->m_smoothingSpin->setToolTip(
        MainWindow::tr("Auto-smoothing tolerance used when a stroke ends; 0 disables automatic smoothing"));
    QObject::connect(m_window->m_smoothingSpin, &QSpinBox::valueChanged, m_window, [this](int value) {
        m_window->m_settings.strokeSmoothing = value;
        m_window->m_view->setStrokeSmoothing(value);
        m_window->m_settings.save();
    });
    mainBar->addWidget(m_window->m_smoothingSpin);
}

void UiToolBarBuilder::createShapeToolbar(QToolBar* mainBar)
{
    // 「图形」区：常见图形在原下拉入口的位置直接平铺成一排按钮，
    // 复用「插入 → 常见图形」同一批动作，避免两套插入逻辑。
    for (QAction* action : m_window->m_shapeMenu->actions()) {
        if (!action || action->isSeparator() || !action->objectName().startsWith(QStringLiteral("ShapeAction_"))) {
            continue;
        }
        auto* shapeButton = new QToolButton(mainBar);
        shapeButton->setObjectName(QStringLiteral("ToolBarShapeButton"));
        shapeButton->setProperty("shapeActionName", action->objectName());
        shapeButton->setIcon(action->icon());
        shapeButton->setIconSize(QSize(MainWindow::kToolBarIconSize, MainWindow::kToolBarIconSize));
        shapeButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
        shapeButton->setToolTip(action->toolTip());
        QObject::connect(shapeButton, &QToolButton::clicked, action, &QAction::trigger);
        mainBar->addWidget(shapeButton);
    }

    // 常见图形之后追加自定义图形下拉，和「插入 → 常见图形」里的同一份目录菜单。
    auto* customShapeButton = new QToolButton(mainBar);
    customShapeButton->setObjectName(QStringLiteral("ToolBarCustomShapeButton"));
    customShapeButton->setIcon(makeIcon(IconId::Shapes, QColor(58, 64, 80), MainWindow::kToolBarIconSize));
    customShapeButton->setIconSize(QSize(MainWindow::kToolBarIconSize, MainWindow::kToolBarIconSize));
    customShapeButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    customShapeButton->setPopupMode(QToolButton::InstantPopup);
    customShapeButton->setToolTip(MainWindow::tr("Custom shapes"));
    customShapeButton->setMenu(m_window->m_customShapeMenu);
    mainBar->addWidget(customShapeButton);

    m_window->m_fillButton = new QToolButton(mainBar);
    m_window->m_fillButton->setObjectName(QStringLiteral("FillButton"));
    m_window->m_fillButton->setIconSize(QSize(MainWindow::kToolBarIconSize, MainWindow::kToolBarIconSize));
    m_window->m_fillButton->setPopupMode(QToolButton::InstantPopup);
    auto* fillMenu = new QMenu(m_window->m_fillButton);
    fillMenu->addAction(m_window->m_fillNoneAct);
    fillMenu->addAction(m_window->m_fillSolidAct);
    fillMenu->addAction(m_window->m_fillHachureAct);
    fillMenu->addAction(m_window->m_fillReverseAct);
    fillMenu->addAction(m_window->m_fillCrossAct);
    fillMenu->addAction(m_window->m_fillGridAct);
    fillMenu->addAction(m_window->m_fillHoneycombAct);
    m_window->m_fillButton->setMenu(fillMenu);
    m_window->updateFillActions();
    mainBar->addWidget(m_window->m_fillButton);

    // 需求 29：填充颜色、描边颜色作为独立按钮，排在填充样式按钮之后。
    m_window->m_fillColorButton = new QToolButton(mainBar);
    m_window->m_fillColorButton->setObjectName(QStringLiteral("FillColorButton"));
    m_window->m_fillColorButton->setIconSize(QSize(MainWindow::kToolBarIconSize, MainWindow::kToolBarIconSize));
    m_window->m_fillColorButton->setDefaultAction(m_window->m_fillColorAct);
    m_window->m_fillColorButton->setToolTip(MainWindow::tr("Fill color: %1 (click to choose)")
                                                .arg(m_window->m_fillColor.isValid()
                                                         ? m_window->m_fillColor.name(QColor::HexArgb).toUpper()
                                                         : MainWindow::tr("Follow brush color")));
    mainBar->addWidget(m_window->m_fillColorButton);

    m_window->m_strokeColorButton = new QToolButton(mainBar);
    m_window->m_strokeColorButton->setObjectName(QStringLiteral("StrokeColorButton"));
    m_window->m_strokeColorButton->setIconSize(QSize(MainWindow::kToolBarIconSize, MainWindow::kToolBarIconSize));
    m_window->m_strokeColorButton->setDefaultAction(m_window->m_strokeColorAct);
    // 菜单项图标是 16px，工具栏按钮另画一份 20px 的，避免比旁边的图标小一圈。
    m_window->m_strokeColorButton->setIcon(
        m_window->m_style->strokeColorIcon(m_window->m_brushColor, MainWindow::kToolBarIconSize));
    m_window->m_strokeColorButton->setToolTip(MainWindow::tr("Border color: %1 (click to choose)")
                                                  .arg(m_window->m_brushColor.name(QColor::HexArgb).toUpper()));
    mainBar->addWidget(m_window->m_strokeColorButton);

    // 描线颜色按钮后紧跟描线宽度下拉，改宽度与改描线宽度走同一套设置。
    m_window->m_widthCombo = new QComboBox(mainBar);
    m_window->m_widthCombo->setObjectName(QStringLiteral("BrushWidthCombo"));
    m_window->m_widthCombo->setFixedWidth(74);
    m_window->m_widthCombo->setFixedHeight(MainWindow::kToolBarControlHeight);
    m_window->m_widthCombo->setToolTip(MainWindow::tr("Brush width"));
    for (int width : brushWidthPresets()) {
        m_window->m_widthCombo->addItem(QStringLiteral("%1 px").arg(width), width);
    }
    QObject::connect(m_window->m_widthCombo, &QComboBox::currentIndexChanged, m_window, [this](int index) {
        const int value = m_window->m_widthCombo->itemData(index).toInt();
        if (value <= 0) {
            return;
        }
        m_window->m_brushWidth = value;
        m_window->m_view->setBrushWidth(value);
        m_window->m_settings.brushWidth = value;
        m_window->m_settings.save();
        if (m_window->m_brushWidthGroup) {
            for (QAction* action : m_window->m_brushWidthGroup->actions()) {
                if (action->data().toInt() == value) {
                    const QSignalBlocker blocker(action);
                    action->setChecked(true);
                }
            }
        }
    });
    m_window->setBrushWidthUi(m_window->m_brushWidth);
    mainBar->addWidget(m_window->m_widthCombo);
}
