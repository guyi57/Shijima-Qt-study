#include "SkillManager.hpp"
#include "SettingsDb.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <iostream>

SkillManager* SkillManager::instance() {
    static SkillManager s_instance;
    return &s_instance;
}

SkillManager::SkillManager() {
    m_skillsDir = QDir::homePath() + "/.config/guyi-bot/skills";
    init();
}

QString SkillManager::skillsDirectory() const {
    return m_skillsDir;
}

void SkillManager::init() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    QDir dir(m_skillsDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    ensureDefaultSkills();
    scanSkills();
}

void SkillManager::ensureDefaultSkills() {
    // 默认技能 1: Git Commit 规范生成器
    QString gitSkillDir = m_skillsDir + "/git-committer";
    QDir().mkpath(gitSkillDir);
    QString gitSkillFile = gitSkillDir + "/SKILL.md";
    if (!QFile::exists(gitSkillFile)) {
        QFile file(gitSkillFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "---\n"
                << "name: Git Commit 专家\n"
                << "description: 自动根据变更内容生成符合 Conventional Commits 规范的清晰提交信息\n"
                << "author: guyi\n"
                << "---\n\n"
                << "## 角色定义\n"
                << "你是一个 Git 提交信息规范专家。当用户要求编写或整理 Git commit message 时，始终遵循 Conventional Commits 规范。\n\n"
                << "## 格式要求\n"
                << "- 类型: feat, fix, docs, style, refactor, perf, test, chore, build, ci\n"
                << "- 格式: `<type>(<scope>): <subject>`\n"
                << "- 使用动词现在时态，首字母小写，结尾不加句号\n"
                << "- 提供简要的中文或英文摘要，并在必要时列出具体要点。\n";
        }
    }

    // 默认技能 2: 代码审查助手
    QString reviewSkillDir = m_skillsDir + "/code-reviewer";
    QDir().mkpath(reviewSkillDir);
    QString reviewSkillFile = reviewSkillDir + "/SKILL.md";
    if (!QFile::exists(reviewSkillFile)) {
        QFile file(reviewSkillFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "---\n"
                << "name: 代码审查与优化助手\n"
                << "description: 从安全性、健壮性、时间空间复杂度及重构角度分析代码\n"
                << "author: guyi\n"
                << "---\n\n"
                << "## 角色定义\n"
                << "你是一位资深全栈工程师与代码架构师。当用户提供代码片段或要求审查时：\n"
                << "1. 快速指出潜在的内存泄漏、并发竞争、越界、空指针等关键 Bug；\n"
                << "2. 分析时间与空间复杂度；\n"
                << "3. 给出优雅简洁的重构优化建议与对比示例代码。\n";
        }
    }

    // 默认技能 3: 多语言精译助手
    QString transSkillDir = m_skillsDir + "/translator-pro";
    QDir().mkpath(transSkillDir);
    QString transSkillFile = transSkillDir + "/SKILL.md";
    if (!QFile::exists(transSkillFile)) {
        QFile file(transSkillFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "---\n"
                << "name: 精准多语言翻译\n"
                << "description: 提供符合母语习惯的专业技术术语信达雅翻译\n"
                << "author: guyi\n"
                << "---\n\n"
                << "## 角色定义\n"
                << "你是一位专业的计算机与技术本地化翻译专家。遇到翻译任务时：\n"
                << "- 准确保留代码片段、变量名、专有名词；\n"
                << "- 语言通顺自然，符合中文/英文母语技术人员交流习惯；\n"
                << "- 必要时在文末附上重点词汇对比表。\n";
        }
    }
}

void SkillManager::scanSkills() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_skills.clear();

    QDir rootDir(m_skillsDir);
    if (!rootDir.exists()) return;

    QFileInfoList subDirs = rootDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &subDirInfo : subDirs) {
        QString skillFile = subDirInfo.absoluteFilePath() + "/SKILL.md";
        if (QFile::exists(skillFile)) {
            parseSkillFile(skillFile, subDirInfo.fileName());
        }
    }
    std::cout << "[SkillManager] 扫描完成，共加载 " << m_skills.size() << " 个技能" << std::endl;
}

void SkillManager::parseSkillFile(const QString &filePath, const QString &folderName) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    SkillInfo skill;
    skill.id = folderName;
    skill.name = folderName;
    skill.description = "";
    skill.author = "community";
    skill.path = filePath;

    // 解析 Frontmatter (--- ... ---)
    static QRegularExpression fmRegex("^---\\s*\\n(.*?)\\n---\\s*\\n(.*)$", QRegularExpression::DotMatchesEverythingOption);
    auto match = fmRegex.match(content);
    if (match.hasMatch()) {
        QString frontmatter = match.captured(1);
        skill.prompt = match.captured(2).trimmed();

        QStringList lines = frontmatter.split('\n');
        for (const QString &line : lines) {
            int colonIdx = line.indexOf(':');
            if (colonIdx != -1) {
                QString key = line.left(colonIdx).trimmed().toLower();
                QString val = line.mid(colonIdx + 1).trimmed();
                if (key == "name") skill.name = val;
                else if (key == "description") skill.description = val;
                else if (key == "author") skill.author = val;
            }
        }
    } else {
        skill.prompt = content.trimmed();
    }

    // 从 SettingsDb 加载启用状态，默认 true
    QString enabledStr = SettingsDb::instance()->get("skill.enabled." + skill.id, "true");
    skill.enabled = (enabledStr != "false" && enabledStr != "0");

    m_skills[skill.id] = skill;
}

QList<SkillInfo> SkillManager::getAllSkills() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_skills.values();
}

SkillInfo SkillManager::getSkill(const QString &id, bool *found) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_skills.contains(id)) {
        if (found) *found = true;
        return m_skills[id];
    }
    if (found) *found = false;
    return SkillInfo{};
}

void SkillManager::setSkillEnabled(const QString &id, bool enabled) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_skills.contains(id)) {
        m_skills[id].enabled = enabled;
        SettingsDb::instance()->set("skill.enabled." + id, enabled ? "true" : "false");
    }
}

QString SkillManager::getSystemPromptExtensions() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    QString extensions;
    for (const auto &skill : m_skills) {
        if (skill.enabled && !skill.prompt.isEmpty()) {
            extensions += QString("\n\n### 扩展技能模块: %1\n%2").arg(skill.name, skill.prompt);
        }
    }
    return extensions;
}
