#pragma once

#include <QString>

class SystemObserver
{
public:
    static SystemObserver* instance();
    void start();
    void stop();

    // 查询当前前台活跃应用名称
    QString currentActiveAppName() const;

private:
    SystemObserver();
    ~SystemObserver();

    bool m_started = false;
    void *m_observerContext = nullptr; // 用于存储 Objective-C 观察者句柄
};
