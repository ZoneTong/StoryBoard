#include "UiMenuBuilder.h"

#include "Alignment.h"
#include "BoolOps.h"
#include "CanvasScene.h"
#include "CanvasView.h"
#include "CurveItem.h"
#include "DockPanelController.h"
#include "IconFactory.h"
#include "ItemDockPanel.h"
#include "ItemStyleVisuals.h"
#include "MainWindow.h"
#include "ShapeItem.h"
#include "ShapePainter.h"
#include "StyleController.h"

#include <QAction>
#include <QActionGroup>
#include <QDockWidget>
#include <QList>
#include <QMenu>
#include <QMenuBar>
#include <QPair>
#include <QStatusBar>
#include <QWidget>

UiMenuBuilder::UiMenuBuilder(MainWindow* window) : m_window(window) {}

void UiMenuBuilder::buildShapeMenu()
{
    m_window->m_shapeMenu = new QMenu(MainWindow::tr("Common shapes"), m_window);
    m_window->m_shapeMenu->setObjectName(QStringLiteral("ShapeMenu"));
    m_window->m_shapeMenu->setIcon(makeIcon(IconId::Shapes, QColor(58, 64, 80), MainWindow::kToolBarIconSize));
    m_window->m_shapeMenu->setToolTipsVisible(true);

    for (ShapeType type : allShapeTypes()) {
        QAction* action =
            m_window->m_shapeMenu->addAction(makeShapeIcon(type, QColor(58, 64, 80), 18), shapeTypeName(type));
        action->setObjectName(QStringLiteral("ShapeAction_%1").arg(shapeTypeToString(type)));
        action->setData(static_cast<int>(type));
        action->setToolTip(MainWindow::tr("Insert %1 (uses the current brush color)").arg(shapeTypeName(type)));
        QObject::connect(action, &QAction::triggered, m_window, [this, type]() { m_window->insertShape(type); });

        // 需求 5：圆角矩形已并入矩形，菜单保留两个入口——矩形（直角）与紧跟其后的
        // 圆角矩形（短边 16% 自动圆角），底层同为 rectangle，仅圆角状态不同。
        if (type == ShapeType::Rectangle) {
            QAction* roundedPreset = m_window->m_shapeMenu->addAction(
                makeShapePresetIcon(QStringLiteral("roundedRect"), QColor(58, 64, 80), 18),
                MainWindow::tr("Rounded rectangle"));
            roundedPreset->setObjectName(QStringLiteral("ShapePreset_roundedRect"));
            roundedPreset->setData(static_cast<int>(type));
            roundedPreset->setToolTip(MainWindow::tr("Insert a rectangle whose corners follow 16% of the short side"));
            QObject::connect(roundedPreset, &QAction::triggered, m_window, [this]() {
                m_window->insertShape(ShapeType::Rectangle, -1.0, -1.0, ArrowStyle::None, ArrowStyle::None, true);
            });
        }

        // 曲线族入口：直线 / 弧线 / 正弦都不是形状类型，按 curveKind 插入；整组放在
        // 「椭圆」之后，保持原先直线、直线箭头、左右花括号的相对顺序。
        if (type == ShapeType::Ellipse) {
            QAction* line = m_window->m_shapeMenu->addAction(
                ShapePainter::makeCurveIcon(CurveKind::Line, QColor(58, 64, 80), 18), MainWindow::tr("Line"));
            line->setObjectName(QStringLiteral("ShapeAction_line"));
            line->setData(-1);
            line->setToolTip(MainWindow::tr("Insert a line (uses the current brush color)"));
            QObject::connect(line, &QAction::triggered, m_window, [this]() {
                m_window->insertCurve(CurveKind::Line);
            });

            // 需求 6：直线箭头紧跟在「直线」之后，复用直线类，默认末端开口箭头，
            // 长边 128（与形状基准边长一致）。
            QAction* lineArrow = m_window->m_shapeMenu->addAction(
                ShapePainter::makeCurveIcon(CurveKind::Line, QColor(58, 64, 80), 18), MainWindow::tr("Line arrow"));
            lineArrow->setObjectName(QStringLiteral("ShapePreset_lineArrow"));
            lineArrow->setData(-1);
            lineArrow->setToolTip(MainWindow::tr("Insert a line with an open arrow head at its end"));
            QObject::connect(lineArrow, &QAction::triggered, m_window, [this]() {
                m_window->insertCurve(CurveKind::Line, ArrowStyle::None, ArrowStyle::OpenArrow);
            });

            // 需求 13：左右花括号改为曲线图元（右括号 = 左括号旋转 180°），
            // 对话里已定：花括号只是 CurveKind::Brace 的取值，不单独建类。
            QAction* braceLeft = m_window->m_shapeMenu->addAction(
                makeShapePresetIcon(QStringLiteral("braceLeft"), QColor(58, 64, 80), 18), MainWindow::tr("Left brace"));
            braceLeft->setObjectName(QStringLiteral("ShapePreset_braceLeft"));
            braceLeft->setData(-1);
            braceLeft->setToolTip(MainWindow::tr("Insert a left brace as a curve (draggable and resizable)"));
            QObject::connect(braceLeft, &QAction::triggered, m_window, [this]() { m_window->insertBrace(true); });

            QAction* braceRight = m_window->m_shapeMenu->addAction(
                makeShapePresetIcon(QStringLiteral("braceRight"), QColor(58, 64, 80), 18),
                MainWindow::tr("Right brace"));
            braceRight->setObjectName(QStringLiteral("ShapePreset_braceRight"));
            braceRight->setData(-1);
            braceRight->setToolTip(MainWindow::tr("Insert a right brace as a curve (left brace rotated 180°)"));
            QObject::connect(braceRight, &QAction::triggered, m_window, [this]() { m_window->insertBrace(false); });

            QAction* arc = m_window->m_shapeMenu->addAction(
                ShapePainter::makeCurveIcon(CurveKind::Arc, QColor(58, 64, 80), 18), MainWindow::tr("Arc"));
            arc->setObjectName(QStringLiteral("ShapeAction_arc"));
            arc->setData(-1);
            arc->setToolTip(
                MainWindow::tr("Insert an arc (start angle and sweep are editable in the properties card)"));
            QObject::connect(arc, &QAction::triggered, m_window, [this]() {
                m_window->insertCurve(CurveKind::Arc);
            });

            QAction* sine = m_window->m_shapeMenu->addAction(
                ShapePainter::makeCurveIcon(CurveKind::Sine, QColor(58, 64, 80), 18), MainWindow::tr("Sine wave"));
            sine->setObjectName(QStringLiteral("ShapeAction_sine_wave"));
            sine->setData(-1);
            sine->setToolTip(MainWindow::tr("Insert a sine wave (cycles and amplitude are editable in the properties card)"));
            QObject::connect(sine, &QAction::triggered, m_window, [this]() {
                m_window->insertCurve(CurveKind::Sine);
            });
        }

        // 需求 1：三角形后追加 45° / 60° 直角三角形预置项。短边沿水平方向，
        // 即底边（水平边）两侧的内角为 90° 与 45° / 60°；底边可调，角度锁定。
        // 预置项用 ShapePreset_ 前缀，工具栏那一排按钮只认 ShapeAction_，不会多出按钮。
        if (type == ShapeType::Triangle) {
            QAction* preset45 = m_window->m_shapeMenu->addAction(
                makeShapePresetIcon(QStringLiteral("triangle45"), QColor(58, 64, 80), 18),
                MainWindow::tr("Right triangle 45°"));
            preset45->setObjectName(QStringLiteral("ShapePreset_triangle45"));
            preset45->setData(static_cast<int>(type));
            preset45->setToolTip(MainWindow::tr("Right triangle 45°: short side horizontal, base adjustable, "
                                                "angles locked at 90° / 45°"));
            QObject::connect(preset45, &QAction::triggered, m_window,
                             [this]() { m_window->insertShape(ShapeType::Triangle, 90.0, 45.0); });

            QAction* preset60 = m_window->m_shapeMenu->addAction(
                makeShapePresetIcon(QStringLiteral("triangle60"), QColor(58, 64, 80), 18),
                MainWindow::tr("Right triangle 60°"));
            preset60->setObjectName(QStringLiteral("ShapePreset_triangle60"));
            preset60->setData(static_cast<int>(type));
            preset60->setToolTip(MainWindow::tr("Right triangle 60°: short side horizontal, base adjustable, "
                                                "angles locked at 90° / 60°"));
            QObject::connect(preset60, &QAction::triggered, m_window,
                             [this]() { m_window->insertShape(ShapeType::Triangle, 90.0, 60.0); });
        }
    }

    // 需求：自定义图形不再挂在「常见图形」下面做二级子菜单，直接作为同一层的一项，
    // 点击就打开自定义图形面板。工具栏下拉仍复用同一批动作，见 UiToolBarBuilder。
    m_window->m_shapeMenu->addSeparator();
    m_window->m_customShapeMenu = new QMenu(MainWindow::tr("Custom shapes"), m_window);
    m_window->m_customShapeMenu->setObjectName(QStringLiteral("CustomShapeMenu"));
    QObject::connect(m_window->m_shapeMenu, &QMenu::aboutToShow, m_window, &MainWindow::updateCustomShapeMenu);
    QObject::connect(m_window->m_customShapeMenu, &QMenu::aboutToShow, m_window, &MainWindow::updateCustomShapeMenu);
    m_window->updateCustomShapeMenu();
}

