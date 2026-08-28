#pragma once

#include <QWidget>
#include <QTimer>
#include <QVariantAnimation>

class PetStatusBarWidget : public QWidget
{
public:
    explicit PetStatusBarWidget(QWidget *parent = nullptr);
    ~PetStatusBarWidget();

    // 状态更新 (脏标记驱动，数据变化时自动触发微光飘字与平滑进度刷新)
    void updateStatus(int stamina, int mood);

    // 智能避让气泡
    void setAvoidBubble(bool avoid);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_stamina = 85;
    int m_mood = 60;

    // 体力变动飘字动画
    QString m_staminaDeltaText;
    double m_staminaAnimProgress = 1.0;
    QVariantAnimation *m_staminaAnim = nullptr;

    // 心情变动飘字动画
    QString m_moodDeltaText;
    double m_moodAnimProgress = 1.0;
    QVariantAnimation *m_moodAnim = nullptr;

    bool m_avoidBubble = false;
};
