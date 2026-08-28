// 
// Shijima-Qt - Interactive Message Bubble with Markdown & Hyperlink Support Implementation
// 

#include "MessageBubble.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QClipboard>
#include <QScrollBar>
#include <QProcess>
#include <QTextDocument>
#include <QDesktopServices>
#include <QRegularExpression>
#include <QDebug>
#include <iostream>
#include "MessageHistoryManager.hpp"
#include "MessageHistoryDialog.hpp"

#if defined(__APPLE__)
#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#elif defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif


#if defined(__APPLE__)
static BOOL NeverBecomeKey(id self, SEL _cmd) {
    (void)self; (void)_cmd;
    return NO;
}

static BOOL NeverBecomeMain(id self, SEL _cmd) {
    (void)self; (void)_cmd;
    return NO;
}
#endif

// ==========================================
// 1:1 对标截图的高质感胶囊按钮组件（独立彩色图标徽章 + 深色文字）
// ==========================================
class IconCardButton : public QWidget {
public:
    IconCardButton(const QString &icon, const QString &text, const QString &iconBg, QWidget *parent = nullptr)
        : QWidget(parent), m_defaultText(text), m_iconBg(iconBg)
    {
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
        
        auto layout = new QHBoxLayout(this);
        layout->setContentsMargins(5, 3, 10, 3);
        layout->setSpacing(6);

        m_iconLabel = new QLabel(icon, this);
        m_iconLabel->setFixedSize(22, 22);
        m_iconLabel->setAlignment(Qt::AlignCenter);
        m_iconLabel->setStyleSheet(QString(
            "QLabel {"
            "  background-color: %1;"
            "  color: #ffffff;"
            "  border-radius: 7px;"
            "  font-size: 11px;"
            "}"
        ).arg(iconBg));

        m_textLabel = new QLabel(text, this);
        m_textLabel->setStyleSheet("QLabel { color: #1e293b; font-size: 12px; font-weight: 600; background: transparent; }");

        layout->addWidget(m_iconLabel);
        layout->addWidget(m_textLabel);

        updateStyle(false);
    }

    void setText(const QString &text) {
        if (m_textLabel) m_textLabel->setText(text);
    }

    void resetText() {
        if (m_textLabel) m_textLabel->setText(m_defaultText);
    }

    std::function<void()> onClicked;

protected:
    void mousePressEvent(QMouseEvent *e) override {
        QWidget::mousePressEvent(e);
        if (onClicked) onClicked();
    }
    void enterEvent(QEnterEvent *e) override {
        QWidget::enterEvent(e);
        updateStyle(true);
    }
    void leaveEvent(QEvent *e) override {
        QWidget::leaveEvent(e);
        updateStyle(false);
    }

private:
    void updateStyle(bool hover) {
        setStyleSheet(QString(
            "IconCardButton {"
            "  background-color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 11px;"
            "}"
        ).arg(hover ? "#f8fafc" : "#ffffff", hover ? "#cbd5e1" : "#e2e8f0"));
    }

    QLabel *m_iconLabel = nullptr;
    QLabel *m_textLabel = nullptr;
    QString m_defaultText;
    QString m_iconBg;
};

static QString highlightCodeSyntax(QString const& code) {
    QString escaped = code.toHtmlEscaped();
    
    // 字符串高亮 (绿色)
    QRegularExpression strRe(R"(&quot;.*?&quot;|&#39;.*?&#39;|".*?"|'.*?')");
    escaped.replace(strRe, "<span style=\"color:#059669;\">\\0</span>");

    // 关键字高亮 (粉紫色)
    QStringList keywords = {
        "import", "from", "as", "def", "return", "class", "if", "elif", "else", 
        "for", "while", "in", "try", "except", "finally", "with", "lambda", "yield",
        "print", "True", "False", "None", "const", "let", "var", "function", "async",
        "await", "new", "this", "true", "false", "null", "struct", "int", "float",
        "double", "bool", "void", "auto", "public", "private", "protected"
    };
    for (const auto &kw : keywords) {
        QRegularExpression kwRe(QString(R"(\b%1\b)").arg(kw));
        escaped.replace(kwRe, QString("<span style=\"color:#9333ea; font-weight:600;\">%1</span>").arg(kw));
    }

    return escaped;
}

