// 
// Shijima-Qt - Cross-platform shimeji simulation app for desktop
// Copyright (C) 2025 pixelomer
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
// 

#include "MessageBubble.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>

MessageBubble::MessageBubble(QWidget *parent): QWidget(parent), m_hideTimer(nullptr)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | 
        Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    hide();
    
    m_hideTimer = new QTimer(this);
    connect(m_hideTimer, &QTimer::timeout, this, &MessageBubble::hideMessage);
}

void MessageBubble::showMessage(QString const& text, int duration)
{
    m_text = text;
    
    if (m_text.isEmpty()) {
        hide();
        return;
    }
    
    QFont font;
    font.setPointSize(10);
    QFontMetrics fm(font);
    
    int padding = 12;
    int maxWidth = 200;
    QRect textRect = fm.boundingRect(0, 0, maxWidth, 1000,
        Qt::TextWordWrap | Qt::AlignCenter, m_text);
    
    int bubbleWidth = textRect.width() + padding * 2;
    int bubbleHeight = textRect.height() + padding * 2 + 10;
    
    setFixedSize(bubbleWidth, bubbleHeight);
    
    show();
    raise();
    update();
    
    if (duration > 0) {
        m_hideTimer->start(duration);
    }
    else {
        m_hideTimer->stop();
    }
}

void MessageBubble::hideMessage()
{
    m_text.clear();
    m_hideTimer->stop();
    hide();
}

void MessageBubble::paintEvent(QPaintEvent *)
{
    if (m_text.isEmpty()) {
        return;
    }
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    int padding = 12;
    int tailSize = 10;
    int radius = 8;
    
    int bubbleWidth = width();
    int bubbleHeight = height() - tailSize;
    
    QPainterPath path;
    path.addRoundedRect(0, 0, bubbleWidth, bubbleHeight, radius, radius);
    
    QPolygon tail;
    tail << QPoint(bubbleWidth / 2 - tailSize / 2, bubbleHeight)
         << QPoint(bubbleWidth / 2, bubbleHeight + tailSize)
         << QPoint(bubbleWidth / 2 + tailSize / 2, bubbleHeight);
    path.addPolygon(tail);
    
    painter.fillPath(path, QColor(255, 255, 255, 230));
    painter.setPen(QPen(QColor(100, 100, 100), 2));
    painter.drawPath(path);
    
    QFont font;
    font.setPointSize(10);
    painter.setFont(font);
    painter.setPen(Qt::black);
    
    QRect textRect(padding, padding, bubbleWidth - padding * 2,
        bubbleHeight - padding * 2);
    painter.drawText(textRect, Qt::TextWordWrap | Qt::AlignCenter, m_text);
}
