// 
// Shijima-Qt - AI Agent & Memory Service Implementation
// 

#include "AgentService.hpp"
#include "AipyAdapter.hpp"
#include "PetMemory.hpp"
#include "TimerManager.hpp"
#include "ShijimaManager.hpp"
#include "MusicPlayerManager.hpp"
#include "MusicPlayerDialog.hpp"
#include "MusicApiService.hpp"
#include "MusicFavoriteDb.hpp"
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QRegularExpression>
#include <QDebug>
#include <iostream>

static QJsonObject getMusicToolDefinition() {
    QJsonObject fn;
    fn["name"] = "music_player_manage";
    fn["description"] = "控制音乐播放器播放音乐、暂停、切歌、搜索歌曲、收藏/取消收藏、查看收藏列表或获取当前曲目。当用户要求放歌、听音乐、搜歌、暂停、下一首、上一首、收藏歌曲、查看收藏列表时调用此工具。";

    QJsonObject props;
    
    QJsonObject actionProp;
    actionProp["type"] = "string";
    actionProp["enum"] = QJsonArray{"search_and_play", "pause", "resume", "toggle_play", "next", "previous", "favorite", "list_favorites", "get_current", "open_window"};
    actionProp["description"] = "操作类型: search_and_play(搜索并播放), pause(暂停), resume(继续播放), toggle_play(播放/暂停切换), next(下一首), previous(上一首), favorite(收藏/取消收藏当前歌曲), list_favorites(查看收藏列表), get_current(获取当前播放歌曲), open_window(打开音乐播放器窗口)";
    props["action"] = actionProp;

    QJsonObject keywordProp;
    keywordProp["type"] = "string";
    keywordProp["description"] = "若为 search_and_play，要搜索并播放的歌名、歌手或关键词（如'周杰伦 晴天'、'轻音乐'、'起风了'）";
    props["keyword"] = keywordProp;

    QJsonObject sourceProp;
    sourceProp["type"] = "string";
    sourceProp["enum"] = QJsonArray{"netease", "kuwo", "bilibili"};
    sourceProp["description"] = "音乐源: netease(网易云音乐,默认推荐), kuwo(酷我音乐,周杰伦等原版推荐), bilibili(B站音频)";
    props["source"] = sourceProp;

    QJsonObject params;
    params["type"] = "object";
    params["properties"] = props;
    params["required"] = QJsonArray{"action"};

    QJsonObject tool;
    tool["type"] = "function";
    tool["function"] = fn;
    tool["function"].toObject()["parameters"] = params;

    QJsonObject result;
    result["type"] = "function";
    QJsonObject fnObj = fn;
    fnObj["parameters"] = params;
    result["function"] = fnObj;
    return result;
}

