// 
// Shijima-Qt - Cross-platform shimeji simulation app for desktop
// Copyright (C) 2025 pixelomer
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
// 

#include "ShijimaWidget.hpp"
#include <QWidget>
#include <QPainter>
#include <QFile>
#include <QDir>
#include <QScreen>
#include <QMouseEvent>
#include <QMenu>
#include <QWindow>
#include <QDebug>
#include <QGuiApplication>
#include <QTextStream>
#include <shijima/shijima.hpp>
#include "Platform/Platform.hpp"
#include "ShimejiInspectorDialog.hpp"
#include "AssetLoader.hpp"
#include "ShijimaContextMenu.hpp"
#include "ShijimaManager.hpp"
#include "SelectionToolbar.hpp"
#include "AskDialog.hpp"
#include "AgentService.hpp"
#include "AgentSettingsDialog.hpp"
#include "TimerManager.hpp"
#include "TimerListDialog.hpp"
#include "AgentSettingsDialog.hpp"
#include "HotkeyManager.hpp"
#include "PetEventBus.hpp"
#include "PetAction.hpp"
#include "BehaviorEngine.hpp"
#include "ScoreBadgeWidget.hpp"
#include "PetStatusBarWidget.hpp"
#include <shimejifinder/utils.hpp>
#include <cmath>
#include <algorithm>

using namespace shijima;

ShijimaWidget::ShijimaWidget(MascotData *mascotData,
    std::unique_ptr<shijima::mascot::manager> mascot,
    int mascotId, bool windowedMode, QWidget *parent):
#if defined(__APPLE__)
    PlatformWidget(nullptr, PlatformWidget::ShowOnAllDesktops),
#else
    PlatformWidget(parent, PlatformWidget::ShowOnAllDesktops),
#endif
    m_windowedMode(windowedMode), m_data(mascotData),
    m_inspector(nullptr), m_mascotId(mascotId), m_messageBubble(nullptr)
{
    m_windowHeight = 128;
    m_windowWidth = 128;
    m_mascot = std::move(mascot);
    
    QDir dir { m_data->imgRoot() };
    if (dir.exists() && dir.cdUp() && dir.cd("sound")) {
        m_sounds.searchPaths.push_back(dir.path());
    }
    
    if (!m_windowedMode) {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_MacShowFocusRect, false);
        Qt::WindowFlags flags = Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint
            | Qt::WindowDoesNotAcceptFocus | Qt::NoDropShadowWindowHint
            | Qt::WindowOverridesSystemGestures;
        #if defined(__APPLE__)
        flags |= Qt::Window;
        #else
        flags |= Qt::Tool;
        #endif
        setWindowFlags(flags);
    }
    setFixedSize(m_windowWidth, m_windowHeight);
    m_messageBubble = new MessageBubble(m_windowedMode ? parent : nullptr);
    m_selectionToolbar = new SelectionToolbar(m_windowedMode ? parent : nullptr);
    m_askDialog = new AskDialog(m_windowedMode ? parent : nullptr);
    m_settingsDialog = new AgentSettingsDialog(m_windowedMode ? parent : nullptr);
    BehaviorEngine::instance()->setActiveWidget(this);

    m_selectionToolbar->onTranslateRequested = [this](QString const& text) {
        onTranslateRequested(text);
    };

    m_selectionToolbar->onAskRequested = [this](QString const& text) {
        onAskRequested(text);
    };

    m_askDialog->onSubmit = [this](QString const& context, QString const& question) {
        onQuestionSubmitted(context, question);
    };

    m_askDialog->onCancel = [this]() {
        setWaitingForAgent(false);
    };

    // 当设置变更时刷新全局快捷键配置
    connect(m_settingsDialog, &QDialog::accepted, this, []() {
        ShijimaManager::defaultManager()->updateGlobalHotkeys();
    });

    // 绑定定时器到期联动：自动弹出提醒气泡或触发 Agent 自动执行任务
    TimerManager::instance()->onTimerTriggered = [this](const ScheduledTimer &timer) {
        if (timer.type == TimerType::AiTask) {
            std::cout << "[定时器调度] 到期自动执行 Agent 任务: " << timer.title.toStdString() << std::endl;
            setWaitingForAgent(true);
            showMessage("🤖 **定时任务触发**\n\n正在自动执行: " + timer.title + "...", 0, "", true);

            QString prompt = timer.taskPrompt.isEmpty() ? timer.title : timer.taskPrompt;
            AgentService::instance()->ask("", prompt,
                [this, timer](QString const& progressMsg) {
                    showMessage(QString("🤖 **定时任务: %1**\n\n%2").arg(timer.title, progressMsg), 0, "", true);
                },
                [this, timer](bool success, QString const& result, QString const& appTarget) {
                    setWaitingForAgent(false);
                    if (success) {
                        BehaviorEngine::instance()->addAffection(3, 8);
                        showMessage(QString("⏰ **定时任务交付: %1**\n\n%2").arg(timer.title, result), 35000, appTarget, true);
                    } else {
                        showMessage(QString("❌ **定时任务失败: %1**\n\n%2").arg(timer.title, result), 10000, appTarget, true);
                    }
                }
            );
        } else {
            std::cout << "[定时器提醒] 到期弹出提醒气泡: " << timer.title.toStdString() << std::endl;
            BehaviorEngine::instance()->addAffection(2, 5);
            QString msg = QString("⏰ **定时提醒到达！**\n\n📌 **%1**\n\n*（时间: %2）*")
                .arg(timer.title)
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
            showMessage(msg, 20000, "", true);
        }
    };
}

ShijimaWidget::ShijimaWidget(ShijimaWidget &old, bool windowedMode,
    QWidget *parent) : ShijimaWidget(old.mascotData(),
    std::move(old.m_mascot), old.m_mascotId,
    windowedMode, parent) {}

void ShijimaWidget::showInspector() {
    if (m_inspector == nullptr) {
        m_inspector = new ShimejiInspectorDialog { this };
    }
    m_inspector->show();
}

bool ShijimaWidget::inspectorVisible() {
    return m_inspector != nullptr && m_inspector->isVisible();
}

void ShijimaWidget::showAgentSettings() {
    if (m_settingsDialog == nullptr) {
        m_settingsDialog = new AgentSettingsDialog(m_windowedMode ? parentWidget() : nullptr);
    }
    m_settingsDialog->refreshValues();
    m_settingsDialog->show();
    m_settingsDialog->raise();
    m_settingsDialog->activateWindow();
}

Asset const& ShijimaWidget::getActiveAsset() {
    auto &name = m_mascot->state->active_frame.get_name(m_mascot->state->looking_right);
    auto lowerName = shimejifinder::to_lower(name);
    auto imagePath = QDir::cleanPath(m_data->imgRoot()
        + QDir::separator() + QString::fromStdString(lowerName));
    return AssetLoader::defaultLoader()->loadAsset(imagePath);
}

bool ShijimaWidget::isMirroredRender() const {
    return m_mascot->state->active_frame.right_name.empty() &&
        m_mascot->state->looking_right;
}

