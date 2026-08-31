#pragma once

#include <QString>
#include <QStringList>
#include <QProcess>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <functional>

enum class McpState {
    Disconnected,
    Connecting,
    Connected,
    Error
};

struct McpToolInfo {
    QString name;
    QString description;
    QJsonObject inputSchema;
    QJsonObject openAiFunctionDefinition; // 转换后的 OpenAI Tool Definition
};

class McpClient {
public:
    explicit McpClient(const QString &serverName,
                      const QString &command,
                      const QStringList &args,
                      const QMap<QString, QString> &env = {});
    ~McpClient();

    QString serverName() const { return m_serverName; }
    QString command() const { return m_command; }
    QStringList args() const { return m_args; }
    McpState state() const { return m_state; }
    QString stateString() const;
    QString lastError() const { return m_lastError; }

    void start();
    void stop();

    QList<McpToolInfo> tools() const;
    bool hasTool(const QString &toolName) const;

    // 调用工具接口
    void callTool(const QString &toolName,
                  const QJsonObject &arguments,
                  std::function<void(bool success, const QString &result)> callback);

    std::function<void(McpState state)> onStateChanged;
    std::function<void()> onToolsChanged;

private:
    void onProcessReadyReadStandardOutput();
    void onProcessReadyReadStandardError();
    void onProcessErrorOccurred(QProcess::ProcessError error);
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

    void sendJsonRpcMessage(const QJsonObject &msg);
    void handleJsonRpcResponse(const QJsonObject &msg);
    void sendInitializeRequest();
    void sendToolsListRequest();
    QJsonObject convertToOpenAiTool(const QJsonObject &mcpTool);

    QString m_serverName;
    QString m_command;
    QStringList m_args;
    QMap<QString, QString> m_env;
    QProcess *m_process = nullptr;
    McpState m_state = McpState::Disconnected;
    QString m_lastError;

    int m_nextRequestId = 1;
    QMap<int, std::function<void(const QJsonObject &response)>> m_pendingCallbacks;
    QMap<QString, McpToolInfo> m_tools;
    QByteArray m_readBuffer;
};
