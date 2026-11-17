#include "CustomShapePanel.h"

#include "CustomShapeTemplates.h"
#include "MainWindow.h"
#include "Settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QTabWidget>
#include <QTextStream>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kColumns = 4;
constexpr int kIconSize = 40;
const QString kOtherTitle = QStringLiteral("其它");
const QString kOrderFile = QStringLiteral("顺序.txt");

struct CachedTemplate {
    qint64 modified = 0;
    qint64 size = 0;
    CustomShapeTemplate entry;
};

// 模板解析按 mtime + size 缓存，面板每次展开只重解析真正改动过的文件。
QHash<QString, CachedTemplate> g_templateCache;

bool loadTemplateCached(const QFileInfo& info, CustomShapeTemplate* entry, QString* error)
{
    const QString path = info.absoluteFilePath();
    const auto it = g_templateCache.constFind(path);
    if (it != g_templateCache.constEnd() && it->modified == info.lastModified().toMSecsSinceEpoch() &&
        it->size == info.size()) {
        *entry = it->entry;
        return true;
    }
    CustomShapeTemplate loaded;
    if (!loadCustomShapeTemplate(path, &loaded, error)) {
        g_templateCache.remove(path);
        return false;
    }
    CachedTemplate fresh;
    fresh.modified = info.lastModified().toMSecsSinceEpoch();
    fresh.size = info.size();
    fresh.entry = loaded;
    g_templateCache.insert(path, fresh);
    *entry = loaded;
    return true;
}

// 「顺序.txt」每行一个文件夹名，空行与 # 注释忽略；未列出的目录按本地化名称排在后面。
QStringList readOrderFile(const QString& rootPath)
{
    QFile file(QDir(rootPath).filePath(kOrderFile));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    QStringList names;
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        names.append(line);
    }
    return names;
}

// 单个目录扫描：同一目录内的模板按文件名排序，解析失败的记入 errors。
QList<CustomShapeTemplate> scanDirectory(const QString& path, QStringList* errors)
{
    QList<CustomShapeTemplate> items;
    QDir dir(path);
    if (!dir.exists()) {
        return items;
    }
    const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.sbcanvas")}, QDir::Files, QDir::Name);
    for (const QFileInfo& info : files) {
        CustomShapeTemplate entry;
        QString error;
        if (loadTemplateCached(info, &entry, &error)) {
            items.append(entry);
        } else if (errors) {
            errors->append(QStringLiteral("%1: %2").arg(info.fileName(), error));
        }
    }
    return items;
}

} // namespace

CustomShapePanel::CustomShapePanel(MainWindow* window) : QWidget(window), m_window(window)
{
    setObjectName(QStringLiteral("CustomShapePanel"));
    // 独立浮动窗口：不抢主窗口焦点，但可拖动、可记忆位置尺寸；点击外部不关闭。
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
    setWindowTitle(tr("Custom shapes"));
    resize(360, 420);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("CustomShapeTabs"));
    m_tabs->setDocumentMode(true);
    layout->addWidget(m_tabs, 1);

    auto* footer = new QHBoxLayout();
    auto* openButton = new QPushButton(tr("Open shapes folder…"), this);
    openButton->setObjectName(QStringLiteral("CustomShapeOpenFolderButton"));
    auto* refreshButton = new QPushButton(tr("Refresh"), this);
    refreshButton->setObjectName(QStringLiteral("CustomShapeRefreshButton"));
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("CustomShapeStatus"));
    m_status->setStyleSheet(QStringLiteral("color: #6B7280;"));
    footer->addWidget(openButton);
    footer->addWidget(refreshButton);
    footer->addStretch(1);
    footer->addWidget(m_status);
    layout->addLayout(footer);

    connect(openButton, &QPushButton::clicked, this, [this]() {
        if (!AppSettings::openShapesFolder() && m_status) {
            m_status->setText(tr("Could not open the shapes folder"));
        }
    });
    connect(refreshButton, &QPushButton::clicked, this, [this]() { showAt(nullptr); });
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (m_rebuilding || !m_tabs || index < 0) {
            return;
        }
        m_currentKey = m_tabs->widget(index)->property("folderKey").toString();
        saveGeometryToSettings();
    });
    restoreGeometryFromSettings();
}

QString CustomShapePanel::currentFolder() const
{
    if (!m_tabs || m_tabs->currentIndex() < 0) {
        return {};
    }
    return m_tabs->tabText(m_tabs->currentIndex());
}

