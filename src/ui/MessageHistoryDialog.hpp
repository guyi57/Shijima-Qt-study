#pragma once

#include <QDialog>
#include <QListWidget>
#include <QTextBrowser>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include "MessageHistoryManager.hpp"

#include <functional>

class MessageHistoryDialog : public QDialog
{
public:
    explicit MessageHistoryDialog(QWidget *parent = nullptr);
    ~MessageHistoryDialog() override = default;

    void refreshList();
    void selectLatest();

    void setOnReplayCallback(std::function<void(const QString &text, const QString &appTarget)> cb) {
        m_onReplayCallback = cb;
    }

private:
    void onItemSelectionChanged();
    void onSearchTextChanged(const QString &text);
    void onFilterTypeChanged(int index);
    void onCopyCurrentClicked();
    void onReplayCurrentClicked();
    void onClearAllClicked();

private:
    void setupUI();

    QLineEdit *m_searchEdit = nullptr;
    QComboBox *m_filterCombo = nullptr;
    QListWidget *m_listWidget = nullptr;
    QTextBrowser *m_previewBrowser = nullptr;
    QLabel *m_timeLabel = nullptr;
    QLabel *m_typeBadge = nullptr;
    QPushButton *m_copyBtn = nullptr;
    QPushButton *m_replayBtn = nullptr;
    QPushButton *m_clearAllBtn = nullptr;

    QList<MessageHistoryItem> m_currentItems;
    std::function<void(const QString &text, const QString &appTarget)> m_onReplayCallback;
};
