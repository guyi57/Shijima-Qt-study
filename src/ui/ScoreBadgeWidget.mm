#include "ScoreBadgeWidget.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QParallelAnimationGroup>
#include <QSequentialAnimationGroup>
#include <QEasingCurve>
#include <QFont>
#include <QGuiApplication>
#include <QFileInfo>

#if defined(__APPLE__)
#import <AppKit/AppKit.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

bool Platform_isAppFrontmost(QString const& appTarget)
{
    if (appTarget.trimmed().isEmpty()) return false;

#if defined(__APPLE__)
    @autoreleasepool {
        NSRunningApplication *frontApp = [[NSWorkspace sharedWorkspace] frontmostApplication];
        if (!frontApp) return false;

        NSString *bundleId = [frontApp bundleIdentifier];
        NSString *appName = [frontApp localizedName];
        QString target = appTarget.trimmed();

        if (bundleId != nil) {
            QString curBundle = QString::fromNSString(bundleId);
            if (curBundle.compare(target, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
        if (appName != nil) {
            QString curName = QString::fromNSString(appName);
            if (curName.compare(target, Qt::CaseInsensitive) == 0) {
                return true;
            }
            if ((target.contains("antigravity-ide", Qt::CaseInsensitive) || target.contains("Antigravity IDE"))
                && curName.contains("Antigravity", Qt::CaseInsensitive)) {
                return true;
            }
        }
    }
#elif defined(_WIN32)
    HWND foreground = GetForegroundWindow();
    if (foreground) {
        DWORD pid = 0;
        GetWindowThreadProcessId(foreground, &pid);
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProc) {
            wchar_t exePath[MAX_PATH] = {0};
            DWORD size = MAX_PATH;
            if (QueryFullProcessImageNameW(hProc, 0, exePath, &size)) {
                QString fullPath = QString::fromWCharArray(exePath);
                QString baseName = QFileInfo(fullPath).baseName();
                CloseHandle(hProc);
                if (baseName.compare(appTarget.trimmed(), Qt::CaseInsensitive) == 0 ||
                    appTarget.contains(baseName, Qt::CaseInsensitive)) {
                    return true;
                }
            }
            CloseHandle(hProc);
        }
    }
#endif
    return false;
}

ScoreBadgeWidget::ScoreBadgeWidget(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::NoDropShadowWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_DeleteOnClose);
    setFocusPolicy(Qt::NoFocus);

#if defined(__APPLE__)
    NSView *badgeView = (__bridge NSView *)((void *)winId());
    NSWindow *badgeWin = [badgeView window];
    if (badgeWin != nil) {
        NSWindowCollectionBehavior behavior = [badgeWin collectionBehavior];
        behavior &= ~NSWindowCollectionBehaviorMoveToActiveSpace;
        behavior |= (NSWindowCollectionBehaviorCanJoinAllSpaces |
                     NSWindowCollectionBehaviorStationary |
                     NSWindowCollectionBehaviorIgnoresCycle);
        [badgeWin setCollectionBehavior:behavior];
        [badgeWin setLevel:NSFloatingWindowLevel];
        [badgeWin setHidesOnDeactivate:NO];
    }
#elif defined(_WIN32)
    HWND hwnd = (HWND)winId();
    if (hwnd) {
        LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        exStyle |= (WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE);
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
    }
#endif


    m_opacityEffect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(m_opacityEffect);
}

void ScoreBadgeWidget::showDelta(const QPoint &globalPos, int moodDelta, int affectionDelta, int staminaDelta)
{
    if (moodDelta == 0 && affectionDelta == 0 && staminaDelta == 0) {
        return;
    }

    QStringList parts;
    bool isPositive = true;

    if (staminaDelta != 0) {
        if (staminaDelta > 0) {
            parts << QString("+%1 ⚡ 体力").arg(staminaDelta);
        } else {
            parts << QString("%1 ⚡ 体力").arg(staminaDelta);
            isPositive = false;
        }
    }

    if (moodDelta != 0) {
        if (moodDelta > 0) {
            parts << QString("+%1 🌟 心情").arg(moodDelta);
        } else {
            parts << QString("%1 💢 心情").arg(moodDelta);
            isPositive = false;
        }
    }

    if (affectionDelta != 0) {
        if (affectionDelta > 0) {
            parts << QString("+%1 💕 亲密").arg(affectionDelta);
        } else {
            parts << QString("%1 💔 亲密").arg(affectionDelta);
            isPositive = false;
        }
    }

    ScoreBadgeWidget *badge = new ScoreBadgeWidget(nullptr);
    badge->startAnimation(globalPos, parts.join("  "), isPositive);
}

void ScoreBadgeWidget::showStaminaDelta(const QPoint &globalPos, int staminaDelta)
{
    showDelta(globalPos, 0, 0, staminaDelta);
}

void ScoreBadgeWidget::startAnimation(const QPoint &startPos, const QString &text, bool isPositive)
{
    m_text = text;
    m_isPositive = isPositive;

    QFont font = QGuiApplication::font();
    font.setPointSize(12);
    font.setBold(true);
    QFontMetrics fm(font);
    int textWidth = fm.horizontalAdvance(m_text);

    int badgeWidth = textWidth + 32;
    int badgeHeight = 32;
    setFixedSize(badgeWidth, badgeHeight);

    QPoint initialPos = startPos - QPoint(badgeWidth / 2, badgeHeight + 10);
    QPoint targetPos = initialPos - QPoint(0, 48); // 向上漂浮 48 像素

    move(initialPos);
    show();
    raise();

    // 位置上移动画
    m_moveAnim = new QPropertyAnimation(this, "pos", this);
    m_moveAnim->setDuration(1600);
    m_moveAnim->setStartValue(initialPos);
    m_moveAnim->setEndValue(targetPos);
    m_moveAnim->setEasingCurve(QEasingCurve::OutCubic);

    // 透明度淡出动画（前 600ms 保持，后 1000ms 淡出）
    m_opacityAnim = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    m_opacityAnim->setDuration(1600);
    m_opacityAnim->setKeyValueAt(0.0, 0.0);
    m_opacityAnim->setKeyValueAt(0.2, 1.0);
    m_opacityAnim->setKeyValueAt(0.65, 1.0);
    m_opacityAnim->setKeyValueAt(1.0, 0.0);

    QParallelAnimationGroup *group = new QParallelAnimationGroup(this);
    group->addAnimation(m_moveAnim);
    group->addAnimation(m_opacityAnim);

    connect(group, &QParallelAnimationGroup::finished, this, [this]() {
        close();
        deleteLater();
    });

    group->start(QAbstractAnimation::DeleteWhenStopped);
}

void ScoreBadgeWidget::paintEvent(QPaintEvent *)
{
    if (m_text.isEmpty()) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();
    int r = h / 2;

    QPainterPath path;
    path.addRoundedRect(1, 1, w - 2, h - 2, r, r);

    // 阴影
    painter.fillPath(path, QColor(0, 0, 0, 30));

    // 背景胶囊颜色
    if (m_isPositive) {
        // 金绿色/蓝绿渐变
        QLinearGradient grad(0, 0, w, h);
        grad.setColorAt(0.0, QColor(16, 185, 129, 235)); // #10b981
        grad.setColorAt(1.0, QColor(5, 150, 105, 245));
        painter.fillPath(path, grad);

        painter.setPen(QPen(QColor(255, 255, 255, 180), 1.2));
        painter.drawPath(path);
    } else {
        // 珊瑚红/橙渐变
        QLinearGradient grad(0, 0, w, h);
        grad.setColorAt(0.0, QColor(244, 63, 94, 235));  // #f43f5e
        grad.setColorAt(1.0, QColor(225, 29, 72, 245));
        painter.fillPath(path, grad);

        painter.setPen(QPen(QColor(255, 255, 255, 180), 1.2));
        painter.drawPath(path);
    }

    // 文字
    QFont font = QGuiApplication::font();
    font.setPointSize(12);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(rect(), Qt::AlignCenter, m_text);
}
