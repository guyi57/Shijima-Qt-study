#pragma once

#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>

struct MemoryItem {
    QString id;
    QString type;       // "preference", "event", "fact"
    QString content;
    int importance = 1; // 1~5
    qint64 createdAt = 0;
};

class PetMemory
{
public:
    static PetMemory *instance();

    void load(const QString &filePath = "memory.json");
    void save(const QString &filePath = "memory.json");

    void addMemory(const QString &type, const QString &content, int importance = 1);
    QList<MemoryItem> getTopMemories(int limit = 5);
    QString formatForPrompt(int limit = 5);

private:
    PetMemory();
    QMutex m_mutex;
    QList<MemoryItem> m_items;
    QString m_filePath;
};
