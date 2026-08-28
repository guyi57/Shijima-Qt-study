#pragma once

// 
// Shijima-Qt - WorkBuddy & CodeX Agent Adapters (Extensible Skeletons)
// 

#include "AgentAdapter.hpp"

class WorkBuddyAdapter : public AgentAdapter {
public:
    QString type() const override { return "workbuddy"; }
    QString displayName() const override { return "WorkBuddy (云端智能体)"; }

    void executeTask(QString const& instruction,
                     QString const&,
                     std::function<void(QString const&)> progressCallback,
                     std::function<void(AgentTaskResult const&)> finishCallback) override {
        if (progressCallback) progressCallback("WorkBuddy 智能体执行中...");
        AgentTaskResult res;
        res.success = true;
        res.appName = "WorkBuddy";
        res.reply = "【WorkBuddy 响应】: 收到任务 - " + instruction;
        finishCallback(res);
    }

    void testConnection(std::function<void(bool success, QString const& message)> callback) override {
        callback(true, "WorkBuddy 适配器就绪");
    }

    void openTask(QString const&) override {}
};

class CodexAdapter : public AgentAdapter {
public:
    QString type() const override { return "codex"; }
    QString displayName() const override { return "CodeX (代码智能体)"; }

    void executeTask(QString const& instruction,
                     QString const&,
                     std::function<void(QString const&)> progressCallback,
                     std::function<void(AgentTaskResult const&)> finishCallback) override {
        if (progressCallback) progressCallback("CodeX 代码生成中...");
        AgentTaskResult res;
        res.success = true;
        res.appName = "CodeX";
        res.reply = "【CodeX 响应】: 已处理代码任务 - " + instruction;
        finishCallback(res);
    }

    void testConnection(std::function<void(bool success, QString const& message)> callback) override {
        callback(true, "CodeX 适配器就绪");
    }

    void openTask(QString const&) override {}
};
