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

void FileDisposalSequence::start(ShijimaWidget *pet, const QString &fileName, const QString &realFilePath, const QPointF &customSpawnPos) {
    if (!pet || m_running) return;

    m_pet = pet;
    m_fileName = fileName.isEmpty() ? "cache_trash.tmp" : fileName;
    m_realFilePath = realFilePath;
    m_running = true;
    m_stage = 0;
    m_stageTickCount = 0;

    auto screen = QGuiApplication::primaryScreen();
    QRect screenGeom = screen ? screen->geometry() : QRect(0, 0, 1920, 1080);

    // 1. 确定垃圾文件的真实屏幕坐标
    double fileX = 0;
    double fileY = 0;

    if (customSpawnPos.x() > 0 && customSpawnPos.y() > 0) {
        fileX = customSpawnPos.x();
        fileY = customSpawnPos.y();
    } else {
        // 尝试使用当前鼠标光标位置（用户刚刚操作的位置）
        QPoint cursorPos = QCursor::pos();
        if (screenGeom.contains(cursorPos)) {
            fileX = cursorPos.x();
            fileY = cursorPos.y();
        } else {
            // 默认屏幕中央区域
            fileX = screenGeom.left() + screenGeom.width() * 0.55;
            fileY = screenGeom.top() + screenGeom.height() * 0.65;
        }
    }

    // 边界安全收缩
    fileX = std::max<double>(screenGeom.left() + 180, std::min<double>(screenGeom.right() - 180, fileX));
    fileY = std::max<double>(screenGeom.top() + 120, std::min<double>(screenGeom.bottom() - 120, fileY));
    m_fileSpawnPos = QPointF(fileX, fileY);

    // 2. 智能就近撕裂黑洞：根据文件所在屏幕左右半区决定黑洞方位
    int screenMidX = screenGeom.left() + screenGeom.width() / 2;
    if (fileX > screenMidX) {
        m_pushingToRight = false; // 向左推
        double holeX = fileX - 175;
        m_blackHolePos = QPointF(holeX, fileY);
        m_petTargetPos = QPointF(fileX + 65, fileY);
    } else {
        m_pushingToRight = true; // 向右推
        double holeX = fileX + 175;
        m_blackHolePos = QPointF(holeX, fileY);
        m_petTargetPos = QPointF(fileX - (pet->width() + 15), fileY);
    }

    // 3. 在文件位置展开浮动文件挂件
    m_fileWidget = new FloatingFileWidget(m_fileName);
    m_fileWidget->spawnAt(m_fileSpawnPos);

    // 4. 在文件侧方撕裂黑洞
    m_trashWidget = new TrashTargetWidget();
    m_trashWidget->showAt(m_blackHolePos);

    // 暂停日常自发漫游逻辑，接管物理引力控制
    pet->m_paused = true;
    if (pet->mascot().state) {
        pet->mascot().state->dragging = true;
    }
    pet->showMessage(QString("👀 发现待处理文件！正在全速赶往现场~"), 2000);

    // 启动序列定时器 (30ms = ~33fps 丝滑刷新)
    m_tickTimer->start(30);
}

