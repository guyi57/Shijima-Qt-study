#include "BehaviorEngine.hpp"
#include "ReactionEngine.hpp"
#include "InitiativeTrigger.hpp"
#include "AgentService.hpp"
#include "ShijimaWidget.hpp"
#include "ShijimaManager.hpp"
#include "ScoreBadgeWidget.hpp"
#include <QRandomGenerator>
#include <QDateTime>
#include <QCursor>
#include <QCoreApplication>
#include <iostream>

BehaviorEngine *BehaviorEngine::instance()
{
    static BehaviorEngine s_instance;
    return &s_instance;
}

BehaviorEngine::BehaviorEngine()
    : m_tickTimer(new QTimer())
{
    QObject::connect(m_tickTimer, &QTimer::timeout, [this]() {
        onTick();
    });
    initEventListeners();
}

void BehaviorEngine::initEventListeners()
{
    PetEventBus::instance()->subscribe("*", [this](const PetEvent &event) {
        handleEvent(event);
    });
}

void BehaviorEngine::start()
{
    if (!m_tickTimer->isActive()) {
        m_tickTimer->start(1000); // 1秒一次决策心跳
    }
}

void BehaviorEngine::stop()
{
    m_tickTimer->stop();
}

void BehaviorEngine::setActiveWidget(ShijimaWidget *widget)
{
    m_activeWidget = widget;
}

MoodTier BehaviorEngine::moodTier() const
{
    if (m_state.mood >= 30) return MoodTier::High;
    if (m_state.mood >= -20) return MoodTier::Medium;
    if (m_state.mood >= -60) return MoodTier::Low;
    return MoodTier::ExtremelyLow;
}

void BehaviorEngine::handleEvent(const PetEvent &event)
{
    // 更新状态记录
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (event.type.startsWith("user.")) {
        m_state.lastInteractionTime = now;
    }
    if (event.type == "agent.task.completed") {
        m_state.lastTaskCompletedTime = now;
    }

    // 1. 评估即时 Reaction 规则 (前台应用时序、系统内存、音乐播放等)
    PetActionCommand cmd;
    int moodDelta = 0, boredomDelta = 0, affectionDelta = 0;
    bool hasReaction = ReactionEngine::instance()->evaluateReaction(event, cmd, moodDelta, boredomDelta, affectionDelta);

    m_state.mood += moodDelta;
    m_state.boredom += boredomDelta;
    m_state.affection += affectionDelta;
    m_state.clamp();

    if (hasReaction) {
        executeAction(cmd);
    }
}

void BehaviorEngine::executeAction(const PetActionCommand &cmd)
{
    m_currentAction = cmd.type;
    m_actionEndTime = QDateTime::currentMSecsSinceEpoch() + (cmd.durationMs > 0 ? cmd.durationMs : 4000);

    auto execInMainThread = [this, cmd]() {
        ShijimaWidget *target = m_activeWidget;
        if (target == nullptr) {
            auto &list = ShijimaManager::defaultManager()->mascots();
            if (!list.empty()) {
                target = list.front();
            }
        }

        if (target != nullptr) {
            target->doAction(cmd);
        }
    };

    if (QCoreApplication::instance() != nullptr) {
        QMetaObject::invokeMethod(QCoreApplication::instance(), execInMainThread, Qt::QueuedConnection);
    } else {
        execInMainThread();
    }
}

void BehaviorEngine::recordUserInteraction()
{
    m_state.lastInteractionTime = QDateTime::currentMSecsSinceEpoch();
    m_state.mood = std::clamp(m_state.mood + 1, -100, 100);
}