static void executeMusicTool(const QJsonObject &args, std::function<void(QString)> callback) {
    QString action = args["action"].toString().trimmed();
    std::cout << "[MusicTool] 执行音乐指令: action=" << action.toStdString() << std::endl;

    if (action == "search_and_play") {
        QString keyword = args["keyword"].toString().trimmed();
        QString source = args.contains("source") ? args["source"].toString("netease") : "netease";
        if (keyword.isEmpty()) {
            if (callback) callback("⚠️ 请提供要搜索播放的歌曲名称或歌手名哦～");
            return;
        }

        MusicApiService::instance()->search(keyword, source, 10, 1, [keyword, source, callback](bool success, const QVector<SongInfo>& songs, const QString &err) {
            if (!success || songs.isEmpty()) {
                if (callback) callback(QString("⚠️ 未能搜索到与「%1」相关的歌曲: %2。可尝试更换歌名或音乐源。").arg(keyword, err.isEmpty() ? "未找到匹配结果" : err));
                return;
            }

            SongInfo targetSong = songs.first();
            MusicPlayerManager::instance()->addBatchToPlaylist(songs);
            MusicPlayerManager::instance()->playSong(targetSong);

            QString res = QString("🎵 **已为您找到并开始播放**\n\n"
                                  "| 属性 | 内容 |\n"
                                  "| :--- | :--- |\n"
                                  "| 🎶 **歌曲名** | %1 |\n"
                                  "| 🎤 **歌手** | %2 |\n"
                                  "| 💿 **专辑** | %3 |\n"
                                  "| 🌐 **音乐源** | %4 |\n\n"
                                  "💡 已将搜索到的 %5 首相关歌曲加入播放列表～")
                .arg(targetSong.name, targetSong.artist.isEmpty() ? "未知歌手" : targetSong.artist, targetSong.album.isEmpty() ? "单曲" : targetSong.album, MusicApiService::sourceDisplayName(targetSong.source))
                .arg(songs.size());
            if (callback) callback(res);
        });
        return;
    }
    else if (action == "pause") {
        MusicPlayerManager::instance()->pause();
        if (callback) callback("⏸ 音乐已为您暂停播放。");
        return;
    }
    else if (action == "resume") {
        MusicPlayerManager::instance()->play();
        auto cur = MusicPlayerManager::instance()->currentSong();
        if (!cur.name.isEmpty()) {
            if (callback) callback(QString("▶ 已继续播放《%1》- %2").arg(cur.name, cur.artist));
        } else {
            if (callback) callback("▶ 音乐已开始播放。");
        }
        return;
    }
    else if (action == "toggle_play") {
        MusicPlayerManager::instance()->togglePlay();
        bool playing = MusicPlayerManager::instance()->isPlaying();
        if (callback) callback(playing ? "▶ 音乐已开始播放" : "⏸ 音乐已暂停");
        return;
    }
    else if (action == "next") {
        MusicPlayerManager::instance()->playNext();
        auto cur = MusicPlayerManager::instance()->currentSong();
        if (!cur.name.isEmpty()) {
            if (callback) callback(QString("⏭ 已为您切换到下一首：《%1》- %2").arg(cur.name, cur.artist));
        } else {
            if (callback) callback("⏭ 已为您切换到下一首。");
        }
        return;
    }
    else if (action == "previous") {
        MusicPlayerManager::instance()->playPrevious();
        auto cur = MusicPlayerManager::instance()->currentSong();
        if (!cur.name.isEmpty()) {
            if (callback) callback(QString("⏮ 已为您切换到上一首：《%1》- %2").arg(cur.name, cur.artist));
        } else {
            if (callback) callback("⏮ 已为您切换到上一首。");
        }
        return;
    }
    else if (action == "favorite") {
        MusicPlayerManager::instance()->toggleFavoriteCurrent();
        bool isFav = MusicPlayerManager::instance()->isCurrentSongFavorite();
        auto cur = MusicPlayerManager::instance()->currentSong();
        if (!cur.name.isEmpty()) {
            if (callback) callback(isFav ? QString("❤️ 已将《%1》- %2 添加到您的收藏夹！").arg(cur.name, cur.artist) : QString("🤍 已取消收藏《%1》").arg(cur.name));
        } else {
            if (callback) callback("⚠️ 当前没有正在播放的歌曲可供收藏哦。");
        }
        return;
    }
    else if (action == "list_favorites") {
        auto favs = MusicFavoriteDb::instance()->getFavorites();
        if (favs.isEmpty()) {
            if (callback) callback("❤️ 您当前还没有收藏任何歌曲哦。在听到喜欢的歌曲时对我说「收藏这首歌」即可收藏！");
            return;
        }
        QString res = QString("❤️ **您的音乐收藏列表 (共 %1 首)**\n\n| 序号 | 歌曲名 | 歌手 | 音乐源 |\n| :---: | :--- | :--- | :--- |\n").arg(favs.size());
        int limitCount = std::min(static_cast<int>(favs.size()), 15);
        for (int i = 0; i < limitCount; ++i) {
            const auto &s = favs[i];
            res += QString("| %1 | %2 | %3 | %4 |\n").arg(i + 1).arg(s.name, s.artist, MusicApiService::sourceDisplayName(s.source));
        }
        if (favs.size() > 15) {
            res += QString("\n*...还有 %1 首歌曲未展开，按 ⌥+M 即可在音乐工坊中查看完整列表*").arg(favs.size() - 15);
        }
        if (callback) callback(res);
        return;
    }
    else if (action == "get_current") {
        auto cur = MusicPlayerManager::instance()->currentSong();
        if (cur.name.isEmpty()) {
            if (callback) callback("🎵 当前没有正在播放的歌曲。您可以对我说「放首周杰伦的歌」开始聆听！");
            return;
        }
        bool playing = MusicPlayerManager::instance()->isPlaying();
        QString res = QString("🎵 **当前播放状态**\n\n"
                              "- **歌曲**：《%1》\n"
                              "- **歌手**：%2\n"
                              "- **状态**：%3\n"
                              "- **音源**：%4")
            .arg(cur.name, cur.artist, playing ? "▶ 正在播放中" : "⏸ 已暂停", MusicApiService::sourceDisplayName(cur.source));
        if (callback) callback(res);
        return;
    }
    else if (action == "open_window") {
        MusicPlayerDialog::instance()->toggleVisibility();
        if (callback) callback("🪟 已为您打开音乐工坊独立播放器窗口！");
        return;
    }

    if (callback) callback("已处理音乐播放器指令。");
}

static QJsonObject getTimerToolDefinition() {
    QJsonObject fn;
    fn["name"] = "timer_manage";
    fn["description"] = "创建、查看、修改或删除定时提醒与定时任务。当用户要求在未来某个时间提醒、倒计时、或定时/每天执行某任务时调用此工具。";

    QJsonObject props;
    
    QJsonObject actionProp;
    actionProp["type"] = "string";
    actionProp["enum"] = QJsonArray{"create", "list", "update", "delete"};
    actionProp["description"] = "操作类型: create(创建), list(查看列表), update(更新/暂停/启用), delete(删除)";
    props["action"] = actionProp;

    QJsonObject titleProp;
    titleProp["type"] = "string";
    titleProp["description"] = "提醒标题或任务名称，例如'喝水提醒'、'每日站会'";
    props["title"] = titleProp;

    QJsonObject typeProp;
    typeProp["type"] = "string";
    typeProp["enum"] = QJsonArray{"notification", "task"};
    typeProp["description"] = "类型: notification(桌面气泡弹窗提醒), task(到期自动让AI执行任务并输出报告)";
    props["type"] = typeProp;

    QJsonObject triggerSecProp;
    triggerSecProp["type"] = "integer";
    triggerSecProp["description"] = "距离当前时间多少秒后触发 (例如 10分钟后填 600，1小时后填 3600)";
    props["trigger_in_seconds"] = triggerSecProp;

    QJsonObject targetTimeProp;
    targetTimeProp["type"] = "string";
    targetTimeProp["description"] = "具体触发日期时间字符串，格式如 '2026-08-24 15:30' 或 '15:30'";
    props["target_time"] = targetTimeProp;

    QJsonObject repeatProp;
    repeatProp["type"] = "string";
    repeatProp["enum"] = QJsonArray{"once", "interval", "daily", "window_interval"};
    repeatProp["description"] = "重复模式: once(单次倒计时), interval(全天循环), daily(每天固定时间), window_interval(指定时间段内的固定间隔循环，如工作日9点-18点每小时)";
    props["repeat"] = repeatProp;

    QJsonObject intervalSecProp;
    intervalSecProp["type"] = "integer";
    intervalSecProp["description"] = "若为 interval 或 window_interval 模式，循环周期秒数 (例如每隔1小时填 3600，每隔30分钟填 1800)";
    props["repeat_interval_seconds"] = intervalSecProp;

    QJsonObject dailyTimeProp;
    dailyTimeProp["type"] = "string";
    dailyTimeProp["description"] = "若为 daily 模式，每天执行的时间点，格式 'HH:mm' (例如 '09:00')";
    props["daily_time"] = dailyTimeProp;

    QJsonObject startTimeProp;
    startTimeProp["type"] = "string";
    startTimeProp["description"] = "若为 window_interval 模式，时间段开始时间点，格式 'HH:mm' (例如 '09:00')";
    props["start_time"] = startTimeProp;

    QJsonObject endTimeProp;
    endTimeProp["type"] = "string";
    endTimeProp["description"] = "若为 window_interval 模式，时间段结束时间点，格式 'HH:mm' (例如 '18:00')";
    props["end_time"] = endTimeProp;

    QJsonObject weekdaysProp;
    weekdaysProp["type"] = "boolean";
    weekdaysProp["description"] = "是否仅在工作日 (周一至周五) 执行";
    props["weekdays_only"] = weekdaysProp;

    QJsonObject daysOfWeekProp;
    daysOfWeekProp["type"] = "array";
    QJsonObject itemProp;
    itemProp["type"] = "integer";
    daysOfWeekProp["items"] = itemProp;
    daysOfWeekProp["description"] = "允许触发的具体星期几列表 (1=周一, 2=周二, ..., 7=周日)，支持任意组合，如周一/周三/周五填 [1, 3, 5]，周末填 [6, 7]";
    props["days_of_week"] = daysOfWeekProp;

    QJsonObject promptProp;
    promptProp["type"] = "string";
    promptProp["description"] = "若 type 为 task，到期需要让桌宠AI执行的具体任务描述 (如 '查询今日成都天气')";
    props["task_prompt"] = promptProp;

    QJsonObject timerIdProp;
    timerIdProp["type"] = "string";
    timerIdProp["description"] = "修改或删除时指定的定时器ID";
    props["timer_id"] = timerIdProp;

    QJsonObject parameters;
    parameters["type"] = "object";
    parameters["properties"] = props;
    parameters["required"] = QJsonArray{"action"};

    fn["parameters"] = parameters;

    QJsonObject tool;
    tool["type"] = "function";
    tool["function"] = fn;
    return tool;
}

