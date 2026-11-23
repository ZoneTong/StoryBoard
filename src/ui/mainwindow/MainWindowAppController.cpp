#include "ui/mainwindow/MainWindowAppController.h"

#include "app/DockPanelController.h"
#include "app/I18n.h"
#include "app/Settings.h"
#include "canvas/CanvasView.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "scene/CanvasScene.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/panels/layers/LayerPanel.h"
#include "ui/settings/SettingsDialog.h"

#include <QAction>
#include <QCoreApplication>
#include <QDialog>
#include <QMenu>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QStringList>

MainWindowAppController::MainWindowAppController(MainWindow* window) : m_window(window) {}

void MainWindowAppController::ApplyUiLanguage()
{
    // 先按字典把已经创建出来的文字整体切到目标语言。
    UiRetranslateWidgetTree(m_window);

    // 形状菜单的文字与提示是拼出来的，字典无法整体命中，按当前语言重新生成。
    if (m_window->m_shapeMenu) {
        for (QAction* action : m_window->m_shapeMenu->actions()) {
            const ShapeType type = static_cast<ShapeType>(action->data().toInt());
            action->setText(ShapeTypes::Name(type));
            action->setToolTip(MainWindow::tr("Insert %1 (uses the current brush color)").arg(ShapeTypes::Name(type)));
        }
    }

    // 图层面板每行文字（图层名、图元数、锁定/隐藏后缀）是动态拼的，重建一次。
    if (m_window->m_docks && m_window->m_docks->layerPanel()) {
        m_window->m_docks->layerPanel()->Refresh();
    }

    if (m_window->m_view) {
        m_window->SetBrushColor(m_window->m_brushColor, false);
    }
    m_window->UpdateFillActions();
    m_window->UpdateModeUi();
    m_window->UpdateStatusBar();
    m_window->UpdateTitle();
    m_window->UpdateCanvasesMenu();
}

void MainWindowAppController::ShowSettings()
{
    AppSettings updated;
    UiLanguage requestedLanguage = UiLanguageSetting();
    {
        SettingsDialog dialog(m_window->m_settings, m_window);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        updated = dialog.Settings();
        requestedLanguage = dialog.Language();
    }

    // 对话框对象已经析构，再切换语言/重建动态文案，避免在已关闭的模态窗上重译。
    if (requestedLanguage != UiLanguageSetting()) {
        SetUiLanguageSetting(requestedLanguage);
        m_window->ApplyUiLanguage();
    }
    // 对话框里的设置是主窗口设置的一份完整副本：语言另存，其余整份拷回，
    // 避免新增参数（例如线稿化档位）漏在“改完却记不住”的队伍里。
    m_window->m_settings = updated;
    // 设置窗里的线稿化参数已经随整份保存落盘，进程内副本跟着刷新。
    m_window->m_lineArtSettings.CopyLineArtFrom(m_window->m_settings);

    m_window->SetBrushColor(m_window->m_settings.brushColor, false);
    m_window->SetBrushWidthUi(m_window->m_settings.brushWidth);
    m_window->SetStrokeSmoothingUi(m_window->m_settings.strokeSmoothing);
    m_window->m_view->SetCanvasColor(m_window->m_settings.canvasColor);
    m_window->m_scene->SetShowGrid(m_window->m_settings.showGrid);
    // 设置里改过辅助网格时，视图菜单的勾选状态跟着走（屏蔽信号，避免回写一遍配置）。
    if (m_window->m_gridAct) {
        const QSignalBlocker blocker(m_window->m_gridAct);
        m_window->m_gridAct->setChecked(m_window->m_settings.showGrid);
    }
    m_window->m_scene->SetCanvasSizeWithUndo(m_window->m_settings.canvasSize);

    if (m_window->m_view->mode() == ToolMode::VIEW) {
        m_window->m_view->FitAll();
    }

    m_window->m_settings.Save();
    m_window->UpdateStatusBar();
    m_window->statusBar()->showMessage(MainWindow::tr("Settings saved"), 3000);
}

