#include "MusicFavoriteDb.hpp"
#include <sqlite3.h>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <iostream>


MusicFavoriteDb* MusicFavoriteDb::instance()
{
    static MusicFavoriteDb s_instance;
    return &s_instance;
}

MusicFavoriteDb::MusicFavoriteDb()
{
    initDb();
}

MusicFavoriteDb::~MusicFavoriteDb()
{
    if (m_sqliteHandle) {
        sqlite3_close(static_cast<sqlite3*>(m_sqliteHandle));
        m_sqliteHandle = nullptr;
    }
}

bool MusicFavoriteDb::initDb()
{
    if (m_sqliteHandle != nullptr) return true;

    QString configDir = QDir::homePath() + "/.config/guyi-bot";
    QDir().mkpath(configDir);
    m_dbPath = configDir + "/pet_music.db";

    QString oldPath = QDir::homePath() + "/.config/shijima-qt/pet_music.db";
    if (!QFile::exists(m_dbPath) && QFile::exists(oldPath)) {
        QFile::copy(oldPath, m_dbPath);
    }

    sqlite3 *db = nullptr;
    int rc = sqlite3_open(m_dbPath.toUtf8().constData(), &db);
    if (rc != SQLITE_OK) {
        std::cerr << "[MusicDB] 打开数据库失败: " << sqlite3_errmsg(db) << std::endl;
        if (db) sqlite3_close(db);
        return false;
    }

    m_sqliteHandle = db;

    const char *createTableSql = 
        "CREATE TABLE IF NOT EXISTS favorite_music ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  track_id TEXT NOT NULL,"
        "  source TEXT NOT NULL,"
        "  name TEXT NOT NULL,"
        "  artist TEXT,"
        "  album TEXT,"
        "  pic_id TEXT,"
        "  pic_url TEXT,"
        "  lyric_id TEXT,"
        "  br INTEGER DEFAULT 999,"
        "  created_at INTEGER,"
        "  UNIQUE(source, track_id)"
        ");"
        "CREATE TABLE IF NOT EXISTS playlist_state ("
        "  id INTEGER PRIMARY KEY CHECK (id = 1),"
        "  playlist_json TEXT,"
        "  current_index INTEGER DEFAULT 0"
        ");"
        "CREATE TABLE IF NOT EXISTS search_history ("
        "  keyword TEXT PRIMARY KEY,"
        "  searched_at INTEGER"
        ");";

    char *errMsg = nullptr;
    rc = sqlite3_exec(db, createTableSql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[MusicDB] 初始化表结构失败: " << (errMsg ? errMsg : "") << std::endl;
        if (errMsg) sqlite3_free(errMsg);
        return false;
    }

    std::cout << "[MusicDB] 音乐收藏数据库初始化成功: " << m_dbPath.toStdString() << std::endl;
    return true;
}

bool MusicFavoriteDb::addFavorite(const SongInfo &song)
{
    if (!m_sqliteHandle) initDb();
    sqlite3 *db = static_cast<sqlite3*>(m_sqliteHandle);
    if (!db) return false;

    const char *sql = 
        "INSERT OR REPLACE INTO favorite_music "
        "(track_id, source, name, artist, album, pic_id, pic_url, lyric_id, br, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    qint64 now = song.createdAt > 0 ? song.createdAt : QDateTime::currentMSecsSinceEpoch();

    sqlite3_bind_text(stmt, 1, song.id.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, song.source.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, song.name.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, song.artist.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, song.album.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, song.picId.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, song.picUrl.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, song.lyricId.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 9, song.br);
    sqlite3_bind_int64(stmt, 10, now);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    std::cout << "[MusicDB] 添加收藏: " << song.name.toStdString() << " - " << song.artist.toStdString() << " (成功: " << (ok ? "是" : "否") << ")" << std::endl;
    return ok;
}

bool MusicFavoriteDb::removeFavorite(const QString &source, const QString &id)
{
    if (!m_sqliteHandle) initDb();
    sqlite3 *db = static_cast<sqlite3*>(m_sqliteHandle);
    if (!db) return false;

    const char *sql = "DELETE FROM favorite_music WHERE source = ? AND track_id = ?;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, source.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, id.toUtf8().constData(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    std::cout << "[MusicDB] 取消收藏: source=" << source.toStdString() << ", id=" << id.toStdString() << std::endl;
    return ok;
}

bool MusicFavoriteDb::isFavorite(const QString &source, const QString &id)
{
    if (!m_sqliteHandle) initDb();
    sqlite3 *db = static_cast<sqlite3*>(m_sqliteHandle);
    if (!db) return false;

    const char *sql = "SELECT COUNT(*) FROM favorite_music WHERE source = ? AND track_id = ?;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, source.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, id.toUtf8().constData(), -1, SQLITE_TRANSIENT);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        found = (sqlite3_column_int(stmt, 0) > 0);
    }
    sqlite3_finalize(stmt);
    return found;
}

