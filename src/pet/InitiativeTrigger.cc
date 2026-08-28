#include "InitiativeTrigger.hpp"
#include <QDateTime>

InitiativeTrigger *InitiativeTrigger::instance()
{
    static InitiativeTrigger s_instance;
    return &s_instance;
}

InitiativeTrigger::InitiativeTrigger()
{
}

bool InitiativeTrigger::evaluateInitiative(PetState &state, const QJsonObject &contextInfo, int &calculatedScore, QString &triggerReason)
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // 检查每日重置
    QDateTime nowDate = QDateTime::fromMSecsSinceEpoch(now);
    QDateTime lastDate = QDateTime::fromMSecsSinceEpoch(state.lastDayResetTime);
    if (nowDate.date() != lastDate.date()) {
        state.initiativeCountToday = 0;
        state.lastDayResetTime = now;
    }

    // 每日上限保护
    if (state.initiativeCountToday >= m_dailyLimit) {
        calculatedScore = 0;
        return false;
    }

    // 冷却时间检查（以秒为单位）
    qint64 timeSinceLastTalkSec = (now - state.lastTalkTime) / 1000;
    if (timeSinceLastTalkSec < m_cooldownSeconds) {
        calculatedScore = -100;
        return false;
    }

    int score = 0;
    QStringList reasons;

    // 1. 无聊度 / 社交渴望度贡献
    if (state.boredom > 40) {
        score += 20;
        reasons << "pet_bored";
    }
    if (state.social > 40) {
        score += 15;
        reasons << "pet_lonely";
    }

    // 2. 用户空闲/停顿思考时长贡献 (2分钟以上无交互即可有 20 分)
    int userIdleSeconds = contextInfo["user_idle_seconds"].toInt(0);
    if (userIdleSeconds > 120) {
        score += 20;
        reasons << "user_idle";
    }

    // 3. 最近有任务完成事件
    qint64 taskTimeDelta = (now - state.lastTaskCompletedTime) / 1000;
    if (taskTimeDelta > 0 && taskTimeDelta < 300) { // 5分钟内有任务完成
        score += 30;
        reasons << "task_completed_recently";
    }

    // 4. 深夜/特殊时段关怀
    int hour = nowDate.time().hour();
    if (hour >= 22 || hour <= 5) {
        score += 20;
        reasons << "late_night_care";
    } else if (hour == 12 || hour == 18) {
        score += 15;
        reasons << "meal_time";
    }

    // 5. 基础活跃意愿（给予保底分，确保达到冷却后可触发）
    score += 15;

    calculatedScore = score;
    triggerReason = reasons.join(", ");

    return (score >= m_triggerThreshold);
}

void InitiativeTrigger::recordTalkSuccess(PetState &state)
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    state.lastTalkTime = now;
    state.initiativeCountToday++;
    state.boredom = std::max(0, state.boredom - 30);
    state.social = std::max(0, state.social - 20);
    state.affection = std::clamp(state.affection + 1, 0, 100);
}
