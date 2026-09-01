#pragma once

#include <QObject>
#include <QString>
#include <QSet>
#include <QFileSystemWatcher>
#include <QDateTime>
#include <QTimer>

class TrashWatcher : public QObject {
public:
    static TrashWatcher* instance();

    void init();
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }
    void onNativeFileEvent(const QString &filePath);

private:
    TrashWatcher();
    ~TrashWatcher() override = default;

    void onDirectoryChanged(const QString &path);
    void updateSnapshot();

    bool m_enabled = true;
    QString m_trashPath;
    QFileSystemWatcher *m_watcher = nullptr;
    void *m_fseventStream = nullptr;
    QSet<QString> m_knownFiles;
    qint64 m_lastTriggerTime = 0;
};
