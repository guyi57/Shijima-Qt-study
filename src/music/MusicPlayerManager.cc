#include "MusicPlayerManager.hpp"
#include "MusicApiService.hpp"
#include "BehaviorEngine.hpp"
#include "ShijimaWidget.hpp"
#include "PetEventBus.hpp"
#include <QUrl>
#include <QTimer>
#include <QRegularExpression>
#include <QRandomGenerator>
#include <QJsonObject>
#include <iostream>


MusicPlayerManager* MusicPlayerManager::instance()
{
    static MusicPlayerManager s_instance;
    return &s_instance;
}

MusicPlayerManager::MusicPlayerManager(QObject *parent)
    : QObject(parent)
{
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);
    m_audioOutput->setVolume(0.85f);

    connect(m_player, &QMediaPlayer::positionChanged, this, [this](qint64 pos) {
        onPlayerPositionChanged(pos);
    });
    connect(m_player, &QMediaPlayer::durationChanged, this, [this](qint64 dur) {
        onPlayerDurationChanged(dur);
    });
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        onPlayerPlaybackStateChanged(state);
    });
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        onMediaStatusChanged(status);
    });

    // 注册切歌时桌宠原地弹出气泡提示并向事件总线广播
    addSongChangedListener([](const SongInfo &song) {
        if (!song.id.isEmpty() && !song.name.isEmpty()) {
            QJsonObject payload;
            payload["song_name"] = song.name;
            payload["artist"] = song.artist;
            PetEventBus::instance()->emitEvent("music.playing", payload);
        }
    });


    // 自动从 SQLite 恢复上次持久化的播放列表与索引
    int savedIdx = 0;
    m_playlist = MusicFavoriteDb::instance()->loadPlaylist(savedIdx);
    if (!m_playlist.isEmpty()) {
        m_currentIndex = (savedIdx >= 0 && savedIdx < m_playlist.size()) ? savedIdx : 0;
    }
}

MusicPlayerManager::~MusicPlayerManager()
{
}

SongInfo MusicPlayerManager::currentSong() const
{
    if (m_currentIndex >= 0 && m_currentIndex < m_playlist.size()) {
        return m_playlist[m_currentIndex];
    }
    return SongInfo();
}

bool MusicPlayerManager::isPlaying() const
{
    return m_player->playbackState() == QMediaPlayer::PlayingState;
}

qint64 MusicPlayerManager::position() const
{
    return m_player->position();
}

qint64 MusicPlayerManager::duration() const
{
    return m_player->duration();
}

float MusicPlayerManager::volume() const
{
    return m_audioOutput->volume();
}

void MusicPlayerManager::setVolume(float volume)
{
    m_audioOutput->setVolume(std::clamp(volume, 0.0f, 1.0f));
}

void MusicPlayerManager::setPlaybackMode(PlaybackMode mode)
{
    m_mode = mode;
}

