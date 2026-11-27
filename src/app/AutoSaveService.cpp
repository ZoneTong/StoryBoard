#include "app/AutoSaveService.h"

#include "app/CanvasSession.h"
#include "app/CanvasTabsController.h"
#include "app/Settings.h"
#include "app/TestMode.h"
#include "scene/CanvasScene.h"
#include "ui/mainwindow/MainWindow.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QPointer>
#include <QSaveFile>
#include <QUndoStack>

namespace {

// 恢复文件扩展名：场景本体与元数据成对出现，按会话 uuid 命名。
const char* const SCENE_SUFFIX = ".sbcanvas";
const char* const META_SUFFIX = ".json";
const int MAX_AGE_DAYS = 7;

bool WriteAtomically(const QString& path, const QByteArray& bytes)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(bytes);
    return file.commit();
}

} // namespace

AutoSaveService::AutoSaveService(MainWindow* window) : m_window(window)
{
    // 单线程池：同一时刻只落一份盘，避免多个大场景同时抢 IO。
    m_pool.setMaxThreadCount(1);
    m_timer.setSingleShot(true);
    m_timer.setInterval(m_intervalMs);
    QObject::connect(&m_timer, &QTimer::timeout, this, [this]() { Fire(); });
}

AutoSaveService::~AutoSaveService()
{
    m_timer.stop();
    // 等待在途写盘结束；完成回调可能还排在事件队列里，QPointer 保证不再进已析构对象。
    m_pool.waitForDone();
}

void AutoSaveService::Watch(CanvasSession* session)
{
    if (!session || !session->Scene() || !session->Undo()) {
        return;
    }
    const QString id = session->RecoveryId();
    if (m_states.contains(id)) {
        return;
    }
    State state;
    state.session = session;
    state.dirty = session->HasUnsavedChanges();
    m_states.insert(id, state);

    CanvasScene* scene = session->Scene();
    QUndoStack* undo = session->Undo();
    QObject::connect(scene, &CanvasScene::contentChanged, this, [this, id]() {
        auto it = m_states.find(id);
        if (it == m_states.end()) {
            return;
        }
        ++it->contentRevision;
        Evaluate();
    });
    QObject::connect(undo, &QUndoStack::cleanChanged, this, [this, id](bool clean) {
        auto it = m_states.find(id);
        if (it == m_states.end()) {
            return;
        }
        it->dirty = !clean;
        if (clean) {
            // 回到已保存状态：备份已无意义，立即清掉，别等下一轮。
            RemoveFiles(id);
            it->backedUpRevision = it->contentRevision;
        }
        Evaluate();
    });
    Evaluate();
}

void AutoSaveService::Forget(CanvasSession* session)
{
    if (!session) {
        return;
    }
    const QString id = session->RecoveryId();
    RemoveFiles(id);
    m_states.remove(id);
    Evaluate();
}

void AutoSaveService::MarkSaved(CanvasSession* session)
{
    if (!session) {
        return;
    }
    const QString id = session->RecoveryId();
    RemoveFiles(id);
    auto it = m_states.find(id);
    if (it != m_states.end()) {
        it->dirty = session->HasUnsavedChanges();
        it->backedUpRevision = it->contentRevision;
    }
    Evaluate();
}

void AutoSaveService::ForgetAll()
{
    for (const State& state : std::as_const(m_states)) {
        RemoveFiles(state.session ? state.session->RecoveryId() : QString());
    }
    m_states.clear();
    m_timer.stop();
}

void AutoSaveService::SetIntervalForTest(int milliseconds)
{
    m_intervalMs = qMax(0, milliseconds);
    m_timer.setInterval(m_intervalMs);
}

bool AutoSaveService::NeedsBackup(const State& state) const
{
    return state.session && state.dirty && !state.writing && state.contentRevision != state.backedUpRevision;
}

