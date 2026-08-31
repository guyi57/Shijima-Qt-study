// 
// Shijima-Qt - AI Model, Memory, Agent & Hotkey Settings Dialog Implementation
// 

#include "AgentSettingsDialog.hpp"
#include "AgentService.hpp"
#include "AipyAdapter.hpp"
#include "PersonaManager.hpp"
#include <QFormLayout>
#include <QMessageBox>
#include <QScreen>
#include <QGroupBox>
#include <QGuiApplication>
#include <QScrollArea>

AgentSettingsDialog::AgentSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("⚙️ 智能助理、角色人格与编程感知配置");
    setWindowFlags(Qt::Dialog | Qt::WindowStaysOnTopHint | Qt::WindowCloseButtonHint);
    setMinimumWidth(560);
    setMinimumHeight(520);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto tabWidget = new QTabWidget(this);
    tabWidget->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #dcdfe6; border-radius: 8px; background: #ffffff; padding: 8px; } "
        "QTabBar::tab { background: #f4f4f5; border: 1px solid #dcdfe6; border-bottom: none; border-top-left-radius: 6px; border-top-right-radius: 6px; padding: 7px 14px; font-weight: 500; color: #606266; margin-right: 2px; } "
        "QTabBar::tab:selected { background: #ffffff; color: #409eff; font-weight: bold; border-color: #dcdfe6; } "
        "QTabBar::tab:hover { background: #ecf5ff; color: #409eff; }"
    );

    // =========================================================================
    // 🎭 TAB 1: 角色人格设定 (Persona Adapter)
    // =========================================================================
    auto personaPage = new QWidget(tabWidget);
    auto personaLayout = new QVBoxLayout(personaPage);
    personaLayout->setSpacing(12);

    auto personaGroup = new QGroupBox("🎭 桌宠人格适配器", personaPage);
    personaGroup->setStyleSheet("QGroupBox { font-weight: bold; font-size: 13px; color: #2c3e50; border: 1px solid #e4e7ed; border-radius: 8px; margin-top: 8px; padding-top: 14px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }");
    auto personaForm = new QFormLayout(personaGroup);
    personaForm->setSpacing(10);
    personaForm->setLabelAlignment(Qt::AlignRight);

    m_personaCombo = new QComboBox(personaGroup);
    for (const auto &p : PersonaManager::instance()->allPersonas()) {
        m_personaCombo->addItem(p.name, p.id);
    }
    m_personaCombo->setStyleSheet("padding: 5px; border-radius: 4px; border: 1px solid #dcdfe6; font-size: 13px;");
    personaForm->addRow("当前人格角色:", m_personaCombo);

    m_personaDescLabel = new QLabel(personaGroup);
    m_personaDescLabel->setStyleSheet("color: #606266; font-size: 12px; background: #fdf6ec; padding: 8px; border-radius: 6px; border: 1px solid #faecd8;");
    m_personaDescLabel->setWordWrap(true);
    personaForm->addRow("性格特征:", m_personaDescLabel);

    m_customPromptEdit = new QTextEdit(personaGroup);
    m_customPromptEdit->setPlaceholderText("在此输入自定义 System Prompt 人设，定义你的桌宠性格、说话口吻与偏好...");
    m_customPromptEdit->setStyleSheet("border: 1px solid #dcdfe6; border-radius: 6px; padding: 6px; font-family: monospace; font-size: 12px;");
    m_customPromptEdit->setMinimumHeight(90);
    personaForm->addRow("人设提示词:", m_customPromptEdit);

    auto personaTip = new QLabel("✨ 提示：模型回复开头附带 [action:jump]、[action:celebrate]、[action:sit] 等指令时，桌宠会自动执行对应动作动画！", personaGroup);
    personaTip->setStyleSheet("font-size: 11px; color: #909399;");
    personaTip->setWordWrap(true);
    personaForm->addRow("", personaTip);

    personaLayout->addWidget(personaGroup);
    personaLayout->addStretch();
    tabWidget->addTab(personaPage, "🎭 角色人格");

    // =========================================================================
    // ⚡ TAB 2: 大模型与 Agent 适配器 (LLM & Agent)
    // =========================================================================
    auto llmPage = new QWidget(tabWidget);
    auto llmScroll = new QScrollArea(llmPage);
    llmScroll->setWidgetResizable(true);
    llmScroll->setFrameShape(QFrame::NoFrame);

    auto llmContainer = new QWidget();
    auto llmLayout = new QVBoxLayout(llmContainer);
    llmLayout->setSpacing(12);

    // 基础直连 LLM
    auto llmGroup = new QGroupBox("⚡ 基础大模型配置 (用于直答、翻译与人格对话)", llmContainer);
    llmGroup->setStyleSheet("QGroupBox { font-weight: bold; font-size: 13px; color: #2c3e50; border: 1px solid #e4e7ed; border-radius: 8px; margin-top: 8px; padding-top: 14px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }");
    auto llmForm = new QFormLayout(llmGroup);
    llmForm->setSpacing(8);
    llmForm->setLabelAlignment(Qt::AlignRight);

    m_presetCombo = new QComboBox(llmGroup);
    m_presetCombo->addItem("自定义配置 (Custom)", 0);
    m_presetCombo->addItem("DeepSeek (官方 API)", 1);
    m_presetCombo->addItem("OpenAI (官方 API)", 2);
    m_presetCombo->addItem("腾讯云 / WorkBuddy (兼容 API)", 3);
    m_presetCombo->addItem("本地 Ollama (127.0.0.1:11434 离线免费)", 4);
    m_presetCombo->setStyleSheet("padding: 4px; border-radius: 4px; border: 1px solid #dcdfe6;");
    llmForm->addRow("服务商预设:", m_presetCombo);

    m_apiBaseEdit = new QLineEdit(llmGroup);
    m_apiBaseEdit->setPlaceholderText("例如: https://api.deepseek.com/v1");
    m_apiBaseEdit->setStyleSheet("padding: 5px; border-radius: 4px; border: 1px solid #dcdfe6;");
    llmForm->addRow("API 地址 (Base):", m_apiBaseEdit);

    m_apiKeyEdit = new QLineEdit(llmGroup);
    m_apiKeyEdit->setPlaceholderText("请输入 API Key (sk-...)");
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_apiKeyEdit->setStyleSheet("padding: 5px; border-radius: 4px; border: 1px solid #dcdfe6;");
    llmForm->addRow("API 密钥 (Key):", m_apiKeyEdit);

    m_modelEdit = new QLineEdit(llmGroup);
    m_modelEdit->setPlaceholderText("例如: deepseek-chat, gpt-4o-mini, qwen2.5:7b");
    m_modelEdit->setStyleSheet("padding: 5px; border-radius: 4px; border: 1px solid #dcdfe6;");
    llmForm->addRow("模型名称 (Model):", m_modelEdit);

    m_memoryTurnsSpin = new QSpinBox(llmGroup);
    m_memoryTurnsSpin->setRange(0, 30);
    m_memoryTurnsSpin->setValue(6);
    m_memoryTurnsSpin->setSuffix(" 轮对话");
    m_memoryTurnsSpin->setStyleSheet("padding: 4px; border-radius: 4px; border: 1px solid #dcdfe6;");
    llmForm->addRow("记忆轮数:", m_memoryTurnsSpin);

    auto testLlmLayout = new QHBoxLayout();
    m_testBtn = new QPushButton("🔌 测试模型连通性", llmGroup);
    m_testBtn->setStyleSheet("padding: 5px 12px; border-radius: 4px; border: 1px solid #409eff; color: #409eff; background: #ecf5ff; font-weight: 500;");
    m_testBtn->setCursor(Qt::PointingHandCursor);

    m_testStatusLabel = new QLabel(llmGroup);
    m_testStatusLabel->setStyleSheet("font-size: 11px; color: #666;");
    m_testStatusLabel->setWordWrap(true);

    testLlmLayout->addWidget(m_testBtn);
    testLlmLayout->addWidget(m_testStatusLabel, 1);
    llmForm->addRow("", testLlmLayout);

    llmLayout->addWidget(llmGroup);

    // aipy-pro Agent 适配器
    auto agentGroup = new QGroupBox("🤖 aipy-pro 复杂任务智能体", llmContainer);
    agentGroup->setStyleSheet("QGroupBox { font-weight: bold; font-size: 13px; color: #2c3e50; border: 1px solid #e4e7ed; border-radius: 8px; margin-top: 8px; padding-top: 14px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }");
    auto agentForm = new QFormLayout(agentGroup);
    agentForm->setSpacing(8);
    agentForm->setLabelAlignment(Qt::AlignRight);

    m_agentTypeCombo = new QComboBox(agentGroup);
    m_agentTypeCombo->addItem("aipy-pro (本地 Agent 项目)", "aipy");
    m_agentTypeCombo->addItem("WorkBuddy (云端智能体)", "workbuddy");
    m_agentTypeCombo->addItem("CodeX (代码智能体)", "codex");
    m_agentTypeCombo->addItem("直连 LLM (无 Agent 模式)", "direct_llm");
    m_agentTypeCombo->setStyleSheet("padding: 4px; border-radius: 4px; border: 1px solid #dcdfe6;");
    agentForm->addRow("Agent 适配器:", m_agentTypeCombo);

    m_routingModeCombo = new QComboBox(agentGroup);
    m_routingModeCombo->addItem("🎯 智能分流 (简单问题直答，复杂任务由 Agent 执行)", "AUTO");
    m_routingModeCombo->addItem("🚀 始终使用 Agent 执行所有问题", "ALWAYS_AGENT");
    m_routingModeCombo->addItem("💬 仅使用轻量模型直答 (不委派 Agent)", "ALWAYS_LLM");
    m_routingModeCombo->setStyleSheet("padding: 4px; border-radius: 4px; border: 1px solid #dcdfe6;");
    agentForm->addRow("分流策略:", m_routingModeCombo);

    m_aipyBaseEdit = new QLineEdit(agentGroup);
    m_aipyBaseEdit->setPlaceholderText("默认: http://127.0.0.1:41970");
    m_aipyBaseEdit->setStyleSheet("padding: 5px; border-radius: 4px; border: 1px solid #dcdfe6;");
    agentForm->addRow("aipy 地址:", m_aipyBaseEdit);

    m_aipyKeyEdit = new QLineEdit(agentGroup);
    m_aipyKeyEdit->setPlaceholderText("可留空自动读取，或手动输入 API 密钥");
    m_aipyKeyEdit->setEchoMode(QLineEdit::Password);
    m_aipyKeyEdit->setStyleSheet("padding: 5px; border-radius: 4px; border: 1px solid #dcdfe6;");

    auto keyBtnLayout = new QHBoxLayout();
    m_autoDetectKeyBtn = new QPushButton("🔑 自动提取本地密钥", agentGroup);
    m_autoDetectKeyBtn->setStyleSheet("padding: 4px 8px; border-radius: 4px; background: #e1f3d8; color: #67c23a; border: 1px solid #c2e7b0; font-weight: 500; font-size: 11px;");
    m_autoDetectKeyBtn->setCursor(Qt::PointingHandCursor);

    m_testAipyBtn = new QPushButton("🔌 测试连通", agentGroup);
    m_testAipyBtn->setStyleSheet("padding: 4px 8px; border-radius: 4px; background: #ecf5ff; color: #409eff; border: 1px solid #d9ecff; font-weight: 500; font-size: 11px;");
    m_testAipyBtn->setCursor(Qt::PointingHandCursor);

    keyBtnLayout->addWidget(m_aipyKeyEdit);
    keyBtnLayout->addWidget(m_autoDetectKeyBtn);
    keyBtnLayout->addWidget(m_testAipyBtn);
    agentForm->addRow("aipy 密钥:", keyBtnLayout);

    m_agentStatusLabel = new QLabel(agentGroup);
    m_agentStatusLabel->setStyleSheet("font-size: 11px; color: #666;");
    m_agentStatusLabel->setWordWrap(true);
    agentForm->addRow("", m_agentStatusLabel);

    llmLayout->addWidget(agentGroup);

    llmScroll->setWidget(llmContainer);
    auto llmMainBox = new QVBoxLayout(llmPage);
    llmMainBox->setContentsMargins(0, 0, 0, 0);
    llmMainBox->addWidget(llmScroll);
    tabWidget->addTab(llmPage, "⚡ 模型与 Agent");

    // =========================================================================
    // 🤖 TAB 3: 编程助手状态感知 (Coding Agent Hook & Token Saver)
    // =========================================================================
    auto hookPage = new QWidget(tabWidget);
    auto hookLayout = new QVBoxLayout(hookPage);
    hookLayout->setSpacing(12);

    auto hookGroup = new QGroupBox("🤖 Coding Agent 状态感知与动作联动", hookPage);
    hookGroup->setStyleSheet("QGroupBox { font-weight: bold; font-size: 13px; color: #2c3e50; border: 1px solid #e4e7ed; border-radius: 8px; margin-top: 8px; padding-top: 14px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }");
    auto hookForm = new QFormLayout(hookGroup);
    hookForm->setSpacing(10);
    hookForm->setLabelAlignment(Qt::AlignRight);

    m_enableStateHookCheck = new QCheckBox("启用 Coding Agent 状态感知 (监听 Cursor / Claude Code / Git Hook 状态)", hookGroup);
    m_enableStateHookCheck->setStyleSheet("font-weight: 500; font-size: 13px; color: #303133;");
    hookForm->addRow("", m_enableStateHookCheck);

    m_enableLlmNarrationCheck = new QCheckBox("启用 AI 智能口语化润色 (⚠️ 开启后完成任务时将调用大模型消耗 Token；关闭则使用 0-Token 纯本地极速播报)", hookGroup);
    m_enableLlmNarrationCheck->setStyleSheet("font-size: 12px; color: #606266;");
    hookForm->addRow("", m_enableLlmNarrationCheck);

    m_stateDebounceSpin = new QSpinBox(hookGroup);
    m_stateDebounceSpin->setRange(0, 30);
    m_stateDebounceSpin->setValue(2);
    m_stateDebounceSpin->setSuffix(" 秒");
    m_stateDebounceSpin->setStyleSheet("padding: 4px; border-radius: 4px; border: 1px solid #dcdfe6;");
    hookForm->addRow("状态防抖间隔:", m_stateDebounceSpin);

    auto apiDocLabel = new QLabel(
        "💡 <b>Webhook API 接入指南</b>：<br>"
        "通过向 <code>http://127.0.0.1:41970/api/agent/status</code> 发送 POST 请求即可驱动桌宠联动：<br>"
        "<pre style='background: #f4f4f5; padding: 6px; border-radius: 4px; font-size: 11px; margin-top: 4px;'>"
        "curl -X POST http://127.0.0.1:41970/api/agent/status \\\n"
        "  -H 'Content-Type: application/json' \\\n"
        "  -d '{\"agent_name\": \"Claude Code\", \"status\": \"working\", \"task\": \"重构数据库模块\"}'"
        "</pre>"
        "支持状态: <code>thinking</code> (思考), <code>working</code> (写代码), <code>need_approval</code> (求审批), <code>finished</code> (完成庆祝), <code>error</code> (报错)",
        hookGroup
    );
    apiDocLabel->setStyleSheet("color: #606266; font-size: 11px; line-height: 1.4;");
    apiDocLabel->setWordWrap(true);
    hookForm->addRow("", apiDocLabel);

    hookLayout->addWidget(hookGroup);
    hookLayout->addStretch();
    tabWidget->addTab(hookPage, "🤖 编程感知 (Hook)");

    // =========================================================================
    // ⌨️ TAB 4: 全局快捷键与记忆 (Hotkeys & Memory)
    // =========================================================================
    auto hkPage = new QWidget(tabWidget);
    auto hkLayout = new QVBoxLayout(hkPage);
    hkLayout->setSpacing(12);

    auto hkGroup = new QGroupBox("⌨️ 全局快捷键", hkPage);
    hkGroup->setStyleSheet("QGroupBox { font-weight: bold; font-size: 13px; color: #2c3e50; border: 1px solid #e4e7ed; border-radius: 8px; margin-top: 8px; padding-top: 14px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }");
    auto hkForm = new QFormLayout(hkGroup);
    hkForm->setSpacing(8);
    hkForm->setLabelAlignment(Qt::AlignRight);

    m_hotkeyTranslateEdit = new QLineEdit(hkGroup);
    m_hotkeyTranslateEdit->setPlaceholderText("例如: Option+T");
    m_hotkeyTranslateEdit->setStyleSheet("padding: 5px; border-radius: 4px; border: 1px solid #dcdfe6; font-weight: bold;");
    hkForm->addRow("划词翻译快捷键:", m_hotkeyTranslateEdit);

    m_hotkeyAskEdit = new QLineEdit(hkGroup);
    m_hotkeyAskEdit->setPlaceholderText("例如: Option+Q");
    m_hotkeyAskEdit->setStyleSheet("padding: 5px; border-radius: 4px; border: 1px solid #dcdfe6; font-weight: bold;");
    hkForm->addRow("划词提问快捷键:", m_hotkeyAskEdit);

    m_hotkeyMusicToggleEdit = new QLineEdit(hkGroup);
    m_hotkeyMusicToggleEdit->setPlaceholderText("例如: Option+M");
    m_hotkeyMusicToggleEdit->setStyleSheet("padding: 5px; border-radius: 4px; border: 1px solid #dcdfe6; font-weight: bold;");
    hkForm->addRow("🎵 音乐打开/隐藏:", m_hotkeyMusicToggleEdit);

    m_hotkeyMusicPlayPauseEdit = new QLineEdit(hkGroup);
    m_hotkeyMusicPlayPauseEdit->setPlaceholderText("例如: Option+Space");
    m_hotkeyMusicPlayPauseEdit->setStyleSheet("padding: 5px; border-radius: 4px; border: 1px solid #dcdfe6; font-weight: bold;");
    hkForm->addRow("🎵 音乐播放/暂停:", m_hotkeyMusicPlayPauseEdit);

    m_hotkeyMusicNextEdit = new QLineEdit(hkGroup);
    m_hotkeyMusicNextEdit->setPlaceholderText("例如: Option+Right");
    m_hotkeyMusicNextEdit->setStyleSheet("padding: 5px; border-radius: 4px; border: 1px solid #dcdfe6; font-weight: bold;");
    hkForm->addRow("🎵 音乐下一首:", m_hotkeyMusicNextEdit);

    m_hotkeyMusicPrevEdit = new QLineEdit(hkGroup);
    m_hotkeyMusicPrevEdit->setPlaceholderText("例如: Option+Left");
    m_hotkeyMusicPrevEdit->setStyleSheet("padding: 5px; border-radius: 4px; border: 1px solid #dcdfe6; font-weight: bold;");
    hkForm->addRow("🎵 音乐上一首:", m_hotkeyMusicPrevEdit);

    m_hotkeyMusicFavEdit = new QLineEdit(hkGroup);
    m_hotkeyMusicFavEdit->setPlaceholderText("例如: Option+L");
    m_hotkeyMusicFavEdit->setStyleSheet("padding: 5px; border-radius: 4px; border: 1px solid #dcdfe6; font-weight: bold;");
    hkForm->addRow("🎵 音乐收藏/取消:", m_hotkeyMusicFavEdit);

    m_clearMemoryBtn = new QPushButton("🧹 清空历史对话记忆", hkGroup);
    m_clearMemoryBtn->setStyleSheet("padding: 5px 12px; color: #e6a23c; border: 1px solid #f3d19e; border-radius: 4px; background: #fdf6ec; font-weight: 500;");
    m_clearMemoryBtn->setCursor(Qt::PointingHandCursor);
    hkForm->addRow("", m_clearMemoryBtn);

    hkLayout->addWidget(hkGroup);
    hkLayout->addStretch();
    tabWidget->addTab(hkPage, "⌨️ 快捷键");

    mainLayout->addWidget(tabWidget);

    // 底部按钮栏
    auto btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_cancelBtn = new QPushButton("取消", this);
    m_saveBtn = new QPushButton("保存并应用配置", this);

    m_cancelBtn->setStyleSheet("padding: 7px 18px; border-radius: 6px; border: 1px solid #dcdfe6; background: #ffffff; color: #606266; font-weight: 500;");
    m_saveBtn->setStyleSheet("padding: 7px 22px; border-radius: 6px; border: none; background: #409eff; color: white; font-weight: bold; font-size: 13px;");

    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    m_saveBtn->setCursor(Qt::PointingHandCursor);

    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addWidget(m_saveBtn);
    mainLayout->addLayout(btnLayout);

    // 信号连接
    connect(m_personaCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AgentSettingsDialog::onPersonaChanged);
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AgentSettingsDialog::applyPreset);
    connect(m_testBtn, &QPushButton::clicked, this, &AgentSettingsDialog::testConnection);
    connect(m_autoDetectKeyBtn, &QPushButton::clicked, this, &AgentSettingsDialog::autoDetectAipyKey);
    connect(m_testAipyBtn, &QPushButton::clicked, this, &AgentSettingsDialog::testAipyConnection);

    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_saveBtn, &QPushButton::clicked, this, &AgentSettingsDialog::saveAndClose);
    
    connect(m_clearMemoryBtn, &QPushButton::clicked, this, [this]() {
        AgentService::instance()->clearMemory();
        QMessageBox::information(this, "提示", "历史会话记忆已全部清空！");
    });

    refreshValues();
}

