#include "SystemObserver.hpp"
#include "PetEventBus.hpp"
#include <QJsonObject>
#include <QDateTime>
#include <QCoreApplication>
#include <iostream>
#include <sys/statvfs.h>
#import <AppKit/AppKit.h>
#import <dispatch/dispatch.h>

@interface MacSystemListener : NSObject
@property (nonatomic, strong) id appActivateObserver;
@property (nonatomic, strong) id sleepObserver;
@property (nonatomic, strong) id wakeObserver;
@property (nonatomic, strong) id screenSleepObserver;
@property (nonatomic, strong) id screenWakeObserver;
@property (nonatomic) dispatch_source_t memoryPressureSource;
@end

@implementation MacSystemListener

- (void)startListening {
    NSNotificationCenter *wsCenter = [[NSWorkspace sharedWorkspace] notificationCenter];
    NSNotificationCenter *defCenter = [NSNotificationCenter defaultCenter];

    // 1. 前台应用切换监听 (使用 queue:nil 同步接收，并在 Qt 主线程派发)
    self.appActivateObserver = [wsCenter addObserverForName:NSWorkspaceDidActivateApplicationNotification
                                                     object:nil
                                                      queue:nil
                                                 usingBlock:^(NSNotification *note) {
        NSRunningApplication *app = note.userInfo[NSWorkspaceApplicationKey];
        if (app) {
            NSString *nsName = app.localizedName ?: @"Unknown";
            NSString *nsBundle = app.bundleIdentifier ?: @"";
            QString appName = QString::fromNSString(nsName);
            QString bundleId = QString::fromNSString(nsBundle);

            std::cout << "[SystemObserver] 捕获到应用切换: " << appName.toStdString() << " (" << bundleId.toStdString() << ")" << std::endl;

            QJsonObject payload;
            payload["app_name"] = appName;
            payload["bundle_id"] = bundleId;

            if (QCoreApplication::instance()) {
                QMetaObject::invokeMethod(QCoreApplication::instance(), [payload]() {
                    PetEventBus::instance()->emitEvent("system.app_activated", payload);
                }, Qt::QueuedConnection);
            }
        }
    }];

    // 2. 系统休眠与唤醒
    self.sleepObserver = [wsCenter addObserverForName:NSWorkspaceWillSleepNotification
                                               object:nil
                                                queue:nil
                                           usingBlock:^(NSNotification *) {
        std::cout << "[SystemObserver] 捕获到系统休眠通知" << std::endl;
        QJsonObject payload;
        payload["reason"] = "system_sleep";
        if (QCoreApplication::instance()) {
            QMetaObject::invokeMethod(QCoreApplication::instance(), [payload]() {
                PetEventBus::instance()->emitEvent("system.sleep", payload);
            }, Qt::QueuedConnection);
        }
    }];

    self.wakeObserver = [wsCenter addObserverForName:NSWorkspaceDidWakeNotification
                                              object:nil
                                               queue:nil
                                          usingBlock:^(NSNotification *) {
        std::cout << "[SystemObserver] 捕获到系统唤醒通知" << std::endl;
        QJsonObject payload;
        payload["reason"] = "system_wake";
        if (QCoreApplication::instance()) {
            QMetaObject::invokeMethod(QCoreApplication::instance(), [payload]() {
                PetEventBus::instance()->emitEvent("system.wake", payload);
            }, Qt::QueuedConnection);
        }
    }];

    // 3. 屏幕熄灭与亮屏 (注册到 defaultCenter)
    self.screenSleepObserver = [defCenter addObserverForName:NSWorkspaceScreensDidSleepNotification
                                                      object:nil
                                                       queue:nil
                                                  usingBlock:^(NSNotification *) {
        std::cout << "[SystemObserver] 捕获到屏幕熄灭" << std::endl;
        QJsonObject payload;
        payload["reason"] = "screen_sleep";
        if (QCoreApplication::instance()) {
            QMetaObject::invokeMethod(QCoreApplication::instance(), [payload]() {
                PetEventBus::instance()->emitEvent("system.sleep", payload);
            }, Qt::QueuedConnection);
        }
    }];

    self.screenWakeObserver = [defCenter addObserverForName:NSWorkspaceScreensDidWakeNotification
                                                     object:nil
                                                      queue:nil
                                                 usingBlock:^(NSNotification *) {
        std::cout << "[SystemObserver] 捕获到屏幕点亮" << std::endl;
        QJsonObject payload;
        payload["reason"] = "screen_wake";
        if (QCoreApplication::instance()) {
            QMetaObject::invokeMethod(QCoreApplication::instance(), [payload]() {
                PetEventBus::instance()->emitEvent("system.wake", payload);
            }, Qt::QueuedConnection);
        }
    }];

    // 4. 系统内存压力内核通知 (GCD dispatch source)
    unsigned long mask = DISPATCH_MEMORYPRESSURE_WARN | DISPATCH_MEMORYPRESSURE_CRITICAL;
    self.memoryPressureSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_MEMORYPRESSURE, 0, mask, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));
    if (self.memoryPressureSource) {
        dispatch_source_set_event_handler(self.memoryPressureSource, ^{
            unsigned long pressureLevel = dispatch_source_get_data(self.memoryPressureSource);
            QString levelStr = (pressureLevel & DISPATCH_MEMORYPRESSURE_CRITICAL) ? "critical" : "warning";
            std::cout << "[SystemObserver] 捕获到内核内存压力告警: " << levelStr.toStdString() << std::endl;

            QJsonObject payload;
            payload["pressure_level"] = levelStr;
            if (QCoreApplication::instance()) {
                QMetaObject::invokeMethod(QCoreApplication::instance(), [payload]() {
                    PetEventBus::instance()->emitEvent("system.memory_pressure", payload);
                }, Qt::QueuedConnection);
            }
        });
        dispatch_resume(self.memoryPressureSource);
    }
}