static QString renderInlineMarkdown(QString text) {
    // 1. 加粗
    QRegularExpression boldRe(R"(\*\*(.*?)\*\*)");
    text.replace(boldRe, "<b style=\"color:#0f172a; font-weight:700;\">\\1</b>");

    // 2. 斜体
    QRegularExpression italicRe(R"(\*(.*?)\*)");
    text.replace(italicRe, "<i>\\1</i>");

    // 3. 行内代码
    QRegularExpression codeRe(R"(`(.*?)`)");
    text.replace(codeRe, "<code style=\"background-color:#f1f5f9; color:#475569; padding:2px 6px; border-radius:4px; font-family:ui-monospace, SFMono-Regular, Menlo, monospace; font-size:12px;\">\\1</code>");

    // 4. 超链接
    QRegularExpression linkRe(R"(\[(.*?)\]\((.*?)\))");
    text.replace(linkRe, "<a href=\"\\2\" style=\"color:#2563eb; text-decoration:none; font-weight:500;\">\\1</a>");

    return text;
}

static QString markdownToRichHtml(QString const& raw) {
    QString text = raw;
    text.replace("\\n", "\n");
    text.replace("\r\n", "\n");
    text.replace("\r", "\n");

    QStringList lines = text.split('\n');
    QString bodyHtml;
    bool inCodeBlock = false;
    QString codeBlockContent;

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];
        QString trimmed = line.trimmed();

        // 1. 代码块
        if (trimmed.startsWith("```")) {
            if (inCodeBlock) {
                bodyHtml += QString(
                    "<pre style=\"background-color:#f8fafc; color:#334155; border:1px solid #e2e8f0; padding:10px 14px; border-radius:8px; font-family:'SF Mono', Menlo, Monaco, Consolas, monospace; font-size:12.5px; line-height:1.5; margin:8px 0; overflow-x:auto;\">"
                    "<code>%1</code>"
                    "</pre>"
                ).arg(highlightCodeSyntax(codeBlockContent));
                codeBlockContent.clear();
                inCodeBlock = false;
            } else {
                inCodeBlock = true;
                codeBlockContent.clear();
            }
            continue;
        }

        if (inCodeBlock) {
            codeBlockContent += (codeBlockContent.isEmpty() ? "" : "\n") + line;
            continue;
        }

        // 2. 空行
        if (trimmed.isEmpty()) {
            continue;
        }

        // 3. 表格解析 (精准 1:1 还原截图无竖线整洁斑马纹表格)
        if (trimmed.startsWith("|") && trimmed.endsWith("|") && trimmed.count("|") >= 2) {
            QStringList tableLines;
            while (i < lines.size() && lines[i].trimmed().startsWith("|") && lines[i].trimmed().endsWith("|")) {
                tableLines.append(lines[i].trimmed());
                i++;
            }
            i--; // 还原多加的索引

            if (tableLines.size() >= 2) {
                QString htmlTable = "<table style=\"border-collapse:separate; border-spacing:0; width:100%; margin:8px 0; background-color:#ffffff; border:1px solid #e2e8f0; border-radius:8px; overflow:hidden;\">\n";
                
                // 表头 (第 0 行)
                QStringList headerCells = tableLines[0].split("|", Qt::SkipEmptyParts);
                htmlTable += "<thead>\n<tr style=\"background-color:#f1f5f9;\">\n";
                for (int c = 0; c < headerCells.size(); ++c) {
                    QString colWidth = (c == 0) ? "width:36%;" : "";
                    htmlTable += QString("<th style=\"padding:7px 12px; font-weight:700; text-align:left; color:#0f172a; border-bottom:1px solid #e2e8f0; font-size:13px; %1\">%2</th>\n")
                        .arg(colWidth, renderInlineMarkdown(headerCells[c].trimmed()));
                }
                htmlTable += "</tr>\n</thead>\n<tbody>\n";

                int startRow = 1;
                if (tableLines.size() > 1 && tableLines[1].contains("---")) {
                    startRow = 2;
                }

                int rowIndex = 0;
                for (int r = startRow; r < tableLines.size(); ++r) {
                    QStringList dataCells = tableLines[r].split("|", Qt::SkipEmptyParts);
                    QString bgColor = (rowIndex % 2 == 0) ? "#ffffff" : "#f8fafc";
                    htmlTable += QString("<tr style=\"background-color:%1;\">\n").arg(bgColor);
                    
                    for (int c = 0; c < dataCells.size(); ++c) {
                        QString fontStyle = (c == 0) ? "color:#334155; font-weight:500;" : "color:#0f172a; font-weight:600;";
                        htmlTable += QString("<td style=\"padding:7px 12px; text-align:left; border-bottom:1px solid #f1f5f9; font-size:13px; %1\">%2</td>\n")
                            .arg(fontStyle, renderInlineMarkdown(dataCells[c].trimmed()));
                    }
                    htmlTable += "</tr>\n";
                    rowIndex++;
                }
                htmlTable += "</tbody>\n</table>";
                bodyHtml += htmlTable;
                continue;
            }
        }

        // 4. 标题 (以 # 开头，或者像 📍、📅、📝 开头的独立标题行)
        if (trimmed.startsWith("#") || 
            trimmed.startsWith("📍") ||
            trimmed.startsWith("📅") ||
            trimmed.startsWith("📝")) {
            QString headingText = trimmed;
            if (headingText.startsWith("#")) {
                headingText = headingText.remove(QRegularExpression("^#+\\s*")).trimmed();
            }
            bodyHtml += QString(
                "<div style=\"color:#0f172a; font-size:14px; font-weight:700; margin-top:8px; margin-bottom:6px; line-height:1.4;\">%1</div>"
            ).arg(renderInlineMarkdown(headingText));
            continue;
        }

        // 5. 列表项 (以 -、*、•、◦ 或 1. 开头)
        if (trimmed.startsWith("- ") || trimmed.startsWith("* ") || trimmed.startsWith("• ") || trimmed.startsWith("◦ ") || QRegularExpression("^[0-9]+\\.\\s+").match(trimmed).hasMatch()) {
            QString itemText = trimmed;
            itemText = itemText.remove(QRegularExpression("^([-\\*•◦]|[0-9]+\\.)\\s+")).trimmed();
            bodyHtml += QString(
                "<div style=\"margin-left:6px; margin-bottom:4px; line-height:1.55; font-size:13px; color:#1e293b;\">"
                "<span style=\"color:#94a3b8; margin-right:6px; font-weight:bold;\">◦</span>%1"
                "</div>"
            ).arg(renderInlineMarkdown(itemText));
            continue;
        }

        // 6. 普通段落
        bodyHtml += QString(
            "<div style=\"margin-bottom:6px; line-height:1.55; font-size:13px; color:#1e293b;\">%1</div>"
        ).arg(renderInlineMarkdown(trimmed));
    }

    return QString(
        "<html><head><style>"
        "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif; font-size: 13px; color: #1e293b; margin: 0; padding: 0; }"
        "b, strong { color: #0f172a; font-weight: 700; }"
        "table { border-collapse: separate; border-spacing: 0; }"
        "</style></head><body>%1</body></html>"
    ).arg(bodyHtml);
}

