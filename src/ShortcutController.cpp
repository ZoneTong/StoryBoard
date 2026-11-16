#include "ShortcutController.h"

#include "MainWindow.h"
#include "UiActionFactory.h"

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>

namespace {

bool selfTestMode()
{
    return qApp && qApp->property("storyboardSelfTest").toBool();
}

// 与 MainWindow 相同的报错出口：自检（离屏）时不许弹模态框。
void reportWarning(QWidget* parent, const QString& title, const QString& text)
{
    if (selfTestMode()) {
        qWarning("self-test suppressed dialog [%s]: %s", qUtf8Printable(title), qUtf8Printable(text));
        return;
    }
    QMessageBox::warning(parent, title, text);
}

// 一览窗口按功能分组：顺着菜单栏的顺序递归收集动作，同一菜单下的快捷键自然挨在一起，
// 顺序与用户平时找入口的顺序一致，语言切换或改键都不影响分组。
void collectMenuActions(QMenu* menu, QList<QAction*>& ordered)
{
    if (!menu) {
        return;
    }
    for (QAction* action : menu->actions()) {
        if (!action) {
            continue;
        }
        if (action->menu()) {
            collectMenuActions(action->menu(), ordered);
            continue;
        }
        if (!ordered.contains(action)) {
            ordered.append(action);
        }
    }
}

} // namespace

ShortcutController::ShortcutController(MainWindow* window) : m_window(window) {}

void ShortcutController::applyShortcutOverrides()
{
    // 用「默认快捷键」当稳定标识：它在代码里写死，换语言、改键后都不变，
    // 比菜单文字更适合做配置键。默认键只在启动时记一次，之后改键不会把它覆盖掉，
    // 否则「恢复默认」就找不回原来的组合键了。
    if (m_defaultShortcuts.isEmpty()) {
        for (QAction* action : m_window->findChildren<QAction*>()) {
            if (!action || action->shortcuts().isEmpty()) {
                continue;
            }
            QStringList portable;
            for (const QKeySequence& sequence : action->shortcuts()) {
                const QString text = sequence.toString(QKeySequence::PortableText);
                if (!text.isEmpty()) {
                    portable.append(text);
                }
            }
            if (!portable.isEmpty()) {
                m_defaultShortcuts.insert(action, portable);
            }
        }
    }
    for (auto it = m_defaultShortcuts.constBegin(); it != m_defaultShortcuts.constEnd(); ++it) {
        QAction* action = it.key();
        if (!action) {
            continue;
        }
        const QString id = it.value().constFirst();
        const auto overrideIt = m_window->m_settings.shortcutOverrides.constFind(id);
        if (overrideIt != m_window->m_settings.shortcutOverrides.constEnd() && !overrideIt.value().isEmpty()) {
            const QKeySequence sequence = QKeySequence::fromString(overrideIt.value(), QKeySequence::PortableText);
            if (!sequence.isEmpty()) {
                action->setShortcut(sequence);
                continue;
            }
        }
        // 没有覆盖（或覆盖为空）就是默认值：多键动作整组恢复，不只恢复第一个。
        QList<QKeySequence> defaults;
        for (const QString& portable : it.value()) {
            defaults.append(QKeySequence::fromString(portable, QKeySequence::PortableText));
        }
        action->setShortcuts(defaults);
    }
    refreshShortcutTips();
}

void ShortcutController::refreshShortcutTips()
{
    for (QAction* action : m_window->findChildren<QAction*>()) {
        if (!action) {
            continue;
        }
        const QVariant base = action->property("storyBoardBaseTip");
        // 没走 makeAction 的动作（如退出、高度 ±1）没有记基串，就从现有提示里
        // 去掉末尾的 “(Ctrl+X) / （Ctrl+X）”，否则改键后旧键会一直留在提示里。
        QString baseTip;
        if (base.isValid()) {
            baseTip = base.toString();
        } else {
            const QString source = action->statusTip().isEmpty() ? action->toolTip() : action->statusTip();
            // 没走 makeAction 的动作（如退出、高度 ±1）没有记基串，就用同一份
            // stripShortcutTail 从现有提示里剥掉末尾键位括号。
            baseTip = stripShortcutTail(source);
        }
        if (baseTip.isEmpty()) {
            if (!base.isValid()) {
                // 原本就没有提示的动作，不要硬塞一条。
                continue;
            }
            action->setToolTip(action->text());
            action->setStatusTip(action->text());
            continue;
        }
        const QString keys = action->shortcut().toString(QKeySequence::NativeText);
        const QString tip = keys.isEmpty() ? baseTip : baseTip + QStringLiteral(" (") + keys + QLatin1Char(')');
        action->setToolTip(tip);
        action->setStatusTip(tip);
    }
}

bool ShortcutController::changeShortcut(QAction* action, const QKeySequence& sequence)
{
    if (!action || sequence.isEmpty()) {
        return false;
    }
    const QString id = m_defaultShortcuts.value(action).value(0);
    if (id.isEmpty()) {
        return false;
    }
    // 同一组合键只允许绑定一个动作，避免按下后 Qt 触发不确定的那个。
    for (QAction* other : m_window->findChildren<QAction*>()) {
        if (!other || other == action || other->shortcut().isEmpty()) {
            continue;
        }
        for (const QKeySequence& existing : other->shortcuts()) {
            if (existing == sequence) {
                reportWarning(m_window, MainWindow::tr("Shortcut already in use"),
                              MainWindow::tr("%1 is already used by “%2”.")
                                  .arg(sequence.toString(QKeySequence::NativeText), other->text()));
                return false;
            }
        }
    }
    action->setShortcut(sequence);
    if (sequence == QKeySequence::fromString(id, QKeySequence::PortableText)) {
        m_window->m_settings.shortcutOverrides.remove(id);
    } else {
        m_window->m_settings.shortcutOverrides.insert(id, sequence.toString(QKeySequence::PortableText));
    }
    m_window->m_settings.save();
    refreshShortcutTips();
    return true;
}