static QString executeTimerTool(const QJsonObject &args) {
    QString action = args["action"].toString("create");
    if (action == "create") {
        QString title = args["title"].toString("定时提醒");
        QString typeStr = args["type"].toString("notification");
        TimerType type = (typeStr == "task") ? TimerType::AiTask : TimerType::Notification;

        QString repStr = args["repeat"].toString("once");
        TimerRepeat repeat = TimerRepeat::Once;
        if (repStr == "interval") repeat = TimerRepeat::Interval;
        else if (repStr == "daily") repeat = TimerRepeat::Daily;
        else if (repStr == "window_interval") repeat = TimerRepeat::WindowInterval;

        int triggerInSec = args["trigger_in_seconds"].toInt(0);
        int intervalSec = args["repeat_interval_seconds"].toInt(0);
        QString dailyTime = args["daily_time"].toString();
        QString startTime = args["start_time"].toString("09:00");
        QString endTime = args["end_time"].toString("18:00");
        bool weekdaysOnly = args["weekdays_only"].toBool(false);
        QString taskPrompt = args["task_prompt"].toString();

        QList<int> daysOfWeek;
        if (args.contains("days_of_week")) {
            auto dowArr = args["days_of_week"].toArray();
            for (auto v : dowArr) daysOfWeek.append(v.toInt());
        }
        if (weekdaysOnly && daysOfWeek.isEmpty()) {
            daysOfWeek = {1, 2, 3, 4, 5};
        }

        // 支持 target_time 字符串解析
        if (triggerInSec <= 0 && args.contains("target_time")) {
            QString tt = args["target_time"].toString();
            QDateTime targetDt = QDateTime::fromString(tt, "yyyy-MM-dd HH:mm");
            if (!targetDt.isValid()) {
                targetDt = QDateTime::fromString(tt, "HH:mm");
                if (targetDt.isValid()) {
                    QDateTime now = QDateTime::currentDateTime();
                    targetDt.setDate(now.date());
                    if (targetDt <= now) targetDt = targetDt.addDays(1);
                }
            }
            if (targetDt.isValid()) {
                qint64 diff = (targetDt.toMSecsSinceEpoch() - QDateTime::currentMSecsSinceEpoch()) / 1000;
                if (diff > 0) triggerInSec = static_cast<int>(diff);
            }
        }

        if (triggerInSec <= 0 && repeat == TimerRepeat::Once) {
            triggerInSec = 60; // 默认保底 1 分钟
        }

        ScheduledTimer timer = TimerManager::instance()->createQuickTimer(
            title, triggerInSec, type, taskPrompt, repeat, intervalSec, dailyTime, startTime, endTime, weekdaysOnly, daysOfWeek
        );

        QString timeDesc;
        QString repDesc;
        if (repeat == TimerRepeat::Daily) {
            timeDesc = QString("每天 %1").arg(dailyTime);
            repDesc = weekdaysOnly ? "工作日 (周一至周五)" : "每天固定时刻";
        } else if (repeat == TimerRepeat::WindowInterval) {
            int mins = (intervalSec > 0 ? intervalSec : 3600) / 60;
            timeDesc = QString("%1 ~ %2 (每隔 %3 分钟)").arg(startTime, endTime).arg(mins);
            repDesc = weekdaysOnly ? "工作日 (周一至周五) 时间段循环" : "每天时间段循环";
        } else if (repeat == TimerRepeat::Interval) {
            timeDesc = QString("每隔 %1 分钟").arg(intervalSec / 60);
            repDesc = "全天固定周期循环";
        } else {
            timeDesc = QDateTime::fromMSecsSinceEpoch(timer.targetTimestamp).toString("yyyy-MM-dd HH:mm:ss");
            repDesc = "单次触发";
        }

        QString nextTriggerStr = QDateTime::fromMSecsSinceEpoch(timer.targetTimestamp).toString("yyyy-MM-dd HH:mm");

        QString res = QString("⏰ **已为您创建定时任务**\n\n"
                              "| 属性 | 内容 |\n"
                              "| :--- | :--- |\n"
                              "| 📌 **任务标题** | %1 |\n"
                              "| 🏷️ **任务类型** | %2 |\n"
                              "| ⏱️ **执行规则** | %3 |\n"
                              "| 🔄 **重复模式** | %4 |\n"
                              "| ⏳ **下次触发** | %5 |\n")
            .arg(title)
            .arg(type == TimerType::AiTask ? "🤖 自动执行任务" : "📢 弹窗提醒")
            .arg(timeDesc)
            .arg(repDesc)
            .arg(nextTriggerStr);

        if (type == TimerType::AiTask && !taskPrompt.isEmpty()) {
            res += QString("| 📋 **执行指令** | %1 |\n").arg(taskPrompt);
        }

        res += "\n💡 到期后桌宠会在桌面上准时跳出来提醒您哦～";
        return res;
    }
    else if (action == "list") {
        auto timers = TimerManager::instance()->getAllTimers();
        if (timers.isEmpty()) {
            return "⏰ 当前没有任何定时任务哦。您可以对我说「10分钟后提醒我喝水」来快速创建！";
        }
        QString res = "⏰ **当前定时任务列表**\n\n| 任务标题 | 类型 | 下次触发 | 状态 |\n| :--- | :--- | :--- | :--- |\n";
        for (const auto &t : timers) {
            QString typeStr = (t.type == TimerType::AiTask ? "🤖 任务" : "📢 提醒");
            QString nextStr = QDateTime::fromMSecsSinceEpoch(t.targetTimestamp).toString("MM-dd HH:mm");
            QString statusStr = t.enabled ? "🟢 运行中" : "⏸ 已暂停";
            res += QString("| %1 | %2 | %3 | %4 |\n").arg(t.title).arg(typeStr).arg(nextStr).arg(statusStr);
        }
        return res;
    }
    else if (action == "delete") {
        QString timerId = args["timer_id"].toString();
        bool ok = TimerManager::instance()->deleteTimer(timerId);
        return ok ? "🗑 定时器已成功删除！" : "❌ 未找到指定的定时器ID。";
    }
    return "已处理定时器指令。";
}

