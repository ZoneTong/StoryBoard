#include "UiToolBarBuilder.h"

#include "CanvasView.h"
#include "IconFactory.h"
#include "MainWindow.h"
#include "StyleController.h"

#include <QColor>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QList>
#include <QMenu>
#include <QSize>
#include <QSpinBox>
#include <QToolBar>
#include <QToolButton>
#include <utility>

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

    // 对齐与合并图元留在菜单栏与编辑图元卡里，主工具栏不再重复放置入口。
    // 画笔区只保留拟合容差输入；画笔颜色 / 粗细由图形描线区统一设置。
    createSmoothingToolbar(mainBar);
    // 布尔运算的右键菜单入口照旧保留，只是不再作为工具栏下拉按钮。
    if (m_window->m_view) {
        m_window->m_view->addContextMenuGroup(MainWindow::tr("Merge items"), m_window->m_boolActions);
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
    auto* smoothingLabel = new QLabel(MainWindow::tr("Brush fit tolerance"), mainBar);
    smoothingLabel->setObjectName(QStringLiteral("ToolBarLabelTight"));
    mainBar->addWidget(smoothingLabel);

    m_window->m_smoothingSpin = new QDoubleSpinBox(mainBar);
    m_window->m_smoothingSpin->setObjectName(QStringLiteral("BrushSmoothingSpin"));
    m_window->m_smoothingSpin->setRange(0.0, 20.0);
    m_window->m_smoothingSpin->setSingleStep(0.1);
    m_window->m_smoothingSpin->setDecimals(1);
    m_window->m_smoothingSpin->setSuffix(QStringLiteral(" px"));
    m_window->m_smoothingSpin->setFixedWidth(74);
    m_window->m_smoothingSpin->setFixedHeight(MainWindow::kToolBarControlHeight);
    m_window->m_smoothingSpin->setValue(m_window->m_settings.strokeSmoothing);
    m_window->m_smoothingSpin->setToolTip(
        MainWindow::tr("Brush fit tolerance in pixels (0–20, 0.1 px steps); 0 disables curve fitting"));
    QObject::connect(m_window->m_smoothingSpin, &QDoubleSpinBox::valueChanged, m_window, [this](qreal value) {
        m_window->m_settings.strokeSmoothing = value;
        m_window->m_view->setStrokeSmoothing(value);
        m_window->m_settings.save();
    });
    mainBar->addWidget(m_window->m_smoothingSpin);
}

void UiToolBarBuilder::createShapeToolbar(QToolBar* mainBar)
{
    // 「图形」区：常见图形与紧随其后的预置项（圆角矩形、45°/60° 直角三角、直线箭头、
    // 左右花括号）按下面的固定顺序平铺成一排，复用「插入 → 常见图形」同一批动作，
    // 避免两套插入逻辑。新图形（需求 11）追加在弧线之后、直线之前；花括号（需求 13）
    // 已改为曲线，菜单里只剩两个预置动作，工具栏按名字显式补在直线之后。
    const QList<ShapeType> shapeOrder = {
        ShapeType::Rectangle, ShapeType::Ellipse,  ShapeType::Triangle, ShapeType::Diamond,  ShapeType::Star,
        ShapeType::Star4,     ShapeType::Hexagram, ShapeType::Heart,    ShapeType::Arrow,    ShapeType::DoubleArrow,
        ShapeType::Callout,   ShapeType::Sector,   ShapeType::Arc,      ShapeType::SineWave, ShapeType::FlagWave,
        ShapeType::Scroll,    ShapeType::Cube,     ShapeType::Cylinder, ShapeType::Line,
    };
    auto addShapeButton = [mainBar](QAction* action) {
        if (!action) {
            return;
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
    };
    for (ShapeType type : shapeOrder) {
        for (QAction* action : m_window->m_shapeMenu->actions()) {
            if (!action || action->isSeparator()) {
                continue;
            }
            const QString name = action->objectName();
            if (!name.startsWith(QStringLiteral("ShapeAction_")) && !name.startsWith(QStringLiteral("ShapePreset_"))) {
                continue;
            }
            if (action->data().toInt() != static_cast<int>(type)) {
                continue;
            }
            addShapeButton(action);
        }
    }
    // 花括号预置动作没有 ShapeType（data 为 -1），按 objectName 显式补在直线之后。
    for (const QString& braceAction :
         {QStringLiteral("ShapePreset_braceLeft"), QStringLiteral("ShapePreset_braceRight")}) {
        addShapeButton(m_window->m_shapeMenu->findChild<QAction*>(braceAction));
    }

    // 常见图形之后追加自定义图形下拉，和「插入 → 常见图形」里的同一份目录菜单。
    auto* customShapeButton = new QToolButton(mainBar);
    customShapeButton->setObjectName(QStringLiteral("ToolBarCustomShapeButton"));
    customShapeButton->setIcon(makeIcon(IconId::Shapes, QColor(58, 64, 80), MainWindow::kToolBarIconSize));
    customShapeButton->setIconSize(QSize(MainWindow::kToolBarIconSize, MainWindow::kToolBarIconSize));
    customShapeButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    customShapeButton->setPopupMode(QToolButton::DelayedPopup);
    customShapeButton->setToolTip(MainWindow::tr("Custom shapes"));
    customShapeButton->setMenu(m_window->m_customShapeMenu);
    // 需求 7：按钮是浮动面板的触发器；菜单保留给键盘与辅助入口。
    QObject::connect(customShapeButton, &QToolButton::clicked, m_window,
                     [this, customShapeButton]() { m_window->openCustomShapePanel(customShapeButton); });
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
    fillMenu->addAction(m_window->m_fillHorizontalAct);
    fillMenu->addAction(m_window->m_fillVerticalAct);
    fillMenu->addAction(m_window->m_fillBrickAct);
    fillMenu->addAction(m_window->m_fillDotsAct);
    m_window->m_fillButton->setMenu(fillMenu);
    m_window->updateFillActions();

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
    // 填充颜色之后接填充样式，再是描线颜色与描线宽度。
    mainBar->addWidget(m_window->m_fillButton);

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

    // 描线颜色按钮后紧跟描线宽度输入框；修改宽度同时作用于选中图形和笔画。
    m_window->m_widthSpin = new QSpinBox(mainBar);
    m_window->m_widthSpin->setObjectName(QStringLiteral("StrokeWidthSpin"));
    m_window->m_widthSpin->setRange(1, 64);
    m_window->m_widthSpin->setSingleStep(1);
    m_window->m_widthSpin->setSuffix(QStringLiteral(" px"));
    m_window->m_widthSpin->setFixedWidth(74);
    m_window->m_widthSpin->setFixedHeight(MainWindow::kToolBarControlHeight);
    m_window->m_widthSpin->setToolTip(MainWindow::tr("Brush width"));
    QObject::connect(m_window->m_widthSpin, &QSpinBox::valueChanged, m_window, &MainWindow::applyBrushWidth);
    m_window->setBrushWidthUi(m_window->m_brushWidth);
    mainBar->addWidget(m_window->m_widthSpin);
}
