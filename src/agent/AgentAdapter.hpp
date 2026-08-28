#pragma once

// 
// Shijima-Qt - Agent Adapter Interface
// 

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>

struct AgentTaskResult {
    bool success = false;
    QString taskId;
    QString reply;
    QString appName;      // 例如 "aipy-pro", "WorkBuddy", "CodeX"
    QString launchTarget; // 客户端或网页唤醒目标
    QString error;
};

class AgentAdapter {
public:
    virtual ~AgentAdapter() = default;

    virtual QString type() const = 0;        // "aipy", "workbuddy", "codex", "direct_llm"
    virtual QString displayName() const = 0; // "aipy-pro (本地 Agent)", "WorkBuddy", "CodeX"

    // 异步执行智能体任务，支持中间进度与最终结果回调
    virtual void executeTask(QString const& instruction,
                             QString const& contextText,
                             std::function<void(QString const& progressMsg)> progressCallback,
                             std::function<void(AgentTaskResult const& result)> finishCallback) = 0;

    // 测试 Agent 连通性
    virtual void testConnection(std::function<void(bool success, QString const& message)> callback) = 0;

    // 打开/联动对应客户端任务页面
    virtual void openTask(QString const& taskId) = 0;
};