MessageBubble::MessageBubble(QWidget *parent)
    : QWidget(parent), m_hideTimer(nullptr), m_lastDuration(0)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | 
        Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);

#if defined(__APPLE__)
    NSView *bubbleView = (__bridge NSView *)((void *)winId());
    NSWindow *bubbleWin = [bubbleView window];
    if (bubbleWin != nil) {
        NSWindowCollectionBehavior behavior = [bubbleWin collectionBehavior];
        behavior &= ~NSWindowCollectionBehaviorMoveToActiveSpace;
        behavior |= (NSWindowCollectionBehaviorCanJoinAllSpaces |
                     NSWindowCollectionBehaviorStationary |
                     NSWindowCollectionBehaviorIgnoresCycle);
        [bubbleWin setCollectionBehavior:behavior];
        [bubbleWin setLevel:NSFloatingWindowLevel];
        [bubbleWin setHidesOnDeactivate:NO];

        // 终极无感防失焦：Runtime 替换为永不激活的 KeyWindow 子类
        Class originalClass = object_getClass(bubbleWin);
        const char *subclassName = "ShijimaBubbleNonActivatingNSWindow";
        Class subclass = objc_getClass(subclassName);
        if (!subclass) {
            subclass = objc_allocateClassPair(originalClass, subclassName, 0);
            if (subclass) {
                class_addMethod(subclass, @selector(canBecomeKeyWindow), (IMP)NeverBecomeKey, "c@:");
                class_addMethod(subclass, @selector(canBecomeMainWindow), (IMP)NeverBecomeMain, "c@:");
                objc_registerClassPair(subclass);
            }
        }
        if (subclass) {
            object_setClass(bubbleWin, subclass);
        }
    }
#elif defined(_WIN32)
    HWND hwnd = (HWND)winId();
    if (hwnd) {
        LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        exStyle |= (WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE);
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
    }