void UiMenuBuilder::buildMenus()
{
    m_window->m_fileMenu = m_window->menuBar()->addMenu(MainWindow::tr("&File"));
    QMenu* fileMenu = m_window->m_fileMenu;
    fileMenu->addAction(m_window->m_newAct);
    fileMenu->addAction(m_window->m_openAct);
    // 需求：在“打开画布”后面提供图片线稿化入口，把色块轮廓变成笔画图元。
    fileMenu->addAction(m_window->m_lineArtAct);
    fileMenu->addSeparator();
    fileMenu->addAction(m_window->m_saveAct);
    fileMenu->addAction(m_window->m_saveAsAct);
    fileMenu->addSeparator();
    // 插入图片只留在“插入”菜单，导入图片线稿化只留在文件菜单，避免同一动作多处出现。
    fileMenu->addAction(m_window->m_exportAct);
    fileMenu->addAction(m_window->m_exportExcalidrawAct);
    fileMenu->addAction(m_window->m_exportCustomShapeAct);
    // 需求：原“画板”菜单不再作为父菜单存在，它的设置、最近画布与画布文件夹
    // 直接并入文件菜单，位于“退出”之上；这一段由 updateCanvasesMenu 动态重建。
    QAction* boardSectionSeparator = fileMenu->addSeparator();
    m_window->m_boardSectionSeparator = boardSectionSeparator;
    fileMenu->addAction(m_window->m_quitAct);
    QObject::connect(m_window->m_fileMenu, &QMenu::aboutToShow, m_window, &MainWindow::updateCanvasesMenu);
    m_window->updateCanvasesMenu();

    // 需求：“视图”一级菜单紧跟“文件”，放在“编辑”之前。
    QMenu* viewMenu = m_window->menuBar()->addMenu(MainWindow::tr("&View"));
    viewMenu->setObjectName(QStringLiteral("ViewMenu"));
    viewMenu->addAction(m_window->m_viewModeAct);
    viewMenu->addAction(m_window->m_editModeAct);
    viewMenu->addAction(m_window->m_brushModeAct);
    viewMenu->addAction(m_window->m_textModeAct);
    viewMenu->addSeparator();
    viewMenu->addAction(m_window->m_fitAct);
    viewMenu->addAction(m_window->m_zoomInAct);
    viewMenu->addAction(m_window->m_zoomOutAct);
    viewMenu->addAction(m_window->m_actualSizeAct);
    viewMenu->addSeparator();
    // 需求：辅助网格除设置外还要能从视图菜单直接开关，开关状态与设置里的复选框同步。
    viewMenu->addAction(m_window->m_gridAct);
    QAction* layerDockAction = m_window->m_docks->layerDock()->toggleViewAction();
    layerDockAction->setText(MainWindow::tr("Layer panel"));
    layerDockAction->setShortcut(QKeySequence(QStringLiteral("Alt+1")));
    layerDockAction->setStatusTip(MainWindow::tr("Show or hide the layer panel"));
    viewMenu->addAction(layerDockAction);

    QAction* itemDockAction = m_window->m_docks->itemDock()->toggleViewAction();
    itemDockAction->setObjectName(QStringLiteral("ItemDockToggleAction"));
    itemDockAction->setText(MainWindow::tr("Edit item panel"));
    itemDockAction->setShortcut(QKeySequence(QStringLiteral("Alt+2")));
    itemDockAction->setStatusTip(MainWindow::tr(
        "Show or hide the edit item panel (it stays visible while docked and follows the item selection while "
        "floating)"));
    viewMenu->addAction(itemDockAction);

    QMenu* editMenu = m_window->menuBar()->addMenu(MainWindow::tr("&Edit"));
    editMenu->addAction(m_window->m_undoAct);
    editMenu->addAction(m_window->m_redoAct);
    editMenu->addSeparator();
    editMenu->addAction(m_window->m_deleteAct);
    editMenu->addAction(m_window->m_copyAct);
    editMenu->addAction(m_window->m_cutAct);
    editMenu->addAction(m_window->m_pasteAct);
    editMenu->addAction(m_window->m_duplicateAct);
    editMenu->addAction(m_window->m_groupAct);
    editMenu->addAction(m_window->m_ungroupAct);
    // 需求：平滑笔画移到“笔画”菜单下，不再留在编辑菜单。
    editMenu->addAction(m_window->m_selectAllAct);
    editMenu->addSeparator();
    editMenu->addAction(m_window->m_itemPropsAct);
    editMenu->addSeparator();

    auto applySelectedHeight = [this](int delta) {
        if (m_window->m_view->mode() != ToolMode::Edit) {
            m_window->statusBar()->showMessage(MainWindow::tr("Height adjustment is available in edit mode only"),
                                               3000);
            return;
        }
        const int changed = m_window->m_scene->adjustSelectedHeights(delta);
        if (changed == 0) {
            m_window->statusBar()->showMessage(
                MainWindow::tr("No selected items can change height (or the height limit was reached)"), 3000);
        } else {
            m_window->statusBar()->showMessage(MainWindow::tr("Changed the height of %1 item(s) by %2")
                                                   .arg(changed)
                                                   .arg(delta > 0 ? QStringLiteral("+1") : QStringLiteral("-1")),
                                               3000);
        }
    };

    // 高度两项只写动作名：快捷键交给 QAction 自己显示，手写 “] / [” 会跟它重复一遍。
    QAction* heightUpAct = editMenu->addAction(MainWindow::tr("Height +1 (stack up)"));
    heightUpAct->setObjectName(QStringLiteral("HeightUpAction"));
    heightUpAct->setIcon(makeIcon(IconId::OrderUp, QColor(58, 64, 80), MainWindow::kToolBarIconSize));
    heightUpAct->setShortcut(QKeySequence(Qt::Key_BracketRight));
    heightUpAct->setStatusTip(MainWindow::tr("Raise the stacking height of selected items (shortcut ])"));
    QObject::connect(heightUpAct, &QAction::triggered, m_window, [applySelectedHeight]() { applySelectedHeight(1); });

    QAction* heightDownAct = editMenu->addAction(MainWindow::tr("Height -1 (stack down)"));
    heightDownAct->setObjectName(QStringLiteral("HeightDownAction"));
    heightDownAct->setIcon(makeIcon(IconId::OrderDown, QColor(58, 64, 80), MainWindow::kToolBarIconSize));
    heightDownAct->setShortcut(QKeySequence(Qt::Key_BracketLeft));
    heightDownAct->setStatusTip(MainWindow::tr("Lower the stacking height of selected items (shortcut [)"));
    QObject::connect(heightDownAct, &QAction::triggered, m_window,
                     [applySelectedHeight]() { applySelectedHeight(-1); });

    editMenu->addSeparator();
    editMenu->addAction(m_window->m_rotateCwAct);
    editMenu->addAction(m_window->m_rotateCcwAct);

    // 多图元对齐：编辑菜单里做成下拉子菜单，工具栏上再给一个下拉按钮，共用同一组动作。
    QMenu* alignMenu = editMenu->addMenu(MainWindow::tr("Align"));
    alignMenu->setObjectName(QStringLiteral("AlignMenu"));
    struct AlignEntry {
        Alignment::Mode mode;
        IconId icon;
        const char* text;
        const char* tip;
    };
    const AlignEntry alignEntries[] = {
        {Alignment::Mode::Left, IconId::AlignLeft, QT_TR_NOOP("Align left"),
         QT_TR_NOOP("Line up the left edges of the selected items")},
        {Alignment::Mode::HorizontalCenter, IconId::AlignCenterH, QT_TR_NOOP("Align horizontal centers"),
         QT_TR_NOOP("Center the selected items on the same vertical axis")},
        {Alignment::Mode::Right, IconId::AlignRight, QT_TR_NOOP("Align right"),
         QT_TR_NOOP("Line up the right edges of the selected items")},
        {Alignment::Mode::Top, IconId::AlignTop, QT_TR_NOOP("Align top"),
         QT_TR_NOOP("Line up the top edges of the selected items")},
        {Alignment::Mode::VerticalCenter, IconId::AlignCenterV, QT_TR_NOOP("Align vertical centers"),
         QT_TR_NOOP("Center the selected items on the same horizontal axis")},
        {Alignment::Mode::Bottom, IconId::AlignBottom, QT_TR_NOOP("Align bottom"),
         QT_TR_NOOP("Line up the bottom edges of the selected items")},
        {Alignment::Mode::DistributeHorizontal, IconId::DistributeH, QT_TR_NOOP("Distribute horizontally"),
         QT_TR_NOOP("Space the selected items evenly from left to right (needs 3 or more)")},
        {Alignment::Mode::DistributeVertical, IconId::DistributeV, QT_TR_NOOP("Distribute vertically"),
         QT_TR_NOOP("Space the selected items evenly from top to bottom (needs 3 or more)")},
    };
    for (const AlignEntry& entry : alignEntries) {
        QAction* action = alignMenu->addAction(MainWindow::tr(entry.text));
        action->setIcon(makeIcon(entry.icon, QColor(58, 64, 80), MainWindow::kToolBarIconSize));
        action->setStatusTip(MainWindow::tr(entry.tip));
        action->setProperty("alignMode", static_cast<int>(entry.mode));
        QObject::connect(action, &QAction::triggered, m_window,
                         [this, mode = entry.mode]() { m_window->applyAlignment(mode); });
        m_window->m_alignActions.append(action);
    }

    // 合并图元：与 PPT「合并形状」同一套子操作命名与顺序——
    // 联合 / 异或（XOR，并集减重叠）/ 相交 / 剪除 / 拆分；
    // 主入口用独立图标（形变图形），五项子操作继续用两圆交叠的运算图标。
    QMenu* boolMenu = editMenu->addMenu(MainWindow::tr("Merge items") + QStringLiteral(" (Ctrl+M)"));
    boolMenu->setObjectName(QStringLiteral("BoolMenu"));
    boolMenu->setIcon(makeIcon(IconId::Morph, QColor(58, 64, 80), MainWindow::kToolBarIconSize));
    // 和弦键（Ctrl+M 之后 1 秒内按字母）不是 QAction 的快捷键，菜单里不会自动显示，
    // 所以直接把键名缀在条目文字后面，跟状态栏提示保持一致。
    const QList<QPair<BoolOps::Operation, QString>> boolEntries = {
        {BoolOps::Operation::Union, MainWindow::tr("Union") + QStringLiteral(" (U)")},
        {BoolOps::Operation::Xor, MainWindow::tr("Xor") + QStringLiteral(" (X)")},
        {BoolOps::Operation::Intersect, MainWindow::tr("Intersect") + QStringLiteral(" (I)")},
        {BoolOps::Operation::Subtract, MainWindow::tr("Subtract") + QStringLiteral(" (S)")},
        {BoolOps::Operation::Fragment, MainWindow::tr("Fragment") + QStringLiteral(" (F)")},
        {BoolOps::Operation::LineSplit, MainWindow::tr("Split along line") + QStringLiteral(" (L)")},
    };
    const QList<QString> boolTips = {
        MainWindow::tr("Merge the selected items into one polygon"),
        MainWindow::tr("Union minus the overlapping part (XOR)"),
        MainWindow::tr("Keep only the overlapping part"),
        MainWindow::tr("Cut the other items out of the first selected item"),
        MainWindow::tr("Split the selected items into separate pieces at every overlap"),
        MainWindow::tr("Cut the filled items along the selected line or curve"),
    };
    // 图标与 PPT「合并形状」一致：两个交叠圆表示各运算的结果区域。
    const QList<IconId> boolIcons = {IconId::MergeUnion,    IconId::MergeCombine,  IconId::MergeIntersect,
                                     IconId::MergeSubtract, IconId::MergeFragment, IconId::MergeLineSplit};
    for (int i = 0; i < boolEntries.size(); ++i) {
        QAction* action = boolMenu->addAction(boolEntries.at(i).second);
        action->setIcon(
            makeIcon(boolIcons.at(qMin(i, boolIcons.size() - 1)), QColor(58, 64, 80), MainWindow::kToolBarIconSize));
        action->setStatusTip(boolTips.at(qMin(i, boolTips.size() - 1)));
        // 分割的参与者规则不同（填充图元 + 恰一条切刀），打上标记供启用状态单独判定。
        action->setProperty("lineSplitAction", boolEntries.at(i).first == BoolOps::Operation::LineSplit);
        QObject::connect(action, &QAction::triggered, m_window,
                         [this, operation = boolEntries.at(i).first]() { m_window->applyBoolOperation(operation); });
        m_window->m_boolActions.append(action);
    }
    // 多选时编辑图元卡直接复用同一批对齐 / 合并图元 / 复制 / 删除 / 旋转 / 组合动作，
    // 快捷键与启用状态不用再维护一份。
    if (m_window->m_docks && m_window->m_docks->itemPanel()) {
        m_window->m_docks->itemPanel()->setSelectionActions(
            m_window->m_alignActions, m_window->m_boolActions, m_window->m_duplicateAct, m_window->m_deleteAct,
            m_window->m_rotateCwAct, m_window->m_rotateCcwAct, m_window->m_groupAct, m_window->m_ungroupAct);
        m_window->m_docks->itemPanel()->setItemActions(m_window->m_smoothAct, m_window->m_itemPropsAct);
        m_window->m_docks->itemPanel()->refresh();
    }

    // 需求：原“样式”菜单拆开——“画笔样式”独立为一级菜单“画笔”，
    // 填充样式与填充／描边颜色并入“插入”菜单，紧跟在“常见图形”之后。
    QMenu* insertMenu = m_window->menuBar()->addMenu(MainWindow::tr("&Insert"));
    insertMenu->setObjectName(QStringLiteral("InsertMenu"));

    insertMenu->addMenu(m_window->m_shapeMenu);
    insertMenu->addSeparator();

    QMenu* fillStyleMenu = insertMenu->addMenu(MainWindow::tr("Fill style"));
    fillStyleMenu->setObjectName(QStringLiteral("FillStyleMenu"));
    fillStyleMenu->setIcon(m_window->m_style->fillStyleIcon(
        m_window->m_fillStyle, m_window->m_fillColor.isValid() ? m_window->m_fillColor : m_window->m_brushColor, 16));
    fillStyleMenu->addAction(m_window->m_fillNoneAct);
    fillStyleMenu->addAction(m_window->m_fillSolidAct);
    fillStyleMenu->addAction(m_window->m_fillHachureAct);
    fillStyleMenu->addAction(m_window->m_fillReverseAct);
    fillStyleMenu->addAction(m_window->m_fillCrossAct);
    fillStyleMenu->addAction(m_window->m_fillGridAct);
    fillStyleMenu->addAction(m_window->m_fillHoneycombAct);
    fillStyleMenu->addAction(m_window->m_fillHorizontalAct);
    fillStyleMenu->addAction(m_window->m_fillVerticalAct);
    fillStyleMenu->addAction(m_window->m_fillBrickAct);
    fillStyleMenu->addAction(m_window->m_fillDotsAct);

    // 需求 29：颜色从填充样式里分离出来，放在填充样式之后。
    insertMenu->addAction(m_window->m_fillColorAct);

    // 需求 1/15：描线样式子菜单，候选与工具栏、编辑卡、属性窗共用 borderStyles()（首项“无”）。
    QMenu* strokeStyleMenu = insertMenu->addMenu(MainWindow::tr("Border style"));
    strokeStyleMenu->setObjectName(QStringLiteral("BorderStyleMenu"));
    strokeStyleMenu->setIcon(ItemStyleVisuals::borderStyleIcon(m_window->m_strokeStyle, 16));
    for (QAction* action : m_window->m_strokeStyleActions) {
        strokeStyleMenu->addAction(action);
    }
    m_window->m_strokeStyleMenu = strokeStyleMenu;

    insertMenu->addAction(m_window->m_strokeColorAct);
    // 需求 1：描线宽度从 SpinBox 行改为普通菜单项，触发后把焦点放到工具栏宽度输入框末尾。
    insertMenu->addAction(m_window->m_strokeWidthFocusAct);
    m_window->setBrushWidthUi(m_window->m_brushWidth);
    m_window->m_style->updateStrokeStyleActions();
    insertMenu->addAction(m_window->m_smoothAct);
    // 需求：原“画布”菜单不再作为父菜单存在，背景相关项直接并入“插入”菜单；
    // 背景属于画布属性，排在“插入图片”之前，避免和插入类动作混在一段。
    insertMenu->addSeparator();
    insertMenu->addAction(m_window->m_replaceBgAct);
    insertMenu->addAction(m_window->m_bgColorAct);
    insertMenu->addAction(m_window->m_clearBgAct);
    QMenu* fillMenu = insertMenu->addMenu(MainWindow::tr("Background fill mode"));
    fillMenu->setIcon(makeIcon(IconId::Background, QColor(58, 64, 80), MainWindow::kToolBarIconSize));
    fillMenu->addAction(m_window->m_bgStretchAct);
    fillMenu->addAction(m_window->m_bgFitAct);
    fillMenu->addAction(m_window->m_bgTileAct);

    insertMenu->addSeparator();
    insertMenu->addAction(m_window->m_importAct);

    QMenu* helpMenu = m_window->menuBar()->addMenu(MainWindow::tr("&Help"));
    helpMenu->addAction(m_window->m_shortcutsAct);
    helpMenu->addAction(m_window->m_aboutAct);
    helpMenu->addAction(m_window->m_aboutQtAct);
}