void ShijimaWidget::paintEvent(QPaintEvent *event) {
    if (!m_visible) {
        return;
    }
    auto &asset = getActiveAsset();
    auto &image = asset.image(isMirroredRender());
    auto scaledSize = image.size() / m_drawScale;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    // 1. 绘制桌宠人偶本体
    painter.drawImage(QRect { m_drawOrigin, scaledSize }, image);

    // 2. 原生合并绘制暗夜双微盘状态栏 (同一图层渲染，0延迟，绝对0闪烁)
    const auto &st = BehaviorEngine::instance()->state();
    int stamina = std::clamp(st.stamina, 0, 100);
    int mood = std::clamp(st.mood, 0, 100);

    QPoint petGlobalPos = m_windowedMode ? mapToParent(QPoint(0, 0)) : mapToGlobal(QPoint(0, 0));
    bool isAtCeiling = (petGlobalPos.y() < 80);

    const int circleSize = 22;
    int orbCenterY = isAtCeiling ? (m_drawOrigin.y() + scaledSize.height() + 14) 
                                 : (m_drawOrigin.y() - 14);
    int centerX = m_drawOrigin.x() + scaledSize.width() / 2;
    int leftOrbX = centerX - 25;
    int rightOrbX = centerX + 3;

    // A. 左侧：翠绿体力环形微盘
    QRect staminaRect(leftOrbX, orbCenterY - circleSize / 2, circleSize, circleSize);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(10, 15, 29, 230));
    painter.drawEllipse(staminaRect);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(30, 41, 59, 180), 2.0));
    painter.drawEllipse(staminaRect.adjusted(1, 1, -1, -1));

    if (stamina > 0) {
        double span = (stamina / 100.0) * -360.0 * 16.0;
        painter.setPen(QPen(QColor(74, 222, 128), 2.2, Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(staminaRect.adjusted(1, 1, -1, -1), 90 * 16, static_cast<int>(span));
    }
    painter.setFont(QFont("-apple-system", 9, QFont::Bold));
    painter.setPen(QColor(74, 222, 128));
    painter.drawText(staminaRect, Qt::AlignCenter, "⚡");

    // B. 右侧：紫罗兰心情环形微盘
    QRect moodRect(rightOrbX, orbCenterY - circleSize / 2, circleSize, circleSize);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(10, 15, 29, 230));
    painter.drawEllipse(moodRect);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(30, 41, 59, 180), 2.0));
    painter.drawEllipse(moodRect.adjusted(1, 1, -1, -1));

    int displayMood = std::clamp(mood < 0 ? (mood + 100) / 2 : (50 + mood / 2), 0, 100);
    if (displayMood > 0) {
        double span = (displayMood / 100.0) * -360.0 * 16.0;
        painter.setPen(QPen(QColor(192, 132, 252), 2.2, Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(moodRect.adjusted(1, 1, -1, -1), 90 * 16, static_cast<int>(span));
    }
    painter.setFont(QFont("-apple-system", 9, QFont::Bold));
    painter.setPen(QColor(192, 132, 252));
    painter.drawText(moodRect, Qt::AlignCenter, "☻");

#ifdef __linux__
    if (Platform::useWindowMasks()) {
        m_windowMask = QBitmap::fromPixmap(asset.mask(isMirroredRender())
            .scaled(scaledSize));
        m_windowMask.translate(m_drawOrigin);
        auto bounding = m_windowMask.boundingRect();
        bounding.setTop(0);
        bounding.setLeft(0);
        if (bounding.width() > 0 && bounding.height() > 0) {
            setMask(m_windowMask);
        }
        else {
            setMask(QRect { m_windowWidth - 2, m_windowHeight - 2, 1, 1 });
        }
    }
#endif
}

bool ShijimaWidget::updateOffsets() {
    bool needsRepaint = false;
    auto &frame = m_mascot->state->active_frame;
    auto &asset = getActiveAsset();
    
    int originalWidth = asset.originalSize().width();
    int originalHeight = asset.originalSize().height();
    double scale = m_mascot->state->env->get_scale();
    int screenWidth = (int)(m_mascot->state->env->screen.width()
        / scale);
    int screenHeight = (int)(m_mascot->state->env->screen.height()
        / scale);
    const int topPadding = 32;
    int windowWidth = (int)(originalWidth / scale);
    int windowHeight = (int)(originalHeight / scale) + topPadding;

    if (windowWidth != m_windowWidth) {
        m_windowWidth = windowWidth;
        setFixedWidth(m_windowWidth);
        needsRepaint = true;
    }
    if (windowHeight != m_windowHeight) {
        m_windowHeight = windowHeight;
        setFixedHeight(m_windowHeight);
        needsRepaint = true;
    }

    if (isMirroredRender()) {
        m_anchorInWindow = {
            (int)((originalWidth - frame.anchor.x) / scale),
            (int)(frame.anchor.y / scale) + topPadding };
    }
    else {
        m_anchorInWindow = { (int)(frame.anchor.x / scale),
            (int)(frame.anchor.y / scale) + topPadding };
    }

    QPoint drawOffset;
    m_visible = true;
    int winX = (int)m_mascot->state->anchor.x - m_anchorInWindow.x()
        - (int)env()->screen.left;
    int winY = (int)m_mascot->state->anchor.y - m_anchorInWindow.y()
        - (int)env()->screen.top;
    if (winX < 0) {
        drawOffset.setX(winX);
        winX = 0;
    }
    else if (winX + windowWidth > screenWidth) {
        drawOffset.setX(winX - screenWidth + windowWidth);
        winX = screenWidth - windowWidth;
    }
    if (winY < 0) {
        drawOffset.setY(winY);
        winY = 0;
    }
    else if (winY + windowHeight > screenHeight) {
        drawOffset.setY(winY - screenHeight + windowHeight);
        winY = screenHeight - windowHeight;
    }
    winX += (int)env()->screen.left;
    winY += (int)env()->screen.top;

    if (isMirroredRender()) {
        drawOffset += QPoint {
            (int)((originalWidth - asset.offset().topRight().x()) / scale),
            (int)(asset.offset().topLeft().y() / scale) + topPadding };
    }
    else {
        drawOffset += asset.offset().topLeft() / scale + QPoint(0, topPadding);
    }
    if (drawOffset != m_drawOrigin) {
        needsRepaint = true;
        m_drawOrigin = drawOffset;
    }
    if (scale != m_drawScale) {
        needsRepaint = true;
        m_drawScale = scale;
    }
    move(winX, winY);

    // 实时更新鼠标悬停提示 (ToolTip 状态卡片)
    const auto &st = BehaviorEngine::instance()->state();
    setToolTip(QString("🐾 阿呆桌宠\n⚡ 体力: %1% %2\n😊 心情: %3 | 💕 亲密: %4")
        .arg(st.stamina)
        .arg(st.isRestingInCorner ? "(角落休整 💤)" : "(元气探索 🌟)")
        .arg(st.mood)
        .arg(st.affection));

    if (m_messageBubble != nullptr && m_messageBubble->hasMessage()) {
        QPoint petGlobalPos = m_windowedMode ? mapToParent(QPoint(0, 0)) : mapToGlobal(QPoint(0, 0));
        auto env = m_mascot->state->env;

        int bubbleW = m_messageBubble->width();
        int bubbleH = m_messageBubble->height();

        // 默认水平居中对齐桌宠，且受屏幕可见边界保护
        int rawX = petGlobalPos.x() + (m_windowWidth / 2) - (bubbleW / 2);
        int scrLeft = (int)env->screen.left;
        int scrRight = (int)(env->screen.left + env->screen.width());
        int scrTop = (int)env->screen.top;
        int scrBottom = (int)(env->screen.top + env->screen.height());

        int clampedX = std::clamp(rawX, scrLeft + 12, scrRight - bubbleW - 12);

        // 垂直方向：桌宠在屏幕顶部高处时弹在脚下，否则弹在头顶
        int rawY = (petGlobalPos.y() < scrTop + bubbleH + 60) 
            ? (petGlobalPos.y() + m_windowHeight + 8)
            : (petGlobalPos.y() - bubbleH - 8);
        int clampedY = std::clamp(rawY, scrTop + 12, scrBottom - bubbleH - 12);

        QPoint targetPos(clampedX, clampedY);
        if (m_messageBubble->pos() != targetPos) {
            m_messageBubble->move(targetPos);
        }
    }

    return needsRepaint;
}

bool ShijimaWidget::pointInside(QPoint const& point) {
    if (!m_visible) {
        return false;
    }
    auto &asset = getActiveAsset();
    auto image = asset.image(isMirroredRender());
    int drawnWidth = (int)(image.width() / m_drawScale);
    int drawnHeight = (int)(image.height() / m_drawScale);
    auto imagePos = point - m_drawOrigin;
    if (imagePos.x() < 0 || imagePos.y() < 0 ||
        imagePos.x() > drawnWidth || imagePos.y() > drawnHeight)
    {
        return false;
    }
    auto color = image.pixelColor(imagePos * m_drawScale);
    if (color.alpha() == 0) {
        return false;
    }
    return true;
}

void ShijimaWidget::tick() {
    if (m_markedForDeletion) {
        close();
        return;
    }
    if (paused()) {
        return;
    }

    if (m_isRunningToCenter) {
        updateOffsets();
        repaint();
        return;
    }

    if (m_isThrowFlying) {
        updateOffsets();
        repaint();
        return;
    }

    const auto &st = BehaviorEngine::instance()->state();

    bool isDialogMessageActive = (m_messageBubble != nullptr && m_messageBubble->hasMessage() && !m_messageBubble->isCompactCuteMode());

    // 1. 大消息弹窗展示中 或 等待 Agent 执行时：严禁任何位移！只在原地做可爱动作（坐着转头、晃腿、看鼠标、端坐）
    if (isDialogMessageActive || m_isWaitingForAgent) {
        static int s_msgInPlaceTimer = 0;
        static int s_msgInPlaceIndex = 0;
        static const std::vector<std::string> s_inPlaceBehaviors = {
            "SitAndSpinHead",       // 坐着转头
            "SitWhileDanglingLegs", // 坐着晃腿
            "SitAndFaceMouse",      // 坐着看鼠标
            "SitDown"               // 端坐
        };

        QString curName = currentBehaviorName();
        bool isInPlaceAction = (
            curName.contains("Sit", Qt::CaseInsensitive) ||
            curName.contains("Spin", Qt::CaseInsensitive) ||
            curName.contains("Dangle", Qt::CaseInsensitive)
        );

        if (!isInPlaceAction) {
            std::string act = s_inPlaceBehaviors[s_msgInPlaceIndex % s_inPlaceBehaviors.size()];
            if (m_mascot->initial_behavior_list().find(act, false) != nullptr) {
                m_mascot->next_behavior(act);
            } else {
                m_mascot->next_behavior("SitDown");
            }
            s_msgInPlaceTimer = 0;
        } else {
            // 每隔约 12 秒（300 ticks，每次 40ms）在原地动作池中平滑切换下一个可爱姿态
            s_msgInPlaceTimer++;
            if (s_msgInPlaceTimer >= 300) {
                s_msgInPlaceTimer = 0;
                s_msgInPlaceIndex = (s_msgInPlaceIndex + 1) % s_inPlaceBehaviors.size();
                std::string nextAct = s_inPlaceBehaviors[s_msgInPlaceIndex];
                if (m_mascot->initial_behavior_list().find(nextAct, false) != nullptr) {
                    m_mascot->next_behavior(nextAct);
                }
            }
        }
    }
    // 2. 疲惫休整保护：当体力耗尽 (stamina <= 15 或正在休整中) 时
    else if (st.isRestingInCorner || st.stamina <= 15) {
        auto env = m_mascot->state->env;
        // 1. 如果还在天花板或高处，立即松开脱离天花板并掉落到底面！
        if (env && m_mascot->state->anchor.y < (env->floor.y - 25.0)) {
            QString curName = currentBehaviorName();
            if (curName.contains("Ceiling", Qt::CaseInsensitive) || 
                curName.contains("Climb", Qt::CaseInsensitive) ||
                curName.contains("Wall", Qt::CaseInsensitive)) {
                std::cout << "[疲惫断电] 体力耗尽，立即松开天花板脱落摔向地面！" << std::endl;
                m_mascot->detach_from_borders();
                m_mascot->next_behavior("Fall");
            }
        }
        // 2. 一旦在地面，立即执行丰富多样的休息动作序列（趴平休息、晃腿、转头、东张西望等）
        else if (env && m_mascot->state->anchor.y >= (env->floor.y - 25.0)) {
            static int s_restBehaviorTimer = 0;
            static int s_restBehaviorIndex = 0;
            static const std::vector<std::string> s_restBehaviors = {
                "LieDown",              // 趴平/躺下休息
                "SitWhileDanglingLegs", // 坐着晃腿
                "SitAndSpinHead",       // 坐着东张西望转头
                "SitAndFaceMouse",      // 坐着看鼠标
                "LieDown",              // 再次趴平大歇
                "SitDown"               // 端坐休息
            };

            QString curName = currentBehaviorName();
            bool isRestAction = (
                curName.contains("Lie", Qt::CaseInsensitive) ||
                curName.contains("Sit", Qt::CaseInsensitive) ||
                curName.contains("Dangle", Qt::CaseInsensitive) ||
                curName.contains("Spin", Qt::CaseInsensitive) ||
                curName.contains("Sleep", Qt::CaseInsensitive)
            );

            // 若当前不是休息动作，立即切入当前轮次的休息动作
            if (!isRestAction) {
                std::string targetAct = s_restBehaviors[s_restBehaviorIndex % s_restBehaviors.size()];
                if (m_mascot->initial_behavior_list().find(targetAct, false) != nullptr) {
                    m_mascot->next_behavior(targetAct);
                } else {
                    m_mascot->next_behavior("SitDown");
                }
                s_restBehaviorTimer = 0;
            } else {
                // 大幅延长每个休息姿态的驻留时长（约 30 秒，750 ticks），从容安详地大歇，绝不频繁抽搐切换！
                s_restBehaviorTimer++;
                if (s_restBehaviorTimer >= 750) {
                    s_restBehaviorTimer = 0;
                    s_restBehaviorIndex = (s_restBehaviorIndex + 1) % s_restBehaviors.size();
                    std::string nextAct = s_restBehaviors[s_restBehaviorIndex];
                    if (m_mascot->initial_behavior_list().find(nextAct, false) != nullptr) {
                        m_mascot->next_behavior(nextAct);
                    }
                }
            }
        }
    } else {
        // 仅在体力充足且非等待 Agent 状态时，才主动触发跳跃跃上窗口
        checkAndJumpToActiveIE();
    }

    // 实时左右光标盯视追踪 (当处于看鼠标/追逐/注视状态时)
    QString curBeh = currentBehaviorName();
    if (curBeh.contains("SitAndFaceMouse", Qt::CaseInsensitive) ||
        curBeh.contains("ChaseMouse", Qt::CaseInsensitive) ||
        curBeh.contains("LookAtCursor", Qt::CaseInsensitive)) {
        QPoint curPos = QCursor::pos();
        QPoint petPos = m_windowedMode ? mapToParent(QPoint(0, 0)) : mapToGlobal(QPoint(0, 0));
        int petCenterX = petPos.x() + (m_windowWidth / 2);
        bool shouldLookRight = (curPos.x() >= petCenterX);
        if (m_mascot && m_mascot->state && m_mascot->state->looking_right != shouldLookRight) {
            m_mascot->state->looking_right = shouldLookRight;
        }
    }

    auto prev_frame = m_mascot->state->active_frame;
    try {
        m_mascot->tick();
    } catch (const std::exception &e) {
        std::cerr << "[Shijima Safe Guard] Tick recovered from: " << e.what() << std::endl;
        m_mascot->reset_position();
        m_mascot->detach_from_borders();
        m_mascot->next_behavior("Fall");
    }

    checkWindowVanished();

    auto &new_frame = m_mascot->state->active_frame;
    auto &new_sound = m_mascot->state->active_sound;
    bool forceRepaint = prev_frame.name != new_frame.name;
    bool offsetsChanged = updateOffsets();

    if (m_mascot->state->dead) {
        forceRepaint = true;
        new_frame.name = "";
        new_sound = "";
        m_mascot->state->active_sound_changed = true;
        markForDeletion();
    }
    if (offsetsChanged || forceRepaint) {
        repaint();
        update();
    }
    if (m_mascot->state->active_sound_changed) {
        m_sounds.stop();
        if (!new_sound.empty()) {
            m_sounds.play(QString::fromStdString(new_sound));
        }
    }
    else if (!m_sounds.playing()) {
        m_mascot->state->active_sound.clear();
    }

    if (m_inspector != nullptr && m_inspector->isVisible()) {
        m_inspector->tick();
    }
}

void ShijimaWidget::contextMenuClosed(QCloseEvent *event) {
    m_contextMenuVisible = false;
}

void ShijimaWidget::showContextMenu(QPoint const& pos) {
    m_contextMenuVisible = true;
    ShijimaContextMenu *menu = new ShijimaContextMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(pos);
}

ShijimaWidget::~ShijimaWidget() {
    if (BehaviorEngine::instance()->activeWidget() == this) {
        BehaviorEngine::instance()->setActiveWidget(nullptr);
    }
    if (m_moveAnimation != nullptr) {
        m_moveAnimation->stop();
        delete m_moveAnimation;
        m_moveAnimation = nullptr;
    }
    if (m_dragTargetPt != nullptr) {
        *m_dragTargetPt = nullptr;
        m_dragTargetPt = nullptr;
    }
    if (m_inspector != nullptr) {
        m_inspector->close();
        delete m_inspector;
    }
    if (m_messageBubble != nullptr) {
        m_messageBubble->close();
        delete m_messageBubble;
    }
    if (m_selectionToolbar != nullptr) {
        m_selectionToolbar->close();
        delete m_selectionToolbar;
    }
    if (m_askDialog != nullptr) {
        m_askDialog->close();
        delete m_askDialog;
    }
    if (m_settingsDialog != nullptr) {
        m_settingsDialog->close();
        delete m_settingsDialog;
    }
    setDragTarget(nullptr);
}

void ShijimaWidget::setDragTarget(ShijimaWidget *target) {
    if (m_dragTarget != nullptr) {
        m_dragTarget->m_dragTargetPt = nullptr;
    }
    if (target != nullptr) {
        if (target->m_dragTargetPt != nullptr) {
            throw std::runtime_error("target widget being dragged by multiple widgets");
        }
        m_dragTarget = target;
        m_dragTarget->m_dragTargetPt = &m_dragTarget;
    }
    else {
        m_dragTarget = nullptr;
    }
}

void ShijimaWidget::mousePressEvent(QMouseEvent *event) {
    auto pos = event->pos();
    if (m_dragTarget != nullptr) {
        m_dragTarget->m_mascot->state->dragging = false;
    }
    if (pointInside(pos)) {
        setDragTarget(this);
    }
    else {
        QPoint envPos;
        if (m_windowedMode) {
            envPos = mapToParent(pos);
        }
        else {
            envPos = mapToGlobal(pos);
        }
        ShijimaWidget *target = ShijimaManager::defaultManager()->hitTest(envPos);
        setDragTarget(target);
        if (target == nullptr) {
            event->ignore();
            return;
        }
    }
    if (event->button() == Qt::MouseButton::LeftButton) {
        if (m_dragTarget->m_throwPhysicsTimer != nullptr) {
            m_dragTarget->m_throwPhysicsTimer->stop();
        }
        m_dragTarget->m_isThrowFlying = false;
        m_dragTarget->m_mascot->state->dragging = true;
        m_dragTarget->m_lastMousePos = event->globalPosition().toPoint();
        m_dragTarget->m_lastMouseMoveTime = QDateTime::currentMSecsSinceEpoch();
        m_dragTarget->m_dragVelocityX = 0.0;
        m_dragTarget->m_dragVelocityY = 0.0;

        // 捕获拖拽前正在执行的意图（如正在爬顶、扑窗口等）
        m_dragTarget->m_interruptedGoal = m_dragTarget->detectCurrentGoal();

        BehaviorEngine::instance()->recordUserInteraction();
        QJsonObject payload;
        payload["mascot_id"] = m_dragTarget->mascotId();
        PetEventBus::instance()->emitEvent("user.drag_pet", payload);
    }
    else if (event->button() == Qt::MouseButton::RightButton) {
        BehaviorEngine::instance()->recordUserInteraction();
        auto screenPos = mapToGlobal(pos);
        m_dragTarget->showContextMenu(screenPos);
        setDragTarget(nullptr);
    }
}

void ShijimaWidget::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragTarget == nullptr || !m_dragTarget->m_mascot || !m_dragTarget->m_mascot->state) {
        return;
    }
    QPoint curPos = event->globalPosition().toPoint();
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lastMouseMoveTime > 0) {
        qint64 dt = now - m_lastMouseMoveTime;
        if (dt > 0 && dt < 200) {
            double vx = (curPos.x() - m_lastMousePos.x()) / (double)dt * 20.0;
            double vy = (curPos.y() - m_lastMousePos.y()) / (double)dt * 20.0;
            // 指数滑动平均滤波 (EMA)
            m_dragVelocityX = m_dragVelocityX * 0.3 + vx * 0.7;
            m_dragVelocityY = m_dragVelocityY * 0.3 + vy * 0.7;
        }
    }
    m_lastMousePos = curPos;
    m_lastMouseMoveTime = now;
}

