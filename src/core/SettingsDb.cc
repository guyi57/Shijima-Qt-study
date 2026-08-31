#include "SettingsDb.hpp"
#include <sqlite3.h>
#include <QDir>
#include <QDateTime>
#include <QJsonDocument>
#include <QMutexLocker>
#include <iostream>

SettingsDb* SettingsDb::instance()
{
    static SettingsDb s_instance;
    return &s_instance;
}

SettingsDb::SettingsDb()
{
    initDb();
}

SettingsDb::~SettingsDb()
{
    QMutexLocker locker(&m_mutex);
    if (m_sqliteHandle) {
        sqlite3_close(static_cast<sqlite3*>(m_sqliteHandle));
        m_sqliteHandle = nullptr;
    }
}

bool SettingsDb::initDb()
{
    QMutexLocker locker(&m_mutex);
    if (m_sqliteHandle != nullptr) return true;

    QString configDir = QDir::homePath() + "/.config/shijima-qt";
    QDir().mkpath(configDir);
    m_dbPath = configDir + "/shijima_settings.db";

    sqlite3 *db = nullptr;
    int rc = sqlite3_open(m_dbPath.toUtf8().constData(), &db);
    if (rc != SQLITE_OK) {
        std::cerr << "[SettingsDb] 打开配置数据库失败: " 
                  << (db ? sqlite3_errmsg(db) : "Unknown") << std::endl;
        if (db) sqlite3_close(db);
        return false;
    }

    m_sqliteHandle = db;

    // 启用 WAL 模式提高并发读写性能与可靠性
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

    const char *createTableSql =
        "CREATE TABLE IF NOT EXISTS app_settings ("
        "  key TEXT PRIMARY KEY,"
        "  value TEXT NOT NULL,"
        "  updated_at INTEGER NOT NULL"
        ");";

    char *errMsg = nullptr;
    rc = sqlite3_exec(db, createTableSql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[SettingsDb] 创建配置表失败: " << (errMsg ? errMsg : "") << std::endl;
        if (errMsg) sqlite3_free(errMsg);
        return false;
    }

    return true;
}

QString SettingsDb::get(QString const& key, QString const& defaultValue) const
{
    QMutexLocker locker(&m_mutex);
    if (!m_sqliteHandle) const_cast<SettingsDb*>(this)->initDb();
    sqlite3 *db = static_cast<sqlite3*>(m_sqliteHandle);
    if (!db) return defaultValue;

    const char *sql = "SELECT value FROM app_settings WHERE key = ? LIMIT 1;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return defaultValue;
    }

    sqlite3_bind_text(stmt, 1, key.toUtf8().constData(), -1, SQLITE_TRANSIENT);

    QString result = defaultValue;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *val = sqlite3_column_text(stmt, 0);
        if (val) {
            result = QString::fromUtf8(reinterpret_cast<const char*>(val));
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

void SettingsDb::set(QString const& key, QString const& value)
{
    QMutexLocker locker(&m_mutex);
    if (!m_sqliteHandle) initDb();
    sqlite3 *db = static_cast<sqlite3*>(m_sqliteHandle);
    if (!db) return;

    const char *sql = "INSERT INTO app_settings (key, value, updated_at) VALUES (?, ?, ?) "
                      "ON CONFLICT(key) DO UPDATE SET value = excluded.value, updated_at = excluded.updated_at;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    sqlite3_bind_text(stmt, 1, key.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, now);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

int SettingsDb::getInt(QString const& key, int defaultValue) const
{
    QString val = get(key, QString::number(defaultValue));
    bool ok = false;
    int res = val.toInt(&ok);
    return ok ? res : defaultValue;
}

void SettingsDb::setInt(QString const& key, int value)
{
    set(key, QString::number(value));
}

bool SettingsDb::getBool(QString const& key, bool defaultValue) const
{
    QString val = get(key, defaultValue ? "true" : "false").toLower().trimmed();
    if (val == "true" || val == "1" || val == "yes") return true;
    if (val == "false" || val == "0" || val == "no") return false;
    return defaultValue;
}

void SettingsDb::setBool(QString const& key, bool value)
{
    set(key, value ? "true" : "false");
}

double SettingsDb::getDouble(QString const& key, double defaultValue) const
{
    QString val = get(key, QString::number(defaultValue));
    bool ok = false;
    double res = val.toDouble(&ok);
    return ok ? res : defaultValue;
}

void SettingsDb::setDouble(QString const& key, double value)
{
    set(key, QString::number(value));
}

QJsonArray SettingsDb::getJsonArray(QString const& key) const
{
    QString val = get(key, "");
    if (val.isEmpty()) return QJsonArray();

    auto doc = QJsonDocument::fromJson(val.toUtf8());
    return doc.isArray() ? doc.array() : QJsonArray();
}

void SettingsDb::setJsonArray(QString const& key, QJsonArray const& array)
{
    QJsonDocument doc(array);
    set(key, QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

QJsonObject SettingsDb::getJsonObject(QString const& key) const
{
    QString val = get(key, "");
    if (val.isEmpty()) return QJsonObject();

    auto doc = QJsonDocument::fromJson(val.toUtf8());
    return doc.isObject() ? doc.object() : QJsonObject();
}

void SettingsDb::setJsonObject(QString const& key, QJsonObject const& object)
{
    QJsonDocument doc(object);
    set(key, QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

void SettingsDb::remove(QString const& key)
{
    QMutexLocker locker(&m_mutex);
    if (!m_sqliteHandle) initDb();
    sqlite3 *db = static_cast<sqlite3*>(m_sqliteHandle);
    if (!db) return;

    const char *sql = "DELETE FROM app_settings WHERE key = ?;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }

    sqlite3_bind_text(stmt, 1, key.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

bool SettingsDb::contains(QString const& key) const
{
    QMutexLocker locker(&m_mutex);
    if (!m_sqliteHandle) const_cast<SettingsDb*>(this)->initDb();
    sqlite3 *db = static_cast<sqlite3*>(m_sqliteHandle);
    if (!db) return false;

    const char *sql = "SELECT 1 FROM app_settings WHERE key = ? LIMIT 1;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, key.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
}
