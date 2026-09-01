#include "TrashTargetWidget.hpp"
#include "Platform/Platform.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <cmath>

TrashTargetWidget::TrashTargetWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_DeleteOnClose);
    setFocusPolicy(Qt::NoFocus);
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);

    Platform::setupFloatingBubbleWindow(this);
    resize(140, 140);
    initParticles();
}

void TrashTargetWidget::initParticles() {
    m_particles.clear();
    for (int i = 0; i < 24; ++i) {
        Particle p;
        p.angle = (i * 15.0) * M_PI / 180.0;
        p.radius = 20.0 + (QRandomGenerator::global()->bounded(35));
        p.speed = 0.04 + (QRandomGenerator::global()->bounded(30) / 1000.0);
        p.size = 2.0 + (QRandomGenerator::global()->bounded(3));
        int colorType = i % 3;
        if (colorType == 0) p.color = QColor(168, 85, 247, 220); // Neon purple
        else if (colorType == 1) p.color = QColor(99, 102, 241, 220); // Electric indigo
        else p.color = QColor(236, 72, 153, 220); // Hot pink
        m_particles.append(p);
    }
}

void TrashTargetWidget::showAt(const QPointF &pos) {
    move(pos.toPoint().x() - width() / 2, pos.toPoint().y() - height() / 2);
    m_opacity = 0.0;
    m_scale = 0.15;
    m_targetScale = 1.0;
    m_absorbing = false;
    m_rotationAngle = 0.0;
    show();
    raise();

    if (m_animTimer) {
        m_animTimer->stop();
        delete m_animTimer;
    }

    m_animTimer = new QTimer(this);
    connect(m_animTimer, &QTimer::timeout, [this]() {
        m_rotationAngle += 4.5;
        if (m_rotationAngle >= 360.0) m_rotationAngle -= 360.0;

        // 推进吸入粒子轨道
        for (auto &p : m_particles) {
            p.angle += p.speed * (m_absorbing ? 2.5 : 1.0);
            p.radius -= (m_absorbing ? 0.8 : 0.15);
            if (p.radius < 8.0) {
                p.radius = 50.0 + (QRandomGenerator::global()->bounded(10));
            }
        }

        if (!m_absorbing) {
            if (m_opacity < 0.98) {
                m_opacity += 0.08;
                m_scale += (m_targetScale - m_scale) * 0.15;
                if (m_opacity > 0.98) m_opacity = 0.98;
            }
        } else {
            m_absorbProgress += 0.05;
            if (m_absorbProgress < 0.4) {
                m_scale += 0.06; // 吞噬瞬间剧烈膨胀
            } else {
                m_scale -= 0.12; // 然后急剧坍缩
                m_opacity -= 0.10;
                if (m_scale <= 0.05 || m_opacity <= 0.0) {
                    m_animTimer->stop();
                    close();
                    return;
                }
            }
        }
        update();
    });
    m_animTimer->start(20);
}

void TrashTargetWidget::playAbsorbEffect() {
    m_absorbing = true;
    m_absorbProgress = 0.0;
    m_scale = 1.35;
    update();
}

void TrashTargetWidget::dismiss() {
    m_absorbing = true;
    m_absorbProgress = 0.5;
}

void TrashTargetWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    painter.save();
    QPointF center(width() / 2.0, height() / 2.0);
    painter.translate(center);
    painter.scale(m_scale, m_scale);
    painter.setOpacity(m_opacity);

    // 1. 引力透镜光晕底晕 (Gravitational Lensing Glow)
    double maxRadius = 58.0;
    QRadialGradient outerGlow(0, 0, maxRadius);
    outerGlow.setColorAt(0.0, QColor(147, 51, 234, 180));
    outerGlow.setColorAt(0.5, QColor(79, 70, 229, 120));
    outerGlow.setColorAt(0.85, QColor(15, 23, 42, 60));
    outerGlow.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.setBrush(outerGlow);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), maxRadius, maxRadius);

    // 2. 旋转吸积盘涡流 (Rotating Accretion Disk Vortex)
    painter.save();
    painter.rotate(m_rotationAngle);

    for (int i = 0; i < 3; ++i) {
        painter.rotate(120.0);
        QLinearGradient diskGrad(-45, -10, 45, 10);
        diskGrad.setColorAt(0.0, QColor(236, 72, 153, 0));
        diskGrad.setColorAt(0.3, QColor(168, 85, 247, 210));
        diskGrad.setColorAt(0.7, QColor(99, 102, 241, 210));
        diskGrad.setColorAt(1.0, QColor(56, 189, 248, 0));

        painter.setBrush(diskGrad);
        painter.drawEllipse(QRectF(-48, -12, 96, 24));
    }
    painter.restore();

    // 3. 绘制围绕吸入的暗物质与发光粒子
    for (const auto &p : m_particles) {
        double px = p.radius * std::cos(p.angle);
        double py = (p.radius * 0.65) * std::sin(p.angle);
        painter.setBrush(p.color);
        painter.drawEllipse(QPointF(px, py), p.size, p.size);
    }

    // 4. 事件视界极黑核心 (Event Horizon Void Core)
    double coreRadius = 22.0;
    QRadialGradient coreGrad(0, 0, coreRadius);
    coreGrad.setColorAt(0.0, QColor(0, 0, 0, 255));
    coreGrad.setColorAt(0.75, QColor(10, 10, 18, 255));
    coreGrad.setColorAt(0.92, QColor(126, 34, 206, 240));
    coreGrad.setColorAt(1.0, QColor(192, 132, 252, 255));

    painter.setBrush(coreGrad);
    painter.setPen(QPen(QColor(216, 180, 254, 200), 1.5));
    painter.drawEllipse(QPointF(0, 0), coreRadius, coreRadius);

    // 5. 奇点微光与中心粒子
    painter.setBrush(QColor(255, 255, 255, 220));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), 2.5, 2.5);

    painter.restore();
}