void ShijimaWidget::closeAction() {
    close();
}

void ShijimaWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (m_dragTarget == nullptr) {
        return;
    }
    if (event->button() == Qt::MouseButton::LeftButton) {
        m_dragTarget->m_mascot->state->dragging = false;

        // 记录用户互动时间（重置心情衰减）
        BehaviorEngine::instance()->recordUserInteraction();

        // 计算鼠标瞬时甩动力度 (Velocity)
        double throwSpeed = std::hypot(m_dragTarget->m_dragVelocityX, m_dragTarget->m_dragVelocityY);

        // 平稳放下或快速甩出均增加互动亲密度
        BehaviorEngine::instance()->addAffection(2, 5);

        // 如果用户快速甩手丢出 (throwSpeed >= 1.5)，启动 60FPS 运动学方程推动抛物线飞行
        if (throwSpeed >= 1.5) {
            std::cout << "[物理投掷] 成功捕获快速甩手! 速度向量: (" 
                      << m_dragTarget->m_dragVelocityX << ", " << m_dragTarget->m_dragVelocityY 
                      << "), 标量速度: " << throwSpeed << std::endl;
            m_dragTarget->applyThrowPhysics(m_dragTarget->m_dragVelocityX, m_dragTarget->m_dragVelocityY);
            m_dragTarget->m_interruptedGoal = PetInterruptedGoal::None;
        } else {
            // 平稳放下时智能磁吸
            m_dragTarget->snapToNearestBorderOrWindow();

            // 如果刚才有目标被硬生生打断，放下来后发个可爱小脾气！
            if (m_dragTarget->m_interruptedGoal != PetInterruptedGoal::None) {
                m_dragTarget->triggerTantrum(m_dragTarget->m_interruptedGoal);
                m_dragTarget->m_interruptedGoal = PetInterruptedGoal::None;
            }
        }

        bool handled = false;
        if (throwSpeed < 1.5) {
            if (BehaviorEngine::instance()->state().isRestingInCorner) {
                handled = BehaviorEngine::instance()->handlePetClickedWhileResting(m_dragTarget);
            } else if (BehaviorEngine::instance()->moodTier() == MoodTier::ExtremelyLow) {
                handled = BehaviorEngine::instance()->handlePetClickedInPoutMode(m_dragTarget);
            }
        }

        if (!handled) {
            QJsonObject payload;
            payload["mascot_id"] = m_dragTarget->mascotId();
            PetEventBus::instance()->emitEvent("user.click_pet", payload);
        }
        setDragTarget(nullptr);
    }
}


