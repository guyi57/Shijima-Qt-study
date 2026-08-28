// 
// Shijima-Qt - AI Model, Memory, Agent & Hotkey Settings Dialog Implementation
// 

#include "AgentSettingsDialog.hpp"
#include "AgentService.hpp"
#include "AipyAdapter.hpp"
#include <QFormLayout>
#include <QMessageBox>
#include <QScreen>
#include <QGroupBox>
#include <QGuiApplication>

AgentSettingsDialog::AgentSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("⚙️ AI 大模型、智能体 (Agent) 与快捷键配置");
    setWindowFlags(Qt::Dialog | Qt::WindowStaysOnTopHint | Qt::WindowCloseButtonHint);
    setMinimumWidth(500);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 18, 18, 18);
    mainLayout->setSpacing(14);

    // ==================== 1. 智能体 Agent 适配器配置 ====================
    auto agentGroup = new QGroupBox("🤖 智能体 (Agent) 适配器与任务分流", this);
    agentGroup->setStyleSheet("QGroupBox { font-weight: bold; font-size: 13px; color: #2c3e50; border: 1px solid #dcdfe6; border-radius: 8px; margin-top: 10px; padding-top: 15px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");
    auto agentForm = new QFormLayout(agentGroup);
    agentForm->setSpacing(8);
    agentForm->setLabelAlignment(Qt::AlignRight);

    m_agentTypeCombo = new QComboBox(this);
    m_agentTypeCombo->addItem("aipy-pro (本地 Agent 项目)", "aipy");
    m_agentTypeCombo->addItem("WorkBuddy (云端智能体)", "workbuddy");
    m_agentTypeCombo->addItem("CodeX (代码智能体)", "codex");
    m_agentTypeCombo->addItem("直连 LLM (无 Agent 模式)", "direct_llm");
    m_agentTypeCombo->setStyleSheet("padding: 4px; border-radius: 4px; border: 1px solid #ccc;");
    agentForm->addRow("Agent 适配器:", m_agentTypeCombo);

    m_routingModeCombo = new QComboBox(this);
    m_routingModeCombo->addItem("🎯 智能分流 (简单问题直答，复杂任务由 Agent 执行)", "AUTO");
    m_routingModeCombo->addItem("🚀 始终使用 Agent 执行所有问题", "ALWAYS_AGENT");
    m_routingModeCombo->addItem("💬 仅使用轻量模型直答 (不委派 Agent)", "ALWAYS_LLM");
    m_routingModeCombo->setStyleSheet("padding: 4px; border-radius: 4px; border: 1px solid #ccc;");
    agentForm->addRow("任务分流策略:", m_routingModeCombo);

    m_aipyBaseEdit = new QLineEdit(this);
    m_aipyBaseEdit->setPlaceholderText("默认: http://127.0.0.1:41970");
    m_aipyBaseEdit->setStyleSheet("padding: 5px; border-radius: 4px; border: 1px solid #ccc;");
    agentForm->addRow("aipy API 地址:", m_aipyBaseEdit);

    m_aipyKeyEdit = new QLineEdit(this);
    m_aipyKeyEdit->setPlaceholderText("可留空自动读取，或手动输入 API 密钥");
    m_aipyKeyEdit->setEchoMode(QLineEdit::Password);
    m_aipyKeyEdit->setStyleSheet("padding: 5px; border-radius: 4px; border: 1px solid #ccc;");

    auto keyBtnLayout = new QHBoxLayout();
    m_autoDetectKeyBtn = new QPushButton("🔑 自动提取 aipy 本地密钥", this);
    m_autoDetectKeyBtn->setStyleSheet("padding: 4px 10px; border-radius: 4px; background: #e1f3d8; color: #67c23a; border: 1px solid #c2e7b0; font-weight: 500;");
    m_autoDetectKeyBtn->setCursor(Qt::PointingHandCursor);

    m_testAipyBtn = new QPushButton("🔌 测试 aipy 连通性", this);
    m_testAipyBtn->setStyleSheet("padding: 4px 10px; border-radius: 4px; background: #ecf5ff; color: #409eff; border: 1px solid #d9ecff; font-weight: 500;");
    m_testAipyBtn->setCursor(Qt::PointingHandCursor);

    keyBtnLayout->addWidget(m_aipyKeyEdit);
    keyBtnLayout->addWidget(m_autoDetectKeyBtn);
    keyBtnLayout->addWidget(m_testAipyBtn);
    agentForm->addRow("aipy API 密钥:", keyBtnLayout);

    m_agentStatusLabel = new QLabel(this);
    m_agentStatusLabel->setStyleSheet("font-size: 11px; color: #666;");
    m_agentStatusLabel->setWordWrap(true);
    agentForm->addRow("", m_agentStatusLabel);

    mainLayout->addWidget(agentGroup);

    // ==================== 2. 基础大模型与分流模型配置 ====================
    auto llmGroup = new QGroupBox("⚡ 基础大模型配置 (用于快速直答与任务复杂度分流)", this);
    llmGroup->setStyleSheet("QGroupBox { font-weight: bold; font-size: 13px; color: #2c3e50; border: 1px solid #dcdfe6; border-radius: 8px; margin-top: 10px; padding-top: 15px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");
    auto llmForm = new QFormLayout(llmGroup);
    llmForm->setSpacing(8);
    llmForm->setLabelAlignment(Qt::AlignRight);

    m_presetCombo = new QComboBox(this);
    m_presetCombo->addItem("自定义配置 (Custom)", 0);
    m_presetCombo->addItem("DeepSeek (官方API)", 1);
    m_presetCombo->addItem("OpenAI (官方API)", 2);
    m_presetCombo->addItem("腾讯云 / WorkBuddy (兼容API)", 3);
    m_presetCombo->addItem("本地 Ollama (离线免费)", 4);
    m_presetCombo->setStyleSheet("padding: 4px; border-radius: 4px; border: 1px solid #ccc;");
    llmForm->addRow("服务商预设:", m_presetCombo);

    m_apiBaseEdit = new QLineEdit(this);
    m_apiBaseEdit->setPlaceholderText("例如: https://api.deepseek.com/v1");
    m_apiBaseEdit->setStyleSheet("padding: 5px; border-radius: 4px; border: 1px solid #ccc;");
    llmForm->addRow("API 地址 (Base):", m_apiBaseEdit);

    m_apiKeyEdit = new QLineEdit(this);
    m_apiKeyEdit->setPlaceholderText("请输入大模型 API Key (sk-...)");
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_apiKeyEdit->setStyleSheet("padding: 5px; border-radius: 4px; border: 1px solid #ccc;");
    llmForm->addRow("API 密钥 (Key):", m_apiKeyEdit);

    m_modelEdit = new QLineEdit(this);
    m_modelEdit->setPlaceholderText("例如: deepseek-chat, gpt-4o-mini");
    m_modelEdit->setStyleSheet("padding: 5px; border-radius: 4px; border: 1px solid #ccc;");
    llmForm->addRow("模型名称 (Model):", m_modelEdit);

    m_memoryTurnsSpin = new QSpinBox(this);
    m_memoryTurnsSpin->setRange(0, 30);
    m_memoryTurnsSpin->setValue(6);
    m_memoryTurnsSpin->setSuffix(" 轮对话");
    m_memoryTurnsSpin->setStyleSheet("padding: 4px; border-radius: 4px; border: 1px solid #ccc;");
    llmForm->addRow("记忆轮数:", m_memoryTurnsSpin);

    auto testLlmLayout = new QHBoxLayout();
    m_testBtn = new QPushButton("🔌 测试模型连通性", this);
    m_testBtn->setStyleSheet("padding: 5px 12px; border-radius: 4px; border: 1px solid #409eff; color: #409eff; background: #ecf5ff; font-weight: 500;");
    m_testBtn->setCursor(Qt::PointingHandCursor);

    m_testStatusLabel = new QLabel(this);
    m_testStatusLabel->setStyleSheet("font-size: 11px; color: #666;");
    m_testStatusLabel->setWordWrap(true);

    testLlmLayout->addWidget(m_testBtn);
    testLlmLayout->addWidget(m_testStatusLabel, 1);
    llmForm->addRow("", testLlmLayout);

    mainLayout->addWidget(llmGroup);

    // ==================== 3. 快捷键与记忆管理 ====================
    auto hkGroup = new QGroupBox("⌨️ 全局快捷键与记忆", this);
    hkGroup->setStyleSheet("QGroupBox { font-weight: bold; font-size: 13px; color: #2c3e50; border: 1px solid #dcdfe6; border-radius: 8px; margin-top: 10px; padding-top: 15px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");
    auto hkForm = new QFormLayout(hkGroup);
    hkForm->setSpacing(8);
    hkForm->setLabelAlignment(Qt::AlignRight);

    m_hotkeyTranslateEdit = new QLineEdit(this);
    m_hotkeyTranslateEdit->setPlaceholderText("例如: Option+T");
    m_hotkeyTranslateEdit->setStyleSheet("padding: 5px; border-radius: 4px; border: 1px solid #ccc; font-weight: bold;");
    hkForm->addRow("划词翻译快捷键:", m_hotkeyTranslateEdit);

    m_hotkeyAskEdit = new QLineEdit(this);
    m_hotkeyAskEdit->setPlaceholderText("例如: Option+Q");
    m_hotkeyAskEdit->setStyleSheet("padding: 5px; border-radius: 4px; border: 1px solid #ccc; font-weight: bold;");
    hkForm->addRow("划词提问快捷键:", m_hotkeyAskEdit);

    m_hotkeyMusicToggleEdit = new QLineEdit(this);
    m_hotkeyMusicToggleEdit->setPlaceholderText("例如: Option+M");
    m_hotkeyMusicToggleEdit->setStyleSheet("padding: 5px; border-radius: 4px; border: 1px solid #ccc; font-weight: bold;");
    hkForm->addRow("🎵 音乐工坊打开/隐藏:", m_hotkeyMusicToggleEdit);

    m_hotkeyMusicPlayPauseEdit = new QLineEdit(this);
    m_hotkeyMusicPlayPauseEdit->setPlaceholderText("例如: Option+Space");
    m_hotkeyMusicPlayPauseEdit->setStyleSheet("padding: 5px; border-radius: 4px; border: 1px solid #ccc; font-weight: bold;");
    hkForm->addRow("🎵 音乐 播放/暂停:", m_hotkeyMusicPlayPauseEdit);

    m_hotkeyMusicNextEdit = new QLineEdit(this);
    m_hotkeyMusicNextEdit->setPlaceholderText("例如: Option+Right");
    m_hotkeyMusicNextEdit->setStyleSheet("padding: 5px; border-radius: 4px; border: 1px solid #ccc; font-weight: bold;");
    hkForm->addRow("🎵 音乐 下一首:", m_hotkeyMusicNextEdit);

    m_hotkeyMusicPrevEdit = new QLineEdit(this);
    m_hotkeyMusicPrevEdit->setPlaceholderText("例如: Option+Left");
    m_hotkeyMusicPrevEdit->setStyleSheet("padding: 5px; border-radius: 4px; border: 1px solid #ccc; font-weight: bold;");
    hkForm->addRow("🎵 音乐 上一首:", m_hotkeyMusicPrevEdit);

    m_hotkeyMusicFavEdit = new QLineEdit(this);
    m_hotkeyMusicFavEdit->setPlaceholderText("例如: Option+L");
    m_hotkeyMusicFavEdit->setStyleSheet("padding: 5px; border-radius: 4px; border: 1px solid #ccc; font-weight: bold;");
    hkForm->addRow("🎵 音乐 收藏/取消收藏:", m_hotkeyMusicFavEdit);

    auto hkTip = new QLabel("💡 提示: 快捷键支持如 Option+M, Option+Space, Option+Right, Ctrl+Shift+P 等组合", this);
    hkTip->setStyleSheet("font-size: 11px; color: #8c939d;");
    hkForm->addRow("", hkTip);

    m_clearMemoryBtn = new QPushButton("🧹 清空历史对话记忆", this);
    m_clearMemoryBtn->setStyleSheet("padding: 4px 10px; color: #e6a23c; border: 1px solid #f3d19e; border-radius: 4px; background: #fdf6ec;");
    m_clearMemoryBtn->setCursor(Qt::PointingHandCursor);
    hkForm->addRow("", m_clearMemoryBtn);

    mainLayout->addWidget(hkGroup);

    // 底部按钮栏
    auto btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_cancelBtn = new QPushButton("取消", this);
    m_saveBtn = new QPushButton("保存并应用配置", this);

    m_cancelBtn->setStyleSheet("padding: 7px 18px; border-radius: 6px; border: 1px solid #dcdfe6; background: #fff;");
    m_saveBtn->setStyleSheet("padding: 7px 22px; border-radius: 6px; border: none; background: #409eff; color: white; font-weight: bold;");

    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    m_saveBtn->setCursor(Qt::PointingHandCursor);

    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addWidget(m_saveBtn);
    mainLayout->addLayout(btnLayout);

    // 事件绑定
    connect(m_presetCombo, &QComboBox::currentIndexChanged, this, &AgentSettingsDialog::applyPreset);
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
            m_testAipyBtn->setText("🔌 测试 aipy 连通性");
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

    AgentService::instance()->setConfig(cfg);
    accept();
}