void BehaviorEngine::addAffection(int delta, int moodDelta)
{
    m_state.affection = std::clamp(m_state.affection + delta, 0, 100);
    if (moodDelta != 0) {
        m_state.mood = std::clamp(m_state.mood + moodDelta, -100, 100);
    }
    m_state.boredom = std::max(0, m_state.boredom - (delta * 5));
    m_state.lastInteractionTime = QDateTime::currentMSecsSinceEpoch();
    m_state.clamp();
    std::cout << "[互动亲密度] 亲密度 " << (delta >= 0 ? "+" : "") << delta 
              << ", 当前亲密度: " << m_state.affection << "%, 心情: " << m_state.mood << std::endl;
}

bool BehaviorEngine::handlePetClickedWhileResting(ShijimaWidget *target)
{
    if (!m_state.isRestingInCorner || target == nullptr) return false;

    static const QStringList sleepyQuotes = {
        "呜…别戳了，再让我睡一小会儿… 💤",
        "好困好困…电量还没充好呢 🔋",
        "呼噜噜…等我睡醒再陪你玩～ (´-ω-`)"
    };
    int idx = QRandomGenerator::global()->bounded(static_cast<int>(sleepyQuotes.size()));
    target->showMessage(sleepyQuotes[idx], 3500);

    // 稍微晃头或坐着发呆
    auto spin = target->mascot().initial_behavior_list().find("SitAndSpinHead", false);
    if (spin != nullptr) {
        target->mascot().next_behavior("SitAndSpinHead");
    }

    addAffection(1, 1);
    return true;
}

bool BehaviorEngine::handlePetClickedInPoutMode(ShijimaWidget *target)
{
    if (moodTier() != MoodTier::ExtremelyLow || target == nullptr) return false;

    m_poutClickCount++;
    addAffection(2, 6); // 每次抚摸显著增加亲密度与心情，助力破冰

    if (m_state.mood > -60) {
        // 破冰成功！
        m_poutClickCount = 0;
        target->showMessage("好啦好啦…看在你这么诚恳的份上原谅你啦！✨", 4000);
        auto happyBeh = target->mascot().initial_behavior_list().find("SitWhileDanglingLegs", false);
        if (happyBeh != nullptr) target->mascot().next_behavior("SitWhileDanglingLegs");
        return true;
    }

    // 50% 概率背过身或稍微躲避
    if (QRandomGenerator::global()->generateDouble() < 0.5) {
        if (target->mascot().state) {
            target->mascot().state->looking_right = !target->mascot().state->looking_right;
        }
    }

    static const QStringList poutQuotes = {
        "哼，现在才想起我！(>д<)",
        "闹别扭中，请勿打扰… 😤",
        "才不理你呢，继续敲你的代码去！",
        "不理我这么久，现在摸摸也没用… 哼！"
    };
    int idx = QRandomGenerator::global()->bounded(static_cast<int>(poutQuotes.size()));
    target->showMessage(poutQuotes[idx], 3500);

    auto sitBeh = target->mascot().initial_behavior_list().find("SitDown", false);
    if (sitBeh != nullptr) target->mascot().next_behavior("SitDown");

    return true;
}

void BehaviorEngine::updateFallRecoverySequence(qint64 now)
{
    if (m_fallRecoveryPhase == FallRecoveryPhase::None || m_activeWidget == nullptr) return;

    auto env = m_activeWidget->mascot().state ? m_activeWidget->mascot().state->env : nullptr;
    if (!env) return;

    const auto &anchor = m_activeWidget->mascot().state->anchor;
    bool isOnFloor = (anchor.y >= (env->floor.y - 25.0));

    switch (m_fallRecoveryPhase) {
        case FallRecoveryPhase::Falling:
            if (isOnFloor) {
                m_fallRecoveryPhase = FallRecoveryPhase::LieDownBreathing;
                m_fallPhaseStartTime = now;
                auto lie = m_activeWidget->mascot().initial_behavior_list().find("LieDown", false);
                if (lie != nullptr) m_activeWidget->mascot().next_behavior("LieDown");
            }
            break;
        case FallRecoveryPhase::LieDownBreathing:
            if ((now - m_fallPhaseStartTime) >= 2200) { // 趴在地上喘气 2.2 秒
                m_fallRecoveryPhase = FallRecoveryPhase::StandingUp;
                m_fallPhaseStartTime = now;
                auto stand = m_activeWidget->mascot().initial_behavior_list().find("StandUp", false);
                if (stand != nullptr) m_activeWidget->mascot().next_behavior("StandUp");
            }
            break;
        case FallRecoveryPhase::StandingUp:
            if ((now - m_fallPhaseStartTime) >= 1200) { // 爬起来拍拍灰 1.2 秒
                m_fallRecoveryPhase = FallRecoveryPhase::SittingResting;
                m_fallPhaseStartTime = now;
                auto sit = m_activeWidget->mascot().initial_behavior_list().find("SitDown", false);
                if (sit != nullptr) m_activeWidget->mascot().next_behavior("SitDown");
            }
            break;
        case FallRecoveryPhase::SittingResting:
            // 平稳进入休整
            break;
        default:
            break;
    }
}