void AgentSettingsDialog::onPersonaChanged(int index) {
    QString id = m_personaCombo->itemData(index).toString();
    auto persona = PersonaManager::instance()->getPersona(id);
    m_personaDescLabel->setText(persona.description);

    if (id == "custom") {
        m_customPromptEdit->setEnabled(true);
        m_customPromptEdit->setText(PersonaManager::instance()->customPersonaPrompt());
    } else {
        m_customPromptEdit->setEnabled(false);
        m_customPromptEdit->setText(persona.defaultSystemPrompt);
    }
}

void AgentSettingsDialog::refreshValues() {
    auto cfg = AgentService::instance()->config();
    m_apiBaseEdit->setText(cfg.apiBase);
    m_apiKeyEdit->setText(cfg.apiKey);
    m_modelEdit->setText(cfg.model);
    m_memoryTurnsSpin->setValue(cfg.maxMemoryTurns);
    m_hotkeyTranslateEdit->setText(cfg.hotkeyTranslate.isEmpty() ? "Option+T" : cfg.hotkeyTranslate);
    m_hotkeyAskEdit->setText(cfg.hotkeyAsk.isEmpty() ? "Option+Q" : cfg.hotkeyAsk);

    m_hotkeyMusicToggleEdit->setText(cfg.hotkeyMusicToggle.isEmpty() ? "Option+M" : cfg.hotkeyMusicToggle);
    m_hotkeyMusicPlayPauseEdit->setText(cfg.hotkeyMusicPlayPause.isEmpty() ? "Option+Space" : cfg.hotkeyMusicPlayPause);
    m_hotkeyMusicNextEdit->setText(cfg.hotkeyMusicNext.isEmpty() ? "Option+Right" : cfg.hotkeyMusicNext);
    m_hotkeyMusicPrevEdit->setText(cfg.hotkeyMusicPrev.isEmpty() ? "Option+Left" : cfg.hotkeyMusicPrev);
    m_hotkeyMusicFavEdit->setText(cfg.hotkeyMusicFav.isEmpty() ? "Option+L" : cfg.hotkeyMusicFav);

    int idx = m_agentTypeCombo->findData(cfg.activeAgentType);
    if (idx >= 0) m_agentTypeCombo->setCurrentIndex(idx);

    int rIdx = m_routingModeCombo->findData(cfg.routingMode);
    if (rIdx >= 0) m_routingModeCombo->setCurrentIndex(rIdx);

    m_aipyBaseEdit->setText(cfg.aipyBase.isEmpty() ? "http://127.0.0.1:41970" : cfg.aipyBase);
    m_aipyKeyEdit->setText(cfg.aipyKey);

    // 人格与状态感知刷新
    QString activePersona = PersonaManager::instance()->activePersonaId();
    int pIdx = m_personaCombo->findData(activePersona);
    if (pIdx >= 0) {
        m_personaCombo->setCurrentIndex(pIdx);
    } else {
        m_personaCombo->setCurrentIndex(0);
    }
    onPersonaChanged(m_personaCombo->currentIndex());

    m_enableStateHookCheck->setChecked(cfg.enableAgentStateHook);
    m_enableLlmNarrationCheck->setChecked(cfg.enableLlmTaskNarration);
    m_stateDebounceSpin->setValue(cfg.stateDebounceSec);

    m_testStatusLabel->clear();
    m_agentStatusLabel->clear();
}

