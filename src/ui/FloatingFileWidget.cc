#include "FloatingFileWidget.hpp"
#include "Platform/Platform.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <cmath>

FloatingFileWidget::FloatingFileWidget(const QString &fileName, QWidget *parent)
    : QWidget(parent),
      m_fileName(fileName)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_DeleteOnClose);
    setFocusPolicy(Qt::NoFocus);
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);

    Platform::setupFloatingBubbleWindow(this);
    resize(70, 75);
}

void FloatingFileWidget::spawnAt(const QPointF &pos) {
    move(pos.toPoint().x() - width() / 2, pos.toPoint().y() - height() / 2);
    m_opacity = 1.0;
    m_scale = 1.0;
    m_rotation = 0.0;
    m_tossing = false;
    show();
    raise();
    Platform::setupFloatingBubbleWindow(this);
    update();
}

void FloatingFileWidget::attachToScreen(const QPointF &handPos) {
    if (m_tossing) return;
    move(handPos.toPoint().x(), handPos.toPoint().y());
    if (!isVisible()) {
        show();
        raise();
    }
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

        m_scale = 1.0 - 0.65 * t;
        m_opacity = 1.0 - 0.75 * t;
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
    int cardW = 50;
    int cardH = 58;
    int x = (width() - cardW) / 2;
    int y = 6;

    QRectF cardRect(x, y, cardW, cardH);

    // 阴影与主体
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 50));
    painter.drawRoundedRect(cardRect.translated(0, 3), 7, 7);

    // 纸张背景渐变 (纯净白/浅蓝)
    QLinearGradient bgGrad(cardRect.topLeft(), cardRect.bottomRight());
    bgGrad.setColorAt(0.0, QColor(255, 255, 255, 250));
    bgGrad.setColorAt(1.0, QColor(235, 245, 255, 240));
    painter.setBrush(bgGrad);
    painter.setPen(QPen(QColor(180, 205, 235), 1.5));
    painter.drawRoundedRect(cardRect, 7, 7);

    // 折角效果 (右上角)
    QPainterPath foldPath;
    foldPath.moveTo(cardRect.right() - 12, cardRect.top());
    foldPath.lineTo(cardRect.right(), cardRect.top() + 12);
    foldPath.lineTo(cardRect.right() - 12, cardRect.top() + 12);
    foldPath.closeSubpath();
    painter.setBrush(QColor(200, 220, 245));
    painter.setPen(Qt::NoPen);
    painter.drawPath(foldPath);

    // 文件图标装饰
    painter.setFont(QFont("-apple-system", 16));
    painter.drawText(QRectF(x, y + 10, cardW, 22), Qt::AlignCenter, "🗑️");

    // 文件名标签
    painter.setFont(QFont("-apple-system", 8, QFont::Bold));
    painter.setPen(QColor(50, 65, 85));
    QFontMetrics fm(painter.font());
    QString elided = fm.elidedText(m_fileName, Qt::ElideMiddle, cardW - 4);
    painter.drawText(QRectF(x + 2, y + 36, cardW - 4, 16), Qt::AlignCenter, elided);

    painter.restore();
}