void BehaviorEngine::onTick()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    ShijimaWidget *target = m_activeWidget;
    if (target == nullptr) {
        auto &list = ShijimaManager::defaultManager()->mascots();
        if (!list.empty()) {
            target = list.front();
            m_activeWidget = target;
        }
    }

    // 1. 体力衰减与恢复动态计算
    if (target != nullptr) {
        QString curBehavior = target->currentBehaviorName();
        updateStamina(curBehavior);
        updateFallRecoverySequence(now);
        target->update(); // 触发头顶微盘实时刷新
    }

    // 2. 寂寞感衰减：超过 1 分钟无互动，每分钟心情 -1% (内部 mood -2)
    static qint64 s_lastMoodDecayTime = 0;
    if (s_lastMoodDecayTime == 0) s_lastMoodDecayTime = now;
    if ((now - m_state.lastInteractionTime) >= 60000) {
        if ((now - s_lastMoodDecayTime) >= 60000) {
            s_lastMoodDecayTime = now;
            m_state.mood = std::clamp(m_state.mood - 2, -100, 100);
            std::cout << "[情绪衰减] 超过1分钟无互动，心情 -1% (当前: " << m_state.mood << ")" << std::endl;
        }
    } else {
        s_lastMoodDecayTime = now;
    }

    // 3. 检查长时间驻留类彩蛋 (如 Chrome > 15分钟)
    PetActionCommand dwellCmd;
    int dwellMoodDelta = 0;
    if (ReactionEngine::instance()->checkContinuousDwell(dwellCmd, dwellMoodDelta)) {
        m_state.mood += dwellMoodDelta;
        m_state.clamp();
        executeAction(dwellCmd);
    }

    // 4. 检查是否正在执行专属外部长动作
    if (now < m_actionEndTime) {
        return;
    }

    // 弹窗展示中 或 等待 Agent 执行时：绝不主动打断原地姿态，保证用户稳定复制阅读
    if (target != nullptr) {
        if (target->isWaitingForAgent()) return;
        if (target->messageBubble() != nullptr && target->messageBubble()->hasMessage() && !target->messageBubble()->isCompactCuteMode()) {
            return;
        }
    }

    // =========================================================================
    // 5. 基于心情 4 阶区间与体力的阶段性行为决策
    // =========================================================================
    if (target != nullptr && !m_state.isRestingInCorner) {
        QString curBehavior = target->currentBehaviorName();
        MoodTier tier = moodTier();

        // A. 渐进式疲惫保护：当体力 < 30% 时，限制剧烈攀爬与跳跃，强制进入低能耗动作池
        if (m_state.stamina < 30) {
            if (curBehavior.contains("Climb", Qt::CaseInsensitive) ||
                curBehavior.contains("Ceiling", Qt::CaseInsensitive) ||
                curBehavior.contains("Jump", Qt::CaseInsensitive) ||
                curBehavior.contains("Run", Qt::CaseInsensitive)) {
                auto env = target->env();
                if (env && target->mascot().state && target->mascot().state->anchor.y >= (env->floor.y - 25.0)) {
                    static const std::vector<std::string> lowEnergyBehaviors = {
                        "WalkAlongWorkAreaFloor",
                        "SitDown",
                        "LieDown",
                        "SitAndFaceMouse"
                    };
                    int idx = QRandomGenerator::global()->bounded(static_cast<int>(lowEnergyBehaviors.size()));
                    target->mascot().next_behavior(lowEnergyBehaviors[idx]);
                }
            }
        }
        // B. 体力充沛 (stamina >= 30%) 时的行为分级
        else {
            static qint64 s_lastBehaviorDecisionTime = 0;
            if ((now - s_lastBehaviorDecisionTime) >= 3000) {
                s_lastBehaviorDecisionTime = now;

                bool isIdleOrGround = curBehavior.contains("Sit", Qt::CaseInsensitive) || 
                                      curBehavior.contains("Lie", Qt::CaseInsensitive) ||
                                      curBehavior.contains("Stand", Qt::CaseInsensitive) ||
                                      curBehavior.contains("WalkAlongWorkAreaFloor", Qt::CaseInsensitive);

                if (isIdleOrGround) {
                    auto env = target->env();

                    // 高心情 (+30 ~ +100): 高探索欲，主动追随光标、在活跃窗口顶部探头
                    if (tier == MoodTier::High) {
                        if (env && env->active_ie.visible()) {
                            int r = rand() % 3;
                            if (r == 0) target->mascot().next_behavior("WalkAlongIECeiling");
                            else if (r == 1) target->mascot().next_behavior("JumpFromBottomOfIE");
                            else target->mascot().next_behavior("ChaseMouse");
                        } else {
                            int r = rand() % 4;
                            if (r == 0) target->mascot().next_behavior("ChaseMouse");
                            else if (r == 1) target->mascot().next_behavior("RunAlongWorkAreaFloor");
                            else if (r == 2) target->mascot().next_behavior("WalkAndGrabBottomLeftWall");
                            else target->mascot().next_behavior("SitWhileDanglingLegs");
                        }
                    }
                    // 中心情 (-20 ~ +30): 正常巡逻、窗口探秘、看光标
                    else if (tier == MoodTier::Medium) {
                        if (env && env->active_ie.visible()) {
                            int r = rand() % 4;
                            if (r == 0) target->mascot().next_behavior("WalkAlongIECeiling");
                            else if (r == 1) target->mascot().next_behavior("JumpFromBottomOfIE");
                            else if (r == 2) target->mascot().next_behavior("SitAndFaceMouse");
                            else target->mascot().next_behavior("WalkAlongWorkAreaFloor");
                        } else {
                            int r = rand() % 3;
                            if (r == 0) target->mascot().next_behavior("WalkAlongWorkAreaFloor");
                            else if (r == 1) target->mascot().next_behavior("SitAndFaceMouse");
                            else target->mascot().next_behavior("WalkAndGrabBottomRightWall");
                        }
                    }
                    // 低心情 (-60 ~ -20): 拒绝剧烈攀爬，缩在屏幕角落漫步
                    else if (tier == MoodTier::Low) {
                        int r = rand() % 2;
                        if (r == 0) target->mascot().next_behavior("WalkAlongWorkAreaFloor");
                        else target->mascot().next_behavior("SitDown");
                    }
                    // 极低心情 (-100 ~ -60): 闹别扭状态，背对屏幕或趴着发呆
                    else if (tier == MoodTier::ExtremelyLow) {
                        target->mascot().next_behavior("SitDown");
                    }
                }
            }
        }
    }

    // 6. 适度主动闲聊（高颜值紧凑气泡，零失焦）
    if (!m_state.isRestingInCorner && moodTier() != MoodTier::ExtremelyLow) {
        checkInitiativeChat();
    }
}

