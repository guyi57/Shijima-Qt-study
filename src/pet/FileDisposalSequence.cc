#include "FileDisposalSequence.hpp"
#include "ShijimaWidget.hpp"
#include "FloatingFileWidget.hpp"
#include "TrashTargetWidget.hpp"
#include "BehaviorEngine.hpp"
#include <QGuiApplication>
#include <QScreen>
#include <cmath>
#include <iostream>

#include <QFile>

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

void FileDisposalSequence::start(ShijimaWidget *pet, const QString &fileName, const QString &realFilePath) {
    if (!pet || m_running) return;

    m_pet = pet;
    m_fileName = fileName.isEmpty() ? "cache_trash.tmp" : fileName;
    m_realFilePath = realFilePath;
    m_running = true;
    m_stage = 0;
    m_stageTickCount = 0;

    auto screen = QGuiApplication::primaryScreen();
    QRect screenGeom = screen ? screen->geometry() : QRect(0, 0, 1920, 1080);

    int petScreenX = pet->x();
    int petScreenY = pet->y();

    // 智能就近判定：若桌宠在屏幕右侧则向左推，在左侧则向右推
    int screenMidX = screenGeom.left() + screenGeom.width() / 2;
    if (petScreenX > screenMidX) {
        m_pushingToRight = false; // 向左推
        double fileX = petScreenX - 70;
        double fileY = petScreenY + pet->height() / 2 - 10;
        m_fileSpawnPos = QPointF(fileX, fileY);

        double holeX = std::max<double>(screenGeom.left() + 60, fileX - 190);
        m_blackHolePos = QPointF(holeX, fileY);
    } else {
        m_pushingToRight = true; // 向右推
        double fileX = petScreenX + pet->width() + 10;
        double fileY = petScreenY + pet->height() / 2 - 10;
        m_fileSpawnPos = QPointF(fileX, fileY);

        double holeX = std::min<double>(screenGeom.right() - 60, fileX + 190);
        m_blackHolePos = QPointF(holeX, fileY);
    }

    // 1. 创建并高亮显示浮动文件挂件
    m_fileWidget = new FloatingFileWidget(m_fileName);
    m_fileWidget->spawnAt(m_fileSpawnPos);

    // 2. 在就近位置展开旋转的时空黑洞特效
    m_trashWidget = new TrashTargetWidget();
    m_trashWidget->showAt(m_blackHolePos);

    // 暂停日常自发漫游逻辑，转由本序列精细驱动
    pet->m_paused = true;
    pet->showMessage(QString("🕳️ 就近召唤时空黑洞！准备推入次元视界~"), 1800);

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

    // =========================================================================
    // 阶段 0: 趴地准备姿态 (Crawl/LieDown pose alignment)
    // =========================================================================
    if (m_stage == 0) {
        state->looking_right = m_pushingToRight;
        if (!m_pet->trySetBehavior("CrawlAlongWorkAreaFloor")) {
            if (!m_pet->trySetBehavior("CrawlAlongIECeiling")) {
                m_pet->trySetBehavior("SitDown");
            }
        }

        if (m_stageTickCount >= 8) { // 280ms 准备完毕
            m_stage = 1;
            m_stageTickCount = 0;
            m_pet->showMessage(m_pushingToRight ? "💪 趴地向前推进黑洞！>>>" : "<<< 💪 趴地向前推进黑洞！", 1500);
        }
    }
    // =========================================================================
    // 阶段 1: 趴地爬行推文件向黑洞移动 (Push while crawling)
    // =========================================================================
    else if (m_stage == 1) {
        state->looking_right = m_pushingToRight;
        if (!m_pet->trySetBehavior("CrawlAlongWorkAreaFloor")) {
            if (!m_pet->trySetBehavior("CrawlAlongIECeiling")) {
                m_pet->trySetBehavior("WalkAlongWorkAreaFloor");
            }
        }

        double pushSpeed = 4.8;
        state->anchor.x += (m_pushingToRight ? pushSpeed : -pushSpeed);

        // 将文件平移绑定在桌宠趴地推进的前方手/头部
        if (m_fileWidget) {
            int handX = m_pushingToRight ? (m_pet->x() + m_pet->width() - 10) : (m_pet->x() - 55);
            int handY = m_pet->y() + (m_pet->height() / 2) - 10;
            m_fileWidget->attachToScreen(QPointF(handX, handY));
        }

        // 判定是否推进到了黑洞事件视界边缘
        double curFileX = m_fileWidget ? m_fileWidget->pos().x() : m_pet->x();
        bool reachedHole = false;
        if (m_pushingToRight) {
            reachedHole = (curFileX >= m_blackHolePos.x() - 35);
        } else {
            reachedHole = (curFileX <= m_blackHolePos.x() + 15);
        }

        if (reachedHole || m_stageTickCount >= 65) {
            m_stage = 2;
            m_stageTickCount = 0;
        }
    }
    // =========================================================================
    // 阶段 2: 推入黑洞事件视界 (Toss into Black Hole vortex & absorb)
    // =========================================================================
    else if (m_stage == 2) {
        if (m_stageTickCount == 1) {
            if (!m_pet->trySetBehavior("WalkAndThrowIEFromRight")) {
                if (!m_pet->trySetBehavior("ThrowIEFromRight")) {
                    m_pet->trySetBehavior("SitAndSpinHead");
                }
            }
            // 触发文件旋转缩小并被黑洞引力吸入
            if (m_fileWidget) {
                m_fileWidget->tossTo(m_blackHolePos, [this]() {
                    if (m_trashWidget) {
                        m_trashWidget->playAbsorbEffect();
                    }
                    if (!m_realFilePath.isEmpty() && QFile::exists(m_realFilePath)) {
                        QFile::moveToTrash(m_realFilePath);
                    }
                    m_fileWidget = nullptr;
                });
            }
        }

        if (m_stageTickCount >= 20) { // 约 700ms 吞噬吸收完成
            m_stage = 3;
            m_stageTickCount = 0;
            if (!m_pet->trySetBehavior("SitWhileDanglingLegs")) {
                m_pet->trySetBehavior("SitAndFaceMouse");
            }
            m_pet->showMessage(QString("✨ 🕳️《%1》已被黑洞吞噬移入系统废纸篓！").arg(m_fileName), 3000);
        }
    }
    // =========================================================================
    // 阶段 3: 庆祝与收尾恢复
    // =========================================================================
    else if (m_stage == 3) {
        if (m_stageTickCount >= 28) {
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
