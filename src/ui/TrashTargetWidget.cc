#include "TrashTargetWidget.hpp"
#include "Platform/Platform.hpp"
#include <QPainter>
#include <QPainterPath>
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
    resize(110, 110);
}

void TrashTargetWidget::showAt(const QPointF &pos) {
    move(pos.toPoint().x() - width() / 2, pos.toPoint().y() - height() / 2);
    m_opacity = 0.0;
    m_scale = 0.8;
    m_absorbing = false;
    show();
    raise();

    if (m_animTimer) {
        m_animTimer->stop();
        delete m_animTimer;
    }

    m_animTimer = new QTimer(this);
    connect(m_animTimer, &QTimer::timeout, [this]() {
        if (!m_absorbing) {
            if (m_opacity < 0.95) {
                m_opacity += 0.08;
                m_scale += 0.02;
                if (m_opacity > 0.95) m_opacity = 0.95;
                if (m_scale > 1.0) m_scale = 1.0;
            }
        } else {
            m_absorbProgress += 0.05;
            if (m_absorbProgress >= 1.0) {
                m_opacity -= 0.08;
                if (m_opacity <= 0.0) {
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
    m_scale = 1.3; // 吞入瞬间放大
    update();
}

void TrashTargetWidget::dismiss() {
    m_absorbing = true;
    m_absorbProgress = 1.0;
}

void TrashTargetWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    painter.save();
    painter.translate(width() / 2.0, height() / 2.0);
    painter.scale(m_scale, m_scale);
    painter.rotate(0);
    painter.translate(-width() / 2.0, -height() / 2.0);
    painter.setOpacity(m_opacity);

    int w = 84;
    int h = 84;
    int x = (width() - w) / 2;
    int y = (height() - h) / 2;

    QRectF boxRect(x, y, w, h);

    // 发光外圈底晕
    QRadialGradient glow(boxRect.center(), w / 2.0);
    glow.setColorAt(0.0, QColor(255, 107, 107, 200));
    glow.setColorAt(0.7, QColor(255, 138, 138, 100));
    glow.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter.setBrush(glow);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(boxRect);

    // 核心玻璃质感圆形卡片
    QRectF cardRect = boxRect.adjusted(8, 8, -8, -8);
    QLinearGradient cardGrad(cardRect.topLeft(), cardRect.bottomRight());
    cardGrad.setColorAt(0.0, QColor(255, 255, 255, 245));
    cardGrad.setColorAt(1.0, QColor(255, 235, 235, 230));
    painter.setBrush(cardGrad);
    painter.setPen(QPen(QColor(255, 110, 110, 220), 2.0));
    painter.drawRoundedRect(cardRect, 16, 16);

    // 垃圾桶 Emoji 与提示
    painter.setFont(QFont("-apple-system", m_absorbing ? 24 : 20));
    painter.drawText(QRectF(cardRect.x(), cardRect.y() + 6, cardRect.width(), 32), Qt::AlignCenter, m_absorbing ? "🪣" : "🗑️");

    painter.setFont(QFont("-apple-system", 9, QFont::Bold));
    painter.setPen(QColor(220, 50, 50));
    painter.drawText(QRectF(cardRect.x(), cardRect.y() + 40, cardRect.width(), 18), Qt::AlignCenter, m_absorbing ? "已清除✨" : "系统废纸篓");

    painter.restore();
}