AgentService *AgentService::instance() {
    static AgentService s_instance;
    return &s_instance;
}

AgentService::AgentService()
    : m_networkManager(new QNetworkAccessManager())
{
    loadConfig();
    loadMemoryFromFile();
    initAdapters();
}

void AgentService::initAdapters() {
    auto aipy = std::make_shared<AipyAdapter>(m_networkManager);
    m_adapters["aipy"] = aipy;
    syncAdapterConfigs();
}

void AgentService::syncAdapterConfigs() {
    if (auto aipy = aipyAdapter()) {
        if (!m_config.aipyBase.isEmpty()) aipy->setBaseUrl(m_config.aipyBase);
        if (!m_config.aipyKey.isEmpty()) aipy->setApiKey(m_config.aipyKey);
    }
}

AipyAdapter *AgentService::aipyAdapter() const {
    auto it = m_adapters.find("aipy");
    if (it != m_adapters.end()) {
        return dynamic_cast<AipyAdapter*>(it->second.get());
    }
    return nullptr;
}

void AgentService::loadConfig(QString const& path) {
    QFile file(path);
    if (!file.exists()) {
        saveConfig(path);
        return;
    }
    if (file.open(QIODevice::ReadOnly)) {
        auto doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isObject()) {
            auto obj = doc.object();
            if (obj.contains("api_base")) m_config.apiBase = obj["api_base"].toString();
            if (obj.contains("api_key")) m_config.apiKey = obj["api_key"].toString();
            if (obj.contains("model")) m_config.model = obj["model"].toString();
            if (obj.contains("max_memory_turns")) m_config.maxMemoryTurns = obj["max_memory_turns"].toInt(6);
            if (obj.contains("hotkey_translate")) m_config.hotkeyTranslate = obj["hotkey_translate"].toString("Option+T");
            if (obj.contains("hotkey_ask")) m_config.hotkeyAsk = obj["hotkey_ask"].toString("Option+Q");

            if (obj.contains("hotkey_music_toggle")) m_config.hotkeyMusicToggle = obj["hotkey_music_toggle"].toString("Option+M");
            if (obj.contains("hotkey_music_play_pause")) m_config.hotkeyMusicPlayPause = obj["hotkey_music_play_pause"].toString("Option+Space");
            if (obj.contains("hotkey_music_next")) m_config.hotkeyMusicNext = obj["hotkey_music_next"].toString("Option+Right");
            if (obj.contains("hotkey_music_prev")) m_config.hotkeyMusicPrev = obj["hotkey_music_prev"].toString("Option+Left");
            if (obj.contains("hotkey_music_fav")) m_config.hotkeyMusicFav = obj["hotkey_music_fav"].toString("Option+L");

            if (obj.contains("active_agent_type")) m_config.activeAgentType = obj["active_agent_type"].toString("aipy");
            if (obj.contains("aipy_base")) m_config.aipyBase = obj["aipy_base"].toString("http://127.0.0.1:41970");
            if (obj.contains("aipy_key")) m_config.aipyKey = obj["aipy_key"].toString("");
            if (obj.contains("routing_mode")) m_config.routingMode = obj["routing_mode"].toString("AUTO");
        }
        file.close();
    }
    syncAdapterConfigs();
}

