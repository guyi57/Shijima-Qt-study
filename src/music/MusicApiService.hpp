#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <functional>
#include "MusicFavoriteDb.hpp"

class MusicApiService : public QObject
{
public:
    static MusicApiService* instance();

    // 1. 搜索曲目接口
    void search(const QString &keyword, 
                const QString &source = "netease", 
                int count = 20, 
                int page = 1, 
                std::function<void(bool success, const QVector<SongInfo>& songs, const QString &err)> callback = nullptr);

    // 2. 获取播放直链
    void fetchPlayUrl(const QString &source, 
                      const QString &id, 
                      int br = 999, 
                      std::function<void(bool success, const QString &url, int actualBr, const QString &err)> callback = nullptr);

    // 3. 获取专辑封面图片
    void fetchPicUrl(const QString &source, 
                     const QString &picId, 
                     int size = 500, 
                     std::function<void(bool success, const QString &picUrl, const QString &err)> callback = nullptr);

    // 4. 获取歌词接口 (返回 lrc 原文与 tlyric 中文翻译)
    void fetchLyric(const QString &source, 
                    const QString &lyricId, 
                    std::function<void(bool success, const QString &lrc, const QString &tlyric, const QString &err)> callback = nullptr);

    // 5. 便捷一站式解析：传入 SongInfo，自动填充 playUrl, picUrl, lyric
    void resolveSongDetails(SongInfo song, 
                            std::function<void(bool success, const SongInfo &resolvedSong, const QString &lrc, const QString &tlyric)> callback);

    // 可用音乐源列表
    static QStringList availableSources();
    static QString sourceDisplayName(const QString &source);

private:
    explicit MusicApiService(QObject *parent = nullptr);
    ~MusicApiService();

    QNetworkAccessManager *m_netManager = nullptr;
    QString m_apiBaseUrl = "https://music-api.gdstudio.xyz/api.php";
};