void AgentSettingsDialog::autoDetectAipyKey() {
    QString key = AipyAdapter::autoDetectLocalApiKey();
    if (!key.isEmpty()) {
        m_aipyKeyEdit->setText(key);
        m_agentStatusLabel->setStyleSheet("font-size: 11px; color: #67c23a; font-weight: bold;");
        m_agentStatusLabel->setText("✅ 成功从本地 aipy-pro 数据库提取到 API Key！");
    } else {
        m_agentStatusLabel->setStyleSheet("font-size: 11px; color: #e6a23c;");
        m_agentStatusLabel->setText("⚠️ 未在默认路径找到 aipy-pro 数据库，请确认 aipy-pro 已启动并开启 API");
    }
}

void AgentSettingsDialog::testAipyConnection() {
    QString baseUrl = m_aipyBaseEdit->text().trimmed();
    QString key = m_aipyKeyEdit->text().trimmed();
    if (baseUrl.isEmpty()) baseUrl = "http://127.0.0.1:41970";

    if (auto aipy = AgentService::instance()->aipyAdapter()) {
        aipy->setBaseUrl(baseUrl);
        aipy->setApiKey(key);
        m_testAipyBtn->setEnabled(false);
        m_testAipyBtn->setText("⏳ 测试中...");

        aipy->testConnection([this](bool success, QString const& msg) {
            m_testAipyBtn->setEnabled(true);
            m_testAipyBtn->setText("🔌 测试连通");
            if (success) {
                m_agentStatusLabel->setStyleSheet("font-size: 11px; color: #67c23a; font-weight: bold;");
                m_agentStatusLabel->setText("✅ " + msg);
                QMessageBox::information(this, "aipy 连接成功", "🎉 " + msg);
            } else {
                m_agentStatusLabel->setStyleSheet("font-size: 11px; color: #f56c6c; font-weight: bold;");
                m_agentStatusLabel->setText("❌ " + msg);
                QMessageBox::warning(this, "aipy 连接失败", msg);
            }
        });
    }
}