QList<CustomShapePanel::Folder> CustomShapePanel::scanFolders() const
{
    QList<Folder> folders;
    QList<CustomShapeTemplate> looseItems;
    QStringList looseErrors;
    QHash<QString, int> folderIndex;

    for (const QString& rootPath : AppSettings::shapesDirs()) {
        QDir root(rootPath);
        if (!root.exists()) {
            continue;
        }

        // 子目录：顺序文件优先，其余按本地化名称排序。
        QStringList subdirs;
        const QFileInfoList entries = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo& info : entries) {
            subdirs.append(info.fileName());
        }
        subdirs.sort(Qt::CaseInsensitive);

        const QStringList ordered = readOrderFile(rootPath);
        QStringList orderedExisting;
        for (const QString& name : ordered) {
            if (subdirs.contains(name) && !orderedExisting.contains(name)) {
                orderedExisting.append(name);
            }
        }
        for (const QString& name : subdirs) {
            if (!orderedExisting.contains(name)) {
                orderedExisting.append(name);
            }
        }

        for (const QString& name : orderedExisting) {
            QStringList errors;
            const QList<CustomShapeTemplate> items = scanDirectory(root.filePath(name), &errors);
            // 空目录不出页；有坏文件时保留页，在页底以灰字提示。
            if (items.isEmpty() && errors.isEmpty()) {
                continue;
            }
            Folder folder;
            folder.key = rootPath + QLatin1Char('|') + name;
            folder.title = name;
            folder.items = items;
            folder.errors = errors;
            if (folderIndex.contains(name)) {
                // 多个根目录同名的文件夹合并到同一页，先扫到的在前。
                Folder& existing = folders[folderIndex.value(name)];
                existing.items.append(items);
                existing.errors.append(errors);
            } else {
                folderIndex.insert(name, folders.size());
                folders.append(folder);
            }
        }

        QStringList errors;
        const QList<CustomShapeTemplate> rootItems = scanDirectory(rootPath, &errors);
        looseItems.append(rootItems);
        looseErrors.append(errors);
    }

    if (!looseItems.isEmpty() || !looseErrors.isEmpty()) {
        Folder other;
        other.key = QStringLiteral("|") + kOtherTitle;
        other.title = kOtherTitle;
        other.items = looseItems;
        other.errors = looseErrors;
        folders.append(other); // 根目录散文件固定最后一页
    }
    return folders;
}

void CustomShapePanel::rebuild(const QList<Folder>& folders)
{
    m_rebuilding = true;
    const QString previousKey = m_currentKey;
    m_tabs->clear();

    for (const Folder& folder : folders) {
        auto* page = new QWidget(m_tabs);
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(6, 6, 6, 6);
        pageLayout->setSpacing(4);

        auto* scroll = new QScrollArea(page);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto* gridHost = new QWidget(scroll);
        auto* grid = new QGridLayout(gridHost);
        grid->setContentsMargins(2, 2, 2, 2);
        grid->setHorizontalSpacing(6);
        grid->setVerticalSpacing(6);

        int index = 0;
        for (const CustomShapeTemplate& entry : folder.items) {
            QToolButton* button = new QToolButton(gridHost);
            button->setObjectName(QStringLiteral("CustomShapeTemplateButton"));
            button->setProperty("shapePath", entry.filePath);
            button->setText(entry.name);
            button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            button->setIcon(makeCustomShapeIcon(entry.unitPath, QColor(58, 64, 80), kIconSize));
            button->setIconSize(QSize(kIconSize, kIconSize));
            button->setToolTip(tr("Insert “%1” from %2").arg(entry.name, QFileInfo(entry.filePath).fileName()));
            button->setAutoRaise(true);
            button->setMinimumWidth(kIconSize + 28);
            // 方向键 / 回车要能落到模板按钮上，焦点策略不能是 NoFocus。
            button->setFocusPolicy(Qt::StrongFocus);
            const QString filePath = entry.filePath;
            connect(button, &QToolButton::clicked, this, [this, filePath]() { insertTemplate(filePath); });
            grid->addWidget(button, index / kColumns, index % kColumns, Qt::AlignLeft | Qt::AlignTop);
            ++index;
        }
        for (int column = 0; column < kColumns; ++column) {
            grid->setColumnStretch(column, 1);
        }
        grid->setRowStretch(grid->rowCount(), 1);
        scroll->setWidget(gridHost);
        pageLayout->addWidget(scroll, 1);

        if (!folder.errors.isEmpty()) {
            QLabel* broken = new QLabel(tr("%n shape file(s) could not be read", "", folder.errors.size()), page);
            broken->setObjectName(QStringLiteral("CustomShapeBrokenLabel"));
            broken->setStyleSheet(QStringLiteral("color: #9CA3AF;"));
            broken->setWordWrap(true);
            broken->setToolTip(folder.errors.join(QLatin1Char('\n')));
            pageLayout->addWidget(broken, 0);
        }

        m_tabs->addTab(page, folder.title);
        page->setProperty("folderKey", folder.key);
    }

    int target = -1;
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (m_tabs->widget(i)->property("folderKey").toString() == previousKey) {
            target = i;
            break;
        }
    }
    if (target < 0) {
        target = 0;
    }
    if (m_tabs->count() > 0) {
        m_tabs->setCurrentIndex(target);
        m_currentKey = m_tabs->widget(target)->property("folderKey").toString();
    } else {
        m_currentKey.clear();
        auto* emptyPage = new QWidget(m_tabs);
        auto* emptyLayout = new QVBoxLayout(emptyPage);
        QLabel* empty = new QLabel(tr("(No custom shapes in the shapes folder)"), emptyPage);
        empty->setStyleSheet(QStringLiteral("color: #9CA3AF;"));
        empty->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(empty);
        m_tabs->addTab(emptyPage, tr("Custom shapes"));
    }
    m_rebuilding = false;

    if (m_status) {
        int total = 0;
        for (const Folder& folder : folders) {
            total += folder.items.size();
        }
        m_status->setText(tr("%1 template(s)").arg(total));
    }
}