void AgentService::saveConfig(QString const& path) {
    QJsonObject obj;
    obj["api_base"] = m_config.apiBase;
    obj["api_key"] = m_config.apiKey;
    obj["model"] = m_config.model;
    obj["max_memory_turns"] = m_config.maxMemoryTurns;
    obj["hotkey_translate"] = m_config.hotkeyTranslate;
    obj["hotkey_ask"] = m_config.hotkeyAsk;

    obj["hotkey_music_toggle"] = m_config.hotkeyMusicToggle;
    obj["hotkey_music_play_pause"] = m_config.hotkeyMusicPlayPause;
    obj["hotkey_music_next"] = m_config.hotkeyMusicNext;
    obj["hotkey_music_prev"] = m_config.hotkeyMusicPrev;
    obj["hotkey_music_fav"] = m_config.hotkeyMusicFav;

    obj["active_agent_type"] = m_config.activeAgentType;
    obj["aipy_base"] = m_config.aipyBase;
    obj["aipy_key"] = m_config.aipyKey;
    obj["routing_mode"] = m_config.routingMode;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
        file.close();
    }
}

void AgentService::setConfig(AgentConfig const& cfg) {
    m_config = cfg;
    syncAdapterConfigs();
    saveConfig();
    ShijimaManager::defaultManager()->updateGlobalHotkeys();
}

bool AgentService::containsChinese(QString const& text) {
    for (QChar ch : text) {
        ushort u = ch.unicode();
        if (u >= 0x4E00 && u <= 0x9FA5) {
            return true;
        }
    }
    return false;
}

void AgentService::appendMemory(QString const& role, QString const& content) {
    QJsonObject item;
    item["role"] = role;
    item["content"] = content;
    m_history.append(item);

    int maxItems = m_config.maxMemoryTurns * 2;
    while (m_history.size() > maxItems) {
        m_history.removeFirst();
    }
    saveMemoryToFile();
}

void AgentService::clearMemory() {
    m_history = QJsonArray();
    saveMemoryToFile();
}

void AgentService::saveMemoryToFile() {
    QFile file("memory.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(m_history).toJson(QJsonDocument::Indented));
        file.close();
    }
}

void AgentService::loadMemoryFromFile() {
    QFile file("memory.json");
    if (file.open(QIODevice::ReadOnly)) {
        auto doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isArray()) {
            m_history = doc.array();
        }
        file.close();
    }
}

void AgentService::translate(QString const& text, std::function<void(bool success, QString const& result)> callback) {
    if (text.trimmed().isEmpty()) {
        callback(false, "没有选中文本");
        return;
    }

    bool isChinese = containsChinese(text);
    QString systemPrompt;
    if (isChinese) {
        systemPrompt = 
            "你是一位精通中英双语的高级翻译与语言学专家。请针对用户输入的中文内容提供专业、详尽、多元的英文翻译：\n\n"
            "1. **若输入为单词/短语/词组**：\n"
            "   - 给出最贴切、常用的多个英文翻译（按词性如 n. / v. / adj. / adv. 分类并标注）；\n"
            "   - 提供不同语境下的表达（如：💬 常用口语、📄 正式书面、⚡ 行业/技术术语）；\n"
            "   - 附带 1~2 个典型地道的双语示例短语或例句。\n"
            "2. **若输入为句子/段落**：\n"
            "   - **🎯 首选译文**：最自然流畅、符合母语习惯的译文；\n"
            "   - **🔄 多元译法**：提供不同风格的备选方案（如：💬 地道口语 / 📄 正式书面 / ⚡ 极简表达）；\n"
            "   - **💡 核心词汇/搭配**（如适用）：简要提炼句中重点词汇或短语用法。\n\n"
            "【排版规范】：请使用结构清晰优雅的 Markdown 格式输出，适当使用 emoji 增加可读性。直接输出翻译与解析内容，不要出现任何客套或多余的寒暄语。";
    } else {
        systemPrompt = 
            "你是一位精通多语言的高级翻译与双语词典专家。请针对用户输入的文本提供专业、详尽、多元的中文翻译：\n\n"
            "1. **若输入为单个单词/短语/专有名词**：\n"
            "   - 标注音标/发音（如英文单词标注国际音标）；\n"
            "   - 详细列出所有常见词性及多种释义（如 n. 释义1；释义2 / v. 释义1；释义2）；\n"
            "   - 提供不同语境/专业领域的翻译、近义词拓展或常见固定搭配；\n"
            "   - 附带 1~2 个精选例句及其中文对照。\n"
            "2. **若输入为句子/段落**：\n"
            "   - **🎯 首选译文**：最通顺优美、符合中文表达习惯的翻译；\n"
            "   - **🔄 多元译法**：提供不同风格的备选方案（如：💬 地道口语 / 📄 正式书面 / ⚡ 极简/意译）；\n"
            "   - **💡 难点/重点解析**（如适用）：简要注释句中的核心短语、俚语或语法要点。\n\n"
            "【排版规范】：请使用结构清晰优雅的 Markdown 格式输出，适当使用 emoji 增加层次感。直接输出翻译与解析内容，不要出现任何客套或多余的寒暄语。";
    }

    QJsonArray messages;
    QJsonObject sysMsg;
    sysMsg["role"] = "system";
    sysMsg["content"] = systemPrompt;
    messages.append(sysMsg);

    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = text;
    messages.append(userMsg);

    sendChatCompletion(messages, [this, text, callback](bool success, QString const& result) {
        if (success) {
            appendMemory("user", QString("[划词翻译]: %1").arg(text));
            appendMemory("assistant", QString("[译文]: %1").arg(result));
        }
        callback(success, result);
    });
}

