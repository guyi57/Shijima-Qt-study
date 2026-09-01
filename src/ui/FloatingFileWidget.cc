#include "FloatingFileWidget.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <cmath>

FloatingFileWidget::FloatingFileWidget(const QString &fileName, QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::NoDropShadowWindowHint),
      m_fileName(fileName)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_DeleteOnClose);
    resize(70, 75);
}

void FloatingFileWidget::spawnAt(const QPointF &pos) {
    move(pos.toPoint().x() - width() / 2, pos.toPoint().y() - height() / 2);
    m_opacity = 1.0;
    m_scale = 1.0;
    m_rotation = 0.0;
    m_tossing = false;
    show();
    update();
}

void FloatingFileWidget::attachTo(const QPointF &petPos, bool facingRight) {
    if (m_tossing) return;
    int offsetX = facingRight ? 35 : -55;
    int offsetY = 20;
    move(petPos.toPoint().x() + offsetX, petPos.toPoint().y() + offsetY);
    update();
}

void FloatingFileWidget::tossTo(const QPointF &targetTrashPos, std::function<void()> onFinished) {
    m_tossing = true;
    m_startPos = pos();
    m_targetPos = targetTrashPos;
    m_progress = 0.0;
    m_onTossFinished = onFinished;

    if (m_tossTimer) {
        m_tossTimer->stop();
        delete m_tossTimer;
    }

    m_tossTimer = new QTimer(this);
    connect(m_tossTimer, &QTimer::timeout, [this]() {
        m_progress += 0.04;
        if (m_progress >= 1.0) {
            m_progress = 1.0;
            m_tossTimer->stop();
            if (m_onTossFinished) {
                m_onTossFinished();
            }
            close();
            return;
        }

        double t = m_progress;
        // 抛物线运动: 水平线性插值，垂直加上向上弧度
        double curX = (1.0 - t) * m_startPos.x() + t * m_targetPos.x();
        double curY = (1.0 - t) * m_startPos.y() + t * m_targetPos.y() - 140.0 * std::sin(M_PI * t);

        move(static_cast<int>(curX), static_cast<int>(curY));

        m_scale = 1.0 - 0.7 * t;
        m_opacity = 1.0 - 0.8 * t;
        m_rotation = t * 360.0 * 1.5;

        update();
    });
    m_tossTimer->start(16); // ~60fps
}

void FloatingFileWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    painter.save();
    painter.translate(width() / 2.0, height() / 2.0);
    painter.scale(m_scale, m_scale);
    painter.rotate(m_rotation);
    painter.translate(-width() / 2.0, -height() / 2.0);
    painter.setOpacity(m_opacity);

    // 绘制可爱文件纸张卡片
    int cardW = 46;
    int cardH = 54;
    int x = (width() - cardW) / 2;
    int y = 4;

    QRectF cardRect(x, y, cardW, cardH);

    // 阴影与主体
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 40));
    painter.drawRoundedRect(cardRect.translated(0, 3), 6, 6);

    // 纸张背景渐变 (纯净白/浅蓝)
    QLinearGradient bgGrad(cardRect.topLeft(), cardRect.bottomRight());
    bgGrad.setColorAt(0.0, QColor(255, 255, 255, 245));
    bgGrad.setColorAt(1.0, QColor(240, 245, 255, 235));
    painter.setBrush(bgGrad);
    painter.setPen(QPen(QColor(200, 215, 235), 1.2));
    painter.drawRoundedRect(cardRect, 6, 6);

    // 折角效果 (右上角)
    QPainterPath foldPath;
    foldPath.moveTo(cardRect.right() - 10, cardRect.top());
    foldPath.lineTo(cardRect.right(), cardRect.top() + 10);
    foldPath.lineTo(cardRect.right() - 10, cardRect.top() + 10);
    foldPath.closeSubpath();
    painter.setBrush(QColor(210, 225, 245));
    painter.setPen(Qt::NoPen);
    painter.drawPath(foldPath);

    // 文件图标装饰
    painter.setFont(QFont("-apple-system", 14));
    painter.drawText(QRectF(x, y + 10, cardW, 20), Qt::AlignCenter, "🗑️");

    // 文件名标签
    painter.setFont(QFont("-apple-system", 7, QFont::Bold));
    painter.setPen(QColor(70, 80, 95));
    QFontMetrics fm(painter.font());
    QString elided = fm.elidedText(m_fileName, Qt::ElideMiddle, cardW - 4);
    painter.drawText(QRectF(x + 2, y + 34, cardW - 4, 14), Qt::AlignCenter, elided);

    painter.restore();
}