void MainWindowAppController::ShowAbout()
{
    QString html;
    html += QStringLiteral("<h3>%1 %2</h3>")
                .arg(MainWindow::tr("StoryBoard Canvas"), QCoreApplication::applicationVersion());
    html += QStringLiteral("<p>%1</p>")
                .arg(MainWindow::tr("A lightweight canvas app for freehand strokes, image items, overview "
                                    "navigation, canvas files and export."));
    // GPLv3 第 5 条：程序与用户交互时要给出许可与版权提示；许可证全文随程序目录分发。
    html += QStringLiteral("<p>%1<br>%2</p>")
                .arg(MainWindow::tr("License: GPL-3.0-only (see LICENSE); Qt 6 is distributed under LGPLv3 "
                                    "(see THIRD-PARTY-NOTICES.md)."),
                     MainWindow::tr("Copyright (C) 2026 ZoneTong"));
    // 关于窗口的线稿化备注：说明入口、参数顺序和输出，和功能实际行为保持一致。
    html += QStringLiteral("<p><b>%1</b></p><ul>").arg(MainWindow::tr("Image outline tracing"));
    html +=
        QStringLiteral("<li>%1</li>")
            .arg(MainWindow::tr(
                "Import an image and the parameter dialog opens first; strokes are created only after you confirm."));
    html += QStringLiteral("<li>%1</li>")
                .arg(MainWindow::tr("Four presets (keep details / balanced / clean lines / bold blocks) set color "
                                    "levels, same-color similarity, adjacent-color similarity, smallest block and "
                                    "simplify tolerance in one go; editing any value switches back to Custom."));
    html +=
        QStringLiteral("<li>%1</li>")
            .arg(MainWindow::tr("Simplify tolerance and thinnest band are adjustable; OK / Cancel apply to "
                                "this run only, Save to configuration file writes the values into StoryBoard.ini."));
    html += QStringLiteral("<li>%1</li>")
                .arg(MainWindow::tr("Block count limit with automatic downsampling: checked by default, the image is "
                                    "processed at a smaller size instead of dropping blocks."));
    html += QStringLiteral("<li>%1</li>")
                .arg(MainWindow::tr("Output line width is fixed at 1 and the result is inserted as plain stroke "
                                    "items."));
    html += QStringLiteral("</ul><p><b>%1</b></p><ul>").arg(MainWindow::tr("Key operations"));
    const QStringList operations = {
        QStringLiteral("H View mode: pan / zoom the whole canvas, automatic overview"),
        QStringLiteral("V Edit mode: items on visible, unlocked layers can be edited; locked layers cannot; ] / [ "
                       "adjust the height of the selection; right-click adjusts layer / height"),
        QStringLiteral("B Brush mode: freehand strokes, with color and width options"),
        QStringLiteral("T Text mode: click the canvas to add text, double-click text to edit it again"),
        QStringLiteral("Layers: the checkbox in the right panel toggles visibility, the lock button locks a "
                       "layer; layers and items both support multi-select"),
        QStringLiteral("Insert → Shapes: quickly insert rectangles, ellipses, arrows, braces, speech bubbles and "
                       "more"),
        QStringLiteral("Mouse: wheel zooms, Space / middle button pans in any mode; in edit mode holding Ctrl "
                       "and dragging also pans the whole view"),
        QStringLiteral("Ctrl+0 returns to the overview, Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z undo / redo"),
        QStringLiteral("Ctrl+I / Ctrl+L / Ctrl+E insert image / import outline image / export canvas; "
                       "Ctrl+Shift+B simplifies the selected curve (Brush); Ctrl+Q or Alt+F4 quits"),
    };
    for (const QString& operation : operations) {
        html += QStringLiteral("<li>%1</li>").arg(UiText(operation));
    }
    html +=
        QStringLiteral("</ul><p>%1</p>").arg(MainWindow::tr("Built with Qt %1.").arg(QString::fromLatin1(qVersion())));
    QMessageBox box(QMessageBox::Information, MainWindow::tr("About StoryBoard Canvas"), html, QMessageBox::NoButton,
                    m_window);
    box.addButton(MainWindow::tr("OK"), QMessageBox::AcceptRole);
    box.exec();
}

void MainWindowAppController::PersistBeforeExit()
{
    m_window->m_settings.windowGeometry = m_window->saveGeometry();
    // 退出时按实际上下位置再记一次卡片顺序，拖拽换位后即使没有触发移动事件也能落盘。
    m_window->m_settings.dockOrder = m_window->m_docks ? m_window->m_docks->CurrentDockOrder() : QStringList();
    if (m_window->m_docks) {
        m_window->m_docks->RememberPropertiesPanelDockState();
    }
    m_window->m_settings.brushColor = m_window->m_brushColor;
    m_window->m_settings.brushWidth = m_window->m_brushWidth;
    if (m_window->m_smoothingSpin) {
        m_window->m_settings.strokeSmoothing = m_window->m_smoothingSpin->value();
    }
    if (m_window->m_scene) {
        m_window->m_settings.canvasSize = m_window->m_scene->CanvasSize();
        m_window->m_settings.showGrid = m_window->m_scene->ShowGrid();
    }
    m_window->m_settings.Save();
}
