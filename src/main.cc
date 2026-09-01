// 
// Shijima-Qt - Cross-platform shimeji simulation app for desktop
// Copyright (C) 2025 pixelomer
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
// 

#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <shijima/log.hpp>
#include "Platform/Platform.hpp"
#include "ShijimaManager.hpp"
#include "AssetLoader.hpp"
#include "BehaviorEngine.hpp"
#include "SystemObserver.hpp"
#include "MusicFavoriteDb.hpp"
#include "SettingsDb.hpp"
#include "TrashWatcher.hpp"
#include "UpdateManager.hpp"
#include <QTimer>
#include "cli.hpp"
#include <httplib.h>

int main(int argc, char **argv) {
    if (argc > 1) {
        return shijimaRunCli(argc, argv);
    }
    Platform::initialize(argc, argv);
    #ifdef SHIJIMA_LOGGING_ENABLED
        shijima::set_log_level(SHIJIMA_LOG_PARSER | SHIJIMA_LOG_WARNINGS);
    #endif
    QApplication app(argc, argv);
    app.setApplicationName("guyi-bot");
    app.setApplicationDisplayName("guyi-bot");
    app.setQuitOnLastWindowClosed(false);
    try {
        httplib::Client pingClient { "http://127.0.0.1:32456" };
        pingClient.set_connection_timeout(0, 500000);
        pingClient.set_read_timeout(0, 500000);
        auto pingResult = pingClient.Get("/guyi/api/v1/ping");
        if (pingResult != nullptr && pingResult->status == 200) {
            throw std::runtime_error("guyi-bot is already running!");
        }
        auto manager = ShijimaManager::defaultManager();
        if (manager->mascots().empty() && !manager->loadedMascots().isEmpty()) {
            QString defaultMascot = SettingsDb::instance()->get("mascot.default_name", "Default Mascot");
            if (manager->loadedMascots().contains(defaultMascot)) {
                manager->spawn(defaultMascot.toStdString());
            } else if (manager->loadedMascots().contains("Default Mascot")) {
                manager->spawn("Default Mascot");
            } else {
                manager->spawn(manager->loadedMascots().firstKey().toStdString());
            }
        }
        manager->setManagerVisible(false);
        MusicFavoriteDb::instance()->initDb();
        BehaviorEngine::instance()->start();
        SystemObserver::instance()->start();
        TrashWatcher::instance()->init();

        // 启动 3 秒后后台静默检测新版本，若有更新则可爱弹气泡提醒
        QTimer::singleShot(3000, []() {
            UpdateManager::instance()->checkForUpdates(true /* silent */, [](const UpdateInfo &info, const QString &) {
                if (info.hasUpdate) {
                    auto manager = ShijimaManager::defaultManager();
                    if (manager && !manager->mascots().empty()) {
                        manager->mascots().front()->showMessage(
                            QString("🎉 **发现 guyi-bot 新版本 %1！**\n\n可右键桌宠点击「🔄 检查更新」一键自动升级~").arg(info.remoteVersion),
                            8000
                        );
                    }
                }
            });
        });
    }
    catch (std::exception &ex) {
        QMessageBox *msg = new QMessageBox {};
        msg->setText("guyi-bot failed to start. Reason: " +
            QString::fromUtf8(ex.what()));
        msg->setStandardButtons(QMessageBox::StandardButton::Close);
        msg->setAttribute(Qt::WA_DeleteOnClose);
        msg->show();
    }
    int ret = app.exec();
    ShijimaManager::finalize();
    AssetLoader::finalize();
    return ret;
}
