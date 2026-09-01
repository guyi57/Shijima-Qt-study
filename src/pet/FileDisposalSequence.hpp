#pragma once

#include <QObject>
#include <QString>
#include <QPointF>
#include <QTimer>

class ShijimaWidget;
class FloatingFileWidget;

class FileDisposalSequence : public QObject {
public:
    static FileDisposalSequence* instance();

    // 启动搬运并删除文件动画序列
    void start(ShijimaWidget *pet, const QString &fileName = "garbage.tmp");

    bool isRunning() const { return m_running; }

private:
    FileDisposalSequence();
    ~FileDisposalSequence();

    void step();
    void finish();

    bool m_running = false;
    ShijimaWidget *m_pet = nullptr;
    FloatingFileWidget *m_fileWidget = nullptr;
    QString m_fileName;

    int m_stage = 0; // 0: approach, 1: grab, 2: push, 3: throw, 4: celebrate
    int m_stageTickCount = 0;
    QPointF m_fileSpawnPos;
    QPointF m_trashPos;
    QTimer *m_tickTimer = nullptr;
};