void MusicPlayerManager::playSong(const SongInfo &song)
{
    // 如果已经在列表中，找到索引并播放；否则插入并播放
    int foundIndex = -1;
    for (int i = 0; i < m_playlist.size(); ++i) {
        if (m_playlist[i].source == song.source && m_playlist[i].id == song.id) {
            foundIndex = i;
            break;
        }
    }

    if (foundIndex != -1) {
        m_currentIndex = foundIndex;
    } else {
        m_playlist.append(song);
        m_currentIndex = m_playlist.size() - 1;
        if (m_onPlaylistUpdated) m_onPlaylistUpdated();
    }

    MusicFavoriteDb::instance()->savePlaylist(m_playlist, m_currentIndex);

    SongInfo current = m_playlist[m_currentIndex];
    m_isCurrentSongFav = MusicFavoriteDb::instance()->isFavorite(current.source, current.id);
    if (m_onFavoriteStateChanged) m_onFavoriteStateChanged(m_isCurrentSongFav);

    std::cout << "[MusicPlayer] 开始准备播放: " << current.name.toStdString() << " - " << current.artist.toStdString() << std::endl;

    // 解析详情 (PlayUrl, PicUrl, Lyric)
    MusicApiService::instance()->resolveSongDetails(current, [this, current](bool success, const SongInfo &resolvedSong, const QString &lrc, const QString &tlyric) {
        if (!success || resolvedSong.playUrl.isEmpty()) {
            std::cerr << "[MusicPlayer] 获取播放链接失败: " << resolvedSong.name.toStdString() << std::endl;
            m_consecutiveErrors++;
            if (m_consecutiveErrors >= 3) {
                m_consecutiveErrors = 0;
                m_player->stop();
                if (m_onErrorOccurred) m_onErrorOccurred("连续多首歌曲暂无可用音频源，已暂停播放");
                ShijimaWidget *target = BehaviorEngine::instance()->activeWidget();
                if (target != nullptr) {
                    target->showMessage("🎵 暂无可用播放源，已为你暂停播放~", 3500);
                }
                return;
            }

            if (m_onErrorOccurred) m_onErrorOccurred(QString("《%1》暂无可用播放源，正在切换下一首...").arg(resolvedSong.name));
            QTimer::singleShot(800, this, [this]() {
                playNext();
            });
            return;
        }

        m_consecutiveErrors = 0;

        if (m_currentIndex >= 0 && m_currentIndex < m_playlist.size() && 
            m_playlist[m_currentIndex].id == resolvedSong.id) {
            m_playlist[m_currentIndex] = resolvedSong;
        }

        parseLrc(lrc, tlyric);
        for (auto &cb : m_songChangedListeners) {
            if (cb) cb(resolvedSong);
        }

        std::cout << "[MusicPlayer] 加载音频流: " << resolvedSong.playUrl.toStdString() << std::endl;
        m_player->setSource(QUrl(resolvedSong.playUrl));
        m_player->play();
    });
}


void MusicPlayerManager::playPlaylist(const QVector<SongInfo> &list, int startIndex)
{
    if (list.isEmpty()) return;
    m_playlist = list;
    if (m_onPlaylistUpdated) m_onPlaylistUpdated();
    if (startIndex >= 0 && startIndex < m_playlist.size()) {
        m_currentIndex = startIndex;
        MusicFavoriteDb::instance()->savePlaylist(m_playlist, m_currentIndex);
        playSong(m_playlist[m_currentIndex]);
    } else {
        MusicFavoriteDb::instance()->savePlaylist(m_playlist, 0);
    }
}

void MusicPlayerManager::addToPlaylist(const SongInfo &song)
{
    for (const auto &item : m_playlist) {
        if (item.source == song.source && item.id == song.id) return;
    }
    m_playlist.append(song);
    MusicFavoriteDb::instance()->savePlaylist(m_playlist, m_currentIndex);
    if (m_onPlaylistUpdated) m_onPlaylistUpdated();
}

int MusicPlayerManager::addBatchToPlaylist(const QVector<SongInfo> &songs)
{
    int addedCount = 0;
    for (const auto &song : songs) {
        bool exists = false;
        for (const auto &item : m_playlist) {
            if (item.source == song.source && item.id == song.id) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            m_playlist.append(song);
            addedCount++;
        }
    }
    if (addedCount > 0) {
        MusicFavoriteDb::instance()->savePlaylist(m_playlist, m_currentIndex);
        if (m_onPlaylistUpdated) m_onPlaylistUpdated();
    }
    return addedCount;
}

void MusicPlayerManager::removeFromPlaylist(int index)
{
    if (index < 0 || index >= m_playlist.size()) return;

    bool isRemovingCurrent = (index == m_currentIndex);
    m_playlist.removeAt(index);

    if (m_playlist.isEmpty()) {
        autoRefillRecommendationsIfNeeded(true);
        return;
    }

    if (isRemovingCurrent) {
        if (m_currentIndex >= m_playlist.size()) {
            m_currentIndex = 0;
        }
        MusicFavoriteDb::instance()->savePlaylist(m_playlist, m_currentIndex);
        if (m_onPlaylistUpdated) m_onPlaylistUpdated();
        playSong(m_playlist[m_currentIndex]);
    } else {
        if (index < m_currentIndex) {
            m_currentIndex--;
        }
        MusicFavoriteDb::instance()->savePlaylist(m_playlist, m_currentIndex);
        if (m_onPlaylistUpdated) m_onPlaylistUpdated();
    }

    // 少于 3 首提前无缝续接
    if (m_playlist.size() < 3) {
        autoRefillRecommendationsIfNeeded(false);
    }
}