void AgentSettingsDialog::applyPreset(int index) {
    if (index == 1) { // DeepSeek
        m_apiBaseEdit->setText("https://api.deepseek.com/v1");
        m_modelEdit->setText("deepseek-chat");
    } else if (index == 2) { // OpenAI
        m_apiBaseEdit->setText("https://api.openai.com/v1");
        m_modelEdit->setText("gpt-4o-mini");
    } else if (index == 3) { // 腾讯云 / WorkBuddy
        m_apiBaseEdit->setText("https://api.lkeap.cloud.tencent.com/v1");
        m_modelEdit->setText("deepseek-r1");
    } else if (index == 4) { // Ollama
        m_apiBaseEdit->setText("http://localhost:11434/v1");
        m_modelEdit->setText("qwen2.5:7b");
        if (m_apiKeyEdit->text().isEmpty()) {
            m_apiKeyEdit->setText("ollama");
        }
    }
}

void AgentSettingsDialog::testConnection() {
    QString apiBase = m_apiBaseEdit->text().trimmed();
    QString apiKey = m_apiKeyEdit->text().trimmed();
    QString model = m_modelEdit->text().trimmed();

    if (apiBase.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入 API 地址");
        return;
    }
    if (apiKey.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入 API 密钥 (Key)");
        return;
    }
    if (model.isEmpty()) {
        model = "gpt-4o-mini";
    }

    m_testBtn->setEnabled(false);
    m_testBtn->setText("⏳ 测试中...");
    m_testStatusLabel->setStyleSheet("font-size: 11px; color: #409eff;");
    m_testStatusLabel->setText("正在发送握手请求...");

    AgentService::instance()->testConnection(apiBase, apiKey, model, [this](bool success, QString const& message) {
        m_testBtn->setEnabled(true);
        m_testBtn->setText("🔌 测试模型连通性");

        if (success) {
            m_testStatusLabel->setStyleSheet("font-size: 11px; color: #67c23a; font-weight: bold;");
            m_testStatusLabel->setText("✅ " + message);
            QMessageBox::information(this, "连接成功", "🎉 " + message);
        } else {
            m_testStatusLabel->setStyleSheet("font-size: 11px; color: #f56c6c; font-weight: bold;");
            m_testStatusLabel->setText("❌ " + message);
            QMessageBox::warning(this, "连接测试失败", message);
        }
    });
}

