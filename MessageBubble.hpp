#pragma once

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

#include <QWidget>
#include <QString>
#include <QTimer>

class QPaintEvent;

class MessageBubble : public QWidget
{
public:
    explicit MessageBubble(QWidget *parent = nullptr);
    void showMessage(QString const& text, int duration = 0);
    void hideMessage();
    bool hasMessage() const { return !m_text.isEmpty(); }
    QString const& message() const { return m_text; }
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    QString m_text;
    QTimer *m_hideTimer;
};
