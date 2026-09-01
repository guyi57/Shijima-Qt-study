#pragma once

#include <QDialog>
#include <QLabel>
#include <QTextBrowser>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "UpdateManager.hpp"

class UpdateDialog : public QDialog {
public:
    explicit UpdateDialog(const UpdateInfo &info, QWidget *parent = nullptr);
    ~UpdateDialog() override = default;

private:
    void startUpdate();

    UpdateInfo m_info;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_versionLabel = nullptr;
    QTextBrowser *m_changelogBrowser = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_statusLabel = nullptr;

    QPushButton *m_updateBtn = nullptr;
    QPushButton *m_browserBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
};
