#include "UpdateManager.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QProcess>
#include <QTimer>
#include <QRegularExpression>
#include <iostream>

UpdateManager* UpdateManager::instance() {
    static UpdateManager s_instance;
    return &s_instance;
}

UpdateManager::UpdateManager() {
    m_networkManager = new QNetworkAccessManager(this);
}

QString UpdateManager::getPlatformAssetKeyword() {
#if defined(Q_OS_MAC)
    #if defined(__arm64__) || defined(__aarch64__)
        return "macOS-AppleSilicon";
    #else
        return "macOS";
    #endif
#elif defined(Q_OS_WIN)
    return "Windows";
#elif defined(Q_OS_LINUX)
    return "Linux";
#else
    return "macOS";
#endif
}

int UpdateManager::compareVersions(const QString &v1Raw, const QString &v2Raw) {
    QString v1 = v1Raw.trimmed();
    if (v1.startsWith('v', Qt::CaseInsensitive)) v1.remove(0, 1);
    QString v2 = v2Raw.trimmed();
    if (v2.startsWith('v', Qt::CaseInsensitive)) v2.remove(0, 1);

    QStringList parts1 = v1.split('.');
    QStringList parts2 = v2.split('.');

    int maxLen = std::max(parts1.size(), parts2.size());
    for (int i = 0; i < maxLen; ++i) {
        int num1 = (i < parts1.size()) ? parts1[i].toInt() : 0;
        int num2 = (i < parts2.size()) ? parts2[i].toInt() : 0;
        if (num1 > num2) return 1;
        if (num1 < num2) return -1;
    }
    return 0;
}

void UpdateManager::checkForUpdates(bool, std::function<void(const UpdateInfo &info, const QString &errorMsg)> callback) {
    QString urlStr = QString("https://api.github.com/repos/%1/releases/latest").arg(GUYI_BOT_REPO);
    QNetworkRequest request{QUrl(urlStr)};
    request.setHeader(QNetworkRequest::UserAgentHeader, "guyi-bot-updater/" GUYI_BOT_VERSION);
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    std::cout << "[UpdateManager] 正在检查 GitHub 最新版本: " << urlStr.toStdString() << std::endl;

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, [this, reply, callback]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            QString err = QString("请求 GitHub 接口失败: %1").arg(reply->errorString());
            std::cerr << "[UpdateManager] " << err.toStdString() << std::endl;
            if (callback) callback(UpdateInfo{}, err);
            return;
        }

        QByteArray data = reply->readAll();
        QJsonParseError parseErr;
        auto doc = QJsonDocument::fromJson(data, &parseErr);
        if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
            QString err = "解析 GitHub Releases 响应失败";
            if (callback) callback(UpdateInfo{}, err);
            return;
        }

        auto rootObj = doc.object();
        QString tagName = rootObj["tag_name"].toString().trimmed();
        QString releaseTitle = rootObj["name"].toString().trimmed();
        QString releaseBody = rootObj["body"].toString();
        QString htmlUrl = rootObj["html_url"].toString();

        UpdateInfo info;
        info.currentVersion = GUYI_BOT_VERSION;
        info.remoteVersion = tagName;
        info.releaseTitle = releaseTitle.isEmpty() ? tagName : releaseTitle;
        info.releaseNotes = releaseBody;
        info.htmlUrl = htmlUrl;

        // 查找匹配当前操作系统架构的资产包
        QString keyword = getPlatformAssetKeyword();
        auto assetsArr = rootObj["assets"].toArray();
        for (auto aVal : assetsArr) {
            auto aObj = aVal.toObject();
            QString name = aObj["name"].toString();
            if (name.contains(keyword, Qt::CaseInsensitive) && name.endsWith(".zip", Qt::CaseInsensitive)) {
                info.downloadUrl = aObj["browser_download_url"].toString();
                info.assetName = name;
                info.assetSize = aObj["size"].toInteger();
                break;
            }
        }

        // 如果未找到平台精准匹配包，做兼容回退
        if (info.downloadUrl.isEmpty() && !assetsArr.isEmpty()) {
            for (auto aVal : assetsArr) {
                auto aObj = aVal.toObject();
                QString name = aObj["name"].toString();
                if (name.contains("macOS", Qt::CaseInsensitive) || name.endsWith(".zip", Qt::CaseInsensitive)) {
                    info.downloadUrl = aObj["browser_download_url"].toString();
                    info.assetName = name;
                    info.assetSize = aObj["size"].toInteger();
                    break;
                }
            }
        }

        // 版本比对
        if (compareVersions(tagName, GUYI_BOT_VERSION) > 0) {
            info.hasUpdate = true;
            std::cout << "[UpdateManager] 发现新版本: " << tagName.toStdString()
                      << " (当前版本: " << GUYI_BOT_VERSION << ")" << std::endl;
        } else {
            info.hasUpdate = false;
            std::cout << "[UpdateManager] 当前已是最新版本: " << GUYI_BOT_VERSION << std::endl;
        }

        m_latestInfo = info;
        if (callback) {
            callback(info, "");
        }
    });
}

