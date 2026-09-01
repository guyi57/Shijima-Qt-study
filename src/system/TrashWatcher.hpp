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

private:
    TrashWatcher();
    ~TrashWatcher() override = default;

    void onDirectoryChanged(const QString &path);
    void checkForNewTrashFiles();
    void updateSnapshot();

    bool m_enabled = true;
    QString m_trashPath;
    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_pollTimer = nullptr;
    QSet<QString> m_knownFiles;
    qint64 m_lastTriggerTime = 0;
};
