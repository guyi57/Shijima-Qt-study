#pragma once

#include <QWidget>
#include <QString>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>

// 跨平台/macOS 前台活跃 App 检测
bool Platform_isAppFrontmost(QString const& appTarget);

class ScoreBadgeWidget : public QWidget
{
public:
    explicit ScoreBadgeWidget(QWidget *parent = nullptr);
    static void showDelta(const QPoint &globalPos, int moodDelta, int affectionDelta, int staminaDelta = 0);
    static void showStaminaDelta(const QPoint &globalPos, int staminaDelta);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void startAnimation(const QPoint &startPos, const QString &text, bool isPositive);

    QString m_text;
    bool m_isPositive = true;
    QPropertyAnimation *m_moveAnim = nullptr;
    QPropertyAnimation *m_opacityAnim = nullptr;
    QGraphicsOpacityEffect *m_opacityEffect = nullptr;
};