void AutoSaveService::Evaluate()
{
    bool needed = false;
    for (const State& state : std::as_const(m_states)) {
        if (NeedsBackup(state)) {
            needed = true;
            break;
        }
    }
    if (needed) {
        if (!m_timer.isActive()) {
            m_timer.start(m_intervalMs);
        }
    } else {
        m_timer.stop();
    }
}

void AutoSaveService::Fire()
{
    // 单次定时器到点即灭；只处理「脏 + 内容变过」的会话，其余不写盘。
    QList<QString> candidates;
    for (auto it = m_states.cbegin(); it != m_states.cend(); ++it) {
        if (NeedsBackup(it.value())) {
            candidates.append(it.key());
        }
    }
    for (const QString& id : std::as_const(candidates)) {
        auto it = m_states.find(id);
        if (it == m_states.end() || !NeedsBackup(it.value())) {
            continue;
        }
        it->writing = true;
        ++it->generation;
        WriteSnapshot(id, it.value());
    }
    // 写完（含在途写盘）后若仍有变过的脏页，再续一轮；否则停表。
    Evaluate();
}

void AutoSaveService::WriteSnapshot(const QString& id, const State& state)
{
    if (!state.session || !state.session->Scene()) {
        return;
    }
    AppSettings::EnsureRecoveryDir();
    const QString dir = AppSettings::RecoveryDir();
    const QString scenePath = QDir(dir).filePath(id + QString::fromLatin1(SCENE_SUFFIX));
    const QString metaPath = QDir(dir).filePath(id + QString::fromLatin1(META_SUFFIX));

    // 序列化必须在 UI 线程（ToJson 要读场景），字节拿到后交给线程池落盘。
    QJsonObject sceneJson = state.session->Scene()->ToJson();
    QJsonObject meta;
    meta.insert(QStringLiteral("format"), QStringLiteral("StoryBoardRecovery"));
    meta.insert(QStringLiteral("version"), 1);
    meta.insert(QStringLiteral("sessionUuid"), id);
    meta.insert(QStringLiteral("displayName"), state.session->DisplayName());
    meta.insert(QStringLiteral("originalPath"), state.session->FilePath());
    meta.insert(QStringLiteral("savedAt"), QDateTime::currentDateTime().toString(Qt::ISODate));

    const quint64 generation = state.generation;
    const quint64 revision = state.contentRevision;
    QPointer<AutoSaveService> guard(this);
    m_pool.start([guard, id, generation, revision, scenePath, metaPath, sceneJson, meta]() {
        const QByteArray sceneBytes = QJsonDocument(sceneJson).toJson(QJsonDocument::Compact);
        const QByteArray metaBytes = QJsonDocument(meta).toJson(QJsonDocument::Compact);
        const bool sceneOk = WriteAtomically(scenePath, sceneBytes);
        const bool metaOk = sceneOk && WriteAtomically(metaPath, metaBytes);
        if (!sceneOk || !metaOk) {
            qWarning("auto-save failed for %s", qUtf8Printable(id));
        }
        if (guard) {
            QMetaObject::invokeMethod(
                guard,
                [guard, id, generation, revision, sceneOk]() {
                    if (guard) {
                        guard->OnWritten(id, generation, revision);
                    }
                    Q_UNUSED(sceneOk)
                },
                Qt::QueuedConnection);
        }
    });
}

void AutoSaveService::OnWritten(const QString& id, quint64 generation, quint64 revision)
{
    auto it = m_states.find(id);
    if (it == m_states.end() || it->generation != generation) {
        // 写盘期间保存 / 关闭 / 又改过：过期快照不许回写记录。
        return;
    }
    it->writing = false;
    it->backedUpRevision = revision;
    Evaluate();
}

void AutoSaveService::RemoveFiles(const QString& id)
{
    if (id.isEmpty()) {
        return;
    }
    const QString dir = AppSettings::RecoveryDir();
    QDir(dir).remove(id + QString::fromLatin1(SCENE_SUFFIX));
    QDir(dir).remove(id + QString::fromLatin1(META_SUFFIX));
}

