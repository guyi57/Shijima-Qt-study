#include "FileDisposalSequence.hpp"
#include "ShijimaWidget.hpp"
#include "FloatingFileWidget.hpp"
#include "TrashTargetWidget.hpp"
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
    QRect screenGeom = screen ? screen->geometry() : QRect(0, 0, 1920, 1080);

    // 屏幕右下角废纸篓真实像素位置 (高于屏幕底边 65px，避开 Dock 底栏)
    m_trashPos = QPointF(screenGeom.right() - 65, screenGeom.bottom() - 65);

    // 确定文件生成位置：优先在桌宠身旁一段距离生成
    int petScreenX = pet->x();
    int petScreenY = pet->y();
    double fileX = petScreenX - 220;
    if (fileX < screenGeom.left() + 100) {
        fileX = petScreenX + 220;
    }
    if (fileX > m_trashPos.x() - 180) {
        fileX = m_trashPos.x() - 240;
    }
    double fileY = petScreenY + pet->height() / 2;
    if (fileY > screenGeom.bottom() - 80) {
        fileY = screenGeom.bottom() - 80;
    }

    m_fileSpawnPos = QPointF(fileX, fileY);

    // 1. 创建并高亮显示浮动文件挂件
    m_fileWidget = new FloatingFileWidget(m_fileName);
    m_fileWidget->spawnAt(m_fileSpawnPos);

    // 2. 在右下角废纸篓位置点亮发光提示目标
    m_trashWidget = new TrashTargetWidget();
    m_trashWidget->showAt(m_trashPos);

    // 暂停日常自发漫游逻辑，转由本序列逐帧精细驱动
    pet->m_paused = true;
    pet->showMessage(QString("🗑️ 发现待删除文件《%1》！").arg(m_fileName), 1800);

    // 启动序列定时器 (35ms = ~28fps 丝滑刷新)
    m_tickTimer->start(35);
}

void FileDisposalSequence::step() {
    if (!m_pet || !m_running) {
        finish();
        return;
    }

    auto state = m_pet->mascot().state;
    m_stageTickCount++;

    // 实时计算桌宠真实屏幕坐标
    int petCurX = m_pet->x();

    // =========================================================================
    // 阶段 0: 跑向文件 (Dash / Walk to file)
    // =========================================================================
    if (m_stage == 0) {
        double diffX = m_fileSpawnPos.x() - (petCurX + m_pet->width() / 2);
        bool movingRight = (diffX > 0);
        state->looking_right = movingRight;

        if (std::abs(diffX) > 25.0) {
            m_pet->trySetBehavior("WalkAlongWorkAreaFloor");
            double stepSpeed = 6.0;
            state->anchor.x += (movingRight ? stepSpeed : -stepSpeed);
        } else {
            // 到达文件旁，进入阶段 1 (拾取/俯身抓稳)
            m_stage = 1;
            m_stageTickCount = 0;
            m_pet->trySetBehavior("SitDown");
        }
    }
    // =========================================================================
    // 阶段 1: 俯身触碰与抓取 (SitDown 停顿 280ms)
    // =========================================================================
    else if (m_stage == 1) {
        m_pet->trySetBehavior("SitDown");
        if (m_stageTickCount >= 8) { // 280ms
            m_stage = 2;
            m_stageTickCount = 0;
            m_pet->showMessage("💪 抓住啦！一路推去右下角废纸篓~", 1600);
        }
    }
    // =========================================================================
    // 阶段 2: 搬运/推向垃圾桶 (Push / Walk straight to Trash position)
    // =========================================================================
    else if (m_stage == 2) {
        // 桌宠目标坐标是垃圾桶左侧 70px
        double targetPetScreenX = m_trashPos.x() - 70.0;
        double diffX = targetPetScreenX - petCurX;
        bool movingRight = (diffX > 0);
        state->looking_right = movingRight;

        if (!m_pet->trySetBehavior("WalkAndGrabBottomRightWall")) {
            m_pet->trySetBehavior("WalkAlongWorkAreaFloor");
        }

        double pushSpeed = 5.0;
        state->anchor.x += (movingRight ? pushSpeed : -pushSpeed);

        // 逐帧精确将文件图标绑定在身前手部屏幕坐标
        if (m_fileWidget) {
            int handX = movingRight ? (m_pet->x() + m_pet->width() - 15) : (m_pet->x() - 55);
            int handY = m_pet->y() + (m_pet->height() / 2) - 15;
            m_fileWidget->attachToScreen(QPointF(handX, handY));
        }

        // 一路走到垃圾桶跟前
        if (diffX <= 15.0) {
            m_stage = 3;
            m_stageTickCount = 0;
        }
    }
    // =========================================================================
    // 阶段 3: 扔进垃圾桶 (ThrowWindow 挥臂甩出并吸收)
    // =========================================================================
    else if (m_stage == 3) {
        if (m_stageTickCount == 1) {
            if (!m_pet->trySetBehavior("WalkAndThrowIEFromRight")) {
                if (!m_pet->trySetBehavior("ThrowIEFromRight")) {
                    m_pet->trySetBehavior("SitAndSpinHead");
                }
            }
            // 触发抛物线下落动画并播放垃圾桶吞入特效
            if (m_fileWidget) {
                m_fileWidget->tossTo(m_trashPos, [this]() {
                    if (m_trashWidget) {
                        m_trashWidget->playAbsorbEffect();
                    }
                    m_fileWidget = nullptr;
                });
            }
        }

        if (m_stageTickCount >= 22) { // 约 770ms 甩出动画完成
            m_stage = 4;
            m_stageTickCount = 0;
            if (!m_pet->trySetBehavior("SitWhileDanglingLegs")) {
                m_pet->trySetBehavior("SitAndFaceMouse");
            }
            m_pet->showMessage(QString("✨ 呼~ 成功将《%1》扔进系统废纸篓啦！🧹").arg(m_fileName), 3000);
        }
    }
    // =========================================================================
    // 阶段 4: 庆祝与收尾恢复
    // =========================================================================
    else if (m_stage == 4) {
        if (m_stageTickCount >= 30) {
            finish();
            return;
        }
    }

    // 核心保证：每一帧都推进底层动画、真实平移窗口坐标并重绘
    m_pet->mascot().tick();
    m_pet->updateOffsets();
    m_pet->repaint();
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

    if (m_trashWidget) {
        m_trashWidget->dismiss();
        m_trashWidget = nullptr;
    }

    if (m_pet) {
        m_pet->m_paused = false;
        m_pet = nullptr;
    }
}