#endif


    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 14, 18, 14);
    mainLayout->setSpacing(8);

    // ==========================================
    // 顶部操作栏（1:1 对标截图：打开应用 / 复制 / 历史记录 / 倒计时 / 关闭）
    // ==========================================
    m_topBarWidget = new QWidget(this);
    auto topBarLayout = new QHBoxLayout(m_topBarWidget);
    topBarLayout->setContentsMargins(0, 0, 0, 0);
    topBarLayout->setSpacing(8);

    m_openAppBtn = new IconCardButton("🚀", "打开应用", "#6366f1", m_topBarWidget);
    m_openAppBtn->hide();

    m_copyBtn = new IconCardButton("📋", "复制", "#10b981", m_topBarWidget);

    m_historyBtn = new IconCardButton("🕒", "历史记录", "#f59e0b", m_topBarWidget);

    // 倒计时指示胶囊
    m_countdownLabel = new QLabel("⏱️ 25s", m_topBarWidget);
    m_countdownLabel->setStyleSheet(
        "QLabel {"
        "  background-color: #f1f5f9;"
        "  color: #64748b;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "  padding: 4px 8px;"
        "  border-radius: 10px;"
        "  border: 1px solid #e2e8f0;"
        "}"
    );
    m_countdownLabel->hide();

    m_closeBtn = new QPushButton("✕", m_topBarWidget);
    m_closeBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: transparent;"
        "  color: #94a3b8;"
        "  font-size: 12px;"
        "  font-weight: bold;"
        "  border: none;"
        "  border-radius: 10px;"
        "  min-width: 22px;"
        "  max-width: 22px;"
        "  min-height: 22px;"
        "  max-height: 22px;"
        "  padding: 0px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #fee2e2;"
        "  color: #ef4444;"
        "}"
    );
    m_closeBtn->setCursor(Qt::PointingHandCursor);

    topBarLayout->addWidget(m_openAppBtn);
    topBarLayout->addWidget(m_copyBtn);
    topBarLayout->addWidget(m_historyBtn);
    topBarLayout->addStretch();
    topBarLayout->addWidget(m_countdownLabel);
    topBarLayout->addWidget(m_closeBtn);
    mainLayout->addWidget(m_topBarWidget);

    // ==========================================
    // 富文本/Markdown 渲染器（排版自适应、无截断）
    // ==========================================
    m_textBrowser = new QTextBrowser(this);
    m_textBrowser->setReadOnly(true);
    m_textBrowser->setOpenExternalLinks(true);
    m_textBrowser->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard | Qt::LinksAccessibleByMouse);
    m_textBrowser->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_textBrowser->setLineWrapMode(QTextEdit::WidgetWidth);
    m_textBrowser->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_textBrowser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_textBrowser->document()->setDocumentMargin(2);
    m_textBrowser->setStyleSheet(
        "QTextBrowser {"
        "  background-color: transparent;"
        "  color: #1e293b;"
        "  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;"
        "  font-size: 13.5px;"
        "  line-height: 1.6;"
        "  border: none;"
        "  selection-background-color: #c7d2fe;"
        "  selection-color: #1e1b4b;"
        "}"
        "QScrollBar:vertical {"
        "  background: transparent;"
        "  width: 5px;"
        "  margin: 0px;"
        "  border-radius: 2.5px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #cbd5e1;"
        "  min-height: 20px;"
        "  border-radius: 2.5px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background: #94a3b8;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0px;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: none;"
        "}"
    );
    mainLayout->addWidget(m_textBrowser);

    // 事件绑定
    static_cast<IconCardButton*>(m_openAppBtn)->onClicked = [this]() {
        openAppTarget();
    };

    static_cast<IconCardButton*>(m_copyBtn)->onClicked = [this]() {
        if (!m_text.isEmpty()) {
            QGuiApplication::clipboard()->setText(m_text);
            static_cast<IconCardButton*>(m_copyBtn)->setText("已复制 ✅");
            std::cout << "[气泡] 已成功复制文本到剪贴板 (" << m_text.length() << " 字符)" << std::endl;
            QTimer::singleShot(1500, [this]() {
                if (m_copyBtn) static_cast<IconCardButton*>(m_copyBtn)->resetText();
            });
        }
    };

    static_cast<IconCardButton*>(m_historyBtn)->onClicked = [this]() {
        showHistoryDialog();
    };

    connect(m_closeBtn, &QPushButton::clicked, this, &MessageBubble::hideMessage);

    // 为气泡本身及所有子控件安装统一事件过滤器
    this->installEventFilter(this);
    if (m_topBarWidget) m_topBarWidget->installEventFilter(this);
    if (m_textBrowser) m_textBrowser->installEventFilter(this);
    if (m_openAppBtn) m_openAppBtn->installEventFilter(this);
    if (m_historyBtn) m_historyBtn->installEventFilter(this);
    if (m_copyBtn) m_copyBtn->installEventFilter(this);
    if (m_countdownLabel) m_countdownLabel->installEventFilter(this);
    if (m_closeBtn) m_closeBtn->installEventFilter(this);

    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    connect(m_hideTimer, &QTimer::timeout, this, &MessageBubble::hideMessage);

    m_countdownTimer = new QTimer(this);
    connect(m_countdownTimer, &QTimer::timeout, this, [this]() {
        if (!m_isCountdownPaused && m_remainingSeconds > 0) {
            m_remainingSeconds--;
            updateCountdownDisplay();
            if (m_remainingSeconds <= 0) {
                m_countdownTimer->stop();
                hideMessage();
            }
        }
    });

    hide();
}

