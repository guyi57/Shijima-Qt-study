#pragma once

// 
// Shijima-Qt - AI Agent & Memory Service with Adapter Architecture
// 

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>
#include <map>
#include <functional>
#include "AgentAdapter.hpp"

class QNetworkAccessManager;
class AipyAdapter;

struct AIBehaviorIntent {
    QString intent = "chat";     // "chat", "seek_attention", "celebrate", "comfort", "explore", "rest", "play"
    QString emotion = "happy";   // "happy", "bored", "angry", "sleepy", "curious"
    QString target = "cursor";   // "cursor", "window", "screen_edge"
    QString speech;              // 简练短句（3~15字）
    int urgency = 1;             // 1~5
};

struct AgentConfig {
    // 基础直连 LLM 配置
    QString apiBase = "https://api.openai.com/v1";
    QString apiKey = "";
    QString model = "gpt-4o-mini";
    int maxMemoryTurns = 6;
    QString hotkeyTranslate = "Option+T";
    QString hotkeyAsk = "Option+Q";

    // 音乐播放器全局快捷键配置
    QString hotkeyMusicToggle = "Option+M";
    QString hotkeyMusicPlayPause = "Option+Space";
    QString hotkeyMusicNext = "Option+Right";
    QString hotkeyMusicPrev = "Option+Left";
    QString hotkeyMusicFav = "Option+L";

    // 智能体 Agent 适配器配置
    QString activeAgentType = "aipy";       // "aipy", "direct_llm", "workbuddy", "codex"
    QString aipyBase = "http://127.0.0.1:41970";
    QString aipyKey = "";
    QString routingMode = "AUTO";            // "AUTO" (智能分流), "ALWAYS_AGENT", "ALWAYS_LLM"
};

class AgentService
{
public:
    static AgentService *instance();

    void loadConfig(QString const& path = "config.json");
    void saveConfig(QString const& path = "config.json");
    AgentConfig const& config() const { return m_config; }
    void setConfig(AgentConfig const& cfg);

    // 智能翻译：中文 -> 英文，非中文 -> 中文
    void translate(QString const& text, std::function<void(bool success, QString const& result)> callback);

    // 智能提问：支持分流路由（简单问题直答，复杂任务交给 Agent 适配器执行）
    void ask(QString const& contextText,
             QString const& question,
             std::function<void(QString const& progressMsg)> progressCallback,
             std::function<void(bool success, QString const& result, QString const& appTarget)> finishCallback);

    // 测试模型接口连通性
    void testConnection(QString const& apiBase, QString const& apiKey, QString const& model, std::function<void(bool success, QString const& message)> callback);

    // 测试指定 Agent 连通性
    void testAgentConnection(QString const& agentType, std::function<void(bool success, QString const& message)> callback);

    // 打开指定任务
    void openTask(QString const& taskId);

    // 获取 aipy 适配器对象
    AipyAdapter *aipyAdapter() const;

    // AI 桌面宠物行为意图生成（人格化思考与主动交互）
    void requestPetIntent(const QJsonObject &contextInfo, std::function<void(bool success, const AIBehaviorIntent &intent)> callback);

    void clearMemory();
    QJsonArray const& memoryHistory() const { return m_history; }

private:
    AgentService();
    void initAdapters();
    void syncAdapterConfigs();
    void sendChatCompletion(QJsonArray const& messages, std::function<void(bool success, QString const& result)> callback);
    bool containsChinese(QString const& text);
    void appendMemory(QString const& role, QString const& content);
    void saveMemoryToFile();
    void loadMemoryFromFile();

    AgentConfig m_config;
    QNetworkAccessManager *m_networkManager;
    QJsonArray m_history;
    std::map<QString, std::shared_ptr<AgentAdapter>> m_adapters;
};