QVector<SongInfo> MusicFavoriteDb::getFavorites(const QString &keyword)
{
    QVector<SongInfo> list;
    if (!m_sqliteHandle) initDb();
    sqlite3 *db = static_cast<sqlite3*>(m_sqliteHandle);
    if (!db) return list;

    QString sqlStr = "SELECT track_id, source, name, artist, album, pic_id, pic_url, lyric_id, br, created_at FROM favorite_music ";
    if (!keyword.trimmed().isEmpty()) {
        sqlStr += "WHERE name LIKE ? OR artist LIKE ? OR album LIKE ? ";
    }
    sqlStr += "ORDER BY created_at DESC;";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sqlStr.toUtf8().constData(), -1, &stmt, nullptr) != SQLITE_OK) {
        return list;
    }

    if (!keyword.trimmed().isEmpty()) {
        QString kwPattern = "%" + keyword.trimmed() + "%";
        sqlite3_bind_text(stmt, 1, kwPattern.toUtf8().constData(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, kwPattern.toUtf8().constData(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, kwPattern.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SongInfo s;
        s.id = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        s.source = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
        s.name = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
        s.artist = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
        s.album = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
        s.picId = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)));
        s.picUrl = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6)));
        s.lyricId = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7)));
        s.br = sqlite3_column_int(stmt, 8);
        s.createdAt = sqlite3_column_int64(stmt, 9);
        list.append(s);
    }

    sqlite3_finalize(stmt);
    return list;
}

QJsonArray MusicFavoriteDb::getAllFavoritesJson()
{
    QJsonArray arr;
    auto favs = getFavorites();
    for (const auto &song : favs) {
        arr.append(song.toJson());
    }
    return arr;
}

int MusicFavoriteDb::getFavoriteCount()
{
    if (!m_sqliteHandle) initDb();
    sqlite3 *db = static_cast<sqlite3*>(m_sqliteHandle);
    if (!db) return 0;

    const char *sql = "SELECT COUNT(*) FROM favorite_music;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

void MusicFavoriteDb::savePlaylist(const QVector<SongInfo> &playlist, int currentIndex)
{
    if (!m_sqliteHandle) initDb();
    sqlite3 *db = static_cast<sqlite3*>(m_sqliteHandle);
    if (!db) return;

    QJsonArray arr;
    for (const auto &s : playlist) {
        arr.append(s.toJson());
    }
    QJsonDocument doc(arr);
    QByteArray jsonBytes = doc.toJson(QJsonDocument::Compact);

    const char *sql = "INSERT OR REPLACE INTO playlist_state (id, playlist_json, current_index) VALUES (1, ?, ?);";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }

    sqlite3_bind_text(stmt, 1, jsonBytes.constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, currentIndex);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

QVector<SongInfo> MusicFavoriteDb::loadPlaylist(int &outCurrentIndex)
{
    QVector<SongInfo> list;
    outCurrentIndex = 0;

    if (!m_sqliteHandle) initDb();
    sqlite3 *db = static_cast<sqlite3*>(m_sqliteHandle);
    if (!db) return list;

    const char *sql = "SELECT playlist_json, current_index FROM playlist_state WHERE id = 1;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return list;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *jsonStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        outCurrentIndex = sqlite3_column_int(stmt, 1);
        if (jsonStr) {
            QJsonDocument doc = QJsonDocument::fromJson(QByteArray(jsonStr));
            if (doc.isArray()) {
                for (const auto &val : doc.array()) {
                    if (val.isObject()) {
                        list.append(SongInfo::fromJson(val.toObject()));
                    }
                }
            }
        }
    }
    sqlite3_finalize(stmt);
    return list;
}

void MusicFavoriteDb::addSearchHistory(const QString &keyword)
{
    QString kw = keyword.trimmed();
    if (kw.isEmpty()) return;

    if (!m_sqliteHandle) initDb();
    sqlite3 *db = static_cast<sqlite3*>(m_sqliteHandle);
    if (!db) return;

    const char *sql = "INSERT OR REPLACE INTO search_history (keyword, searched_at) VALUES (?, ?);";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    sqlite3_bind_text(stmt, 1, kw.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, now);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

QStringList MusicFavoriteDb::getSearchHistories(int limit)
{
    QStringList list;
    if (!m_sqliteHandle) initDb();
    sqlite3 *db = static_cast<sqlite3*>(m_sqliteHandle);
    if (!db) return list;

    const char *sql = "SELECT keyword FROM search_history ORDER BY searched_at DESC LIMIT ?;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return list;
    }

    sqlite3_bind_int(stmt, 1, limit <= 0 ? 15 : limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (txt) {
            list.append(QString::fromUtf8(txt));
        }
    }
    sqlite3_finalize(stmt);
    return list;
}

void MusicFavoriteDb::clearSearchHistory()
{
    if (!m_sqliteHandle) initDb();
    sqlite3 *db = static_cast<sqlite3*>(m_sqliteHandle);
    if (!db) return;

    const char *sql = "DELETE FROM search_history;";
    sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
}

void MusicFavoriteDb::removeSearchHistory(const QString &keyword)
{
    QString kw = keyword.trimmed();
    if (kw.isEmpty()) return;

    if (!m_sqliteHandle) initDb();
    sqlite3 *db = static_cast<sqlite3*>(m_sqliteHandle);
    if (!db) return;

    const char *sql = "DELETE FROM search_history WHERE keyword = ?;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }

    sqlite3_bind_text(stmt, 1, kw.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}