void ShijimaWidget::applyThrowPhysics(double vx, double vy) {
    if (!m_mascot || !m_mascot->state || !m_mascot->state->env) return;

    if (m_throwPhysicsTimer == nullptr) {
        m_throwPhysicsTimer = new QTimer(this);
    }
    m_throwPhysicsTimer->stop();
    m_throwPhysicsTimer->disconnect();

    // 独占接管物理坐标，避免主循环 tick 并发冲突
    m_isThrowFlying = true;

    // 适度放大初速度
    m_throwVx = std::clamp(vx * 1.6, -50.0, 50.0);
    m_throwVy = std::clamp(vy * 1.6, -50.0, 50.0);

    m_mascot->next_behavior("Thrown");

    connect(m_throwPhysicsTimer, &QTimer::timeout, this, [this]() {
        if (!m_mascot || !m_mascot->state || !m_mascot->state->env || m_mascot->state->dragging) {
            m_isThrowFlying = false;
            if (m_throwPhysicsTimer) {
                m_throwPhysicsTimer->stop();
            }
            return;
        }

        auto env = m_mascot->state->env;
        auto &anchor = m_mascot->state->anchor;

        // 物理更新: 重力加速度 g = 0.95, 空气阻力
        m_throwVy += 0.95;
        m_throwVx *= 0.985;

        anchor.x += m_throwVx;
        anchor.y += m_throwVy;

        bool landed = false;

        // 1. 活跃窗口碰撞判定
        if (env->active_ie.visible()) {
            auto &ie = env->active_ie;
            // 落在窗口顶梁上
            if (anchor.x >= (ie.left - 10.0) && anchor.x <= (ie.right + 10.0) &&
                anchor.y >= (ie.top - 15.0) && anchor.y <= (ie.top + 25.0) && m_throwVy > 0) {
                anchor.y = ie.top;
                landed = true;
                m_mascot->next_behavior("WalkAlongIECeiling");
            }
            // 撞到窗口左边壁
            else if (std::abs(anchor.x - ie.left) <= 18.0 &&
                     anchor.y >= ie.top && anchor.y <= ie.bottom) {
                anchor.x = ie.left;
                m_mascot->state->looking_right = false;
                landed = true;
                m_mascot->next_behavior("ClimbAlongWall");
            }
            // 撞到窗口右边壁
            else if (std::abs(anchor.x - ie.right) <= 18.0 &&
                     anchor.y >= ie.top && anchor.y <= ie.bottom) {
                anchor.x = ie.right;
                m_mascot->state->looking_right = true;
                landed = true;
                m_mascot->next_behavior("ClimbAlongWall");
            }
        }

        // 2. 地面碰撞判定
        if (!landed && anchor.y >= env->floor.y) {
            anchor.y = env->floor.y;
            if (std::abs(m_throwVy) > 6.0) {
                m_throwVy = -m_throwVy * 0.35; // 触地弹跳
                m_throwVx *= 0.6;
            } else {
                landed = true;
                m_mascot->next_behavior("Stand");
            }
        }

        // 3. 天花板碰撞判定
        if (!landed && anchor.y <= env->ceiling.y) {
            anchor.y = env->ceiling.y;
            m_throwVy = -m_throwVy * 0.3;
        }

        // 4. 屏幕左/右主墙壁碰撞判定
        if (!landed) {
            if (anchor.x <= env->work_area.left) {
                anchor.x = env->work_area.left;
                m_mascot->state->looking_right = false;
                landed = true;
                m_mascot->next_behavior("ClimbAlongWall");
            } else if (anchor.x >= env->work_area.right) {
                anchor.x = env->work_area.right;
                m_mascot->state->looking_right = true;
                landed = true;
                m_mascot->next_behavior("ClimbAlongWall");
            }
        }

        updateOffsets();
        repaint();

        if (landed) {
            m_isThrowFlying = false;
            if (m_throwPhysicsTimer) {
                m_throwPhysicsTimer->stop();
            }
        }
    });

    m_throwPhysicsTimer->start(16); // 60 FPS 抛物线
}