void MessageBubble::openAppTarget()
{
    if (m_appTarget.trimmed().isEmpty()) return;

    QString target = m_appTarget.trimmed();
    bool launched = false;
    std::cout << "[应用唤醒] 准备打开目标: " << target.toStdString() << std::endl;

#if defined(__APPLE__)
    NSString *nsTarget = target.toNSString();

    // 1. 如果是 URL 链接或自定义协议 (如 https://, vscode://, weixin://)
    if (target.contains("://")) {
        NSURL *url = [NSURL URLWithString:nsTarget];
        if (url) {
            launched = [[NSWorkspace sharedWorkspace] openURL:url];
        }
    }

    // 2. 如果是 Bundle ID (例如 com.apple.Safari, com.tencent.xinWeChat, com.microsoft.VSCode)
    if (!launched) {
        NSURL *appUrl = [[NSWorkspace sharedWorkspace] URLForApplicationWithBundleIdentifier:nsTarget];
        if (appUrl != nil) {
            NSWorkspaceOpenConfiguration *config = [NSWorkspaceOpenConfiguration configuration];
            config.activates = YES;
            [[NSWorkspace sharedWorkspace] openApplicationAtURL:appUrl configuration:config completionHandler:nil];
            launched = true;
        }
    }

    // 3. 如果是本地 .app 完整路径 (例如 /Applications/Safari.app)
    if (!launched && (target.endsWith(".app", Qt::CaseInsensitive) || target.startsWith("/"))) {
        NSURL *appUrl = [NSURL fileURLWithPath:nsTarget];
        if (appUrl != nil) {
            NSWorkspaceOpenConfiguration *config = [NSWorkspaceOpenConfiguration configuration];
            config.activates = YES;
            [[NSWorkspace sharedWorkspace] openApplicationAtURL:appUrl configuration:config completionHandler:nil];
            launched = true;
        }
    }

    // 4. 尝试以应用名称查找完整路径启动
    if (!launched) {
        NSString *fullPath = [[NSWorkspace sharedWorkspace] fullPathForApplication:nsTarget];
        if (fullPath != nil) {
            NSURL *appUrl = [NSURL fileURLWithPath:fullPath];
            if (appUrl != nil) {
                NSWorkspaceOpenConfiguration *config = [NSWorkspaceOpenConfiguration configuration];
                config.activates = YES;
                [[NSWorkspace sharedWorkspace] openApplicationAtURL:appUrl configuration:config completionHandler:nil];
                launched = true;
            }
        }
    }

    // 5. 兜底使用 macOS open 命令启动
    if (!launched) {
        if (target.endsWith(".app", Qt::CaseInsensitive) || target.startsWith("/")) {
            QProcess::startDetached("open", QStringList() << target);
        } else if (target.contains(".") && !target.contains(" ")) {
            QProcess::startDetached("open", QStringList() << "-b" << target);
        } else {
            QProcess::startDetached("open", QStringList() << "-a" << target);
        }
        launched = true;
    }
#elif defined(_WIN32)
    if (target.contains("://") || target.startsWith("http", Qt::CaseInsensitive)) {
        launched = QDesktopServices::openUrl(QUrl::fromUserInput(target));
    } else {
        HINSTANCE hInst = ShellExecuteW(NULL, L"open", reinterpret_cast<const wchar_t*>(target.utf16()), NULL, NULL, SW_SHOWNORMAL);
        launched = ((INT_PTR)hInst > 32);
    }
#else
    launched = QDesktopServices::openUrl(QUrl::fromUserInput(target));
#endif

    std::cout << "[应用唤醒] 应用启动完成: " << target.toStdString() << " (成功: " << (launched ? "是" : "否") << ")" << std::endl;


    if (m_openAppBtn) {
        static_cast<IconCardButton*>(m_openAppBtn)->setText("已打开 ✅");
        QTimer::singleShot(2000, [this]() {
            if (m_openAppBtn) {
                static_cast<IconCardButton*>(m_openAppBtn)->resetText();
            }
        });
    }
}

