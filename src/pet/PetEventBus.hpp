#pragma once

#include <QString>
#include <QJsonObject>
#include <functional>
#include <vector>
#include <map>
#include <mutex>

// 标准事件定义
struct PetEvent {
    QString type;       // 如 "agent.task.completed", "user.click_pet", "user.drag_pet", "user.idle"
    QJsonObject payload;
    qint64 timestamp = 0;
};

class PetEventBus
{
public:
    using EventHandler = std::function<void(const PetEvent &)>;

    static PetEventBus *instance();

    // 订阅特定事件类型或所有事件（type 为 "*" 时监听全部）
    int subscribe(const QString &eventType, EventHandler handler);
    void unsubscribe(int subscriptionId);

    // 发送事件
    void emitEvent(const QString &eventType, const QJsonObject &payload = QJsonObject());
    void emitEvent(const PetEvent &event);

private:
    PetEventBus();
    struct Listener {
        int id;
        QString type;
        EventHandler handler;
    };

    std::mutex m_mutex;
    int m_nextId = 1;
    std::vector<Listener> m_listeners;
};
