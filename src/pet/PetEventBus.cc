#include "PetEventBus.hpp"
#include <QDateTime>
#include <algorithm>

PetEventBus *PetEventBus::instance()
{
    static PetEventBus s_instance;
    return &s_instance;
}

PetEventBus::PetEventBus()
{
}

int PetEventBus::subscribe(const QString &eventType, EventHandler handler)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    int id = m_nextId++;
    m_listeners.push_back({id, eventType, handler});
    return id;
}

void PetEventBus::unsubscribe(int subscriptionId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_listeners.erase(
        std::remove_if(m_listeners.begin(), m_listeners.end(),
                       [subscriptionId](const Listener &l) { return l.id == subscriptionId; }),
        m_listeners.end());
}

void PetEventBus::emitEvent(const QString &eventType, const QJsonObject &payload)
{
    PetEvent evt;
    evt.type = eventType;
    evt.payload = payload;
    evt.timestamp = QDateTime::currentMSecsSinceEpoch();
    emitEvent(evt);
}

void PetEventBus::emitEvent(const PetEvent &event)
{
    std::vector<EventHandler> handlersToCall;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto &l : m_listeners) {
            if (l.type == "*" || l.type == event.type) {
                handlersToCall.push_back(l.handler);
            }
        }
    }

    for (const auto &handler : handlersToCall) {
        if (handler) {
            handler(event);
        }
    }
}
