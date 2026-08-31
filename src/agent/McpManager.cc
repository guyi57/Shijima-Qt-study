#include "McpManager.hpp"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <iostream>

McpManager* McpManager::instance() {
    static McpManager s_instance;
    return &s_instance;
}

McpManager::McpManager() {
    m_configPath = QDir::homePath() + "/.config/guyi-bot/mcp_servers.json";
}

McpManager::~McpManager() {
    stopAll();
}

QString McpManager::configFilePath() const {
    return m_configPath;
}

void McpManager::init() {
    ensureDefaultConfig();
    loadConfig();
}

void McpManager::ensureDefaultConfig() {
    QFileInfo fi(m_configPath);
    if (!fi.dir().exists()) {
        fi.dir().mkpath(".");
    }

    if (!QFile::exists(m_configPath)) {
        QJsonObject root;
        QJsonObject servers;

        // 示例配置模版（带注释字段）
        QJsonObject demoFs;
        demoFs["command"] = "npx";
        demoFs["args"] = QJsonArray{"-y", "@modelcontextprotocol/server-filesystem", QDir::homePath()};
        demoFs["enabled"] = false; // 默认关闭，供用户参考开启
        servers["filesystem_demo"] = demoFs;

        root["mcpServers"] = servers;

        QFile file(m_configPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
            file.close();
        }
    }
}

void McpManager::loadConfig() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    stopAll();
    m_configs.clear();

    QFile file(m_configPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    auto doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) return;
    auto rootObj = doc.object();
    if (!rootObj.contains("mcpServers")) return;

    auto serversObj = rootObj["mcpServers"].toObject();
    for (auto it = serversObj.begin(); it != serversObj.end(); ++it) {
        QString sName = it.key();
        auto sObj = it.value().toObject();

        McpServerConfig conf;
        conf.name = sName;
        conf.command = sObj["command"].toString();
        conf.enabled = sObj.contains("enabled") ? sObj["enabled"].toBool(true) : true;

        if (sObj.contains("args")) {
            for (auto a : sObj["args"].toArray()) {
                conf.args.append(a.toString());
            }
        }
        if (sObj.contains("env")) {
            auto envObj = sObj["env"].toObject();
            for (auto eIt = envObj.begin(); eIt != envObj.end(); ++eIt) {
                conf.env[eIt.key()] = eIt.value().toString();
            }
        }

        m_configs[sName] = conf;

        if (conf.enabled && !conf.command.isEmpty()) {
            auto client = new McpClient(conf.name, conf.command, conf.args, conf.env);
            client->onToolsChanged = [this]() {
                if (onToolsChanged) onToolsChanged();
            };
            client->onStateChanged = [this](McpState) {
                if (onServersChanged) onServersChanged();
            };
            m_clients[sName] = client;
            client->start();
        }
    }

    std::cout << "[McpManager] 配置文件已加载，共配置 " << m_configs.size()
              << " 个服务，已启动 " << m_clients.size() << " 个" << std::endl;
    if (onServersChanged) onServersChanged();
}

void McpManager::reload() {
    loadConfig();
}

void McpManager::stopAll() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (auto client : m_clients) {
        client->stop();
        delete client;
    }
    m_clients.clear();
}

QList<McpClient*> McpManager::getClients() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_clients.values();
}

QJsonArray McpManager::getAllToolDefinitions() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    QJsonArray toolsArr;
    for (auto client : m_clients) {
        if (client->state() == McpState::Connected) {
            for (const auto &t : client->tools()) {
                toolsArr.append(t.openAiFunctionDefinition);
            }
        }
    }
    return toolsArr;
}

bool McpManager::hasTool(const QString &toolName) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (auto client : m_clients) {
        if (client->hasTool(toolName)) {
            return true;
        }
    }
    return false;
}

void McpManager::executeToolCall(const QString &toolName,
                                const QJsonObject &args,
                                std::function<void(bool success, const QString &result)> callback)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (auto client : m_clients) {
        if (client->hasTool(toolName)) {
            client->callTool(toolName, args, callback);
            return;
        }
    }
    if (callback) callback(false, "未找到对应的 MCP 工具: " + toolName);
}