void MessageBubble::showMessage(QString const& text, int duration, QString const& appTarget)
{
    m_text = text.trimmed();
    m_text.replace("\\n", "\n");
    m_text.replace("\r\n", "\n");
    m_text.replace("\r", "\n");
    m_lastDuration = duration;
    m_appTarget = appTarget.trimmed();

    if (m_text.isEmpty()) {
        hide();
        return;
    }

    // 仅在存在明确跳转目标、或长篇/结构化 Markdown (代码块、表格、长列表、大段落) 时才展示大卡片
    bool hasMarkdownStructure = m_text.contains("```") ||
                               (m_text.contains("|") && m_text.count("|") >= 4) ||
                               m_text.contains("###") ||
                               m_text.startsWith("# ") ||
                               ((m_text.contains("- ") || m_text.contains("1. ")) && m_text.count('\n') >= 2);

    bool isComplexNotification = !m_appTarget.isEmpty() ||
                                 hasMarkdownStructure ||
                                 m_text.length() > 90 ||
                                 m_text.count('\n') >= 3;

    m_isCompactCuteMode = !isComplexNotification;

    if (m_isCompactCuteMode) {
        if (m_topBarWidget) m_topBarWidget->hide();
        if (layout()) {
            layout()->setContentsMargins(12, 6, 12, 12);
            layout()->setSpacing(0);
        }
        m_textBrowser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_textBrowser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_textBrowser->document()->setDocumentMargin(0);

        QFont font = m_textBrowser->font();
        font.setPointSize(13);
        font.setWeight(QFont::DemiBold);
        m_textBrowser->setFont(font);

        m_textBrowser->setHtml(QString(
            "<div style=\"text-align: center; font-size: 13px; font-weight: 600; color: #1e293b; line-height: 1.35;\">"
            "%1"
            "</div>"
        ).arg(m_text.toHtmlEscaped()));

        QFontMetrics fm(font);
        int textWidth = fm.horizontalAdvance(m_text);

        int bubbleWidth = 0;
        int bubbleHeight = 0;

        if (textWidth <= 340 && !m_text.contains('\n')) {
            bubbleWidth = std::clamp(textWidth + 36, 100, 390);
            bubbleHeight = 44;
            m_textBrowser->document()->setTextWidth(-1);
            m_textBrowser->setFixedWidth(bubbleWidth - 24);
            m_textBrowser->setFixedHeight(bubbleHeight - 12);
        } else {
            bubbleWidth = std::clamp(std::min(textWidth + 36, 360), 220, 380);
            int innerW = bubbleWidth - 24;
            m_textBrowser->setFixedWidth(innerW);
            m_textBrowser->document()->setTextWidth(innerW);
            int docH = static_cast<int>(std::ceil(m_textBrowser->document()->size().height()));
            bubbleHeight = std::clamp(docH + 22, 54, 110);
            m_textBrowser->setFixedHeight(bubbleHeight - 14);
        }

        setFixedSize(bubbleWidth, bubbleHeight);
    } else {

        if (m_topBarWidget) m_topBarWidget->show();
        if (layout()) {
            layout()->setContentsMargins(18, 14, 18, 14);
            layout()->setSpacing(8);
        }
        m_textBrowser->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_textBrowser->document()->setDocumentMargin(2);

        QString richHtml = markdownToRichHtml(m_text);

        if (!m_appTarget.isEmpty()) {
            QString btnTitle = "打开应用";
            if (!m_appTarget.contains('.') && !m_appTarget.contains('/')) {
                btnTitle = QString("打开 %1").arg(m_appTarget);
            }
            static_cast<IconCardButton*>(m_openAppBtn)->setText(btnTitle);
            m_openAppBtn->show();
        } else {
            m_openAppBtn->hide();
        }

        // 自适应排版宽度与精确高度计算，防止上下文字被截断
        int bubbleWidth = 480;
        if (m_text.contains("|") || m_text.contains("```") || m_text.length() > 150) {
            bubbleWidth = 520;
        }

        int contentWidth = bubbleWidth - 36;
        m_textBrowser->setFixedWidth(contentWidth);
        m_textBrowser->document()->setTextWidth(contentWidth);
        m_textBrowser->setHtml(richHtml);

        int docHeight = static_cast<int>(std::ceil(m_textBrowser->document()->size().height()));
        int bubbleHeight = std::clamp(docHeight + 82, 110, 580);

        setFixedSize(bubbleWidth, bubbleHeight);
        m_textBrowser->setFixedHeight(bubbleHeight - 48);
    }

    // 自动记录重要消息到历史消息管理器 (长篇 Markdown 或任务结果)
    if (!m_text.isEmpty() && (!m_isCompactCuteMode || m_text.length() > 20)) {
        QString type = "notice";
        if (m_text.startsWith("🔍") || m_text.contains("翻译")) type = "translate";
        else if (m_text.startsWith("🤖") || !m_appTarget.isEmpty() || m_text.contains("aipy") || m_text.contains("Agent")) type = "agent_task";
        else if (m_text.startsWith("🤔") || m_text.contains("问题")) type = "ask";

        QString title = m_text.left(30).trimmed();
        if (title.contains('\n')) title = title.split('\n').first();
        MessageHistoryManager::instance()->addRecord(type, title, m_text, m_appTarget);
    }

    show();
    update();

    if (duration > 0) {
        m_remainingSeconds = std::max(1, (int)std::ceil(duration / 1000.0));
        m_isCountdownPaused = false;
        updateCountdownDisplay();
        if (!m_isCompactCuteMode && m_countdownLabel) {
            m_countdownLabel->show();
        }
        m_countdownTimer->start(1000);
    } else {
        if (m_countdownLabel) m_countdownLabel->hide();
        m_countdownTimer->stop();
    }
}

