#include "FileDisposalSequence.hpp"
#include "ShijimaWidget.hpp"
#include "FloatingFileWidget.hpp"
#include "BehaviorEngine.hpp"
#include <QGuiApplication>
#include <QScreen>
#include <cmath>
#include <iostream>

FileDisposalSequence* FileDisposalSequence::instance() {
    static FileDisposalSequence s_instance;
    return &s_instance;
}

FileDisposalSequence::FileDisposalSequence() {
    m_tickTimer = new QTimer(this);
    connect(m_tickTimer, &QTimer::timeout, this, &FileDisposalSequence::step);
}

FileDisposalSequence::~FileDisposalSequence() {
    if (m_tickTimer) {
        m_tickTimer->stop();
    }
}

void FileDisposalSequence::start(ShijimaWidget *pet, const QString &fileName) {
    if (!pet || m_running) return;

    m_pet = pet;
    m_fileName = fileName.isEmpty() ? "cache_trash.tmp" : fileName;
    m_running = true;
    m_stage = 0;
    m_stageTickCount = 0;

    auto screen = QGuiApplication::primaryScreen();
    QRect screenGeom = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    // 确定垃圾桶位置 (通常在屏幕右下角)
    m_trashPos = QPointF(screenGeom.right() - 80, screenGeom.bottom() - 40);

    // 确定文件刷新位置 (桌宠当前位置旁边一段距离，或屏幕中左侧)
    double currentX = pet->mascot().state->anchor.x;
    double currentY = pet->mascot().state->anchor.y;

    double fileX = currentX - 260;
    if (fileX < screenGeom.left() + 100) {
        fileX = currentX + 260;
    }
    double fileY = screenGeom.bottom() - 20;

    m_fileSpawnPos = QPointF(fileX, fileY);

    // 创建浮动文件挂件
    m_fileWidget = new FloatingFileWidget(m_fileName);
    m_fileWidget->spawnAt(m_fileSpawnPos);

    // 暂停日常自主行为干涉
    pet->m_paused = true;
    pet->showMessage(QString("🔍 发现待清理文件《%1》！").arg(m_fileName), 2000);

    // 启动序列定时器 (40ms = 25fps)
    m_tickTimer->start(40);
}

void FileDisposalSequence::step() {
    if (!m_pet || !m_running) {
        finish();
        return;
    }

    auto state = m_pet->mascot().state;
    m_stageTickCount++;

    // =========================================================================
    // 阶段 0: 跑向文件 (Dash / Walk to file)
    // =========================================================================
    if (m_stage == 0) {
        double diffX = m_fileSpawnPos.x() - state->anchor.x;
        bool movingRight = (diffX > 0);
        state->looking_right = movingRight;

        if (std::abs(diffX) > 25.0) {
            // 设置跑/走动作
            m_pet->trySetBehavior("WalkAlongWorkAreaFloor");
            double stepSpeed = 6.0;
            state->anchor.x += (movingRight ? stepSpeed : -stepSpeed);
            // 贴近地面
            state->anchor.y = m_fileSpawnPos.y();
        } else {
            // 到达文件旁，进入阶段 1 (拾取)
            m_stage = 1;
            m_stageTickCount = 0;
            m_pet->trySetBehavior("SitDown");
        }
    }
    // =========================================================================
    // 阶段 1: 俯身触碰与抓取 (SitDown 停顿 300ms)
    // =========================================================================
    else if (m_stage == 1) {
        m_pet->trySetBehavior("SitDown");
        if (m_stageTickCount >= 8) { // ~320ms
            m_stage = 2;
            m_stageTickCount = 0;
            m_pet->showMessage("💪 抓住啦！开始推向垃圾桶~", 1500);
        }
    }
    // =========================================================================
    // 阶段 2: 搬运/推向垃圾桶 (PushWindow / Walk with File attached)
    // =========================================================================
    else if (m_stage == 2) {
        double diffX = m_trashPos.x() - state->anchor.x;
        bool movingRight = (diffX > 0);
        state->looking_right = movingRight;

        // 尝试使用经典推窗动作，如果不存在则使用行走
        if (!m_pet->trySetBehavior("WalkAndGrabBottomRightWall")) {
            m_pet->trySetBehavior("WalkAlongWorkAreaFloor");
        }

        double pushSpeed = 4.5;
        state->anchor.x += (movingRight ? pushSpeed : -pushSpeed);
        state->anchor.y = m_trashPos.y();

        // 将文件图标绑定在身前
        if (m_fileWidget) {
            m_fileWidget->attachTo(QPointF(state->anchor.x, state->anchor.y), movingRight);
        }

        if (std::abs(diffX) <= 60.0) {
            // 到达垃圾桶旁，进入阶段 3 (扔出)
            m_stage = 3;
            m_stageTickCount = 0;
        }
    }
    // =========================================================================
    // 阶段 3: 扔进垃圾桶 (ThrowWindow 挥臂甩出)
    // =========================================================================
    else if (m_stage == 3) {
        if (m_stageTickCount == 1) {
            if (!m_pet->trySetBehavior("WalkAndThrowIEFromRight")) {
                if (!m_pet->trySetBehavior("ThrowIEFromRight")) {
                    m_pet->trySetBehavior("SitAndSpinHead");
                }
            }
            // 触发抛物线下落动画
            if (m_fileWidget) {
                m_fileWidget->tossTo(m_trashPos, [this]() {
                    // 抛物线落地
                    m_fileWidget = nullptr;
                });
            }
        }

        if (m_stageTickCount >= 18) { // 约 700ms 动画播放完成
            m_stage = 4;
            m_stageTickCount = 0;
            if (!m_pet->trySetBehavior("SitWhileDanglingLegs")) {
                m_pet->trySetBehavior("SitAndFaceMouse");
            }
            m_pet->showMessage(QString("✨ 呼~ 成功将《%1》扔进回收站啦！🧹").arg(m_fileName), 3000);
        }
    }
    // =========================================================================
    // 阶段 4: 庆祝与收尾恢复
    // =========================================================================
    else if (m_stage == 4) {
        if (m_stageTickCount >= 25) { // 1秒后收尾
            finish();
        }
    }
}

void FileDisposalSequence::finish() {
    m_tickTimer->stop();
    m_running = false;
    m_stage = 0;
    m_stageTickCount = 0;

    if (m_fileWidget) {
        m_fileWidget->close();
        m_fileWidget = nullptr;
    }

    if (m_pet) {
        m_pet->m_paused = false;
        m_pet = nullptr;
    }
}