void ShijimaWidget::snapToNearestBorderOrWindow() {
    if (!m_mascot || !m_mascot->state || !m_mascot->state->env) return;

    auto env = m_mascot->state->env;
    auto &anchor = m_mascot->state->anchor;
    const double snapThreshold = 35.0; // 35 像素吸附容差

    // 1. 活跃窗口吸附（Active IE）
    if (env->active_ie.visible()) {
        auto &ie = env->active_ie;

        // A. 靠近窗口顶部 (Top of IE) -> 站立在窗口顶沿
        if (std::abs(anchor.y - ie.top) <= snapThreshold &&
            anchor.x >= (ie.left - snapThreshold) && anchor.x <= (ie.right + snapThreshold)) {
            anchor.y = ie.top;
            anchor.x = std::clamp(anchor.x, ie.left + 10.0, ie.right - 10.0);
            m_mascot->next_behavior("WalkAlongIECeiling");
            updateOffsets();
            repaint();
            return;
        }

        // B. 靠近窗口左边缘 (Left Border of IE) -> 抓墙往上爬
        if (std::abs(anchor.x - ie.left) <= snapThreshold &&
            anchor.y >= (ie.top - snapThreshold) && anchor.y <= (ie.bottom + snapThreshold)) {
            anchor.x = ie.left;
            m_mascot->state->looking_right = false; // 朝向窗口
            m_mascot->next_behavior("ClimbAlongWall");
            updateOffsets();
            repaint();
            return;
        }

        // C. 靠近窗口右边缘 (Right Border of IE) -> 抓墙往上爬
        if (std::abs(anchor.x - ie.right) <= snapThreshold &&
            anchor.y >= (ie.top - snapThreshold) && anchor.y <= (ie.bottom + snapThreshold)) {
            anchor.x = ie.right;
            m_mascot->state->looking_right = true; // 朝向窗口
            m_mascot->next_behavior("ClimbAlongWall");
            updateOffsets();
            repaint();
            return;
        }
    }

    // 2. 屏幕天花板吸附 (Ceiling) -> 倒挂攀爬
    if (std::abs(anchor.y - env->ceiling.y) <= snapThreshold) {
        anchor.y = env->ceiling.y;
        m_mascot->next_behavior("ClimbAlongCeiling");
        updateOffsets();
        repaint();
        return;
    }

    // 3. 屏幕左/右侧边缘主墙壁吸附 (Work Area Walls) -> 沿主屏幕墙壁爬行
    if (std::abs(anchor.x - env->work_area.left) <= snapThreshold) {
        anchor.x = env->work_area.left;
        m_mascot->state->looking_right = false;
        m_mascot->next_behavior("ClimbAlongWall");
        updateOffsets();
        repaint();
        return;
    }
    if (std::abs(anchor.x - env->work_area.right) <= snapThreshold) {
        anchor.x = env->work_area.right;
        m_mascot->state->looking_right = true;
        m_mascot->next_behavior("ClimbAlongWall");
        updateOffsets();
        repaint();
        return;
    }
}

