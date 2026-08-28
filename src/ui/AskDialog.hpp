#pragma once

// 
// Shijima-Qt - Ask / Question Dialog
// 

#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <functional>

class AskDialog : public QDialog
{
public:
    explicit AskDialog(QWidget *parent = nullptr);
    void promptForContext(QString const& contextText);

    std::function<void(QString const& context, QString const& question)> onSubmit;
    std::function<void()> onCancel;

private:
    QString m_contextText;
    QLabel *m_titleLabel;
    QWidget *m_contextWidget = nullptr;
    QLabel *m_previewLabel = nullptr;
    QPushButton *m_clearContextBtn = nullptr;
    QLineEdit *m_inputEdit = nullptr;
    QPushButton *m_sendBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
};
