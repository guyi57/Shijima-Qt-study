#include "TrashWatcher.hpp"
#include "FileDisposalSequence.hpp"
#include "ShijimaManager.hpp"
#include "ShijimaWidget.hpp"
#include <QDir>
#include <QFileInfo>
#include <iostream>

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

    m_watcher = new QFileSystemWatcher(this);
    m_watcher->addPath(m_trashPath);

    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &TrashWatcher::onDirectoryChanged);

    std::cout << "[TrashWatcher] 已启动回收站监听: " << m_trashPath.toStdString()
              << " (初始文件数: " << m_knownFiles.size() << ")" << std::endl;
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
        m_knownFiles.insert(entry);
    }
}

void TrashWatcher::onDirectoryChanged(const QString &) {
    if (!m_enabled) return;

    QDir trashDir(m_trashPath);
    if (!trashDir.exists()) return;

    QStringList currentEntries = trashDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    QSet<QString> currentSet;
    for (const auto &e : currentEntries) {
        currentSet.insert(e);
    }

    // 查找新增进入回收站的文件
    QSet<QString> newFiles = currentSet - m_knownFiles;
    m_knownFiles = currentSet;

    if (newFiles.isEmpty()) return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastTriggerTime < 2500) {
        // 2.5秒内防抖（防止批量删除多个文件触发多次）
        return;
    }
    m_lastTriggerTime = now;

    QString latestDeletedFile = *newFiles.begin();
    std::cout << "[TrashWatcher] 监听到真实文件被删除移入废纸篓: "
              << latestDeletedFile.toStdString() << std::endl;

    // 寻找当前屏幕上的主桌宠并触发搬运与丢弃动画
    auto manager = ShijimaManager::defaultManager();
    if (!manager) return;

    manager->onTickSync([latestDeletedFile](ShijimaManager *mgr) {
        auto &mascots = mgr->mascots();
        if (!mascots.empty() && !FileDisposalSequence::instance()->isRunning()) {
            FileDisposalSequence::instance()->start(mascots.front(), latestDeletedFile);
        }
    });
}