void MusicPlayerManager::clearPlaylist()
{
    m_player->stop();
    m_playlist.clear();
    m_currentIndex = -1;
    m_parsedLyrics.clear();
    m_currentLyricIndex = -1;
    MusicFavoriteDb::instance()->savePlaylist(m_playlist, -1);
    if (m_onPlaylistUpdated) m_onPlaylistUpdated();
    for (auto &cb : m_songChangedListeners) {
        if (cb) cb(SongInfo());
    }
}

void MusicPlayerManager::autoRefillRecommendationsIfNeeded(bool autoPlay)
{
    if (m_playlist.size() >= 3 || m_isRefilling) return;
    m_isRefilling = true;

    QVector<SongInfo> favorites = MusicFavoriteDb::instance()->getFavorites();
    std::cout << "[MusicPlayer] 播放列表剩余少于 3 首 (当前 " << m_playlist.size() << " 首)，正在基于 " << favorites.size() << " 首收藏歌曲智能推荐 10 首曲目..." << std::endl;

    auto finishRefill = [this, autoPlay](const QVector<SongInfo> &recommended) {
        m_isRefilling = false;
        if (recommended.isEmpty()) {
            if (m_playlist.isEmpty()) {
                clearPlaylist();
            }
            return;
        }

        bool wasEmpty = m_playlist.isEmpty();

        // 智能追加：去重后追加到列表末尾
        int added = 0;
        for (const auto &s : recommended) {
            bool exists = false;
            for (const auto &item : m_playlist) {
                if (item.source == s.source && item.id == s.id) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                m_playlist.append(s);
                added++;
                if (added >= 10) break;
            }
        }

        if (wasEmpty) {
            m_currentIndex = 0;
        }

        MusicFavoriteDb::instance()->savePlaylist(m_playlist, m_currentIndex);
        if (m_onPlaylistUpdated) m_onPlaylistUpdated();

        // 提示桌宠气泡 (紧凑萌系气泡，零失焦)
        ShijimaWidget *target = BehaviorEngine::instance()->activeWidget();
        if (target != nullptr) {
            if (wasEmpty) {
                target->showMessage("🎵 播放列表已空，根据你的收藏为你推荐了 10 首好歌~ ✨", 4000);
            } else {
                target->showMessage("🎵 待播曲目快见底啦，已为你自动续上 10 首推荐好歌~ ✨", 4000);
            }
        }

        if (autoPlay && wasEmpty && !m_playlist.isEmpty()) {
            playSong(m_playlist[0]);
        }
    };

    // 1. 若有收藏歌曲，基于收藏喜好推荐
    if (!favorites.isEmpty()) {
        std::vector<SongInfo> shuffledFav(favorites.begin(), favorites.end());
        std::shuffle(shuffledFav.begin(), shuffledFav.end(), *QRandomGenerator::global());

        // 提取收藏歌手与歌曲关键词
        QStringList artists;
        for (const auto &s : shuffledFav) {
            QString art = s.artist.trimmed();
            if (!art.isEmpty() && art != "未知歌手" && !artists.contains(art)) {
                artists.append(art);
            }
        }

        QString searchKeyword = artists.isEmpty() ? shuffledFav[0].name : artists[QRandomGenerator::global()->bounded(artists.size())];
        QString searchSource = shuffledFav[0].source.isEmpty() ? "netease" : shuffledFav[0].source;

        MusicApiService::instance()->search(searchKeyword, searchSource, 15, 1, [this, shuffledFav, finishRefill](bool success, const QVector<SongInfo> &searchResult, const QString &) {
            QVector<SongInfo> pool;
            // 优先混入 3~5 首收藏中的精品歌曲
            for (size_t i = 0; i < shuffledFav.size() && pool.size() < 5; ++i) {
                pool.append(shuffledFav[i]);
            }

            // 混入关联推荐歌曲
            if (success) {
                for (const auto &s : searchResult) {
                    bool exists = false;
                    for (const auto &p : pool) {
                        if (p.id == s.id || (p.name == s.name && p.artist == s.artist)) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        pool.append(s);
                        if (pool.size() >= 10) break;
                    }
                }
            }

            // 若仍不足 10 首，使用其余收藏歌曲补齐
            for (size_t i = pool.size(); i < shuffledFav.size() && pool.size() < 10; ++i) {
                bool exists = false;
                for (const auto &p : pool) {
                    if (p.id == shuffledFav[i].id) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) pool.append(shuffledFav[i]);
            }

            std::shuffle(pool.begin(), pool.end(), *QRandomGenerator::global());
            if (pool.size() > 10) {
                pool.resize(10);
            }

            finishRefill(pool);
        });
    } else {
        // 2. 没有任何收藏歌曲时的冷启动：搜索热门流行精选
        static const QStringList fallbackKeywords = { "华语热歌", "流行精选", "治愈系", "轻音乐", "欧美流行", "ACG精选" };
        QString kw = fallbackKeywords[QRandomGenerator::global()->bounded(static_cast<int>(fallbackKeywords.size()))];
        MusicApiService::instance()->search(kw, "netease", 15, 1, [finishRefill](bool success, const QVector<SongInfo> &searchResult, const QString &) {
            QVector<SongInfo> pool = searchResult;
            std::shuffle(pool.begin(), pool.end(), *QRandomGenerator::global());
            if (pool.size() > 10) pool.resize(10);
            finishRefill(pool);
        });
    }
}



