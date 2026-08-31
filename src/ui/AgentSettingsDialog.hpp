#pragma once

// 
// guyi-bot - AI Model, Memory, Agent, Skills & MCP Settings Dialog
// 

#include <QDialog>
#include <QTabWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>

class AgentSettingsDialog : public QDialog
{
public:
    explicit AgentSettingsDialog(QWidget *parent = nullptr);
    void refreshValues();

private:
    void applyPreset(int index);
    void onPersonaChanged(int index);
    void saveAndClose();
    void testConnection();
    void testAipyConnection();
    void autoDetectAipyKey();

    void refreshSkillsTab();
    void refreshMcpTab();

    // 基础模型配置
    QComboBox *m_presetCombo;
    QLineEdit *m_apiBaseEdit;
    QLineEdit *m_apiKeyEdit;
    QLineEdit *m_modelEdit;
    QSpinBox *m_memoryTurnsSpin;
    QLineEdit *m_hotkeyTranslateEdit;
    QLineEdit *m_hotkeyAskEdit;
    QLineEdit *m_hotkeyMusicToggleEdit;
    QLineEdit *m_hotkeyMusicPlayPauseEdit;
    QLineEdit *m_hotkeyMusicNextEdit;
    QLineEdit *m_hotkeyMusicPrevEdit;
    QLineEdit *m_hotkeyMusicFavEdit;

    QPushButton *m_testBtn;
    QLabel *m_testStatusLabel;

    // 🎭 人格适配器 (Persona)
    QComboBox *m_personaCombo;
    QLabel *m_personaDescLabel;
    QTextEdit *m_customPromptEdit;

    // 🤖 Coding Agent 状态感知与 Token 省流开关
    QCheckBox *m_enableStateHookCheck;
    QCheckBox *m_enableLlmNarrationCheck;
    QSpinBox *m_stateDebounceSpin;

    // 智能体 Agent 适配器配置
    QComboBox *m_agentTypeCombo;
    QComboBox *m_routingModeCombo;
    QLineEdit *m_aipyBaseEdit;
    QLineEdit *m_aipyKeyEdit;
    QPushButton *m_autoDetectKeyBtn;
    QPushButton *m_testAipyBtn;
    QLabel *m_agentStatusLabel;

    // 🛠️ 技能库 (Skills)
    QListWidget *m_skillsListWidget;
    QLabel *m_skillDetailLabel;
    QPushButton *m_openSkillsDirBtn;
    QPushButton *m_refreshSkillsBtn;

    // 🔌 MCP 服务
    QListWidget *m_mcpListWidget;
    QLabel *m_mcpDetailLabel;
    QPushButton *m_openMcpConfigBtn;
    QPushButton *m_reloadMcpBtn;

    QPushButton *m_clearMemoryBtn;
    QPushButton *m_saveBtn;
    QPushButton *m_cancelBtn;
};