bool ShijimaWidget::checkAndJumpToActiveIE() {
    if (!m_mascot || !m_mascot->state || !m_mascot->state->env) return false;
    if (m_mascot->state->dragging || m_moveAnimation != nullptr) return false;

    auto env = m_mascot->state->env;
    if (!env->active_ie.visible()) return false;

    auto &ie = env->active_ie;
    auto &anchor = m_mascot->state->anchor;

    // 如果已经在空中、窗口顶、或者爬墙中，不重复触发
    QString cur = currentBehaviorName();
    if (cur.contains("Jump") || cur.contains("Fall") || cur.contains("IECeiling") || cur.contains("Climb")) {
        return false;
    }

    static qint64 s_lastJumpTime = 0;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if ((now - s_lastJumpTime) < 4000) {
        return false;
    }

    // 1. 窗口在桌宠正上方 (距离 0 ~ 220px) -> 纵身向上起跳跃上窗口底部
    if (anchor.x >= (ie.left - 20.0) && anchor.x <= (ie.right + 20.0)) {
        double verticalDist = anchor.y - ie.bottom;
        if (verticalDist >= 0.0 && verticalDist <= 220.0) {
            s_lastJumpTime = now;
            std::cout << "[灵动感知] 发现窗口在头顶下方，主动起跳抓取窗口!" << std::endl;
            m_mascot->next_behavior("JumpFromBottomOfIE");
            return true;
        }
    }

    // 2. 窗口在桌宠右侧 (距离 0 ~ 140px，且高度在窗口范围内) -> 跳向窗口左边壁
    if (anchor.x < ie.left && (ie.left - anchor.x) <= 140.0 &&
        anchor.y >= (ie.top - 50.0) && anchor.y <= (ie.bottom + 50.0)) {
        s_lastJumpTime = now;
        m_mascot->state->looking_right = true;
        std::cout << "[灵动感知] 发现窗口在右侧，跳向窗口左壁!" << std::endl;
        m_mascot->next_behavior("JumpOnIELeftWall");
        return true;
    }

    // 3. 窗口在桌宠左侧 (距离 0 ~ 140px，且高度在窗口范围内) -> 跳向窗口右边壁
    if (anchor.x > ie.right && (anchor.x - ie.right) <= 140.0 &&
        anchor.y >= (ie.top - 50.0) && anchor.y <= (ie.bottom + 50.0)) {
        s_lastJumpTime = now;
        m_mascot->state->looking_right = false;
        std::cout << "[灵动感知] 发现窗口在左侧，跳向窗口右壁!" << std::endl;
        m_mascot->next_behavior("JumpOnIERightWall");
        return true;
    }

    return false;
}

void ShijimaWidget::showMessage(QString const& text, int duration, QString const& appTarget, bool moveToCenter) {
    if (m_messageBubble == nullptr || !m_mascot || !m_mascot->state || !m_mascot->state->env) {
        return;
    }

    // 普通气泡（moveToCenter == false）直接在桌宠当前位置头上弹出，不打断物理位置！
    if (!moveToCenter) {
        m_isRunningToCenter = false;
        if (m_moveAnimation != nullptr) {
            m_moveAnimation->stop();
            delete m_moveAnimation;
            m_moveAnimation = nullptr;
        }
        updateOffsets();
        m_messageBubble->showMessage(text, duration, appTarget);
        updateOffsets();
        return;
    }

    auto env = m_mascot->state->env;
    // 右下角适中位置：离右边缘留出 260px 边距（不贴边，且气泡有充足展开空间）
    double targetX = std::max(env->screen.left + 200.0, env->screen.left + env->screen.width() - 260.0);
    double targetY = env->floor.y;
    if (targetY <= 0) {
        targetY = env->screen.bottom;
    }

    double currentX = m_mascot->state->anchor.x;
    double currentY = m_mascot->state->anchor.y;

    m_pendingMessageText = text;
    m_pendingMessageDuration = duration;
    m_pendingAppTarget = appTarget;

    // 停止并清理旧动画
    if (m_moveAnimation != nullptr) {
        m_moveAnimation->stop();
        delete m_moveAnimation;
        m_moveAnimation = nullptr;
    }

    // 如果已经在屏幕底部正中间（容差 25 像素以内），直接就坐并弹出气泡
    if (std::fabs(currentX - targetX) < 25.0 && std::fabs(currentY - targetY) < 25.0) {
        m_isRunningToCenter = false;
        m_mascot->state->anchor = { targetX, targetY };
        auto sitBehavior = m_mascot->initial_behavior_list().find("SitDown", false);
        if (sitBehavior != nullptr) {
            m_mascot->next_behavior("SitDown");
        }
        updateOffsets();
        m_messageBubble->showMessage(text, duration, appTarget);
        updateOffsets();
        return;
    }

    // 设置朝向
    m_mascot->state->looking_right = (targetX >= currentX);

    // 切换为跳跃/飞行动作姿态
    auto jumpBehavior = m_mascot->initial_behavior_list().find("JumpFromBottomOfIE", false);
    if (jumpBehavior != nullptr) {
        m_mascot->next_behavior("JumpFromBottomOfIE");
    } else {
        auto fallBehavior = m_mascot->initial_behavior_list().find("Fall", false);
        if (fallBehavior != nullptr) {
            m_mascot->next_behavior("Fall");
        } else {
            auto runBehavior = m_mascot->initial_behavior_list().find("RunAlongWorkAreaFloor", false);
            if (runBehavior != nullptr) {
                m_mascot->next_behavior("RunAlongWorkAreaFloor");
            }
        }
    }

    m_isRunningToCenter = true;
    m_moveAnimation = new QVariantAnimation(this);
    double distance = std::hypot(targetX - currentX, targetY - currentY);
    int animDuration = std::clamp(static_cast<int>(distance * 0.75), 450, 850);

    // 计算抛物线跳跃/飞行的拱形最高点高度 (根据距离自适应，最高 120~260 像素)
    double jumpHeight = std::clamp(distance * 0.35, 120.0, 260.0);

    m_moveAnimation->setDuration(animDuration);
    m_moveAnimation->setStartValue(0.0);
    m_moveAnimation->setEndValue(1.0);
    m_moveAnimation->setEasingCurve(QEasingCurve::InOutSine);

    // 抛物线轨迹更新: y = linear_y - 4 * H * t * (1 - t)
    connect(m_moveAnimation, &QVariantAnimation::valueChanged, this, [this, currentX, currentY, targetX, targetY, jumpHeight](QVariant const& value) {
        double t = value.toDouble(); // 0.0 -> 1.0
        if (m_mascot && m_mascot->state) {
            double curX = currentX + (targetX - currentX) * t;
            double arcOffset = 4.0 * jumpHeight * t * (1.0 - t);
            double curY = currentY + (targetY - currentY) * t - arcOffset;

            m_mascot->state->anchor = { curX, curY };
            updateOffsets();
            repaint();
        }
    });

    connect(m_moveAnimation, &QVariantAnimation::finished, this, [this, targetX, targetY]() {
        m_isRunningToCenter = false;
        if (m_mascot && m_mascot->state) {
            m_mascot->state->anchor = { targetX, targetY };
            auto sitBehavior = m_mascot->initial_behavior_list().find("SitDown", false);
            if (sitBehavior != nullptr) {
                m_mascot->next_behavior("SitDown");
            }
        }
        updateOffsets();
        if (m_messageBubble != nullptr && !m_pendingMessageText.isEmpty()) {
            m_messageBubble->showMessage(m_pendingMessageText, m_pendingMessageDuration, m_pendingAppTarget);
            updateOffsets();
        }
        if (m_moveAnimation != nullptr) {
            m_moveAnimation->deleteLater();
            m_moveAnimation = nullptr;
        }
    });

    m_moveAnimation->start();
}

