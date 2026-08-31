#pragma once

#include <QString>
#include <QList>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QMutex>

#include "SettingsDb.hpp"

struct MessageHistoryItem {
    QString id;
    QString type;       // "agent_task", "translate", "ask", "notice"
    QString title;      // 简短摘要/用户指令
    QString content;    // 完整回复/Markdown 内容
    QString appTarget;  // 关联应用
    qint64 timestamp = 0;
};

class MessageHistoryManager
{
public:
    static MessageHistoryManager *instance() {
        static MessageHistoryManager s_mgr;
        return &s_mgr;
    }

    void addRecord(const QString &type, const QString &title, const QString &content, const QString &appTarget = "") {
        QMutexLocker locker(&m_mutex);
        MessageHistoryItem item;
        item.id = QString::number(QDateTime::currentMSecsSinceEpoch());
        item.type = type;
        item.title = title;
        item.content = content;
        item.appTarget = appTarget;
        item.timestamp = QDateTime::currentMSecsSinceEpoch();

        m_items.prepend(item);
        if (m_items.size() > 200) {
            m_items.removeLast();
        }
        save();
    }

    QList<MessageHistoryItem> allRecords() {
        QMutexLocker locker(&m_mutex);
        return m_items;
    }

    void clearAll() {
        QMutexLocker locker(&m_mutex);
        m_items.clear();
        save();
    }

    void load(const QString &/* path */ = "") {
        QMutexLocker locker(&m_mutex);
        m_items.clear();

        auto db = SettingsDb::instance();
        if (db->contains("ui.message_history")) {
            for (auto v : db->getJsonArray("ui.message_history")) {
                auto obj = v.toObject();
                MessageHistoryItem item;
                item.id = obj["id"].toString();
                item.type = obj["type"].toString();
                item.title = obj["title"].toString();
                item.content = obj["content"].toString();
                item.appTarget = obj["appTarget"].toString();
                item.timestamp = obj["timestamp"].toVariant().toLongLong();
                m_items.append(item);
            }
            return;
        }

        // 尝试从旧文件做一次迁移
        QFile file("message_history.json");
        if (file.open(QIODevice::ReadOnly)) {
            auto doc = QJsonDocument::fromJson(file.readAll());
            if (doc.isArray()) {
                for (auto v : doc.array()) {
                    auto obj = v.toObject();
                    MessageHistoryItem item;
                    item.id = obj["id"].toString();
                    item.type = obj["type"].toString();
                    item.title = obj["title"].toString();
                    item.content = obj["content"].toString();
                    item.appTarget = obj["appTarget"].toString();
                    item.timestamp = obj["timestamp"].toVariant().toLongLong();
                    m_items.append(item);
                }
                save();
            }
            file.close();
        }
    }

    void save(const QString &/* path */ = "") {
        QJsonArray arr;
        for (const auto &item : m_items) {
            QJsonObject obj;
            obj["id"] = item.id;
            obj["type"] = item.type;
            obj["title"] = item.title;
            obj["content"] = item.content;
            obj["appTarget"] = item.appTarget;
            obj["timestamp"] = item.timestamp;
            arr.append(obj);
        }
        SettingsDb::instance()->setJsonArray("ui.message_history", arr);
    }

private:
    MessageHistoryManager() {
        load();
    }

    QMutex m_mutex;
    QList<MessageHistoryItem> m_items;
};