void MessageBubble::updateCountdownDisplay()
{
    if (!m_countdownLabel) return;
    if (m_isCountdownPaused) {
        m_countdownLabel->setText(QString("⏸ %1s").arg(m_remainingSeconds));
        m_countdownLabel->setStyleSheet(
            "QLabel {"
            "  background-color: #fef3c7;"
            "  color: #d97706;"
            "  font-size: 11px;"
            "  font-weight: 600;"
            "  padding: 4px 8px;"
            "  border-radius: 10px;"
            "  border: 1px solid #fde68a;"
            "}"
        );
    } else {
        m_countdownLabel->setText(QString("⏱️ %1s").arg(m_remainingSeconds));
        m_countdownLabel->setStyleSheet(
            "QLabel {"
            "  background-color: #f1f5f9;"
            "  color: #64748b;"
            "  font-size: 11px;"
            "  font-weight: 600;"
            "  padding: 4px 8px;"
            "  border-radius: 10px;"
            "  border: 1px solid #e2e8f0;"
            "}"
        );
    }
}

void MessageBubble::hideMessage()
{
    m_text.clear();
    m_appTarget.clear();
    if (m_openAppBtn) m_openAppBtn->hide();
    if (m_countdownLabel) m_countdownLabel->hide();
    if (m_hideTimer) m_hideTimer->stop();
    if (m_countdownTimer) m_countdownTimer->stop();
    hide();
}

void MessageBubble::showHistoryDialog()
{
    if (!m_historyDialog) {
        m_historyDialog = new MessageHistoryDialog(this);
        m_historyDialog->setOnReplayCallback([this](const QString &text, const QString &appTarget) {
            showMessage(text, 25000, appTarget);
        });
    }
    m_historyDialog->selectLatest();
    m_historyDialog->show();
    m_historyDialog->raise();
    m_historyDialog->activateWindow();
}

bool MessageBubble::eventFilter(QObject *watched, QEvent *event)
{
    (void)watched;
    if (event->type() == QEvent::Enter || 
        event->type() == QEvent::HoverEnter || 
        event->type() == QEvent::MouseMove ||
        event->type() == QEvent::MouseButtonPress) {
        // 鼠标移入/悬停，暂停倒计时，用户想看多久就看多久
        if (!m_isCountdownPaused) {
            m_isCountdownPaused = true;
            updateCountdownDisplay();
        }
    } else if (event->type() == QEvent::Leave || event->type() == QEvent::HoverLeave) {
        // 检查鼠标是否完全离开当前气泡全局区域
        QPoint globalMousePos = QCursor::pos();
        QRect globalRect = QRect(mapToGlobal(QPoint(0, 0)), size());
        if (!globalRect.contains(globalMousePos)) {
            if (m_isCountdownPaused) {
                m_isCountdownPaused = false;
                if (m_remainingSeconds < 6) {
                    m_remainingSeconds = 6; // 移出后至少给 6 秒从容阅读时间
                }
                updateCountdownDisplay();
            }
        }
    }
    return false;
}

void MessageBubble::enterEvent(QEnterEvent *)
{
    if (!m_isCountdownPaused) {
        m_isCountdownPaused = true;
        updateCountdownDisplay();
    }
}