- (void)stopListening {
    NSNotificationCenter *wsCenter = [[NSWorkspace sharedWorkspace] notificationCenter];
    NSNotificationCenter *defCenter = [NSNotificationCenter defaultCenter];

    if (self.appActivateObserver) {
        [wsCenter removeObserver:self.appActivateObserver];
        self.appActivateObserver = nil;
    }
    if (self.sleepObserver) {
        [wsCenter removeObserver:self.sleepObserver];
        self.sleepObserver = nil;
    }
    if (self.wakeObserver) {
        [wsCenter removeObserver:self.wakeObserver];
        self.wakeObserver = nil;
    }
    if (self.screenSleepObserver) {
        [defCenter removeObserver:self.screenSleepObserver];
        self.screenSleepObserver = nil;
    }
    if (self.screenWakeObserver) {
        [defCenter removeObserver:self.screenWakeObserver];
        self.screenWakeObserver = nil;
    }
    if (self.memoryPressureSource) {
        dispatch_source_cancel(self.memoryPressureSource);
        self.memoryPressureSource = nil;
    }
}

@end

SystemObserver::SystemObserver()
{
}

SystemObserver::~SystemObserver()
{
    stop();
}

SystemObserver* SystemObserver::instance()
{
    static SystemObserver s_instance;
    return &s_instance;
}

void SystemObserver::start()
{
    if (m_started) return;
    m_started = true;

    MacSystemListener *listener = [[MacSystemListener alloc] init];
    [listener startListening];
    m_observerContext = (__bridge_retained void*)listener;

    std::cout << "[SystemObserver] macOS 系统级事件观察器已启动" << std::endl;

    // 低频检查硬盘（启动时检查一次，耗时 0.001ms）
    struct statvfs stat;
    if (statvfs("/", &stat) == 0) {
        double freeGb = (double)(stat.f_bavail * stat.f_frsize) / (1024.0 * 1024.0 * 1024.0);
        if (freeGb < 10.0) {
            QJsonObject payload;
            payload["free_gb"] = freeGb;
            PetEventBus::instance()->emitEvent("system.disk_low", payload);
        }
    }
}

void SystemObserver::stop()
{
    if (!m_started) return;
    m_started = false;

    if (m_observerContext) {
        MacSystemListener *listener = (__bridge_transfer MacSystemListener*)m_observerContext;
        [listener stopListening];
        m_observerContext = nullptr;
    }
}

QString SystemObserver::currentActiveAppName() const
{
    @autoreleasepool {
        NSRunningApplication *app = [[NSWorkspace sharedWorkspace] frontmostApplication];
        if (app && app.localizedName) {
            return QString::fromNSString(app.localizedName);
        }
    }
    return "";
}
