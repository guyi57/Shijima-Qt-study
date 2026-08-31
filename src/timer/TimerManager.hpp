#pragma once

// 
// Shijima-Qt - Persistent Timer & Task Scheduler Manager
// 

#include <QString>
#include <QList>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QTimer>
#include <functional>
#include <mutex>
#include <memory>

enum class TimerType {
    Notification, // 纯气泡/弹窗提醒
    AiTask        // 到期自动调度 Agent/LLM 执行任务并汇报
};

enum class TimerRepeat {
    Once,           // 单次触发
    Interval,       // 全天固定间隔循环 (如每隔 60 分钟)
    Daily,          // 每天固定时间 (如每天 09:00)
    WindowInterval  // 指定时间段内固定间隔循环 (如工作日 09:00 - 18:00 每隔 1 小时)
};

struct ScheduledTimer {
    QString id;                 // 唯一ID
    QString title;              // 提醒标题或任务名称
    TimerType type = TimerType::Notification;
    TimerRepeat repeat = TimerRepeat::Once;
    int intervalSeconds = 0;    // 若为 Interval/WindowInterval，间隔秒数
    QString dailyTime;          // 若为 Daily，"HH:mm"
    QString startTime;          // 若为 WindowInterval，时间段开始 "09:00"
    QString endTime;            // 若为 WindowInterval，时间段结束 "18:00"
    bool weekdaysOnly = false;  // 仅工作日 (周一至周五)
    QList<int> daysOfWeek;      // 允许的星期列表 (1~7)
    qint64 targetTimestamp = 0; // 下一次触发的时间戳 (毫秒)
    bool enabled = true;        // 是否启用
    QString taskPrompt;         // 若为 AiTask，具体执行的 Prompt
    qint64 createdAt = 0;       // 创建时间 (毫秒)

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["title"] = title;
        obj["type"] = (type == TimerType::Notification ? "notification" : "task");
        QString repStr = "once";
        if (repeat == TimerRepeat::Interval) repStr = "interval";
        else if (repeat == TimerRepeat::Daily) repStr = "daily";
        else if (repeat == TimerRepeat::WindowInterval) repStr = "window_interval";
        obj["repeat"] = repStr;
        obj["intervalSeconds"] = intervalSeconds;
        obj["dailyTime"] = dailyTime;
        obj["startTime"] = startTime;
        obj["endTime"] = endTime;
        obj["weekdaysOnly"] = weekdaysOnly;
        
        QJsonArray dowArr;
        for (int d : daysOfWeek) dowArr.append(d);
        obj["daysOfWeek"] = dowArr;

        obj["targetTimestamp"] = targetTimestamp;
        obj["enabled"] = enabled;
        obj["taskPrompt"] = taskPrompt;
        obj["createdAt"] = createdAt;
        return obj;
    }

    static ScheduledTimer fromJson(const QJsonObject &obj) {
        ScheduledTimer t;
        t.id = obj["id"].toString();
        t.title = obj["title"].toString();
        QString typeStr = obj["type"].toString();
        t.type = (typeStr == "task" ? TimerType::AiTask : TimerType::Notification);
        QString repeatStr = obj["repeat"].toString();
        if (repeatStr == "interval") t.repeat = TimerRepeat::Interval;
        else if (repeatStr == "daily") t.repeat = TimerRepeat::Daily;
        else if (repeatStr == "window_interval") t.repeat = TimerRepeat::WindowInterval;
        else t.repeat = TimerRepeat::Once;
        t.intervalSeconds = obj["intervalSeconds"].toInt();
        t.dailyTime = obj["dailyTime"].toString();
        t.startTime = obj["startTime"].toString();
        t.endTime = obj["endTime"].toString();
        t.weekdaysOnly = obj["weekdaysOnly"].toBool(false);
        
        if (obj.contains("daysOfWeek")) {
            QJsonArray dowArr = obj["daysOfWeek"].toArray();
            for (auto v : dowArr) t.daysOfWeek.append(v.toInt());
        }
        if (t.daysOfWeek.isEmpty() && t.weekdaysOnly) {
            t.daysOfWeek = {1, 2, 3, 4, 5};
        }

        t.targetTimestamp = obj["targetTimestamp"].toVariant().toLongLong();
        t.enabled = obj["enabled"].toBool(true);
        t.taskPrompt = obj["taskPrompt"].toString();
        t.createdAt = obj["createdAt"].toVariant().toLongLong();
        return t;
    }
};

class TimerManager
{
public:
    static TimerManager *instance();

    void init();
    void loadTimers();
    void saveTimers();

    // 增删改查
    QString addTimer(ScheduledTimer timer);
    bool updateTimer(const ScheduledTimer &timer);
    bool deleteTimer(const QString &timerId);
    bool setTimerEnabled(const QString &timerId, bool enabled);
    QList<ScheduledTimer> getAllTimers();
    ScheduledTimer getTimer(const QString &timerId, bool *found = nullptr);

    // 快捷创建接口 (方便大模型 Tool 调用)
    ScheduledTimer createQuickTimer(const QString &title,
                                   int triggerInSeconds,
                                   TimerType type = TimerType::Notification,
                                   const QString &taskPrompt = "",
                                   TimerRepeat repeat = TimerRepeat::Once,
                                   int repeatIntervalSeconds = 0,
                                   const QString &dailyTime = "",
                                   const QString &startTime = "",
                                   const QString &endTime = "",
                                   bool weekdaysOnly = false,
                                   const QList<int> &daysOfWeek = {});

    // 触发回调：当定时器到达时调用
    std::function<void(const ScheduledTimer &timer)> onTimerTriggered;
    // 列表变更回调 (UI 刷新)
    std::function<void()> onTimersChanged;

private:
    TimerManager();
    ~TimerManager();

    void calculateNextTrigger(ScheduledTimer &timer);
    void checkTimers();

    mutable std::recursive_mutex m_mutex;
    QList<ScheduledTimer> m_timers;
    QTimer *m_tickTimer = nullptr;
    QString m_storagePath;
};
