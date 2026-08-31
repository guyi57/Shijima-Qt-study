#pragma once

#include <QString>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>
#include <mutex>
#include <functional>
#include "McpClient.hpp"

struct McpServerConfig {
    QString name;
    QString command;
    QStringList args;
    QMap<QString, QString> env;
    bool enabled = true;
};

class McpManager {
public:
    static McpManager* instance();

    void init();
    void reload();
    void stopAll();

    QString configFilePath() const;

    // 获取所有已连接 MCP 服务注册的工具定义 (OpenAI Tool 格式)
    QJsonArray getAllToolDefinitions() const;

    // 检查是否存在指定工具
    bool hasTool(const QString &toolName) const;

    // 异步执行指定 MCP 工具
    void executeToolCall(const QString &toolName,
                         const QJsonObject &args,
                         std::function<void(bool success, const QString &result)> callback);

    QList<McpClient*> getClients() const;

    std::function<void()> onServersChanged;
    std::function<void()> onToolsChanged;

private:
    McpManager();
    ~McpManager();

    void ensureDefaultConfig();
    void loadConfig();

    mutable std::recursive_mutex m_mutex;
    QString m_configPath;
    QMap<QString, McpClient*> m_clients;
    QMap<QString, McpServerConfig> m_configs;
};
