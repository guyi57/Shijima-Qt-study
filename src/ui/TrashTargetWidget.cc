#include "TrashTargetWidget.hpp"
#include "BlackHoleAsset.hpp"
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
    resize(240, 160);

    // 加载嵌入的超清卡冈图雅黑洞资产
    m_gargantuaPixmap.loadFromData(blackhole_gargantua_png, blackhole_gargantua_png_len);

    initParticles();
}

void TrashTargetWidget::initParticles() {
    m_particles.clear();
    for (int i = 0; i < 28; ++i) {
        Particle p;
        p.angle = (i * 12.8) * M_PI / 180.0;
        p.radius = 28.0 + (QRandomGenerator::global()->bounded(55));
        p.speed = 0.035 + (QRandomGenerator::global()->bounded(30) / 1000.0);
        p.size = 1.5 + (QRandomGenerator::global()->bounded(3));
        int colorType = i % 4;
        if (colorType == 0) p.color = QColor(254, 240, 138, 230); // Bright gold
        else if (colorType == 1) p.color = QColor(245, 158, 11, 230); // Molten amber
        else if (colorType == 2) p.color = QColor(255, 255, 255, 240); // Pure photon white
        else p.color = QColor(236, 72, 153, 200); // Hot accretion pink
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
    m_glowPhase = 0.0;
    show();
    raise();
    Platform::setupFloatingBubbleWindow(this);

    if (m_animTimer) {
        m_animTimer->stop();
        delete m_animTimer;
    }

    m_animTimer = new QTimer(this);
    connect(m_animTimer, &QTimer::timeout, [this]() {
        m_glowPhase += 0.08;
        m_rotationAngle += 3.5;
        if (m_rotationAngle >= 360.0) m_rotationAngle -= 360.0;

        // 推进吸入粒子轨道
        for (auto &p : m_particles) {
            p.angle += p.speed * (m_absorbing ? 3.0 : 1.0);
            p.radius -= (m_absorbing ? 1.0 : 0.18);
            if (p.radius < 12.0) {
                p.radius = 70.0 + (QRandomGenerator::global()->bounded(15));
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
            if (m_absorbProgress < 0.35) {
                m_scale += 0.08; // 吞噬瞬间剧烈黄金辉光膨胀
            } else {
                m_scale -= 0.14; // 然后急剧坍缩入奇点
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
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    painter.save();
    QPointF center(width() / 2.0, height() / 2.0);
    painter.translate(center);
    painter.scale(m_scale, m_scale);
    painter.setOpacity(m_opacity);

    // 1. 卡冈图雅黄金吸积盘辉光底晕 (Accretion Disc Ambient Corona Glow)
    double glowSize = 95.0 + 5.0 * std::sin(m_glowPhase);
    QRadialGradient outerGlow(0, 0, glowSize);
    outerGlow.setColorAt(0.0, QColor(254, 240, 138, m_absorbing ? 230 : 160));
    outerGlow.setColorAt(0.35, QColor(245, 158, 11, m_absorbing ? 180 : 110));
    outerGlow.setColorAt(0.7, QColor(180, 83, 9, 50));
    outerGlow.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.setBrush(outerGlow);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), glowSize, glowSize * 0.7);

    // 2. 绘制高清《星际穿越》卡冈图雅黑洞本体 (Gargantua Ultra-HD Texture)
    if (!m_gargantuaPixmap.isNull()) {
        int imgW = 210;
        int imgH = static_cast<int>(imgW * (560.0 / 880.0)); // 保持 1.57:1 比例
        QRectF drawRect(-imgW / 2.0, -imgH / 2.0, imgW, imgH);
        painter.drawPixmap(drawRect.toRect(), m_gargantuaPixmap);
    }

    // 3. 动态绘制围绕吸入的黄金与星光光子粒子 (Orbiting Photon Particles)
    for (const auto &p : m_particles) {
        double px = p.radius * std::cos(p.angle);
        double py = (p.radius * 0.42) * std::sin(p.angle); // 椭圆吸积盘倾角
        painter.setBrush(p.color);
        painter.drawEllipse(QPointF(px, py), p.size, p.size);
    }

    // 4. 事件视界中心深空奇点微光
    if (m_absorbing) {
        painter.setBrush(QColor(255, 255, 255, 240));
        painter.drawEllipse(QPointF(0, 0), 4.0, 4.0);
    }

    painter.restore();
}
