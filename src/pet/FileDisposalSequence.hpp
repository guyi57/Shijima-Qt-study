#pragma once

#include <QObject>
#include <QString>
#include <QPointF>
#include <QTimer>
#include <QPointer>

class ShijimaWidget;
class FloatingFileWidget;
class TrashTargetWidget;

class FileDisposalSequence : public QObject {
public:
    static FileDisposalSequence* instance();

    // 启动搬运并删除文件动画序列
    void start(ShijimaWidget *pet, const QString &fileName = "garbage.tmp", const QString &realFilePath = "");

    bool isRunning() const { return m_running; }

private:
    FileDisposalSequence();
    ~FileDisposalSequence();

    void step();
    void finish();

    bool m_running = false;
    QPointer<ShijimaWidget> m_pet;
    QPointer<FloatingFileWidget> m_fileWidget;
    QPointer<TrashTargetWidget> m_trashWidget;
    QString m_fileName;
    QString m_realFilePath;

    int m_stage = 0; // 0: align, 1: crawl push, 2: black hole swallow, 3: celebrate
    int m_stageTickCount = 0;
    bool m_pushingToRight = true;
    QPointF m_fileSpawnPos;
    QPointF m_blackHolePos;
    QTimer *m_tickTimer = nullptr;
};