void FileDisposalSequence::step() {
    if (!m_pet || !m_running) {
        finish();
        return;
    }

    auto state = m_pet->mascot().state;
    if (!state) {
        finish();
        return;
    }

    state->dragging = true; // 持续锁定物理引擎，防止误判下落
    m_stageTickCount++;

    // =========================================================================
    // 阶段 0: 跨屏幕全速奔跑/寻路接近文件位置 (Approach target file)
    // =========================================================================
    if (m_stage == 0) {
        double curPetX = m_pet->x();
        double curPetY = m_pet->y();
        double dx = m_petTargetPos.x() - curPetX;
        double dy = m_petTargetPos.y() - curPetY;

        state->looking_right = (dx > 0);
        if (!m_pet->trySetBehavior("RunAlongWorkAreaFloor")) {
            if (!m_pet->trySetBehavior("WalkAlongWorkAreaFloor")) {
                m_pet->trySetBehavior("ChaseMouse");
            }
        }

        // 高速奔跑位移推进
        double stepSpeedX = std::min(16.0, std::abs(dx));
        double stepSpeedY = std::min(14.0, std::abs(dy));
        if (std::abs(dx) > 1.0) state->anchor.x += (dx > 0 ? stepSpeedX : -stepSpeedX);
        if (std::abs(dy) > 1.0) state->anchor.y += (dy > 0 ? stepSpeedY : -stepSpeedY);

        bool reachedPetTarget = (std::abs(dx) <= 18.0 && std::abs(dy) <= 25.0);
        if (reachedPetTarget || m_stageTickCount >= 85) { // 约 2.5s 超时强制进入就位
            m_stage = 1;
            m_stageTickCount = 0;
            state->anchor.y = m_petTargetPos.y();
            m_pet->showMessage(m_pushingToRight ? "💪 趴地向前推进黑洞！>>>" : "<<< 💪 趴地向前推进黑洞！", 1500);
        }
    }
    // =========================================================================
    // 阶段 1: 趴地准备姿态 (Crawl/LieDown pose alignment)
    // =========================================================================
    else if (m_stage == 1) {
        state->looking_right = m_pushingToRight;
        state->anchor.y = m_petTargetPos.y();
        if (!m_pet->trySetBehavior("CrawlAlongWorkAreaFloor")) {
            if (!m_pet->trySetBehavior("CrawlAlongIECeiling")) {
                m_pet->trySetBehavior("SitDown");
            }
        }

        if (m_stageTickCount >= 6) { // 180ms 准备完毕
            m_stage = 2;
            m_stageTickCount = 0;
        }
    }
    // =========================================================================
    // 阶段 2: 趴地爬行推文件向黑洞移动 (Push while crawling)
    // =========================================================================
    else if (m_stage == 2) {
        state->looking_right = m_pushingToRight;
        state->anchor.y = m_petTargetPos.y();
        if (!m_pet->trySetBehavior("CrawlAlongWorkAreaFloor")) {
            if (!m_pet->trySetBehavior("CrawlAlongIECeiling")) {
                m_pet->trySetBehavior("WalkAlongWorkAreaFloor");
            }
        }

        double pushSpeed = 5.2;
        state->anchor.x += (m_pushingToRight ? pushSpeed : -pushSpeed);

        // 将文件平移绑定在桌宠趴地推进的前方
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
            m_stage = 3;
            m_stageTickCount = 0;
        }
    }
    // =========================================================================
    // 阶段 3: 推入黑洞事件视界 (Toss into Black Hole vortex & absorb)
    // =========================================================================
    else if (m_stage == 3) {
        state->anchor.y = m_petTargetPos.y();
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

        if (m_stageTickCount >= 22) { // 约 660ms 吞噬吸收完成
            m_stage = 4;
            m_stageTickCount = 0;
            if (!m_pet->trySetBehavior("SitWhileDanglingLegs")) {
                m_pet->trySetBehavior("SitAndFaceMouse");
            }
            m_pet->showMessage(QString("✨ 🕳️《%1》已被黑洞吞噬移入系统废纸篓！").arg(m_fileName), 3000);
        }
    }
    // =========================================================================
    // 阶段 4: 庆祝与收尾恢复
    // =========================================================================
    else if (m_stage == 4) {
        state->anchor.y = m_petTargetPos.y();
        if (m_stageTickCount >= 28) {
            finish();
            return;
        }
    }

    // 核心保证：每一帧都推进底层动画、锁定坐标并重绘
    if (m_pet) {
        m_pet->mascot().tick();
        if (m_stage >= 1 && m_stage <= 4 && m_pet->mascot().state) {
            m_pet->mascot().state->anchor.y = m_petTargetPos.y();
        }
        m_pet->updateOffsets();
        m_pet->repaint();
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

    if (m_trashWidget) {
        m_trashWidget->dismiss();
        m_trashWidget = nullptr;
    }

    if (m_pet) {
        if (m_pet->mascot().state) {
            m_pet->mascot().state->dragging = false;
        }
        m_pet->m_paused = false;
        m_pet = nullptr;
    }
}