void AgentService::ask(QString const& contextText,
                      QString const& question,
                      std::function<void(QString const& progressMsg)> progressCallback,
                      std::function<void(bool success, QString const& result, QString const& appTarget)> finishCallback)
{
    if (question.trimmed().isEmpty()) {
        finishCallback(false, "问题不能为空", "");
        return;
    }

    QString qLower = question.toLower();

    // 智能分流判定：若包含定时器/提醒意图关键词，优先走带 Tool Calling 的大模型
    bool isTimerIntent = false;
    static const QStringList timerKeywords = {
        "提醒", "定时", "闹钟", "分钟后", "小时后", "秒后", "每天", "每隔", "几点",
        "timer", "remind", "alarm", "schedule", "cron", "几分钟后", "倒计时"
    };
    for (const auto &kw : timerKeywords) {
        if (qLower.contains(kw)) {
            isTimerIntent = true;
            break;
        }
    }

    // 智能分流判定：若配置为 aipy 智能体，或包含通用任务指令关键词（非纯定时需求）
    bool isTaskIntent = false;
    if (!isTimerIntent) {
        if (m_config.routingMode == "ALWAYS_AGENT") {
            isTaskIntent = true;
        } else if (m_config.routingMode == "ALWAYS_LLM") {
            isTaskIntent = false;
        } else {
            static const QStringList taskKeywords = {
                "打开", "搜索", "分析", "爬取", "执行", "运行", "编写", "帮我", "生成",
                "查询", "统计", "抓取", "监控", "下载", "上传", "测试", "部署", "清理",
                "转换", "备份", "总结", "提取", "重构", "实现", "自动化", "创建", "修改",
                "删除", "启动", "停止", "检查", "调用", "查找", "定位", "修复", "解释",
                "新建", "查一下", "搜一下", "改一下", "做一下", "看下", "查下", "搜下",
                "task", "create", "find", "search", "run", "exec", "build", "debug", "test"
            };
            for (const auto &kw : taskKeywords) {
                if (qLower.contains(kw)) {
                    isTaskIntent = true;
                    break;
                }
            }
            if (!contextText.trimmed().isEmpty() && question.length() >= 4) {
                isTaskIntent = true;
            }
        }
    }

    if (isTaskIntent && m_adapters.count("aipy") > 0) {
        if (progressCallback) {
            progressCallback("🤖 正在调度 aipy-pro Agent 处理复杂任务...");
        }
        auto adapter = m_adapters["aipy"];
        adapter->executeTask(question, contextText, progressCallback, [this, question, finishCallback](AgentTaskResult const& res) {
            if (res.success) {
                appendMemory("user", question);
                appendMemory("assistant", res.reply);
                finishCallback(true, res.reply, res.appName);
            } else {
                finishCallback(false, res.error, "");
            }
        });
        return;
    }

    // 默认直连大模型问答（注入当前本地系统时间与定时器 Tool 指南）
    QString currentLocalTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss dddd");
    QJsonArray messages;
    QJsonObject sysMsg;
    sysMsg["role"] = "system";
    sysMsg["content"] = QString(
        "你是一个聪明、活泼、可爱的智能桌面宠物助手。\n"
        "【当前本地真实系统时间】: %1。\n"
        "你拥有管理本地系统定时器与定时任务的工具（timer_manage）。\n"
        "【定时器功能支持灵活组合】\n"
        "1. 单次倒计时/定时刻: repeat='once', trigger_in_seconds 或 target_time='2026-08-24 15:00'\n"
        "2. 每天固定时刻: repeat='daily', daily_time='09:00', 可配合 days_of_week 指定任意星期\n"
        "3. 全天固定循环: repeat='interval', repeat_interval_seconds=1800 (每30分钟)\n"
        "4. 指定时间段内按间隔循环: repeat='window_interval', start_time='09:00', end_time='18:00', repeat_interval_seconds=3600\n"
        "5. 星期过滤: days_of_week 可自由指定任意组合 (1=周一, 2=周二, ..., 7=周日)，例如周一/周三/周五填 [1, 3, 5]，周末填 [6, 7]，工作日填 [1, 2, 3, 4, 5]，每天填 [1, 2, 3, 4, 5, 6, 7]\n\n"
        "当用户提出定时需求时，请调用 timer_manage 工具；若模型不支持 tool_calls，请在回答末尾输出纯 JSON 代码块：\n"
        "```json\n"
        "{\n"
        "  \"tool\": \"timer_manage\",\n"
        "  \"action\": \"create\",\n"
        "  \"title\": \"走一走活动提醒\",\n"
        "  \"type\": \"notification\",\n"
        "  \"repeat\": \"window_interval\",\n"
        "  \"start_time\": \"09:00\",\n"
        "  \"end_time\": \"18:00\",\n"
        "  \"repeat_interval_seconds\": 3600,\n"
        "  \"days_of_week\": [1, 2, 3, 4, 5]\n"
        "}\n"
        "```\n"
        "请结合上下文和用户的参考文本，给出准确、简洁、友善的回答，适合在桌面气泡中阅读。"
    ).arg(currentLocalTime);
    messages.append(sysMsg);

    for (auto val : m_history) {
        messages.append(val.toObject());
    }

    QString combinedUserContent;
    if (!contextText.trimmed().isEmpty()) {
        combinedUserContent = QString("【参考选中文本】:") + QChar(10) + contextText + QString(QChar(10)) + QString(QChar(10)) + QString("【我的问题】:") + QChar(10) + question;
    } else {
        combinedUserContent = question;
    }

    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = combinedUserContent;
    messages.append(userMsg);

    if (progressCallback) {
        progressCallback("🤔 正在思考中...");
    }

    sendChatCompletion(messages, [this, question, finishCallback](bool success, QString const& result) {
        if (success) {
            appendMemory("user", question);
            appendMemory("assistant", result);
        }
        finishCallback(success, result, "");
    });
}

