#pragma once

#include <QWidget>
#include <QPointF>
#include <QTimer>

class TrashTargetWidget : public QWidget {
public:
    explicit TrashTargetWidget(QWidget *parent = nullptr);
    ~TrashTargetWidget() override = default;

    void showAt(const QPointF &pos);
    void playAbsorbEffect();
    void dismiss();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    double m_opacity = 0.0;
    double m_scale = 0.8;
    bool m_absorbing = false;
    double m_absorbProgress = 0.0;
    QTimer *m_animTimer = nullptr;
};
