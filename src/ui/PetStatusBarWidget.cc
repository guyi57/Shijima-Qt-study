// 
// Shijima-Qt - High-Quality Dark Glassmorphic Follow Status Bar Widget
// 

#include "PetStatusBarWidget.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QGuiApplication>
#include <algorithm>
#include <cmath>

PetStatusBarWidget::PetStatusBarWidget(QWidget *parent)
    : QWidget(parent, Qt::ToolTip | Qt::FramelessWindowHint | 
                      Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

    setFixedSize(76, 46); // 小巧紧凑，刚好与人偶头顶宽度一致
    hide();
}

PetStatusBarWidget::~PetStatusBarWidget()
{
    if (m_staminaAnim) {
        m_staminaAnim->stop();
        delete m_staminaAnim;
    }
    if (m_moodAnim) {
        m_moodAnim->stop();
        delete m_moodAnim;
    }
}

void PetStatusBarWidget::setAvoidBubble(bool avoid)
{
    if (m_avoidBubble != avoid) {
        m_avoidBubble = avoid;
        update();
    }
}

void PetStatusBarWidget::updateStatus(int stamina, int mood)
{
    stamina = std::clamp(stamina, 0, 100);


    // 1. 检查体力变动 -> 触发左侧绿光飘字微动画 (防高频打断)
    int staminaDelta = stamina - m_stamina;
    if (staminaDelta != 0 && isVisible()) {
        if (m_staminaAnim == nullptr || m_staminaAnim->state() != QAbstractAnimation::Running) {
            m_staminaDeltaText = (staminaDelta > 0 ? QString("+%1").arg(staminaDelta) : QString::number(staminaDelta));
            m_staminaAnimProgress = 0.0;

            if (m_staminaAnim == nullptr) {
                m_staminaAnim = new QVariantAnimation(this);
                m_staminaAnim->setDuration(1200);
                m_staminaAnim->setStartValue(0.0);
                m_staminaAnim->setEndValue(1.0);
                m_staminaAnim->setEasingCurve(QEasingCurve::OutCubic);
                connect(m_staminaAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &val) {
                    m_staminaAnimProgress = val.toDouble();
                    update();
                });
            }
            m_staminaAnim->start();
        }
    }

    // 2. 检查心情变动 -> 触发右侧紫光飘字微动画 (防高频打断)
    int moodDelta = mood - m_mood;
    if (moodDelta != 0 && isVisible()) {
        if (m_moodAnim == nullptr || m_moodAnim->state() != QAbstractAnimation::Running) {
            m_moodDeltaText = (moodDelta > 0 ? QString("+%1").arg(moodDelta) : QString::number(moodDelta));
            m_moodAnimProgress = 0.0;

            if (m_moodAnim == nullptr) {
                m_moodAnim = new QVariantAnimation(this);
                m_moodAnim->setDuration(1200);
                m_moodAnim->setStartValue(0.0);
                m_moodAnim->setEndValue(1.0);
                m_moodAnim->setEasingCurve(QEasingCurve::OutCubic);
                connect(m_moodAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &val) {
                    m_moodAnimProgress = val.toDouble();
                    update();
                });
            }
            m_moodAnim->start();
        }
    }

    if (m_stamina != stamina || m_mood != mood) {
        m_stamina = stamina;
        m_mood = mood;
        update();
    }
}

void PetStatusBarWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    if (m_avoidBubble) {
        painter.setOpacity(0.15); // 气泡弹出时半透明淡化避让
    }

    const int circleSize = 26;
    const int circleY = 16;
    const int leftX = 6;
    const int rightX = 44;

    // ==========================================
    // 1. 左侧：翠绿体力环形圆盘 (Green Stamina Orb)
    // ==========================================
    QRect staminaCircleRect(leftX, circleY, circleSize, circleSize);
    
    // 深黑毛玻璃微透底圆
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(10, 15, 29, 230));
    painter.drawEllipse(staminaCircleRect);

    // 外圈底环 (暗深蓝灰)
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(30, 41, 59, 180), 2.2));
    painter.drawEllipse(staminaCircleRect.adjusted(1, 1, -1, -1));

    // 外圈发光进度圆弧 (翠绿渐变)
    double staminaSpan = (m_stamina / 100.0) * -360.0 * 16.0; // 顺时针
    if (m_stamina > 0) {
        QPen greenPen(QColor(74, 222, 128), 2.4, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(greenPen);
        painter.drawArc(staminaCircleRect.adjusted(1, 1, -1, -1), 90 * 16, static_cast<int>(staminaSpan));
    }

    // 中心闪电图标 (⚡)
    painter.setFont(QFont("-apple-system", 10, QFont::Bold));
    painter.setPen(QColor(74, 222, 128));
    painter.drawText(staminaCircleRect, Qt::AlignCenter, "⚡");

    // ==========================================
    // 2. 右侧：紫罗兰心情环形圆盘 (Purple Mood Orb)
    // ==========================================
    QRect moodCircleRect(rightX, circleY, circleSize, circleSize);

    // 深黑毛玻璃微透底圆
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(10, 15, 29, 230));
    painter.drawEllipse(moodCircleRect);

    // 外圈底环 (暗深蓝灰)
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(30, 41, 59, 180), 2.2));
    painter.drawEllipse(moodCircleRect.adjusted(1, 1, -1, -1));

    // 外圈发光进度圆弧 (紫粉渐变)
    int displayMood = std::clamp(m_mood < 0 ? (m_mood + 100) / 2 : (50 + m_mood / 2), 0, 100);
    double moodSpan = (displayMood / 100.0) * -360.0 * 16.0;
    if (displayMood > 0) {
        QPen purplePen(QColor(192, 132, 252), 2.4, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(purplePen);
        painter.drawArc(moodCircleRect.adjusted(1, 1, -1, -1), 90 * 16, static_cast<int>(moodSpan));
    }

    // 中心笑脸图标 (☻)
    painter.setFont(QFont("-apple-system", 11, QFont::Bold));
    painter.setPen(QColor(192, 132, 252));
    painter.drawText(moodCircleRect, Qt::AlignCenter, "☻");

    // ==========================================
    // 3. 头部漂浮微动画 (Floating Particle Numbers)
    // ==========================================
    // A. 左侧体力变动飘字
    if (m_staminaAnimProgress < 1.0 && !m_staminaDeltaText.isEmpty()) {
        painter.save();
        double opacity = 1.0 - m_staminaAnimProgress;
        double offsetY = 14.0 - m_staminaAnimProgress * 12.0; // 上浮 12px
        painter.setOpacity(opacity);
        painter.setFont(QFont("-apple-system", 9, QFont::Bold));
        
        QColor textColor = m_staminaDeltaText.startsWith("+") ? QColor(74, 222, 128) : QColor(251, 146, 60);
        painter.setPen(textColor);
        painter.drawText(leftX - 2, static_cast<int>(offsetY), m_staminaDeltaText);
        painter.restore();
    }

    // B. 右侧心情变动飘字
    if (m_moodAnimProgress < 1.0 && !m_moodDeltaText.isEmpty()) {
        painter.save();
        double opacity = 1.0 - m_moodAnimProgress;
        double offsetY = 14.0 - m_moodAnimProgress * 12.0;
        painter.setOpacity(opacity);
        painter.setFont(QFont("-apple-system", 9, QFont::Bold));
        
        QColor textColor = m_moodDeltaText.startsWith("+") ? QColor(192, 132, 252) : QColor(244, 63, 94);
        painter.setPen(textColor);
        painter.drawText(rightX - 2, static_cast<int>(offsetY), m_moodDeltaText);
        painter.restore();
    }
}