void ShijimaWidget::hideMessage() {
    if (m_messageBubble != nullptr) {
        m_messageBubble->hideMessage();
    }
}

bool ShijimaWidget::trySetBehavior(const std::string &name) {
    if (!m_mascot) return false;
    auto b = m_mascot->initial_behavior_list().find(name, false);
    if (b != nullptr) {
        m_mascot->next_behavior(name);
        return true;
    }
    return false;
}

void ShijimaWidget::doAction(const PetActionCommand &cmd) {
    if (!m_mascot || !m_mascot->state) return;

    // 彻底取消自发飘字，避免不断创建临时窗口导致输入失焦
    (void)cmd.moodDelta;
    (void)cmd.affectionDelta;

    // 1. 如果有伴随的气泡文本，展示气泡（带 moveToCenter 控制）
    if (!cmd.speechText.isEmpty()) {
        showMessage(cmd.speechText, cmd.durationMs > 0 ? cmd.durationMs : 4000, cmd.appTarget, cmd.moveToCenter);
    }

    // 2. 根据动作类型调度底层 Behavior
    switch (cmd.type) {
        case PetActionType::Idle:
            if (!trySetBehavior("SitAndFaceMouse")) {
                trySetBehavior("StandUp");
            }
            break;
        case PetActionType::Walk:
            if (!trySetBehavior("WalkAlongWorkAreaFloor")) {
                trySetBehavior("WalkAlongIECeiling");
            }
            break;
        case PetActionType::Sit:
            if (!trySetBehavior("SitDown")) {
                trySetBehavior("SitAndFaceMouse");
            }
            break;
        case PetActionType::LieDown:
        case PetActionType::Sleep:
            if (!trySetBehavior("LieDown")) {
                trySetBehavior("SitWhileDanglingLegs");
            }
            break;
        case PetActionType::Jump:
            if (!trySetBehavior("JumpFromBottomOfIE")) {
                if (!trySetBehavior("WalkLeftAlongIEAndJump")) {
                    trySetBehavior("Fall");
                }
            }
            break;
        case PetActionType::Fall:
            trySetBehavior("Fall");
            break;
        case PetActionType::ChaseMouse:
        case PetActionType::LookAtCursor:
        case PetActionType::FollowCursor:
            if (!trySetBehavior("ChaseMouse")) {
                trySetBehavior("SitAndFaceMouse");
            }
            break;
        case PetActionType::Happy:
            if (!trySetBehavior("JumpFromBottomOfIE")) {
                if (!trySetBehavior("SitAndSpinHead")) {
                    trySetBehavior("WalkAlongWorkAreaFloor");
                }
            }
            break;
        case PetActionType::Angry:
            if (!trySetBehavior("Dragged")) {
                trySetBehavior("SitAndSpinHead");
            }
            break;
        case PetActionType::CustomBehavior:
            if (!cmd.customBehaviorName.isEmpty()) {
                trySetBehavior(cmd.customBehaviorName.toStdString());
            }
            break;
        case PetActionType::Talk:
            // 仅说话，不强制打断当前移动/坐姿
            break;
    }
}

void ShijimaWidget::setWaitingForAgent(bool waiting) {
    m_isWaitingForAgent = waiting;
    if (waiting && m_mascot && m_mascot->state) {
        auto spinBehavior = m_mascot->initial_behavior_list().find("SitAndSpinHead", false);
        if (spinBehavior != nullptr) {
            m_mascot->next_behavior("SitAndSpinHead");
        } else {
            m_mascot->next_behavior("SitDown");
        }
        updateOffsets();
        repaint();
    }
}

void ShijimaWidget::onTranslateRequested(QString const& text) {
    if (text.trimmed().isEmpty()) return;

    setWaitingForAgent(true);
    showMessage("🔍 正在翻译...", 0, "", true);

    AgentService::instance()->translate(text, [this](bool success, QString const& result) {
        setWaitingForAgent(false);
        if (success) {
            BehaviorEngine::instance()->addAffection(2, 5);
            showMessage(result, 25000, "", true);
        } else {
            showMessage("❌ " + result, 8000, "", true);
        }
    });
}

void ShijimaWidget::onAskRequested(QString const& text) {
    setWaitingForAgent(true);
    if (m_askDialog != nullptr) {
        m_askDialog->promptForContext(text);
    }
}

void ShijimaWidget::showMessageHistory() {
    if (m_messageBubble != nullptr) {
        m_messageBubble->showHistoryDialog();
    }
}

void ShijimaWidget::showTimerManager() {
    if (m_timerDialog == nullptr) {
        m_timerDialog = new TimerListDialog(m_windowedMode ? parentWidget() : nullptr);
    }
    m_timerDialog->refreshList();
    m_timerDialog->show();
    m_timerDialog->raise();
    m_timerDialog->activateWindow();
}

void ShijimaWidget::onQuestionSubmitted(QString const& context, QString const& question) {
    setWaitingForAgent(true);
    showMessage("🤔 正在分析问题...", 0, "", true);

    AgentService::instance()->ask(context, question,
        [this](QString const& progressMsg) {
            showMessage(progressMsg, 0, "", true);
        },
        [this](bool success, QString const& result, QString const& appTarget) {
            setWaitingForAgent(false);
            if (success) {
                BehaviorEngine::instance()->addAffection(3, 8);
                showMessage(result, 30000, appTarget, true);
            } else {
                showMessage("❌ " + result, 8000, appTarget, true);
            }
        });
}

QString ShijimaWidget::currentBehaviorName() const {
    if (m_mascot && m_mascot->active_behavior()) {
        return QString::fromStdString(m_mascot->active_behavior()->name);
    }
    return "";
}