void AgentSettingsDialog::saveAndClose() {
    AgentConfig cfg;
    cfg.apiBase = m_apiBaseEdit->text().trimmed();
    cfg.apiKey = m_apiKeyEdit->text().trimmed();
    cfg.model = m_modelEdit->text().trimmed();
    cfg.maxMemoryTurns = m_memoryTurnsSpin->value();
    cfg.hotkeyTranslate = m_hotkeyTranslateEdit->text().trimmed();
    cfg.hotkeyAsk = m_hotkeyAskEdit->text().trimmed();
    cfg.hotkeyMusicToggle = m_hotkeyMusicToggleEdit->text().trimmed();
    cfg.hotkeyMusicPlayPause = m_hotkeyMusicPlayPauseEdit->text().trimmed();
    cfg.hotkeyMusicNext = m_hotkeyMusicNextEdit->text().trimmed();
    cfg.hotkeyMusicPrev = m_hotkeyMusicPrevEdit->text().trimmed();
    cfg.hotkeyMusicFav = m_hotkeyMusicFavEdit->text().trimmed();

    cfg.activeAgentType = m_agentTypeCombo->currentData().toString();
    cfg.routingMode = m_routingModeCombo->currentData().toString();
    cfg.aipyBase = m_aipyBaseEdit->text().trimmed();
    cfg.aipyKey = m_aipyKeyEdit->text().trimmed();

    // 状态感知与 Token 省流
    cfg.enableAgentStateHook = m_enableStateHookCheck->isChecked();
    cfg.enableLlmTaskNarration = m_enableLlmNarrationCheck->isChecked();
    cfg.stateDebounceSec = m_stateDebounceSpin->value();

    if (cfg.apiBase.isEmpty()) cfg.apiBase = "https://api.openai.com/v1";
    if (cfg.model.isEmpty()) cfg.model = "gpt-4o-mini";
    if (cfg.hotkeyTranslate.isEmpty()) cfg.hotkeyTranslate = "Option+T";
    if (cfg.hotkeyAsk.isEmpty()) cfg.hotkeyAsk = "Option+Q";
    if (cfg.hotkeyMusicToggle.isEmpty()) cfg.hotkeyMusicToggle = "Option+M";
    if (cfg.hotkeyMusicPlayPause.isEmpty()) cfg.hotkeyMusicPlayPause = "Option+Space";
    if (cfg.hotkeyMusicNext.isEmpty()) cfg.hotkeyMusicNext = "Option+Right";
    if (cfg.hotkeyMusicPrev.isEmpty()) cfg.hotkeyMusicPrev = "Option+Left";
    if (cfg.hotkeyMusicFav.isEmpty()) cfg.hotkeyMusicFav = "Option+L";
    if (cfg.aipyBase.isEmpty()) cfg.aipyBase = "http://127.0.0.1:41970";

    // 保存人格设置
    QString chosenPersonaId = m_personaCombo->currentData().toString();
    PersonaManager::instance()->setActivePersonaId(chosenPersonaId);
    if (chosenPersonaId == "custom") {
        PersonaManager::instance()->setCustomPersonaPrompt(m_customPromptEdit->toPlainText().trimmed());
    }

    AgentService::instance()->setConfig(cfg);
    accept();
}