void MusicPlayerManager::play()
{
    if (m_player->playbackState() == QMediaPlayer::PausedState) {
        m_player->play();
    } else if (m_currentIndex >= 0 && m_currentIndex < m_playlist.size()) {
        playSong(m_playlist[m_currentIndex]);
    } else if (!m_playlist.isEmpty()) {
        playSong(m_playlist[0]);
    }
}

void MusicPlayerManager::pause()
{
    m_player->pause();
}

void MusicPlayerManager::togglePlay()
{
    if (isPlaying()) {
        pause();
    } else {
        play();
    }
}

void MusicPlayerManager::playNext()
{
    if (m_playlist.isEmpty()) return;

    if (m_mode == PlaybackMode::Random && m_playlist.size() > 1) {
        int nextIdx = m_currentIndex;
        while (nextIdx == m_currentIndex) {
            nextIdx = QRandomGenerator::global()->bounded(m_playlist.size());
        }
        m_currentIndex = nextIdx;
    } else {
        m_currentIndex = (m_currentIndex + 1) % m_playlist.size();
    }

    playSong(m_playlist[m_currentIndex]);
}

void MusicPlayerManager::playPrevious()
{
    if (m_playlist.isEmpty()) return;

    if (m_mode == PlaybackMode::Random && m_playlist.size() > 1) {
        int prevIdx = m_currentIndex;
        while (prevIdx == m_currentIndex) {
            prevIdx = QRandomGenerator::global()->bounded(m_playlist.size());
        }
        m_currentIndex = prevIdx;
    } else {
        m_currentIndex = (m_currentIndex - 1 + m_playlist.size()) % m_playlist.size();
    }

    playSong(m_playlist[m_currentIndex]);
}

void MusicPlayerManager::seek(qint64 positionMs)
{
    m_player->setPosition(positionMs);
}

void MusicPlayerManager::toggleFavoriteCurrent()
{
    SongInfo cur = currentSong();
    if (cur.id.isEmpty()) return;

    if (m_isCurrentSongFav) {
        MusicFavoriteDb::instance()->removeFavorite(cur.source, cur.id);
        m_isCurrentSongFav = false;
    } else {
        MusicFavoriteDb::instance()->addFavorite(cur);
        m_isCurrentSongFav = true;
    }
    if (m_onFavoriteStateChanged) m_onFavoriteStateChanged(m_isCurrentSongFav);
}

bool MusicPlayerManager::isCurrentSongFavorite() const
{
    return m_isCurrentSongFav;
}

void MusicPlayerManager::onPlayerPositionChanged(qint64 position)
{
    updateLyricLine(position);
    if (m_onPositionChanged) m_onPositionChanged(position, m_player->duration());
}

void MusicPlayerManager::onPlayerDurationChanged(qint64 duration)
{
    if (m_onPositionChanged) m_onPositionChanged(m_player->position(), duration);
}

void MusicPlayerManager::onPlayerPlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    if (state == QMediaPlayer::PlayingState) {
        m_consecutiveErrors = 0;
    }
    if (m_onPlayStateChanged) m_onPlayStateChanged(state == QMediaPlayer::PlayingState);
}