void ShijimaWidget::moveToCorner(bool toLeftCorner) {
    if (!m_mascot || !m_mascot->state || !m_mascot->state->env) return;

    auto env = m_mascot->state->env;
    double targetX = toLeftCorner ? (env->screen.left + 120.0) : (env->screen.left + env->screen.width() - 140.0);
    double targetY = env->floor.y > 0 ? env->floor.y : env->screen.bottom;

    double currentX = m_mascot->state->anchor.x;
    double currentY = m_mascot->state->anchor.y;

    if (m_moveAnimation != nullptr) {
        m_moveAnimation->stop();
        delete m_moveAnimation;
        m_moveAnimation = nullptr;
    }

    m_mascot->state->looking_right = (targetX >= currentX);

    // 尝试播放慢步/漫步姿态
    auto walkBehavior = m_mascot->initial_behavior_list().find("WalkAlongWorkAreaFloor", false);
    if (walkBehavior != nullptr) {
        m_mascot->next_behavior("WalkAlongWorkAreaFloor");
    }

    double distance = std::hypot(targetX - currentX, targetY - currentY);
    int animDuration = std::clamp(static_cast<int>(distance * 1.5), 800, 2000); // 慢步回巢

    m_isRunningToCenter = true;
    m_moveAnimation = new QVariantAnimation(this);
    m_moveAnimation->setDuration(animDuration);
    m_moveAnimation->setStartValue(0.0);
    m_moveAnimation->setEndValue(1.0);
    m_moveAnimation->setEasingCurve(QEasingCurve::InOutQuad);

    connect(m_moveAnimation, &QVariantAnimation::valueChanged, this, [this, currentX, currentY, targetX, targetY](QVariant const& value) {
        double t = value.toDouble();
        if (m_mascot && m_mascot->state) {
            double curX = currentX + (targetX - currentX) * t;
            double curY = currentY + (targetY - currentY) * t;
            m_mascot->state->anchor = { curX, curY };
            updateOffsets();
            repaint();
        }
    });

    connect(m_moveAnimation, &QVariantAnimation::finished, this, [this, targetX, targetY]() {
        m_isRunningToCenter = false;
        if (m_mascot && m_mascot->state) {
            m_mascot->state->anchor = { targetX, targetY };
            auto dangleBehavior = m_mascot->initial_behavior_list().find("SitWhileDanglingLegs", false);
            if (dangleBehavior != nullptr) {
                m_mascot->next_behavior("SitWhileDanglingLegs");
            } else {
                auto sitBehavior = m_mascot->initial_behavior_list().find("SitDown", false);
                if (sitBehavior != nullptr) {
                    m_mascot->next_behavior("SitDown");
                }
            }
        }
        updateOffsets();
        repaint();
        if (m_moveAnimation != nullptr) {
            m_moveAnimation->deleteLater();
            m_moveAnimation = nullptr;
        }
    });

    m_moveAnimation->start();
}

ShijimaWidget::PetInterruptedGoal ShijimaWidget::detectCurrentGoal() {
    if (!m_mascot || !m_mascot->state) return PetInterruptedGoal::None;
    QString beh = currentBehaviorName();
    auto env = m_mascot->state->env;
    const auto &anchor = m_mascot->state->anchor;

    // 1. 爬墙 / 爬天花板中（离地高度超过 80px）
    if ((beh.contains("Climb", Qt::CaseInsensitive) || beh.contains("Ceiling", Qt::CaseInsensitive)) &&
        env && anchor.y < (env->floor.y - 80.0)) {
        return PetInterruptedGoal::ClimbingCeiling;
    }
    // 2. 跳跃 / 扑向 / 扔窗口
    if (beh.contains("Jump", Qt::CaseInsensitive) || beh.contains("ThrowIE", Qt::CaseInsensitive)) {
        return PetInterruptedGoal::JumpingWindow;
    }
    // 3. 站在窗口上巡逻
    if (beh.contains("IECeiling", Qt::CaseInsensitive) || beh.contains("IEWall", Qt::CaseInsensitive) ||
        beh.contains("EdgeOfIE", Qt::CaseInsensitive)) {
        return PetInterruptedGoal::StandingOnWindow;
    }
    // 4. 分裂或拔萝卜拉同伴
    if (beh.contains("PullUp", Qt::CaseInsensitive) || beh.contains("Split", Qt::CaseInsensitive)) {
        return PetInterruptedGoal::Breeding;
    }
    // 5. 快速狂奔
    if (beh.contains("Run", Qt::CaseInsensitive)) {
        return PetInterruptedGoal::RunningFast;
    }
    return PetInterruptedGoal::None;
}

void ShijimaWidget::triggerTantrum(PetInterruptedGoal goal) {
    if (goal == PetInterruptedGoal::None) return;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastTantrumTime < 4000) return; // 4秒防刷屏
    m_lastTantrumTime = now;

    // 心情微降，进入傲娇受挫状态
    BehaviorEngine::instance()->addAffection(0, -4);

    // 原地转头/坐下
    if (m_mascot) {
        auto spin = m_mascot->initial_behavior_list().find("SitAndSpinHead", false);
        if (spin != nullptr) {
            m_mascot->next_behavior("SitAndSpinHead");
        } else {
            auto sit = m_mascot->initial_behavior_list().find("SitDown", false);
            if (sit != nullptr) m_mascot->next_behavior("SitDown");
        }
    }

    QStringList quotes;
    switch (goal) {
    case PetInterruptedGoal::ClimbingCeiling:
        quotes = {
            "💢 呜哇！我明明快要爬到天花板上看风景了，你怎么抓我呀！(>д<)",
            "😤 坏蛋主人！差一点点就登顶了，全被你破坏啦！",
            "🥺 人家辛辛苦苦爬那么高，一秒钟回到解放前...哼！",
            "😤 讨厌！天花板上的风景我都还没看清呢！"
        };
        break;
    case PetInterruptedGoal::JumpingWindow:
        quotes = {
            "💢 呀！我刚看准角度准备跳上窗口的，手滑被你拎起来了！",
            "😤 抓我干嘛呀！我正准备大显身手跳上窗口呢！",
            "🥺 差一点点就跳上去啦！主人讨厌鬼～",
            "😠 呜...我的窗口起跳连招全被打乱啦！"
        };
        break;
    case PetInterruptedGoal::StandingOnWindow:
        quotes = {
            "💫 诶？！刚才那么大一个窗口去哪了？！是谁把它关掉的呀！",
            "😵 哎哟喂！立足点突然没了...屁股摔得好痛痛！(╥﹏╥)",
            "😤 哼！窗口怎么说没就没，害我直接掉下来啦！",
            "🥺 怎么脚下一空就摔地上了...呜呜呜..."
        };
        break;
    case PetInterruptedGoal::Breeding:
        quotes = {
            "💢 哎呀！我刚要把小伙伴从地里拔出来呢，被打断啦！",
            "😤 哼！把我的小伙伴还给我！"
        };
        break;
    case PetInterruptedGoal::RunningFast:
        quotes = {
            "💢 呼哧呼哧...我刚跑得正起劲呢！急刹车差点摔倒！😤",
            "😤 别挡道别挡道！我正赶着巡逻呢！"
        };
        break;
    default:
        break;
    }

    if (!quotes.isEmpty()) {
        QString speech = quotes[rand() % quotes.size()];
        showMessage(speech, 3500);
    }
}

void ShijimaWidget::checkWindowVanished() {
    if (!m_mascot || !m_mascot->state || !m_mascot->state->env) return;
    QString beh = currentBehaviorName();
    bool currentlyOnWindow = (beh.contains("IECeiling", Qt::CaseInsensitive) || 
                              beh.contains("IEWall", Qt::CaseInsensitive) ||
                              beh.contains("EdgeOfIE", Qt::CaseInsensitive));
    
    // 如果之前在窗口上，现在突然由于窗口被关闭/移走而进入下落状态
    if (m_wasOnWindow && !currentlyOnWindow && beh.contains("Fall", Qt::CaseInsensitive)) {
        m_wasOnWindow = false;
        triggerTantrum(PetInterruptedGoal::StandingOnWindow);
    } else {
        m_wasOnWindow = currentlyOnWindow;
    }
}

