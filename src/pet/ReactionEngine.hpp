#pragma once

#include <QString>
#include <QJsonObject>
#include <QVector>
#include <deque>
#include <functional>
#include "PetAction.hpp"
#include "PetEventBus.hpp"

struct ReactionRule {
    QString eventType;
    std::vector<PetActionType> actions;
    std::vector<QString> speechOptions;
    double speechProbability = 1.0;
    int moodDelta = 0;
    int energyDelta = 0;
    int boredomDelta = 0;
    int affectionDelta = 0;
};

struct AppSwitchRecord {
    QString appName;
    QString bundleId;
    qint64 timestamp = 0;
};

class ReactionEngine
{
public:
    static ReactionEngine *instance();

    void initDefaultRules();
    bool evaluateReaction(const PetEvent &event, PetActionCommand &outCommand, int &moodDelta, int &boredomDelta, int &affectionDelta);

    // 检查长时间驻留类彩蛋（由 BehaviorEngine onTick 周期性驱动）
    bool checkContinuousDwell(PetActionCommand &outCommand, int &moodDelta);

private:
    ReactionEngine();

    bool handleAppActivated(const PetEvent &event, PetActionCommand &outCommand, int &moodDelta, int &boredomDelta, int &affectionDelta);

    // 应用分类识别辅助函数
    bool isCodeEditor(const QString &appName, const QString &bundleId) const;
    bool isGitTool(const QString &appName, const QString &bundleId) const;
    bool isTerminal(const QString &appName, const QString &bundleId) const;
    bool isChatApp(const QString &appName, const QString &bundleId) const;
    bool isBrowser(const QString &appName, const QString &bundleId) const;

    std::vector<ReactionRule> m_rules;

    // 前台应用时序记录 (保持最近 20 条，最长 5 分钟)
    std::deque<AppSwitchRecord> m_appHistory;
    QString m_currentFrontmostApp;
    QString m_currentFrontmostBundle;
    qint64 m_currentAppStartTime = 0;

    // 冷却时间记录 (ms)
    qint64 m_lastDevChainTime = 0;
    qint64 m_lastChatSpamTime = 0;
    qint64 m_lastSourcetreeEggTime = 0;
    qint64 m_lastTerminalEggTime = 0;
    qint64 m_lastBrowserEggTime = 0;
};