void MusicPlayerManager::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::EndOfMedia) {
        std::cout << "[MusicPlayer] 当前曲目播放结束" << std::endl;
        if (m_mode == PlaybackMode::SingleLoop) {
            m_player->setPosition(0);
            m_player->play();
        } else if (m_autoRemovePlayed && m_currentIndex >= 0 && m_currentIndex < m_playlist.size()) {
            // 消费型待播队列：播完自动移出列表
            std::cout << "[MusicPlayer] 播完自动移出播放列表: " << m_playlist[m_currentIndex].name.toStdString() << std::endl;
            m_playlist.removeAt(m_currentIndex);
            if (m_playlist.isEmpty()) {
                autoRefillRecommendationsIfNeeded(true);
            } else {
                if (m_currentIndex >= m_playlist.size()) {
                    m_currentIndex = 0;
                }
                MusicFavoriteDb::instance()->savePlaylist(m_playlist, m_currentIndex);
                if (m_onPlaylistUpdated) m_onPlaylistUpdated();
                playSong(m_playlist[m_currentIndex]);

                // 若剩余待播曲目少于 3 首，提前触发推荐补充，实现无缝续接！
                if (m_playlist.size() < 3) {
                    autoRefillRecommendationsIfNeeded(false);
                }
            }
        } else {
            playNext();
            if (m_playlist.size() < 3) {
                autoRefillRecommendationsIfNeeded(false);
            }
        }

    }
}


void MusicPlayerManager::parseLrc(const QString &lrc, const QString &tlyric)
{
    m_parsedLyrics.clear();
    m_currentLyricIndex = -1;

    if (lrc.isEmpty()) return;

    // 解析翻译歌词映射
    QMap<qint64, QString> transMap;
    if (!tlyric.isEmpty()) {
        QRegularExpression lrcRe(R"(\[(\d{2}):(\d{2})\.(\d{2,3})\](.*))");
        for (const QString &line : tlyric.split('\n')) {
            auto match = lrcRe.match(line.trimmed());
            if (match.hasMatch()) {
                qint64 min = match.captured(1).toLongLong();
                qint64 sec = match.captured(2).toLongLong();
                qint64 ms = match.captured(3).toLongLong();
                if (match.captured(3).length() == 2) ms *= 10;
                qint64 totalMs = min * 60000 + sec * 1000 + ms;
                transMap[totalMs] = match.captured(4).trimmed();
            }
        }
    }

    // 解析主歌词
    QRegularExpression lrcRe(R"(\[(\d{2}):(\d{2})\.(\d{2,3})\](.*))");
    for (const QString &line : lrc.split('\n')) {
        auto match = lrcRe.match(line.trimmed());
        if (match.hasMatch()) {
            qint64 min = match.captured(1).toLongLong();
            qint64 sec = match.captured(2).toLongLong();
            qint64 ms = match.captured(3).toLongLong();
            if (match.captured(3).length() == 2) ms *= 10;
            qint64 totalMs = min * 60000 + sec * 1000 + ms;

            QString text = match.captured(4).trimmed();
            if (!text.isEmpty()) {
                LyricLine ll;
                ll.timestampMs = totalMs;
                ll.text = text;
                if (transMap.contains(totalMs)) {
                    ll.translation = transMap[totalMs];
                }
                m_parsedLyrics.append(ll);
            }
        }
    }

    // 按时间排序
    std::sort(m_parsedLyrics.begin(), m_parsedLyrics.end(), [](const LyricLine &a, const LyricLine &b) {
        return a.timestampMs < b.timestampMs;
    });
}

void MusicPlayerManager::updateLyricLine(qint64 positionMs)
{
    if (m_parsedLyrics.isEmpty()) return;

    int activeIdx = -1;
    for (int i = 0; i < m_parsedLyrics.size(); ++i) {
        if (positionMs >= m_parsedLyrics[i].timestampMs) {
            activeIdx = i;
        } else {
            break;
        }
    }

    if (activeIdx != m_currentLyricIndex && activeIdx >= 0 && activeIdx < m_parsedLyrics.size()) {
        m_currentLyricIndex = activeIdx;
        if (m_onLyricLineChanged) {
            m_onLyricLineChanged(activeIdx, m_parsedLyrics[activeIdx].text, m_parsedLyrics[activeIdx].translation);
        }
    }
}