void MessageBubble::leaveEvent(QEvent *)
{
    QPoint globalMousePos = QCursor::pos();
    QRect globalRect = QRect(mapToGlobal(QPoint(0, 0)), size());
    if (!globalRect.contains(globalMousePos)) {
        if (m_isCountdownPaused) {
            m_isCountdownPaused = false;
            if (m_remainingSeconds < 6) {
                m_remainingSeconds = 6;
            }
            updateCountdownDisplay();
        }
    }
}

void MessageBubble::paintEvent(QPaintEvent *)
{
    if (m_text.isEmpty()) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_isCompactCuteMode) {
        // === 萌系漫画风高颜值圆润气泡 ===
        int tailSize = 8;
        int bubbleWidth = width();
        int bubbleHeight = height() - tailSize;
        int radius = std::min(18, bubbleHeight / 2);

        QPainterPath path;
        path.addRoundedRect(0, 0, bubbleWidth, bubbleHeight, radius, radius);

        // 底部小巧尖角指向桌宠头顶
        QPolygon tail;
        tail << QPoint(bubbleWidth / 2 - 6, bubbleHeight - 1)
             << QPoint(bubbleWidth / 2, bubbleHeight + tailSize)
             << QPoint(bubbleWidth / 2 + 6, bubbleHeight - 1);
        path.addPolygon(tail);

        // 柔和微光弥散阴影
        for (int i = 3; i > 0; --i) {
            QPainterPath shadowPath;
            shadowPath.addRoundedRect(1, 1 + i, bubbleWidth - 2, bubbleHeight - 2, radius, radius);
            QPolygon shadowTail;
            shadowTail << QPoint(bubbleWidth / 2 - 6, bubbleHeight - 1 + i)
                       << QPoint(bubbleWidth / 2, bubbleHeight + tailSize + i)
                       << QPoint(bubbleWidth / 2 + 6, bubbleHeight - 1 + i);
            shadowPath.addPolygon(shadowTail);
            painter.fillPath(shadowPath, QColor(15, 23, 42, 6 * i));
        }

        // 高透纯白晶莹底色
        QLinearGradient bgGrad(0, 0, 0, bubbleHeight);
        bgGrad.setColorAt(0.0, QColor(255, 255, 255, 252));
        bgGrad.setColorAt(1.0, QColor(250, 250, 255, 250));
        painter.fillPath(path, bgGrad);

        // 梦幻微蓝紫描边
        QLinearGradient borderGrad(0, 0, bubbleWidth, bubbleHeight);
        borderGrad.setColorAt(0.0, QColor(199, 210, 254, 240));
        borderGrad.setColorAt(1.0, QColor(165, 180, 252, 230));
        painter.setPen(QPen(borderGrad, 1.2));
        painter.drawPath(path);
    } else {
        // === 专业通知卡片 (用于 Markdown / 表格 / 长文本) ===
        int tailSize = 10;
        int radius = 14;

        int bubbleWidth = width();
        int bubbleHeight = height() - tailSize;

        QPainterPath path;
        path.addRoundedRect(0, 0, bubbleWidth, bubbleHeight, radius, radius);

        QPolygon tail;
        tail << QPoint(bubbleWidth / 2 - tailSize / 2, bubbleHeight)
             << QPoint(bubbleWidth / 2, bubbleHeight + tailSize)
             << QPoint(bubbleWidth / 2 + tailSize / 2, bubbleHeight);
        path.addPolygon(tail);

        // 绘制多重弥散阴影
        for (int i = 4; i > 0; --i) {
            QPainterPath shadowPath;
            shadowPath.addRoundedRect(1, 1 + i, bubbleWidth - 2, bubbleHeight - 2, radius, radius);
            QPolygon shadowTail;
            shadowTail << QPoint(bubbleWidth / 2 - tailSize / 2, bubbleHeight + i / 2)
                       << QPoint(bubbleWidth / 2, bubbleHeight + tailSize + i)
                       << QPoint(bubbleWidth / 2 + tailSize / 2, bubbleHeight + i / 2);
            shadowPath.addPolygon(shadowTail);
            painter.fillPath(shadowPath, QColor(15, 23, 42, 4 * i));
        }

        QLinearGradient bgGrad(0, 0, 0, bubbleHeight);
        bgGrad.setColorAt(0.0, QColor(255, 255, 255, 252));
        bgGrad.setColorAt(1.0, QColor(248, 250, 252, 252));
        painter.fillPath(path, bgGrad);

        QLinearGradient borderGrad(0, 0, bubbleWidth, bubbleHeight);
        borderGrad.setColorAt(0.0, QColor(226, 232, 240, 240));
        borderGrad.setColorAt(1.0, QColor(203, 213, 225, 220));
        painter.setPen(QPen(borderGrad, 1.2));
        painter.drawPath(path);
    }
}