void BehaviorEngine::updateStamina(const QString &curBehavior)
{
    static QString s_lastBehavior = "";
    static int s_actionStepCount = 0;
    static int s_movingTickCounter = 0;

    bool isMovingActive = (
        curBehavior.contains("Climb", Qt::CaseInsensitive) ||
        curBehavior.contains("Ceiling", Qt::CaseInsensitive) ||
        curBehavior.contains("Wall", Qt::CaseInsensitive) ||
        curBehavior.contains("Jump", Qt::CaseInsensitive) ||
        curBehavior.contains("Run", Qt::CaseInsensitive) ||
        curBehavior.contains("Walk", Qt::CaseInsensitive) ||
        curBehavior.contains("Throw", Qt::CaseInsensitive) ||
        curBehavior.contains("Crawl", Qt::CaseInsensitive)
    );

    // 1. 动作切换结算：每完成 1 个探索大动作，适度扣减 2%
    if (!curBehavior.isEmpty() && curBehavior != s_lastBehavior) {
        if (!s_lastBehavior.isEmpty() && isMovingActive && !m_state.isRestingInCorner) {
            s_actionStepCount++;
            m_state.stamina = std::clamp(m_state.stamina - 2, 0, 100);
            std::cout << "[体力消耗] 完成第 " << s_actionStepCount << " 个动作 (" 
                      << s_lastBehavior.toStdString() << "), 体力 -2%, 剩余: " 
                      << m_state.stamina << "%" << std::endl;
        }
        s_lastBehavior = curBehavior;
    }

    // 2. 持续运动温和消耗：每 2 秒消耗 1% 体力
    if (isMovingActive) {
        s_movingTickCounter++;
        if (s_movingTickCounter >= 2) {
            s_movingTickCounter = 0;
            m_state.stamina = std::clamp(m_state.stamina - 1, 0, 100);
        }
    } else {
        s_movingTickCounter = 0;
    }

    // 3. 处于休整/静止状态时缓慢恢复体力 (+1%/秒，约 85 秒平缓恢复满血)
    if (m_state.isRestingInCorner && (
        curBehavior.contains("Sit", Qt::CaseInsensitive) ||
        curBehavior.contains("Lie", Qt::CaseInsensitive) ||
        curBehavior.contains("Sleep", Qt::CaseInsensitive) ||
        curBehavior.contains("Sprawl", Qt::CaseInsensitive) ||
        curBehavior.contains("Dangle", Qt::CaseInsensitive) ||
        curBehavior.contains("Spin", Qt::CaseInsensitive) ||
        curBehavior.isEmpty()
    )) {
        m_state.stamina = std::clamp(m_state.stamina + 1, 0, 100);
    }

    // 4. 体力耗尽 (stamina <= 10) 触发休整（平滑跌落与拍灰过渡）
    if (m_state.stamina <= 10 && !m_state.isRestingInCorner) {
        triggerRestInCorner();
    }
    // 5. 满血复活 (stamina >= 95)
    else if (m_state.stamina >= 95 && m_state.isRestingInCorner) {
        s_actionStepCount = 0;
        triggerStaminaRecovered();
    }
}

