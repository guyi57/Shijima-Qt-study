#include "TrashTargetWidget.hpp"
#include <QPainter>
#include <QPainterPath>
#include <cmath>

TrashTargetWidget::TrashTargetWidget(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::NoDropShadowWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_DeleteOnClose);
    resize(100, 100);
}

void TrashTargetWidget::showAt(const QPointF &pos) {
    move(pos.toPoint().x() - width() / 2, pos.toPoint().y() - height() / 2);
    m_opacity = 0.0;
    m_scale = 0.8;
    m_absorbing = false;
    show();

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
    m_scale = 1.25; // 吞入瞬间放大
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
    painter.translate(-width() / 2.0, -height() / 2.0);
    painter.setOpacity(m_opacity);

    int w = 76;
    int h = 76;
    int x = (width() - w) / 2;
    int y = (height() - h) / 2;

    QRectF boxRect(x, y, w, h);

    // 发光外圈底晕
    QRadialGradient glow(boxRect.center(), w / 2.0);
    glow.setColorAt(0.0, QColor(255, 107, 107, 180));
    glow.setColorAt(0.7, QColor(255, 138, 138, 90));
    glow.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter.setBrush(glow);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(boxRect);

    // 核心玻璃质感圆形卡片
    QRectF cardRect = boxRect.adjusted(8, 8, -8, -8);
    QLinearGradient cardGrad(cardRect.topLeft(), cardRect.bottomRight());
    cardGrad.setColorAt(0.0, QColor(255, 255, 255, 240));
    cardGrad.setColorAt(1.0, QColor(250, 230, 230, 220));
    painter.setBrush(cardGrad);
    painter.setPen(QPen(QColor(255, 120, 120, 200), 1.8));
    painter.drawRoundedRect(cardRect, 14, 14);

    // 垃圾桶 Emoji 与提示
    painter.setFont(QFont("-apple-system", m_absorbing ? 22 : 19));
    painter.drawText(QRectF(cardRect.x(), cardRect.y() + 4, cardRect.width(), 30), Qt::AlignCenter, m_absorbing ? "🪣" : "🗑️");

    painter.setFont(QFont("-apple-system", 8, QFont::Bold));
    painter.setPen(QColor(230, 70, 70));
    painter.drawText(QRectF(cardRect.x(), cardRect.y() + 36, cardRect.width(), 16), Qt::AlignCenter, m_absorbing ? "已清除✨" : "系统废纸篓");

    painter.restore();
}