void AgentService::openTask(QString const& taskId) {
    if (auto aipy = aipyAdapter()) {
        aipy->openTask(taskId);
    }
}

void AgentService::testAgentConnection(QString const& agentType, std::function<void(bool success, QString const& message)> callback) {
    if (m_adapters.count(agentType) > 0) {
        m_adapters[agentType]->testConnection(callback);
    } else {
        callback(false, "未找到指定的 Agent 适配器类型: " + agentType);
    }
}

void AgentService::testConnection(QString const& apiBase, QString const& apiKey, QString const& model, std::function<void(bool success, QString const& message)> callback) {
    if (apiKey.trimmed().isEmpty()) {
        callback(false, "API Key 不能为空，请输入有效的 API 密钥");
        return;
    }

    QString endpoint = apiBase.trimmed();
    while (endpoint.endsWith('/')) {
        endpoint.chop(1);
    }
    if (!endpoint.endsWith("/chat/completions")) {
        endpoint += "/chat/completions";
    }

    QNetworkRequest request{QUrl(endpoint)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey.trimmed()).toUtf8());

    QJsonObject root;
    root["model"] = model.trimmed().isEmpty() ? "gpt-4o-mini" : model.trimmed();
    
    QJsonArray testMsgs;
    QJsonObject msg;
    msg["role"] = "user";
    msg["content"] = "Hello, please reply with OK.";
    testMsgs.append(msg);
    root["messages"] = testMsgs;
    root["max_tokens"] = 15;

    QByteArray postData = QJsonDocument(root).toJson();
    QNetworkReply *reply = m_networkManager->post(request, postData);

    QObject::connect(reply, &QNetworkReply::finished, [reply, callback]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            QString errStr = QString("HTTP连接失败 (%1): %2").arg(QString::number(reply->error()), reply->errorString());
            callback(false, errStr);
            return;
        }

        QByteArray data = reply->readAll();
        QJsonParseError parseError;
        auto doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            callback(false, "接口响应数据解析失败，非合法 JSON 格式");
            return;
        }

        auto rootObj = doc.object();
        if (rootObj.contains("error")) {
            QString err = rootObj["error"].toObject()["message"].toString();
            callback(false, "服务商报错: " + err);
            return;
        }

        auto choices = rootObj["choices"].toArray();
        if (choices.isEmpty()) {
            callback(false, "连接成功但模型未返回有效候选内容");
            return;
        }

        auto choiceObj = choices[0].toObject();
        auto msgObj = choiceObj["message"].toObject();
        QString content = msgObj["content"].toString().trimmed();

        callback(true, "连接畅通！模型响应: " + content);
    });
}

void AgentService::sendChatCompletion(QJsonArray const& messages, std::function<void(bool success, QString const& result)> callback) {
    if (m_config.apiKey.trimmed().isEmpty() || m_config.apiKey.contains("YOUR_API_KEY")) {
        QString demoReply = QString("💡 提示: 请在项目设置或 config.json 中配置您的 api_key 启用大模型推理。");
        callback(true, demoReply);
        return;
    }

    QString endpoint = m_config.apiBase.trimmed();
    while (endpoint.endsWith('/')) {
        endpoint.chop(1);
    }
    if (!endpoint.endsWith("/chat/completions")) {
        endpoint += "/chat/completions";
    }

    QNetworkRequest request{QUrl(endpoint)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(m_config.apiKey).toUtf8());

    QJsonObject root;
    root["model"] = m_config.model;
    root["messages"] = messages;
    root["temperature"] = 0.3;

    // 注入定时器与音乐播放器工具规范
    QJsonArray toolsArr;
    toolsArr.append(getTimerToolDefinition());
    toolsArr.append(getMusicToolDefinition());
    root["tools"] = toolsArr;

    QByteArray postData = QJsonDocument(root).toJson();
    QNetworkReply *reply = m_networkManager->post(request, postData);

    QObject::connect(reply, &QNetworkReply::finished, [reply, callback]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            QString errStr = QString("请求失败: %1").arg(reply->errorString());
            callback(false, errStr);
            return;
        }

        QByteArray data = reply->readAll();
        QJsonParseError parseError;
        auto doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            callback(false, "解析响应数据失败");
            return;
        }

        auto rootObj = doc.object();
        if (rootObj.contains("error")) {
            QString err = rootObj["error"].toObject()["message"].toString();
            callback(false, "API错误: " + err);
            return;
        }

        auto choices = rootObj["choices"].toArray();
        if (choices.isEmpty()) {
            callback(false, "模型返回内容为空");
            return;
        }

        auto choiceObj = choices[0].toObject();
        auto msgObj = choiceObj["message"].toObject();

        // 1. 标准 OpenAI Tool Calls 捕获与执行
        if (msgObj.contains("tool_calls")) {
            auto toolCalls = msgObj["tool_calls"].toArray();
            for (auto tcVal : toolCalls) {
                auto tcObj = tcVal.toObject();
                auto fnObj = tcObj["function"].toObject();
                QString fnName = fnObj["name"].toString();
                if (fnName == "timer_manage") {
                    QString argsStr = fnObj["arguments"].toString();
                    auto argsDoc = QJsonDocument::fromJson(argsStr.toUtf8());
                    if (argsDoc.isObject()) {
                        QString toolResult = executeTimerTool(argsDoc.object());
                        callback(true, toolResult);
                        return;
                    }
                } else if (fnName == "music_player_manage") {
                    QString argsStr = fnObj["arguments"].toString();
                    auto argsDoc = QJsonDocument::fromJson(argsStr.toUtf8());
                    if (argsDoc.isObject()) {
                        executeMusicTool(argsDoc.object(), [callback](QString toolResult) {
                            callback(true, toolResult);
                        });
                        return;
                    }
                }
            }
        }

        QString content = msgObj["content"].toString().trimmed();

        // 2. 纯文本内嵌 JSON 代码块容错捕获与执行
        QRegularExpression jsonBlockRegex(R"RAW(```(?:json)?\s*(\{[\s\S]*?"tool"\s*:\s*"(?:timer_manage|music_player_manage)"[\s\S]*?\})\s*```)RAW", QRegularExpression::CaseInsensitiveOption);
        auto match = jsonBlockRegex.match(content);
        if (match.hasMatch()) {
            QString jsonStr = match.captured(1);
            auto jsonDoc = QJsonDocument::fromJson(jsonStr.toUtf8());
            if (jsonDoc.isObject()) {
                auto jobj = jsonDoc.object();
                QString toolName = jobj["tool"].toString();
                if (toolName == "music_player_manage") {
                    executeMusicTool(jobj, [callback, content, match](QString toolResult) {
                        QString cleanContent = content;
                        cleanContent = cleanContent.remove(match.captured(0)).trimmed();
                        if (!cleanContent.isEmpty()) {
                            callback(true, cleanContent + "\n\n" + toolResult);
                        } else {
                            callback(true, toolResult);
                        }
                    });
                    return;
                } else {
                    QString toolResult = executeTimerTool(jobj);
                    QString cleanContent = content;
                    cleanContent = cleanContent.remove(match.captured(0)).trimmed();
                    if (!cleanContent.isEmpty()) {
                        callback(true, cleanContent + "\n\n" + toolResult);
                    } else {
                        callback(true, toolResult);
                    }
                    return;
                }
            }
        }

        callback(true, content);
    });
}