void BehaviorEngine::triggerRestInCorner()
{
    m_state.isRestingInCorner = true;

    PetActionCommand cmd;
    cmd.speechText = "累瘫了...跑不动了，歇会儿 💤";
    cmd.durationMs = 3500;
    cmd.moveToCenter = false;
    executeAction(cmd);

    if (m_activeWidget != nullptr) {
        auto &mascot = m_activeWidget->mascot();
        auto env = mascot.state ? mascot.state->env : nullptr;
        if (env && mascot.state->anchor.y < (env->floor.y - 25.0)) {
            m_fallRecoveryPhase = FallRecoveryPhase::Falling;
            mascot.detach_from_borders();
            mascot.next_behavior("Fall");
        } else {
            m_fallRecoveryPhase = FallRecoveryPhase::LieDownBreathing;
            m_fallPhaseStartTime = QDateTime::currentMSecsSinceEpoch();
            auto lieBehavior = mascot.initial_behavior_list().find("LieDown", false);
            if (lieBehavior != nullptr) {
                mascot.next_behavior("LieDown");
            } else {
                mascot.next_behavior("SitDown");
            }
        }
    }
}

void BehaviorEngine::triggerStaminaRecovered()
{
    m_state.isRestingInCorner = false;
    m_fallRecoveryPhase = FallRecoveryPhase::None;

    PetActionCommand cmd;
    cmd.durationMs = 3500;
    cmd.moveToCenter = false;

    // 根据当前心情决定唤醒台词与动作
    if (m_state.mood >= 0) {
        cmd.type = PetActionType::Jump;
        cmd.speechText = "充电完毕！继续巡逻！🌟";
        cmd.moodDelta = +10;
        executeAction(cmd);

        if (m_activeWidget != nullptr) {
            static const std::vector<std::string> exploreBehaviors = {
                "RunAlongWorkAreaFloor",
                "WalkAndGrabBottomLeftWall",
                "WalkAndGrabBottomRightWall",
                "JumpFromBottomOfIE"
            };
            int idx = QRandomGenerator::global()->bounded(static_cast<int>(exploreBehaviors.size()));
            m_activeWidget->mascot().next_behavior(exploreBehaviors[idx]);
        }
    } else {
        cmd.type = PetActionType::Sit;
        cmd.speechText = "睡醒了，但还是有点无聊… 💭";
        executeAction(cmd);

        if (m_activeWidget != nullptr) {
            m_activeWidget->mascot().next_behavior("WalkAlongWorkAreaFloor");
        }
    }
}

