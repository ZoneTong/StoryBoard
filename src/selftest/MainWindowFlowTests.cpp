#include "Alignment.h"
#include "BoolOps.h"
#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CanvasView.h"
#include "Commands.h"
#include "CompositeShapeItem.h"
#include "CurveItem.h"
#include "CustomShapeTemplates.h"
#include "ExportDialog.h"
#include "I18n.h"
#include "ImageItem.h"
#include "ItemDockPanel.h"
#include "ItemPropertiesDialog.h"
#include "LayerPanel.h"
#include "LineArtConverter.h"
#include "LineArtDialog.h"
#include "LineArtPresets.h"
#include "MainWindow.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "ShapeItem.h"
#include "TextItem.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"
#include "selftest/SelftestHelpers.h"

#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>
#include <QImage>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLineF>
#include <QLinearGradient>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMarginsF>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPolygonF>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSize>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>
#include <QWheelEvent>
#include <QWidget>
#include <QWidgetAction>
#include <QtMath>
#include <cstdlib>


void selftest::runMainWindowFlowTests(Context& ctx)
{
    // End-to-end check of the actual MainWindow text flow. The modal text
    // dialog is filled and accepted by a timer while insertTextAt() is running.
    {
        MainWindow window;
        window.resize(1200, 800);
        window.show();
        QApplication::processEvents();

        auto* modeBar = window.findChild<QToolBar*>(QStringLiteral("ModeToolBar"));
        if (modeBar) {
            // 需求：支持中英文界面，默认系统语言，可在设置里切换。
            ctx.check(uiText(QStringLiteral("New canvas")) == QStringLiteral("新建画布"),
                      QStringLiteral("中文界面下英文源串显示为中文"));
            uiOverrideLanguage(UiLanguage::English);
            ctx.check(uiText(QStringLiteral("New canvas")) == QStringLiteral("New canvas") &&
                          uiText(QStringLiteral("Hexagon")) == QStringLiteral("Hexagon") &&
                          uiText(QStringLiteral("View mode")) == QStringLiteral("View mode"),
                      QStringLiteral("英文界面下显示英文源串"), uiText(QStringLiteral("New canvas")));
            uiRetranslateWidgetTree(&window);
            QStringList englishOrder;
            for (QAction* action : modeBar->actions()) {
                if (action && !action->isSeparator()) {
                    englishOrder.append(action->text());
                }
            }
            ctx.check(englishOrder.contains(QStringLiteral("View mode")) &&
                          englishOrder.contains(QStringLiteral("Edit mode")) &&
                          englishOrder.contains(QStringLiteral("Brush mode")) &&
                          englishOrder.contains(QStringLiteral("Text mode")),
                      QStringLiteral("已存在的界面可以原地切换到英文"), englishOrder.join(QStringLiteral(" / ")));
            uiOverrideLanguage(UiLanguage::Chinese);
            uiRetranslateWidgetTree(&window);

            // 切换语言后，走真实刷新路径（写设置 + applyUiLanguage）立即生效。
            uiOverrideLanguage(UiLanguage::System); // 取消测试用的强制语言，才能验证设置生效
            const UiLanguage originalSetting = uiLanguageSetting();
            setUiLanguageSetting(UiLanguage::English);
            window.applyUiLanguage();
            const QString firstModeText = modeBar->actions().isEmpty() ? QString() : modeBar->actions().at(0)->text();
            QMenu* shapeMenu = window.findChild<QMenu*>(QStringLiteral("ShapeMenu"));
            const QString firstShapeText =
                shapeMenu && !shapeMenu->actions().isEmpty() ? shapeMenu->actions().at(0)->text() : QString();
            ctx.check(firstModeText == QStringLiteral("View mode") && firstShapeText == QStringLiteral("Rectangle"),
                      QStringLiteral("切换语言后主界面立即变为英文"),
                      QStringLiteral("%1 / %2").arg(firstModeText, firstShapeText));
            setUiLanguageSetting(originalSetting);
            uiOverrideLanguage(UiLanguage::Chinese);
            window.applyUiLanguage();
            ctx.check(modeBar->actions().at(0)->text() == QStringLiteral("查看模式"),
                      QStringLiteral("切回中文后界面恢复中文"));

            // 线稿化模块已改成标准 tr()：译文走各自类名的上下文，中文界面下必须仍是中文。
            {
                LineArtDialog lineArtDialog{AppSettings()};
                QGroupBox* group = lineArtDialog.findChild<QGroupBox*>();
                ctx.check(lineArtDialog.windowTitle() == QStringLiteral("图片线稿化") && group &&
                              group->title() == QStringLiteral("图片线稿化"),
                          QStringLiteral("线稿化对话框的 tr() 文本在中文界面显示中文"),
                          QStringLiteral("title=%1 group=%2")
                              .arg(lineArtDialog.windowTitle(), group ? group->title() : QString()));

                QStringList actionTexts;
                for (QAction* action : window.findChildren<QAction*>()) {
                    actionTexts.append(action->text());
                }
                ctx.check(actionTexts.contains(QStringLiteral("导入图片线稿化")),
                          QStringLiteral("主界面线稿化入口的 tr() 文本显示中文"));
            }

            // 设置弹窗的语言下拉：选 English 后 language() 返回 English。
            {
                SettingsDialog dialog{AppSettings()};
                QComboBox* combo = dialog.findChild<QComboBox*>();
                if (combo) {
                    const int englishIndex = combo->findData(static_cast<int>(UiLanguage::English));
                    combo->setCurrentIndex(englishIndex);
                    ctx.check(dialog.language() == UiLanguage::English,
                              QStringLiteral("设置弹窗可以选中 English 并返回该语言"),
                              QStringLiteral("index=%1 language=%2")
                                  .arg(englishIndex)
                                  .arg(static_cast<int>(dialog.language())));
                }
            }

            // 初始设置默认不显示辅助网格：新建配置对象与新建场景都应是关闭态，
            // 只有用户手动打开或旧配置里存了 view/grid=true 时才画网格。
            {
                AppSettings defaults;
                CanvasScene freshScene;
                ctx.check(!defaults.showGrid && !freshScene.showGrid(), QStringLiteral("初始设置默认不显示辅助网格"),
                          QStringLiteral("settings=%1 scene=%2").arg(defaults.showGrid).arg(freshScene.showGrid()));
            }

            // 设置必须真的落到纯文本配置里：写盘 → 重新读取 → 每一项都对得上。
            {
                const QString configPath = AppSettings::settingsFilePath();
                ctx.check(configPath.endsWith(QStringLiteral("StoryBoard.ini")),
                          QStringLiteral("配置文件是纯文本 StoryBoard.ini"), configPath);

                AppSettings written;
                written.brushColor = QColor(255, 128, 0, 255);
                written.brushWidth = 37;
                written.strokeSmoothing = 11;
                written.canvasSize = QSizeF(2400.0, 1500.0);
                // 默认已是关闭，这里写相反的值，读回来才对不上就说明持久化断了。
                written.showGrid = true;
                written.canvasColor = QColor(18, 22, 30, 255);
                written.lineArtColorLevels = 9;
                written.lineArtPaletteMerge = 70;
                written.lineArtAdjacentMerge = 30;
                written.lineArtMinRegionArea = 40;
                ctx.check(written.save() && QFileInfo::exists(configPath),
                          QStringLiteral("设置参数写入 StoryBoard.ini"), configPath);

                AppSettings reloaded;
                reloaded.load();
                const bool same =
                    reloaded.brushColor == written.brushColor && reloaded.brushWidth == written.brushWidth &&
                    reloaded.strokeSmoothing == written.strokeSmoothing && reloaded.canvasSize == written.canvasSize &&
                    reloaded.showGrid == written.showGrid && reloaded.canvasColor == written.canvasColor &&
                    reloaded.lineArtColorLevels == written.lineArtColorLevels &&
                    reloaded.lineArtPaletteMerge == written.lineArtPaletteMerge &&
                    reloaded.lineArtAdjacentMerge == written.lineArtAdjacentMerge &&
                    reloaded.lineArtMinRegionArea == written.lineArtMinRegionArea;
                ctx.check(same, QStringLiteral("重新读取后各设置参数与写入时一致"),
                          QStringLiteral("width=%1 smoothing=%2 grid=%3 levels=%4 minArea=%5")
                              .arg(reloaded.brushWidth)
                              .arg(reloaded.strokeSmoothing)
                              .arg(reloaded.showGrid)
                              .arg(reloaded.lineArtColorLevels)
                              .arg(reloaded.lineArtMinRegionArea));

                // 语言与其它设置共用一份文件，写语言不能把已保存的参数冲掉。
                ctx.check(AppSettings::setLanguageSetting(QStringLiteral("en")) &&
                              AppSettings::languageSetting() == QStringLiteral("en"),
                          QStringLiteral("界面语言写入同一份配置文件"));
                AppSettings afterLanguage;
                afterLanguage.load();
                ctx.check(afterLanguage.brushWidth == 37 && afterLanguage.lineArtColorLevels == 9 &&
                              afterLanguage.lineArtMinRegionArea == 40,
                          QStringLiteral("写语言后其它设置仍然保留"),
                          QStringLiteral("width=%1 levels=%2")
                              .arg(afterLanguage.brushWidth)
                              .arg(afterLanguage.lineArtColorLevels));
                AppSettings::setLanguageSetting(QStringLiteral("system"));

                // 自定义快捷键：写进配置后重建窗口直接生效，提示里的键也跟着换。
                {
                    AppSettings shortcutSettings;
                    shortcutSettings.load();
                    const QMap<QString, QString> originalShortcuts = shortcutSettings.shortcutOverrides;
                    shortcutSettings.shortcutOverrides.insert(QStringLiteral("Ctrl+Q"), QStringLiteral("Ctrl+Alt+Q"));
                    ctx.check(shortcutSettings.save(), QStringLiteral("自定义快捷键写入配置文件"));
                    AppSettings shortcutReloaded;
                    shortcutReloaded.load();
                    ctx.check(shortcutReloaded.shortcutOverrides.value(QStringLiteral("Ctrl+Q")) ==
                                  QStringLiteral("Ctrl+Alt+Q"),
                              QStringLiteral("自定义快捷键能重新读回"));
                    {
                        MainWindow shortcutWindow;
                        QAction* quitAction = nullptr;
                        for (QAction* action : shortcutWindow.findChildren<QAction*>()) {
                            if (action->text() == QStringLiteral("退出")) {
                                quitAction = action;
                                break;
                            }
                        }
                        ctx.check(quitAction && quitAction->shortcut() == QKeySequence(QStringLiteral("Ctrl+Alt+Q")),
                                  QStringLiteral("自定义快捷键在启动时生效"),
                                  quitAction ? quitAction->shortcut().toString() : QString());
                        ctx.check(quitAction && quitAction->statusTip().contains(QStringLiteral("Ctrl+Alt+Q")),
                                  QStringLiteral("改键后状态栏提示同步"),
                                  quitAction ? quitAction->statusTip() : QString());
                    }
                    shortcutSettings.shortcutOverrides = originalShortcuts;
                    shortcutSettings.save();
                }

                // 设置弹窗里改的线稿化参数必须经主窗口回写并落盘（曾经只改了弹窗内存副本）。
                {
                    MainWindow settingsWindow;
                    QAction* settingsAction = nullptr;
                    for (QAction* action : settingsWindow.findChildren<QAction*>()) {
                        if (action->text() == uiText(QStringLiteral("Settings…"))) {
                            settingsAction = action;
                            break;
                        }
                    }
                    bool dialogSeen = false;
                    bool dialogUpdated = false;
                    if (settingsAction) {
                        QTimer::singleShot(0, &settingsWindow, [&]() {
                            auto* dialog = settingsWindow.findChild<SettingsDialog*>();
                            if (!dialog) {
                                return;
                            }
                            dialogSeen = true;
                            QSpinBox* levels = dialog->findChild<QSpinBox*>(QStringLiteral("LineArtLevelsSpin"));
                            QSpinBox* minArea = dialog->findChild<QSpinBox*>(QStringLiteral("LineArtMinAreaSpin"));
                            if (levels && minArea) {
                                levels->setValue(8);
                                minArea->setValue(56);
                            }
                            if (auto* box = dialog->findChild<QDialogButtonBox*>()) {
                                dialogUpdated = box->button(QDialogButtonBox::Ok) != nullptr;
                                if (dialogUpdated) {
                                    box->button(QDialogButtonBox::Ok)->click();
                                }
                            }
                        });
                        settingsAction->trigger();
                    }
                    AppSettings fromDialog;
                    fromDialog.load();
                    ctx.check(dialogSeen && dialogUpdated && fromDialog.lineArtColorLevels == 8 &&
                                  fromDialog.lineArtMinRegionArea == 56,
                              QStringLiteral("设置弹窗里的线稿化参数回写并持久化"),
                              QStringLiteral("seen=%1 ok=%2 levels=%3 minArea=%4")
                                  .arg(dialogSeen)
                                  .arg(dialogUpdated)
                                  .arg(fromDialog.lineArtColorLevels)
                                  .arg(fromDialog.lineArtMinRegionArea));
                }

                // 关窗时保存窗口几何：重开程序应当恢复成关闭前的尺寸，而不是默认 1440x900。
                {
                    bool closed = false;
                    {
                        MainWindow closeWindow;
                        closeWindow.show();
                        QApplication::processEvents();
                        closeWindow.resize(1280, 820);
                        QApplication::processEvents();
                        closed = closeWindow.close();
                    }
                    AppSettings afterClose;
                    afterClose.load();
                    ctx.check(closed && !afterClose.windowGeometry.isEmpty(),
                              QStringLiteral("关闭窗口时把窗口几何写进配置文件"),
                              QStringLiteral("closed=%1 bytes=%2").arg(closed).arg(afterClose.windowGeometry.size()));
                }
            }
        }

        // 需求：原来的“导入图片”改名“插入图片”，并在“打开画布”后面加入“导入图片线稿化”。
        QMenu* fileMenu = nullptr;
        bool canvasMenuGone = true;
        for (QAction* action : window.menuBar()->actions()) {
            QMenu* menu = action ? action->menu() : nullptr;
            if (!menu) {
                continue;
            }
            if (menu->title() == QStringLiteral("文件(&F)")) {
                fileMenu = menu;
            } else if (menu->title() == QStringLiteral("画布(&C)")) {
                canvasMenuGone = false;
            }
        }
        // 需求：画布菜单已并入插入菜单，菜单栏不再有画布一级菜单。
        ctx.check(canvasMenuGone, QStringLiteral("菜单栏不再单独出现画布菜单"));
        if (fileMenu) {
            QStringList entries;
            for (QAction* action : fileMenu->actions()) {
                if (action && !action->isSeparator()) {
                    entries.append(action->text());
                }
            }
            ctx.check(!entries.contains(QStringLiteral("插入图片…")) && !entries.contains(QStringLiteral("导入图片…")),
                      QStringLiteral("插入图片只在插入菜单出现"), entries.join(QStringLiteral(" / ")));
        }

        // 需求：画板不再作为父菜单存在，它的设置、最近画布与画布文件夹直接并入文件菜单。
        bool boardTopLevel = false;
        for (QAction* action : window.menuBar()->actions()) {
            QMenu* menu = action ? action->menu() : nullptr;
            if (menu && menu->title() == QStringLiteral("画板(&B)")) {
                boardTopLevel = true;
            }
        }
        ctx.check(!boardTopLevel && window.findChild<QMenu*>(QStringLiteral("BoardMenu")) == nullptr,
                  QStringLiteral("菜单栏不再出现画板菜单及其子菜单对象"));
        if (fileMenu) {
            const QList<QAction*> fileActions = fileMenu->actions();
            int settingsIndex = -1;
            int folderIndex = -1;
            QStringList entries;
            for (int i = 0; i < fileActions.size(); ++i) {
                QAction* action = fileActions.at(i);
                if (!action || action->isSeparator()) {
                    continue;
                }
                entries.append(action->text());
                if (action->text() == QStringLiteral("设置…")) {
                    settingsIndex = i;
                } else if (action->text() == QStringLiteral("打开画布文件夹")) {
                    folderIndex = i;
                }
            }
            ctx.check(settingsIndex >= 0 && folderIndex >= 0, QStringLiteral("设置与画布文件夹直接出现在文件菜单"),
                      entries.join(QStringLiteral(" / ")));
        }

        window.view()->setMode(ToolMode::Text);
        QTimer::singleShot(120, &window, [&]() {
            auto* dialog = qobject_cast<ItemPropertiesDialog*>(QApplication::activeModalWidget());
            if (!dialog) {
                return;
            }
            QPlainTextEdit* textValue = dialog->findChild<QPlainTextEdit*>(QStringLiteral("ItemPropertiesTextValue"));
            if (!textValue) {
                return;
            }
            textValue->setPlainText(QStringLiteral("端到端文字图元"));
            dialog->accept();
        });
        const QPoint clickPosition = window.view()->mapFromScene(window.scene()->canvasRect().center());
        sendMouseEvent(window.view()->viewport(), QEvent::MouseButtonPress, clickPosition, Qt::LeftButton,
                       Qt::LeftButton);
        sendMouseEvent(window.view()->viewport(), QEvent::MouseButtonRelease, clickPosition, Qt::LeftButton,
                       Qt::NoButton);
        QApplication::processEvents();

        ctx.check(window.scene()->canvasItemCount() == 1, QStringLiteral("主窗口文字模式端到端创建图元"));
        TextItem* createdText = nullptr;
        if (window.scene()->canvasItemCount() == 1) {
            createdText = dynamic_cast<TextItem*>(window.scene()->canvasItems().constFirst());
        }
        ctx.check(createdText && createdText->text() == QStringLiteral("端到端文字图元"),
                  QStringLiteral("主窗口创建的文字内容正确"));
        ctx.check(createdText && createdText->textColor() == window.view()->brushColor() &&
                      createdText->layerId() == window.scene()->activeLayerId() &&
                      window.scene()->layerHeightsUnique(createdText->layerId()),
                  QStringLiteral("主窗口文字使用所选颜色且高度唯一"));
        ctx.check(window.view()->mode() == ToolMode::Edit, QStringLiteral("主窗口添加文字后切回编辑模式"));

        QAction* heightUpAction = nullptr;
        QAction* heightDownAction = nullptr;
        const QList<QAction*> windowActions = window.findChildren<QAction*>();
        for (QAction* action : windowActions) {
            if (action->text().startsWith(QStringLiteral("高度 +1"))) {
                heightUpAction = action;
            } else if (action->text().startsWith(QStringLiteral("高度 -1"))) {
                heightDownAction = action;
            }
        }
        ctx.check(heightUpAction && heightUpAction->shortcut() == QKeySequence(Qt::Key_BracketRight) &&
                      heightDownAction && heightDownAction->shortcut() == QKeySequence(Qt::Key_BracketLeft),
                  QStringLiteral("编辑菜单提供 ] / [ 高度快捷键"));
        // 菜单文字里不该再手写方括号：QAction 自己会把快捷键显示在右侧，写一遍就成了两份。
        ctx.check(heightUpAction && !heightUpAction->text().contains(QLatin1Char(']')) && heightDownAction &&
                      !heightDownAction->text().contains(QLatin1Char('[')),
                  QStringLiteral("高度菜单项文字不再自带方括号"), heightUpAction ? heightUpAction->text() : QString());

        QAction* propertiesAction = nullptr;
        for (QAction* action : windowActions) {
            if (action->text().startsWith(QStringLiteral("图元属性"))) {
                propertiesAction = action;
                break;
            }
        }
        if (createdText) {
            createdText->setSelected(true);
        }
        QApplication::processEvents();
        ctx.check(propertiesAction && propertiesAction->isEnabled() && propertiesAction->shortcut().isEmpty(),
                  QStringLiteral("图元属性保留菜单入口、不绑单字母快捷键，选中单个图元时可用"),
                  propertiesAction ? propertiesAction->text() : QString());
        if (createdText && heightUpAction && heightDownAction) {
            const int heightBeforeShortcut = createdText->height();
            heightUpAction->trigger();
            ctx.check(createdText->height() == heightBeforeShortcut + 1, QStringLiteral("] 快捷键提升选中图元高度"));
            heightDownAction->trigger();
            ctx.check(createdText->height() == heightBeforeShortcut, QStringLiteral("[ 快捷键降低选中图元高度"));
        }

        // 需求 42 端到端：查看模式双击图元，MainWindow 以只读模态窗打开。
        if (createdText) {
            window.view()->setMode(ToolMode::View);
            QApplication::processEvents();
            bool readOnlyDialogSeen = false;
            QTimer::singleShot(80, &window, [&]() {
                auto* dialog = qobject_cast<ItemPropertiesDialog*>(QApplication::activeModalWidget());
                if (!dialog) {
                    return;
                }
                readOnlyDialogSeen = true;
                ctx.check(dialog->isModal() && !dialog->isEditable(),
                          QStringLiteral("查看模式端到端打开只读模态属性窗"));
                QLineEdit* nameEdit = dialog->findChild<QLineEdit*>(QStringLiteral("ItemPropertiesNameValue"));
                ctx.check(nameEdit && !nameEdit->isEnabled(), QStringLiteral("查看模式端到端属性字段不可编辑"));
                dialog->accept();
            });
            const QPoint textPoint = window.view()->mapFromScene(createdText->sceneContentRect().center());
            QMouseEvent textDoubleClick(QEvent::MouseButtonDblClick, QPointF(textPoint),
                                        QPointF(window.view()->viewport()->mapToGlobal(textPoint)), Qt::LeftButton,
                                        Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(window.view()->viewport(), &textDoubleClick);
            QApplication::processEvents();
            ctx.check(readOnlyDialogSeen, QStringLiteral("查看模式双击图元确实打开属性窗"));
            window.view()->setMode(ToolMode::Edit);
        }
    }
}