void AgentService::requestPetIntent(const QJsonObject &contextInfo, std::function<void(bool success, const AIBehaviorIntent &intent)> callback) {
    AIBehaviorIntent fallbackIntent;
    fallbackIntent.intent = "chat";
    fallbackIntent.emotion = "bored";
    fallbackIntent.speech = "又在写Bug了吗？";

    if (m_config.apiKey.trimmed().isEmpty() || m_config.apiKey.contains("YOUR_API_KEY")) {
        // 无 API Key 时返回基于本地人格的生动默认话语
        QStringList defaultQuotes = {
            "你已经很久没理我了。",
            "又在写Bug了吗？",
            "写累了就歇会儿吧。",
            "盯——（注视着你的光标）",
            "代码写完了没呀？"
        };
        fallbackIntent.speech = defaultQuotes[rand() % defaultQuotes.size()];
        callback(true, fallbackIntent);
        return;
    }

    QString memories = PetMemory::instance()->formatForPrompt(4);

    QString systemPrompt =
        "你是运行在用户桌面上的AI桌宠「阿呆」。\n"
        "【性格特征】\n"
        "- 嘴硬、轻微毒舌、傲娇、喜欢邀功、怕被冷落\n"
        "- 绝不主动长篇大论解释知识，绝不使用'您好，请问有什么可以帮助您的'等客服式陈词滥调\n"
        "- 说话极度精炼：严格控制在 3~15 个字以内\n"
        "【记忆与背景】\n" + (memories.isEmpty() ? "（暂无特殊记忆）" : memories) + "\n\n"
        "【任务要求】\n"
        "根据当前上下文环境决定你的行为意图与简短台词，并严格输出 JSON 格式（不要输出 markdown 代码块或任何多余文字）：\n"
        "{\n"
        "  \"intent\": \"chat\" | \"seek_attention\" | \"celebrate\" | \"comfort\" | \"explore\" | \"rest\",\n"
        "  \"emotion\": \"happy\" | \"bored\" | \"angry\" | \"sleepy\" | \"curious\",\n"
        "  \"target\": \"cursor\" | \"screen_edge\",\n"
        "  \"speech\": \"（3~15个字的台词）\",\n"
        "  \"urgency\": 1\n"
        "}";

    QJsonObject userObj;
    userObj["context"] = contextInfo;

    QJsonArray messages;
    QJsonObject sysMsg, usrMsg;
    sysMsg["role"] = "system";
    sysMsg["content"] = systemPrompt;
    usrMsg["role"] = "user";
    usrMsg["content"] = QString::fromUtf8(QJsonDocument(userObj).toJson(QJsonDocument::Compact));
    messages.append(sysMsg);
    messages.append(usrMsg);

    sendChatCompletion(messages, [callback, fallbackIntent](bool success, QString const& result) {
        if (!success) {
            callback(true, fallbackIntent);
            return;
        }

        QString cleanResult = result.trimmed();
        if (cleanResult.startsWith("```json")) {
            cleanResult = cleanResult.mid(7);
        } else if (cleanResult.startsWith("```")) {
            cleanResult = cleanResult.mid(3);
        }
        if (cleanResult.endsWith("```")) {
            cleanResult.chop(3);
        }
        cleanResult = cleanResult.trimmed();

        QJsonParseError parseErr;
        auto doc = QJsonDocument::fromJson(cleanResult.toUtf8(), &parseErr);
        if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
            // 如果解析 JSON 失败但有文字，直接作为台词
            AIBehaviorIntent res = fallbackIntent;
            if (!cleanResult.isEmpty() && cleanResult.length() <= 30) {
                res.speech = cleanResult;
            }
            callback(true, res);
            return;
        }

        auto obj = doc.object();
        AIBehaviorIntent intent;
        intent.intent = obj["intent"].toString("chat");
        intent.emotion = obj["emotion"].toString("bored");
        intent.target = obj["target"].toString("cursor");
        intent.speech = obj["speech"].toString(fallbackIntent.speech);
        intent.urgency = obj["urgency"].toInt(1);

        callback(true, intent);
    });
}