void ShortcutController::showShortcuts()
{
    // 直接从现有动作汇总，新加的快捷键自动进表。
    struct Entry {
        QAction* action = nullptr;
        QString text;
        QString keys;
    };
    // 菜单栏顺序就是功能分组顺序：文件 → 视图 → 编辑 → 插入 → 帮助；
    // 只有工具栏 / 卡片入口、没有菜单项的动作按文字顺序补在末尾。
    QList<QAction*> orderedActions;
    QMenuBar* menuBar = m_window->menuBar();
    if (menuBar) {
        for (QAction* top : menuBar->actions()) {
            if (top && top->menu()) {
                collectMenuActions(top->menu(), orderedActions);
            }
        }
    }
    QList<QAction*> menuLessActions;
    for (QAction* action : m_window->findChildren<QAction*>()) {
        if (!action || orderedActions.contains(action)) {
            continue;
        }
        menuLessActions.append(action);
    }
    std::sort(menuLessActions.begin(), menuLessActions.end(), [](QAction* left, QAction* right) {
        return left->text().compare(right->text(), Qt::CaseInsensitive) < 0;
    });
    orderedActions.append(menuLessActions);

    QList<Entry> entries;
    for (QAction* action : orderedActions) {
        if (!action || action->shortcut().isEmpty()) {
            continue;
        }
        QString text = action->text();
        text = text.trimmed();
        if (text.isEmpty()) {
            continue;
        }
        const QString keys = action->shortcut().toString(QKeySequence::NativeText);
        bool duplicate = false;
        for (const Entry& existing : entries) {
            if (existing.text == text && existing.keys == keys) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            entries.append({action, text, keys});
        }
    }

    QDialog dialog(m_window);
    dialog.setWindowTitle(MainWindow::tr("Keyboard shortcuts"));
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(18, 16, 18, 12);
    layout->setSpacing(10);

    auto* title = new QLabel(MainWindow::tr("Click a shortcut to change it; Esc closes this dialog."), &dialog);
    title->setWordWrap(true);
    layout->addWidget(title);

    // 改成新键后要就地回显键帽文字，不能等下次打开列表才更新。
    QList<QPair<QAction*, QPushButton*>> keyButtons;

    // 分成三栏，条目多时也不至于拉成一条细长的竖列。
    constexpr int kColumns = 3;
    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(6);
    const int rows = (entries.size() + kColumns - 1) / kColumns;
    for (int i = 0; i < entries.size(); ++i) {
        const int column = i / qMax(1, rows);
        const int row = i % qMax(1, rows);
        auto* name = new QLabel(entries.at(i).text, &dialog);
        name->setToolTip(entries.at(i).text);
        auto* keys = new QPushButton(entries.at(i).keys, &dialog);
        keys->setFlat(true);
        keys->setCursor(Qt::PointingHandCursor);
        keys->setToolTip(MainWindow::tr("Change this shortcut"));
        QAction* action = entries.at(i).action;
        keyButtons.append(qMakePair(action, keys));
        QObject::connect(keys, &QPushButton::clicked, &dialog, [this, &dialog, action, keys]() {
            QDialog edit(&dialog);
            edit.setWindowTitle(MainWindow::tr("Change shortcut"));
            auto* editLayout = new QVBoxLayout(&edit);
            auto* prompt =
                new QLabel(MainWindow::tr("Press the new key combination for “%1”.").arg(action->text()), &edit);
            prompt->setWordWrap(true);
            editLayout->addWidget(prompt);
            auto* editor = new QKeySequenceEdit(action->shortcut(), &edit);
            editLayout->addWidget(editor);
            auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &edit);
            editLayout->addWidget(buttons);
            QObject::connect(buttons, &QDialogButtonBox::accepted, &edit, &QDialog::accept);
            QObject::connect(buttons, &QDialogButtonBox::rejected, &edit, &QDialog::reject);
            if (edit.exec() != QDialog::Accepted) {
                return;
            }
            if (changeShortcut(action, editor->keySequence())) {
                keys->setText(action->shortcut().toString(QKeySequence::NativeText));
            }
        });
        grid->addWidget(name, row, column * 2);
        grid->addWidget(keys, row, column * 2 + 1);
    }
    for (int column = 0; column < kColumns; ++column) {
        grid->setColumnStretch(column * 2, 1);
    }
    layout->addLayout(grid);
    layout->addStretch(1);

    auto* buttonRow = new QHBoxLayout();
    auto* resetButton = new QPushButton(MainWindow::tr("Restore default shortcuts"), &dialog);
    buttonRow->addWidget(resetButton);
    buttonRow->addStretch(1);
    auto* closeButton = new QPushButton(MainWindow::tr("Close"), &dialog);
    closeButton->setDefault(true);
    buttonRow->addWidget(closeButton);
    layout->addLayout(buttonRow);
    QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    QObject::connect(resetButton, &QPushButton::clicked, &dialog, [this, &keyButtons]() {
        m_window->m_settings.shortcutOverrides.clear();
        m_window->m_settings.save();
        applyShortcutOverrides();
        // 恢复默认后键帽也要当场回到默认键。
        for (const auto& item : keyButtons) {
            if (item.first) {
                item.second->setText(item.first->shortcut().toString(QKeySequence::NativeText));
            }
        }
    });

    dialog.resize(920, 560);
    dialog.exec();
}