QList<QJsonObject> AutoSaveService::CollectRecoverable() const
{
    QList<QJsonObject> result;
    const QString dirPath = AppSettings::RecoveryDir();
    QDir dir(dirPath);
    if (!dir.exists()) {
        return result;
    }
    const QDateTime now = QDateTime::currentDateTime();
    const QStringList metas = dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Time);
    for (const QString& name : metas) {
        const QString path = dir.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QJsonObject meta = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        const QString id = meta.value(QStringLiteral("sessionUuid")).toString();
        if (id.isEmpty()) {
            continue;
        }
        const QString scenePath = dir.filePath(id + QString::fromLatin1(SCENE_SUFFIX));
        const QFileInfo info(scenePath);
        if (!info.exists()) {
            // 只剩元数据：没有可恢复内容，清掉这对残片。
            dir.remove(name);
            continue;
        }
        const QDateTime savedAt = info.lastModified();
        if (savedAt.isValid() && savedAt.daysTo(now) > MAX_AGE_DAYS) {
            dir.remove(name);
            dir.remove(scenePath);
            continue;
        }
        QJsonObject entry = meta;
        entry.insert(QStringLiteral("_scenePath"), scenePath);
        result.append(entry);
    }
    return result;
}

void AutoSaveService::ScanOnStartup()
{
    if (TestMode::IsSelfTestMode()) {
        // 自检会自己造恢复文件，启动扫描既不弹框也不删它们。
        return;
    }
    const QList<QJsonObject> recoverable = CollectRecoverable();
    if (recoverable.isEmpty()) {
        return;
    }
    QStringList lines;
    for (const QJsonObject& meta : recoverable) {
        lines << MainWindow::tr("%1 — saved %2")
                     .arg(meta.value(QStringLiteral("displayName")).toString(),
                          meta.value(QStringLiteral("savedAt")).toString());
    }
    const auto answer = QMessageBox::question(
        m_window, MainWindow::tr("Unsaved canvases found"),
        MainWindow::tr("Found %1 unsaved canvas(es) from a previous session:\n%2\n\nRecover them now?")
            .arg(recoverable.size())
            .arg(lines.join(QStringLiteral("\n"))),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (answer == QMessageBox::Yes) {
        for (const QJsonObject& meta : recoverable) {
            RestoreOne(meta);
        }
        return;
    }
    for (const QJsonObject& meta : recoverable) {
        RemoveFiles(meta.value(QStringLiteral("sessionUuid")).toString());
    }
}

void AutoSaveService::RestoreOne(const QJsonObject& meta)
{
    if (!m_window || !m_window->Tabs()) {
        return;
    }
    const QString id = meta.value(QStringLiteral("sessionUuid")).toString();
    const QString scenePath = meta.value(QStringLiteral("_scenePath")).toString();
    QFile file(scenePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonObject sceneJson = QJsonDocument::fromJson(file.readAll()).object();

    CanvasSession* session = m_window->Tabs()->CreateSession(false);
    if (!session) {
        return;
    }
    QString error;
    if (!session->Scene()->FromJson(sceneJson, &error) || !session->Undo()) {
        qWarning("recovery failed for %s: %s", qUtf8Printable(id), qUtf8Printable(error));
        m_window->Tabs()->RemoveSession(session);
        return;
    }
    session->Undo()->clear();
    session->Undo()->setClean();
    const QString originalName = meta.value(QStringLiteral("displayName")).toString();
    // 恢复出的画布不带路径：保存必须走「另存为」，绝不覆盖原文件。
    session->SetFilePath(QString());
    session->SetDisplayNameOverride(MainWindow::tr("%1 (recovered)").arg(originalName));
    m_window->Tabs()->UpdateTabLabel(session);
    m_window->Tabs()->SetCurrent(session);
    // 恢复完成即视为已落定：清掉这对临时文件，避免下次启动重复提示。
    RemoveFiles(id);
}
