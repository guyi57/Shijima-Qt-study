#include "MusicApiService.hpp"
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <iostream>

MusicApiService* MusicApiService::instance()
{
    static MusicApiService s_instance;
    return &s_instance;
}

MusicApiService::MusicApiService(QObject *parent)
    : QObject(parent)
{
    m_netManager = new QNetworkAccessManager(this);
}

MusicApiService::~MusicApiService()
{
}

QStringList MusicApiService::availableSources()
{
    return { "netease", "kuwo", "bilibili" };
}

QString MusicApiService::sourceDisplayName(const QString &source)
{
    if (source == "netease") return "网易云音乐 (推荐)";
    if (source == "kuwo") return "酷我音乐 (推荐)";
    if (source == "bilibili") return "哔哩哔哩音频";
    if (source == "tencent" || source == "qq") return "QQ音乐 (接口维护中)";
    if (source == "apple") return "Apple Music (接口维护中)";
    if (source == "ytmusic") return "YouTube Music (接口维护中)";
    if (source == "spotify") return "Spotify (接口维护中)";
    return source;
}

static QNetworkRequest buildRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36");
    request.setRawHeader("Accept", "application/json, text/plain, */*");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}

void MusicApiService::search(const QString &keyword, 
                             const QString &source, 
                             int count, 
                             int page, 
                             std::function<void(bool, const QVector<SongInfo>&, const QString&)> callback)
{
    if (keyword.trimmed().isEmpty()) {
        if (callback) callback(false, {}, "搜索关键词不能为空");
        return;
    }

    QString actualSource = source.trimmed().toLower();
    if (actualSource == "qq" || actualSource == "tencent") {
        actualSource = "netease"; // 自动容错降级至网易云
    } else if (actualSource.isEmpty()) {
        actualSource = "netease";
    }

    QUrl url(m_apiBaseUrl);
    QUrlQuery query;
    query.addQueryItem("types", "search");
    query.addQueryItem("source", actualSource);
    query.addQueryItem("name", keyword.trimmed());
    query.addQueryItem("count", QString::number(count <= 0 ? 20 : count));
    query.addQueryItem("pages", QString::number(page <= 0 ? 1 : page));
    url.setQuery(query);

    std::cout << "[MusicAPI] 发起搜索: " << url.toString().toStdString() << std::endl;

    QNetworkReply *reply = m_netManager->get(buildRequest(url));

    connect(reply, &QNetworkReply::finished, this, [reply, callback, actualSource]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QString errDetail;
            auto errDoc = QJsonDocument::fromJson(data);
            if (errDoc.isObject() && errDoc.object().contains("detail")) {
                errDetail = errDoc.object()["detail"].toString();
            }
            QString err = errDetail.isEmpty() ? reply->errorString() : QString("音源提示: %1 (建议切换为酷我或网易云)").arg(errDetail);
            std::cerr << "[MusicAPI] 搜索网络错误: " << err.toStdString() << std::endl;
            if (callback) callback(false, {}, err);
            return;
        }

        QByteArray data = reply->readAll();
        QJsonParseError parseErr;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
        if (parseErr.error != QJsonParseError::NoError || !doc.isArray()) {
            QString errDetail;
            if (doc.isObject() && doc.object().contains("detail")) {
                errDetail = doc.object()["detail"].toString();
            }
            QString err = errDetail.isEmpty() ? ("搜索响应解析失败: " + parseErr.errorString()) : QString("音源提示: %1").arg(errDetail);
            std::cerr << "[MusicAPI] " << err.toStdString() << " (Raw: " << data.left(100).toStdString() << ")" << std::endl;
            if (callback) callback(false, {}, err);
            return;
        }

        QVector<SongInfo> songs;
        QJsonArray arr = doc.array();
        for (const auto &val : arr) {
            if (!val.isObject()) continue;
            QJsonObject obj = val.toObject();

            SongInfo s;
            s.id = obj["id"].toVariant().toString();
            s.name = obj["name"].toString();
            s.source = obj.contains("source") ? obj["source"].toString() : actualSource;
            s.album = obj["album"].toString();
            s.picId = obj["pic_id"].toVariant().toString();
            s.lyricId = obj["lyric_id"].toVariant().toString();
            if (s.lyricId.isEmpty()) s.lyricId = s.id;

            // 歌手处理 (可能是数组或者字符串)
            if (obj["artist"].isArray()) {
                QStringList artList;
                for (const auto &artVal : obj["artist"].toArray()) {
                    artList << artVal.toString();
                }
                s.artist = artList.join(" / ");
            } else {
                s.artist = obj["artist"].toString();
            }

            if (!s.id.isEmpty() && !s.name.isEmpty()) {
                songs.append(s);
            }
        }

        std::cout << "[MusicAPI] 搜索成功，返回 " << songs.size() << " 首歌曲" << std::endl;
        if (callback) callback(true, songs, "");
    });
}