void BehaviorEngine::evaluateUtilityAI()
{
}

void BehaviorEngine::checkInitiativeChat()
{
    if (m_isThinking) return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    int userIdleSec = static_cast<int>((now - m_state.lastInteractionTime) / 1000);

    QJsonObject contextInfo;
    contextInfo["user_idle_seconds"] = userIdleSec;
    contextInfo["mood"] = m_state.mood;
    contextInfo["energy"] = m_state.energy;
    contextInfo["boredom"] = m_state.boredom;
    contextInfo["affection"] = m_state.affection;

    int score = 0;
    QString reason;
    if (InitiativeTrigger::instance()->evaluateInitiative(m_state, contextInfo, score, reason)) {
        m_isThinking = true;
        contextInfo["trigger_reason"] = reason;
        contextInfo["score"] = score;

        AgentService::instance()->requestPetIntent(contextInfo, [this](bool success, const AIBehaviorIntent &intent) {
            m_isThinking = false;
            if (success) {
                InitiativeTrigger::instance()->recordTalkSuccess(m_state);

                PetActionCommand cmd;
                cmd.type = PetActionType::Talk;
                cmd.speechText = intent.speech;
                cmd.durationMs = 6500;
                cmd.moveToCenter = false;

                // 配合 AI 意图生动执行对应小动作（若在地面）
                if (m_activeWidget != nullptr) {
                    auto &mascot = m_activeWidget->mascot();
                    auto env = mascot.state ? mascot.state->env : nullptr;
                    if (env && mascot.state->anchor.y >= (env->floor.y - 25.0)) {
                        if (intent.emotion == "happy" || intent.intent == "celebrate") {
                            mascot.next_behavior("SitWhileDanglingLegs");
                        } else if (intent.emotion == "curious" || intent.intent == "seek_attention") {
                            mascot.next_behavior("SitAndFaceMouse");
                        } else if (intent.emotion == "bored") {
                            mascot.next_behavior("SitAndSpinHead");
                        } else if (intent.intent == "explore") {
                            mascot.next_behavior("WalkAlongWorkAreaFloor");
                        }
                    }
                }

                executeAction(cmd);
            }
        });
    }
}

