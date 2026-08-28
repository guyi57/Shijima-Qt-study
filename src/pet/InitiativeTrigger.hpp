#pragma once

#include <QString>
#include <QJsonObject>
#include "PetState.hpp"

class InitiativeTrigger
{
public:
    static InitiativeTrigger *instance();

    // 检查并评估当前主动交互得分
    // contextInfo 可包含当前活跃 App、空闲时长、最近事件等
    bool evaluateInitiative(PetState &state, const QJsonObject &contextInfo, int &calculatedScore, QString &triggerReason);

    void recordTalkSuccess(PetState &state);

    int triggerThreshold() const { return m_triggerThreshold; }
    void setTriggerThreshold(int val) { m_triggerThreshold = val; }

    int cooldownSeconds() const { return m_cooldownSeconds; }
    void setCooldownSeconds(int val) { m_cooldownSeconds = val; }

    int dailyLimit() const { return m_dailyLimit; }
    void setDailyLimit(int val) { m_dailyLimit = val; }

private:
    InitiativeTrigger();

    int m_triggerThreshold = 30; // 降低触发门槛，让 AI 能够自然自言自语/互动
    int m_cooldownSeconds = 150; // 2.5分钟冷却
    int m_dailyLimit = 30;       // 每日最多主动互动 30 次
};