void UpdateManager::startDownloadAndInstall(const QString &downloadUrl,
                                           std::function<void(qint64 received, qint64 total)> progressCallback,
                                           std::function<void(bool success, const QString &errorMsg)> finishCallback)
{
    if (downloadUrl.isEmpty()) {
        if (finishCallback) finishCallback(false, "下载链接为空");
        return;
    }

    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString zipPath = tempDir + "/guyi-bot-update.zip";
    QFile::remove(zipPath); // 清理旧残留

    auto file = std::make_shared<QFile>(zipPath);
    if (!file->open(QIODevice::WriteOnly)) {
        if (finishCallback) finishCallback(false, "无法创建临时升级包文件");
        return;
    }

    QNetworkRequest request{QUrl(downloadUrl)};
    request.setHeader(QNetworkRequest::UserAgentHeader, "guyi-bot-updater/" GUYI_BOT_VERSION);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    std::cout << "[UpdateManager] 开始下载更新包: " << downloadUrl.toStdString() << std::endl;

    m_downloadReply = m_networkManager->get(request);

    connect(m_downloadReply, &QNetworkReply::downloadProgress, [progressCallback](qint64 bytesReceived, qint64 bytesTotal) {
        if (progressCallback) {
            progressCallback(bytesReceived, bytesTotal);
        }
    });

    connect(m_downloadReply, &QNetworkReply::readyRead, [this, file]() {
        if (m_downloadReply && file->isOpen()) {
            file->write(m_downloadReply->readAll());
        }
    });

    connect(m_downloadReply, &QNetworkReply::finished, [this, file, zipPath, finishCallback]() {
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;

        file->flush();
        file->close();

        if (file->size() < 1024) {
            QFile::remove(zipPath);
            if (finishCallback) finishCallback(false, "升级包下载数据异常（文件大小过小）");
            return;
        }

        std::cout << "[UpdateManager] 升级包下载完成 (" << file->size() << " 字节)，开始准备热替换..." << std::endl;

        applyUpdateAndRestart(zipPath, finishCallback);
    });
}

void UpdateManager::applyUpdateAndRestart(const QString &zipFilePath, std::function<void(bool ok, const QString &err)> callback) {
#if defined(Q_OS_MAC)
    // 获取当前 .app bundle 的根目录路径
    QString appPath = QCoreApplication::applicationDirPath(); // 位于 guyi-bot.app/Contents/MacOS
    QDir dir(appPath);
    dir.cdUp(); // Contents
    dir.cdUp(); // guyi-bot.app
    QString bundlePath = dir.canonicalPath();

    if (!bundlePath.endsWith(".app")) {
        // 如果是在开发构建目录下运行独立二进制，回退为更新可执行文件
        bundlePath = QCoreApplication::applicationDirPath() + "/guyi-bot.app";
    }

    QString scriptPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/apply_guyi_update.sh";
    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (callback) callback(false, "无法生成更新引导脚本");
        return;
    }

    QTextStream out(&scriptFile);
    out << "#!/bin/bash\n";
    out << "sleep 1\n";
    out << "TARGET_APP=\"" << bundlePath << "\"\n";
    out << "ZIP_PATH=\"" << zipFilePath << "\"\n";
    out << "EXTRACT_DIR=\"$(mktemp -d /tmp/guyi_update_XXXXXX)\"\n";
    out << "unzip -q -o \"$ZIP_PATH\" -d \"$EXTRACT_DIR\"\n";
    out << "if [ -d \"$EXTRACT_DIR/guyi-bot.app\" ]; then\n";
    out << "    rm -rf \"$TARGET_APP\"\n";
    out << "    cp -R \"$EXTRACT_DIR/guyi-bot.app\" \"$TARGET_APP\"\n";
    out << "    xattr -cr \"$TARGET_APP\" 2>/dev/null || true\n";
    out << "    open -n \"$TARGET_APP\"\n";
    out << "elif [ -d \"$EXTRACT_DIR/Shijima-Qt.app\" ]; then\n";
    out << "    rm -rf \"$TARGET_APP\"\n";
    out << "    cp -R \"$EXTRACT_DIR/Shijima-Qt.app\" \"$TARGET_APP\"\n";
    out << "    xattr -cr \"$TARGET_APP\" 2>/dev/null || true\n";
    out << "    open -n \"$TARGET_APP\"\n";
    out << "fi\n";
    out << "rm -rf \"$EXTRACT_DIR\" \"$ZIP_PATH\" \"" << scriptPath << "\"\n";
    scriptFile.close();

    QFile::setPermissions(scriptPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadUser | QFile::ExeUser);

    std::cout << "[UpdateManager] 启动分离更新进程，即将重启应用..." << std::endl;

    if (callback) callback(true, "");

    // 启动后台分离进程执行解压替换并重启
    QProcess::startDetached("/bin/bash", {scriptPath});

    // 退出当前运行的旧版进程
    QTimer::singleShot(300, []() {
        QCoreApplication::quit();
    });

#elif defined(Q_OS_WIN)
    QString scriptPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/apply_guyi_update.bat";
    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (callback) callback(false, "无法生成 Windows 更新脚本");
        return;
    }
    QString appDir = QCoreApplication::applicationDirPath();
    QTextStream out(&scriptFile);
    out << "@echo off\r\n";
    out << "timeout /t 1 /nobreak >nul\r\n";
    out << "tar -xf \"" << zipFilePath << "\" -C \"%TEMP%\\guyi_update_tmp\"\r\n";
    out << "xcopy /s /e /y \"%TEMP%\\guyi_update_tmp\\*\" \"" << appDir << "\\\"\r\n";
    out << "start \"\" \"" << appDir << "\\guyi-bot.exe\"\r\n";
    out << "rd /s /q \"%TEMP%\\guyi_update_tmp\"\r\n";
    out << "del \"" << zipFilePath << "\"\r\n";
    out << "del \"%~f0\"\r\n";
    scriptFile.close();

    if (callback) callback(true, "");

    QProcess::startDetached("cmd.exe", {"/c", scriptPath});
    QTimer::singleShot(300, []() {
        QCoreApplication::quit();
    });
#else
    if (callback) callback(false, "当前系统不支持全自动热替换，请手动前往 GitHub Release 下载。");
#endif
}