void MusicApiService::fetchPlayUrl(const QString &source, 
                                   const QString &id, 
                                   int br, 
                                   std::function<void(bool, const QString&, int, const QString&)> callback)
{
    QString actualSource = source.isEmpty() ? "netease" : source;
    QUrl url(m_apiBaseUrl);
    QUrlQuery query;
    query.addQueryItem("types", "url");
    query.addQueryItem("source", actualSource);
    query.addQueryItem("id", id);
    query.addQueryItem("br", QString::number(br > 0 ? br : 999));
    url.setQuery(query);

    QNetworkReply *reply = m_netManager->get(buildRequest(url));

    connect(reply, &QNetworkReply::finished, this, [reply, callback, actualSource, id]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            if (actualSource == "netease") {
                QString fallbackUrl = QString("https://music.163.com/song/media/outer/url?id=%1.mp3").arg(id);
                if (callback) callback(true, fallbackUrl, 320, "");
                return;
            }
            if (callback) callback(false, "", 0, reply->errorString());
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            if (actualSource == "netease") {
                QString fallbackUrl = QString("https://music.163.com/song/media/outer/url?id=%1.mp3").arg(id);
                if (callback) callback(true, fallbackUrl, 320, "");
                return;
            }
            if (callback) callback(false, "", 0, "无法解析播放链接响应");
            return;
        }

        QJsonObject obj = doc.object();
        QString playUrl = obj["url"].toString();
        int actualBr = obj["br"].toInt(999);

        if (playUrl.isEmpty()) {
            if (actualSource == "netease") {
                QString fallbackUrl = QString("https://music.163.com/song/media/outer/url?id=%1.mp3").arg(id);
                if (callback) callback(true, fallbackUrl, 320, "");
                return;
            }
            if (callback) callback(false, "", 0, "未获取到有效的音频播放直链");
            return;
        }

        if (callback) callback(true, playUrl, actualBr, "");
    });
}


void MusicApiService::fetchPicUrl(const QString &source, 
                                  const QString &picId, 
                                  int size, 
                                  std::function<void(bool, const QString&, const QString&)> callback)
{
    if (picId.isEmpty()) {
        if (callback) callback(false, "", "picId 为空");
        return;
    }

    QUrl url(m_apiBaseUrl);
    QUrlQuery query;
    query.addQueryItem("types", "pic");
    query.addQueryItem("source", source.isEmpty() ? "netease" : source);
    query.addQueryItem("id", picId);
    query.addQueryItem("size", QString::number(size > 0 ? size : 500));
    url.setQuery(query);

    QNetworkReply *reply = m_netManager->get(buildRequest(url));

    connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            if (callback) callback(false, "", reply->errorString());
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            if (callback) callback(false, "", "无法解析封面图片响应");
            return;
        }

        QJsonObject obj = doc.object();
        QString picUrl = obj["url"].toString();
        if (picUrl.isEmpty()) {
            if (callback) callback(false, "", "未获取到有效的专辑封面直链");
            return;
        }

        if (callback) callback(true, picUrl, "");
    });
}

void MusicApiService::fetchLyric(const QString &source, 
                                 const QString &lyricId, 
                                 std::function<void(bool, const QString&, const QString&, const QString&)> callback)
{
    if (lyricId.isEmpty()) {
        if (callback) callback(false, "", "", "lyricId 为空");
        return;
    }

    QUrl url(m_apiBaseUrl);
    QUrlQuery query;
    query.addQueryItem("types", "lyric");
    query.addQueryItem("source", source.isEmpty() ? "netease" : source);
    query.addQueryItem("id", lyricId);
    url.setQuery(query);

    QNetworkReply *reply = m_netManager->get(buildRequest(url));

    connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            if (callback) callback(false, "", "", reply->errorString());
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            if (callback) callback(false, "", "", "无法解析歌词响应");
            return;
        }

        QJsonObject obj = doc.object();
        QString lrc = obj["lyric"].toString();
        QString tlyric = obj["tlyric"].toString();

        if (callback) callback(true, lrc, tlyric, "");
    });
}

void MusicApiService::resolveSongDetails(SongInfo song, 
                                         std::function<void(bool, const SongInfo&, const QString&, const QString&)> callback)
{
    // 并行获取 PlayUrl、PicUrl、Lyric
    struct Context {
        SongInfo song;
        QString lyric;
        QString tlyric;
        bool playUrlDone = false;
        bool picUrlDone = false;
        bool lyricDone = false;
        bool playUrlSuccess = false;
    };

    auto ctx = std::make_shared<Context>();
    ctx->song = song;

    auto checkFinished = [ctx, callback]() {
        if (ctx->playUrlDone && ctx->picUrlDone && ctx->lyricDone) {
            if (ctx->playUrlSuccess && !ctx->song.playUrl.isEmpty()) {
                if (callback) callback(true, ctx->song, ctx->lyric, ctx->tlyric);
            } else {
                if (callback) callback(false, ctx->song, "", "");
            }
        }
    };

    // 1. 获取 PlayUrl
    fetchPlayUrl(song.source, song.id, song.br, [ctx, checkFinished](bool success, const QString &url, int br, const QString &) {
        ctx->playUrlDone = true;
        ctx->playUrlSuccess = success;
        if (success) {
            ctx->song.playUrl = url;
            ctx->song.br = br;
        }
        checkFinished();
    });

    // 2. 获取 PicUrl
    if (!song.picUrl.isEmpty()) {
        ctx->picUrlDone = true;
        checkFinished();
    } else if (!song.picId.isEmpty()) {
        fetchPicUrl(song.source, song.picId, 500, [ctx, checkFinished](bool success, const QString &picUrl, const QString &) {
            ctx->picUrlDone = true;
            if (success) ctx->song.picUrl = picUrl;
            checkFinished();
        });
    } else {
        ctx->picUrlDone = true;
        checkFinished();
    }

    // 3. 获取 Lyric
    QString lid = song.lyricId.isEmpty() ? song.id : song.lyricId;
    fetchLyric(song.source, lid, [ctx, checkFinished](bool success, const QString &lrc, const QString &tlyric, const QString &) {
        ctx->lyricDone = true;
        if (success) {
            ctx->lyric = lrc;
            ctx->tlyric = tlyric;
        }
        checkFinished();
    });
}
