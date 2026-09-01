#pragma once

#include <QWidget>
#include <QPointF>
#include <QTimer>
#include <QVector>

#include <QPixmap>

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
    double m_scale = 0.2;
    double m_targetScale = 1.0;
    double m_rotationAngle = 0.0;
    double m_glowPhase = 0.0;
    bool m_absorbing = false;
    double m_absorbProgress = 0.0;
    QTimer *m_animTimer = nullptr;
    QPixmap m_gargantuaPixmap;

    struct Particle {
        double angle;
        double radius;
        double speed;
        double size;
        QColor color;
    };
    QVector<Particle> m_particles;
    void initParticles();
};