void CustomShapePanel::showAt(QWidget* anchor)
{
    const QList<Folder> folders = scanFolders();
    rebuild(folders);

    if (anchor) {
        const QPoint origin = anchor->mapToGlobal(QPoint(0, anchor->height() + 4));
        QRect available = screen() ? screen()->availableGeometry() : QRect(origin, size());
        QPoint target(origin.x(), origin.y());
        if (target.x() + width() > available.right()) {
            target.setX(available.right() - width());
        }
        if (target.y() + height() > available.bottom()) {
            target.setY(anchor->mapToGlobal(QPoint(0, 0)).y() - height() - 4);
        }
        move(target);
    }
    show();
    raise();
    activateWindow();
    if (m_tabs && m_tabs->count() > 0) {
        m_tabs->setFocus();
    }
}

void CustomShapePanel::insertTemplate(const QString& filePath)
{
    if (!m_window) {
        return;
    }
    // 插入后收起（Q2 已确认关闭），下一轮通过工具栏/菜单再次展开。
    hide();
    m_window->insertCustomShape(filePath);
}

void CustomShapePanel::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        event->accept();
        return;
    }
    // 回车触发当前焦点模板；方向键在模板按钮间移动焦点，键盘也能整页挑模板。
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (auto* button = qobject_cast<QToolButton*>(focusWidget());
            button && button->objectName() == QStringLiteral("CustomShapeTemplateButton")) {
            button->click();
            event->accept();
            return;
        }
    }
    switch (event->key()) {
    case Qt::Key_Left:
        moveFocusByStep(-1);
        event->accept();
        return;
    case Qt::Key_Right:
        moveFocusByStep(1);
        event->accept();
        return;
    case Qt::Key_Up:
        moveFocusByStep(-kColumns);
        event->accept();
        return;
    case Qt::Key_Down:
        moveFocusByStep(kColumns);
        event->accept();
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void CustomShapePanel::moveFocusByStep(int step)
{
    QWidget* page = m_tabs ? m_tabs->currentWidget() : nullptr;
    if (!page) {
        return;
    }
    const QList<QToolButton*> buttons = page->findChildren<QToolButton*>(QStringLiteral("CustomShapeTemplateButton"));
    if (buttons.isEmpty()) {
        return;
    }
    int index = buttons.indexOf(qobject_cast<QToolButton*>(focusWidget()));
    if (index < 0) {
        index = 0;
    } else {
        index = qBound(0, index + step, buttons.size() - 1);
    }
    buttons.at(index)->setFocus(Qt::OtherFocusReason);
}

void CustomShapePanel::closeEvent(QCloseEvent* event)
{
    saveGeometryToSettings();
    QWidget::closeEvent(event);
}

void CustomShapePanel::restoreGeometryFromSettings()
{
    QSettings settings(AppSettings::settingsFilePath(), QSettings::IniFormat);
    const QByteArray geometry = settings.value(QStringLiteral("customShapes/geometry")).toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    m_currentKey = settings.value(QStringLiteral("customShapes/page")).toString();
}

void CustomShapePanel::saveGeometryToSettings()
{
    QSettings settings(AppSettings::settingsFilePath(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("customShapes/geometry"), saveGeometry());
    if (!m_currentKey.isEmpty()) {
        settings.setValue(QStringLiteral("customShapes/page"), m_currentKey);
    }
}
