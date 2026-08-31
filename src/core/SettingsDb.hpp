#pragma once

#include <QString>
#include <QVariantMap>
#include <QJsonArray>
#include <QJsonObject>
#include <QMutex>

class SettingsDb {
public:
    static SettingsDb* instance();

    bool initDb();

    // 基础键值存储 (Key-Value)
    QString get(QString const& key, QString const& defaultValue = "") const;
    void set(QString const& key, QString const& value);

    int getInt(QString const& key, int defaultValue = 0) const;
    void setInt(QString const& key, int value);

    bool getBool(QString const& key, bool defaultValue = false) const;
    void setBool(QString const& key, bool value);

    double getDouble(QString const& key, double defaultValue = 0.0) const;
    void setDouble(QString const& key, double value);

    // JSON 复合数据存取
    QJsonArray getJsonArray(QString const& key) const;
    void setJsonArray(QString const& key, QJsonArray const& array);

    QJsonObject getJsonObject(QString const& key) const;
    void setJsonObject(QString const& key, QJsonObject const& object);

    void remove(QString const& key);
    bool contains(QString const& key) const;

    QString dbPath() const { return m_dbPath; }

private:
    SettingsDb();
    ~SettingsDb();

    mutable QMutex m_mutex;
    void *m_sqliteHandle = nullptr;
    QString m_dbPath;
};
