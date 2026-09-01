#include "TrashWatcher.hpp"
#include "FileDisposalSequence.hpp"
#include "ShijimaManager.hpp"
#include "ShijimaWidget.hpp"
#include <QDir>
#include <QFileInfo>
#include <iostream>

#if defined(Q_OS_MAC)
#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>

static void fseventTrashCallback(
    ConstFSEventStreamRef /*streamRef*/,
    void *clientCallBackInfo,
    size_t numEvents,
    void *eventPaths,
    const FSEventStreamEventFlags /*eventFlags*/[],
    const FSEventStreamEventId /*eventIds*/[])
{
    TrashWatcher *watcher = static_cast<TrashWatcher*>(clientCallBackInfo);
    if (!watcher || !watcher->isEnabled()) return;

    char **paths = (char **)eventPaths;
    for (size_t i = 0; i < numEvents; ++i) {
        QString changedPath = QString::fromUtf8(paths[i]);
        watcher->onNativeFileEvent(changedPath);
    }
}
#endif

TrashWatcher* TrashWatcher::instance() {
    static TrashWatcher s_instance;
    return &s_instance;
}

TrashWatcher::TrashWatcher() {
#if defined(Q_OS_MAC)
    m_trashPath = QDir::homePath() + "/.Trash";
#elif defined(Q_OS_LINUX)
    m_trashPath = QDir::homePath() + "/.local/share/Trash/files";
#else
    m_trashPath = "";
#endif
}

void TrashWatcher::init() {
    if (m_trashPath.isEmpty()) return;

    QDir trashDir(m_trashPath);
    if (!trashDir.exists()) return;

    updateSnapshot();

#if defined(Q_OS_MAC)
    // 采用 macOS 原生内核级 FSEvents (0% CPU 占用、零轮询、由系统内核直接中断推送事件)
    CFStringRef mypath = CFStringCreateWithCString(kCFAllocatorDefault, m_trashPath.toUtf8().constData(), kCFStringEncodingUTF8);
    CFArrayRef pathsToWatch = CFArrayCreate(NULL, (const void **)&mypath, 1, NULL);
    CFOptionFlags flags = kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer;

    FSEventStreamContext context = {0, (void*)this, NULL, NULL, NULL};
    FSEventStreamRef stream = FSEventStreamCreate(
        kCFAllocatorDefault,
        &fseventTrashCallback,
        &context,
        pathsToWatch,
        kFSEventStreamEventIdSinceNow,
        0.05, // 0.05s 极速内核响应
        flags
    );

    if (stream) {
        FSEventStreamSetDispatchQueue(stream, dispatch_get_main_queue());
        FSEventStreamStart(stream);
        m_fseventStream = (void*)stream;
    }
    CFRelease(pathsToWatch);
    CFRelease(mypath);

    std::cout << "[TrashWatcher] 已启动 macOS 原生内核级 FSEvents 零轮询事件监听: " << m_trashPath.toStdString()
              << " (初始文件数: " << m_knownFiles.size() << ")" << std::endl;
#else
    m_watcher = new QFileSystemWatcher(this);
    m_watcher->addPath(m_trashPath);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &TrashWatcher::onDirectoryChanged);
    std::cout << "[TrashWatcher] 已启动 QFileSystemWatcher 目录事件监听: " << m_trashPath.toStdString()
              << " (初始文件数: " << m_knownFiles.size() << ")" << std::endl;
#endif
}

void TrashWatcher::setEnabled(bool enabled) {
    m_enabled = enabled;
}

void TrashWatcher::updateSnapshot() {
    m_knownFiles.clear();
    QDir trashDir(m_trashPath);
    if (!trashDir.exists()) return;

    QStringList entries = trashDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &entry : entries) {
        if (entry != ".DS_Store") {
            m_knownFiles.insert(entry);
        }
    }
}

void TrashWatcher::onDirectoryChanged(const QString &path) {
    onNativeFileEvent(path);
}

void TrashWatcher::onNativeFileEvent(const QString &filePath) {
    if (!m_enabled) return;

    QFileInfo fi(filePath);
    QString filename = fi.fileName();

    // 1. 如果内核直接报告了被移入废纸篓的具体文件
    if (!filename.isEmpty() && filename != ".Trash" && filename != ".DS_Store") {
        if (!m_knownFiles.contains(filename)) {
            m_knownFiles.insert(filename);

            qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (now - m_lastTriggerTime < 2000) return;
            m_lastTriggerTime = now;

            std::cout << "[TrashWatcher] macOS FSEvents 零轮询捕获到文件移入废纸篓: "
                      << filename.toStdString() << std::endl;

            QTimer::singleShot(0, [filename]() {
                auto manager = ShijimaManager::defaultManager();
                if (!manager) return;
                auto &mascots = manager->mascots();
                if (!mascots.empty() && !FileDisposalSequence::instance()->isRunning()) {
                    FileDisposalSequence::instance()->start(mascots.front(), filename);
                }
            });
            return;
        }
    }

    // 2. 否则只在收到系统内核通知时做一次增量快照比对
    QDir trashDir(m_trashPath);
    if (!trashDir.exists()) return;

    QStringList currentEntries = trashDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    QSet<QString> currentSet;
    for (const auto &e : currentEntries) {
        if (e != ".DS_Store") {
            currentSet.insert(e);
        }
    }

    QSet<QString> newFiles = currentSet - m_knownFiles;
    m_knownFiles = currentSet;

    if (newFiles.isEmpty()) return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastTriggerTime < 2000) {
        return;
    }
    m_lastTriggerTime = now;

    QString latestDeletedFile = *newFiles.begin();
    std::cout << "[TrashWatcher] macOS FSEvents 零轮询捕获到文件移入废纸篓: "
              << latestDeletedFile.toStdString() << std::endl;

    // 确保在 Qt GUI 主线程触发黑洞吞噬与爬行推文件动画
    QTimer::singleShot(0, [latestDeletedFile]() {
        auto manager = ShijimaManager::defaultManager();
        if (!manager) return;
        auto &mascots = manager->mascots();
        if (!mascots.empty() && !FileDisposalSequence::instance()->isRunning()) {
            FileDisposalSequence::instance()->start(mascots.front(), latestDeletedFile);
        }
    });
}
