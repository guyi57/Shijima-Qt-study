#pragma once

#include <QWidget>
#include <QString>
#include <QPointF>
#include <QTimer>
#include <functional>

class FloatingFileWidget : public QWidget {
public:
    explicit FloatingFileWidget(const QString &fileName = "delete_me.tmp", QWidget *parent = nullptr);
    ~FloatingFileWidget() override = default;

    void spawnAt(const QPointF &pos);
    void attachToScreen(const QPointF &handPos);
    void tossTo(const QPointF &targetTrashPos, std::function<void()> onFinished = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_fileName;
    double m_opacity = 1.0;
    double m_scale = 1.0;
    double m_rotation = 0.0;
    bool m_tossing = false;

    QTimer *m_tossTimer = nullptr;
    QPointF m_startPos;
    QPointF m_targetPos;
    double m_progress = 0.0;
    std::function<void()> m_onTossFinished;
};
