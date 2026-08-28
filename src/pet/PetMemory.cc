#include "PetMemory.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QDateTime>
#include <QUuid>
#include <algorithm>

PetMemory *PetMemory::instance()
{
    static PetMemory s_instance;
    return &s_instance;
}

PetMemory::PetMemory()
{
    load();
}

void PetMemory::load(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);
    m_filePath = filePath;
    m_items.clear();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return;

    QJsonObject root = doc.object();
    QJsonArray memArray = root["memories"].toArray();

    for (const auto &val : memArray) {
        QJsonObject obj = val.toObject();
        MemoryItem item;
        item.id = obj["id"].toString();
        item.type = obj["type"].toString();
        item.content = obj["content"].toString();
        item.importance = obj["importance"].toInt(1);
        item.createdAt = obj["created_at"].toVariant().toLongLong();
        m_items.append(item);
    }
}

void PetMemory::save(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);
    QString targetPath = filePath.isEmpty() ? m_filePath : filePath;
    if (targetPath.isEmpty()) targetPath = "memory.json";

    QJsonObject root;
    QJsonArray memArray;
    for (const auto &item : m_items) {
        QJsonObject obj;
        obj["id"] = item.id;
        obj["type"] = item.type;
        obj["content"] = item.content;
        obj["importance"] = item.importance;
        obj["created_at"] = item.createdAt;
        memArray.append(obj);
    }
    root["memories"] = memArray;

    QFile file(targetPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.close();
    }
}

void PetMemory::addMemory(const QString &type, const QString &content, int importance)
{
    QMutexLocker locker(&m_mutex);
    MemoryItem item;
    item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    item.type = type;
    item.content = content;
    item.importance = importance;
    item.createdAt = QDateTime::currentMSecsSinceEpoch();

    m_items.append(item);
    // 保持最大 50 条记忆
    if (m_items.size() > 50) {
        m_items.removeFirst();
    }

    save();
}

QList<MemoryItem> PetMemory::getTopMemories(int limit)
{
    QMutexLocker locker(&m_mutex);
    QList<MemoryItem> sorted = m_items;
    // 按重要度和时间排序
    std::sort(sorted.begin(), sorted.end(), [](const MemoryItem &a, const MemoryItem &b) {
        if (a.importance != b.importance) {
            return a.importance > b.importance;
        }
        return a.createdAt > b.createdAt;
    });

    if (sorted.size() > limit) {
        sorted = sorted.mid(0, limit);
    }
    return sorted;
}

QString PetMemory::formatForPrompt(int limit)
{
    auto topList = getTopMemories(limit);
    if (topList.isEmpty()) {
        return "";
    }

    QStringList lines;
    for (const auto &item : topList) {
        lines << QString("- %1").arg(item.content);
    }
    return lines.join("\n");
}
