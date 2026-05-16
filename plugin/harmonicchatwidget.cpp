#include "harmonicchatwidget.h"
#include "harmonicacp.h"
#include "harmonicmarkdown.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QDir>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QScrollBar>
#include <QShortcut>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtMath>

namespace {
QString messageBackgroundForRole(const QString &role) {
    if (role == QStringLiteral("user")) {
        return QStringLiteral("#e3f2fd");
    }
    if (role == QStringLiteral("error")) {
        return QStringLiteral("#ffebee");
    }
    if (role == QStringLiteral("status")) {
        return QStringLiteral("#f5f5f5");
    }
    return QStringLiteral("#ffffff");
}

QString messageTitleForRole(const QString &role) {
    if (role == QStringLiteral("user")) {
        return i18n("You");
    }
    if (role == QStringLiteral("error")) {
        return i18n("Error");
    }
    if (role == QStringLiteral("status")) {
        return i18n("Status");
    }
    return i18n("Harmonic");
}

QString renderMessageHtml(const QString &role, const QString &text, bool textIsHtml = false) {
    if (role == QStringLiteral("status")) {
        return QStringLiteral(
                   "<div style=\"background-color:%1; border-bottom:1px solid #e0e0e0; "
                   "margin:0; padding:6px; color:#666666; font-style:italic; white-space:pre-wrap;\">%2</div>")
            .arg(messageBackgroundForRole(role), text.toHtmlEscaped());
    }

    const QString body = textIsHtml ? text : text.toHtmlEscaped();
    const QString contentStyle = textIsHtml ? QString() : QStringLiteral("white-space:pre-wrap;");
    return QStringLiteral(
               "<div style=\"background-color:%1; border-bottom:1px solid #e0e0e0; "
               "margin:0; padding:8px 6px 10px 6px;\">"
               "<div style=\"font-weight:600; margin-bottom:4px;\">%2</div>"
               "<div style=\"%3\">%4</div>"
               "</div>")
        .arg(messageBackgroundForRole(role),
             messageTitleForRole(role).toHtmlEscaped(),
             contentStyle,
             body);
}
} // namespace

class ChatInputEdit : public QPlainTextEdit {
    Q_OBJECT

  public:
    explicit ChatInputEdit(QWidget *parent = nullptr)
        : QPlainTextEdit(parent) {
        setPlaceholderText(i18n("Ask something..."));
        setLineWrapMode(QPlainTextEdit::WidgetWidth);
        setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        connect(this, &QPlainTextEdit::textChanged, this, &ChatInputEdit::updateHeight);
        QTimer::singleShot(0, this, &ChatInputEdit::updateHeight);
    }

  Q_SIGNALS:
    void submitPressed();
    void historyPreviousRequested();
    void historyNextRequested();

  protected:
    void keyPressEvent(QKeyEvent *event) override {
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && !(event->modifiers() & Qt::ShiftModifier)) {
            Q_EMIT submitPressed();
            return;
        }

        if (toPlainText().isEmpty()) {
            if (event->key() == Qt::Key_Up) {
                Q_EMIT historyPreviousRequested();
                return;
            }
            if (event->key() == Qt::Key_Down) {
                Q_EMIT historyNextRequested();
                return;
            }
        }

        QPlainTextEdit::keyPressEvent(event);
    }

    void resizeEvent(QResizeEvent *event) override {
        QPlainTextEdit::resizeEvent(event);
        updateHeight();
    }

  private:
    void updateHeight() {
        const int lineHeight = QFontMetrics(font()).lineSpacing();
        constexpr int verticalPadding = 8;
        const int minHeight = lineHeight + (frameWidth() * 2) + verticalPadding;
        const int maxHeight = (lineHeight * 4) + (frameWidth() * 2) + verticalPadding;
        const int documentHeight = qCeil(document()->documentLayout()->documentSize().height()) + (frameWidth() * 2);
        const int targetHeight = qBound(minHeight, documentHeight, maxHeight);

        setFixedHeight(targetHeight);
        setVerticalScrollBarPolicy(targetHeight >= maxHeight ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    }
};

HarmonicChatWidget::HarmonicChatWidget(QWidget *parent)
    : QWidget(parent), m_acp(new HarmonicAcp(this)) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_chatLog = new QTextEdit(this);
    m_chatLog->setReadOnly(true);
    m_chatLog->setPlaceholderText(i18n("Chat with your AI coding assistant..."));
    layout->addWidget(m_chatLog);

    m_typingIndicator = new QLabel(this);
    m_typingIndicator->setStyleSheet(QStringLiteral("color: #666666; padding-left: 4px;"));
    m_typingIndicator->hide();
    layout->addWidget(m_typingIndicator);

    m_typingTimer = new QTimer(this);
    m_typingTimer->setInterval(400);
    connect(m_typingTimer, &QTimer::timeout, this, &HarmonicChatWidget::updateTypingIndicator);

    auto *inputLayout = new QHBoxLayout();
    m_input = new ChatInputEdit(this);
    m_sendButton = new QPushButton(i18n("Send"), this);
    m_sendButton->setDefault(true);

    auto *menuButton = new QToolButton(this);
    menuButton->setText(QStringLiteral("⋮"));
    menuButton->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(menuButton);

    auto *clearAction = new QAction(i18n("Clear conversation"), this);
    clearAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    clearAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(clearAction);
    menu->addAction(clearAction);
    menuButton->setMenu(menu);

    inputLayout->addWidget(m_input, 1);
    inputLayout->addWidget(m_sendButton);
    inputLayout->addWidget(menuButton);
    layout->addLayout(inputLayout);

    m_permissionBar = new QWidget(this);
    m_permissionBar->setStyleSheet(QStringLiteral(
        "background-color: #fff3cd; border: 1px solid #ffe082; border-radius: 4px;"));
    m_permissionLayout = new QHBoxLayout(m_permissionBar);
    m_permissionLayout->setContentsMargins(8, 4, 8, 4);
    m_permissionLabel = new QLabel(m_permissionBar);
    m_permissionLabel->setTextFormat(Qt::PlainText);
    m_permissionLabel->setWordWrap(true);
    m_permissionLayout->addWidget(m_permissionLabel, 1);
    m_permissionBar->hide();
    layout->addWidget(m_permissionBar);

    auto *cancelShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    cancelShortcut->setContext(Qt::WidgetWithChildrenShortcut);

    connect(clearAction, &QAction::triggered, this, &HarmonicChatWidget::clearSession);
    connect(m_sendButton, &QPushButton::clicked, this, [this]() {
        if (m_isStreaming) {
            cancelCurrentGeneration();
            return;
        }
        sendMessage();
    });
    connect(m_input, &ChatInputEdit::submitPressed, this, &HarmonicChatWidget::sendMessage);
    connect(m_input, &ChatInputEdit::historyPreviousRequested, this, &HarmonicChatWidget::showPreviousHistoryMessage);
    connect(m_input, &ChatInputEdit::historyNextRequested, this, &HarmonicChatWidget::showNextHistoryMessage);
    connect(cancelShortcut, &QShortcut::activated, this, &HarmonicChatWidget::cancelCurrentGeneration);

    connect(m_acp, &HarmonicAcp::initialized, this, &HarmonicChatWidget::onAcpInitialized);
    connect(m_acp, &HarmonicAcp::sessionCreated, this, &HarmonicChatWidget::onAcpSessionCreated);
    connect(m_acp, &HarmonicAcp::textChunk, this, &HarmonicChatWidget::onAcpTextChunk);
    connect(m_acp, &HarmonicAcp::thoughtChunk, this, &HarmonicChatWidget::onAcpThoughtChunk);
    connect(m_acp, &HarmonicAcp::toolCall, this, &HarmonicChatWidget::onAcpToolCall);
    connect(m_acp, &HarmonicAcp::toolCallUpdate, this, &HarmonicChatWidget::onAcpToolCallUpdate);
    connect(m_acp, &HarmonicAcp::permissionRequested, this, &HarmonicChatWidget::onAcpPermissionRequested);
    connect(m_acp, &HarmonicAcp::promptFinished, this, &HarmonicChatWidget::onAcpPromptFinished);
    connect(m_acp, &HarmonicAcp::errorOccurred, this, &HarmonicChatWidget::onAcpError);
    connect(m_acp, &HarmonicAcp::processFinished, this, &HarmonicChatWidget::onAcpProcessFinished);
}

HarmonicChatWidget::~HarmonicChatWidget() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }

    if (m_acp && m_acp->isRunning()) {
        m_acp->stop();
    }
}

void HarmonicChatWidget::setContext(const QString &context) {
    m_context = context;
}

void HarmonicChatWidget::setWorkingDirectory(const QString &workingDirectory) {
    m_workingDirectory = workingDirectory;
}

void HarmonicChatWidget::clearSession() {
    m_conversation.clear();
    m_streamBuffer.clear();
    m_pendingMessage.clear();
    m_cancelRequested = false;

    if (m_process) {
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
            m_process->waitForFinished(1000);
        }
        m_process->deleteLater();
        m_process = nullptr;
    }

    if (m_acp->isRunning()) {
        m_acp->stop();
    }

    resetAcpState();
    m_isStreaming = false;
    hideTypingIndicator();
    hidePermissionPrompt();
    updatePrimaryButton();
    refreshChatLog();
}

QString HarmonicChatWidget::buildConversationPrompt(const QString &message) const {
    QString prompt;

    if (!m_context.isEmpty()) {
        prompt += QStringLiteral("Current file context:\n```\n%1\n```\n\n").arg(m_context);
    }

    if (!m_conversation.isEmpty()) {
        prompt += QStringLiteral("Conversation so far:\n");
        for (const auto &msg : std::as_const(m_conversation)) {
            if (msg.role == QStringLiteral("user")) {
                prompt += QStringLiteral("User: %1\n").arg(msg.content);
            } else if (msg.role == QStringLiteral("assistant")) {
                prompt += QStringLiteral("Assistant: %1\n").arg(msg.content);
            }
        }
        prompt += QStringLiteral("\n");
    }

    prompt += QStringLiteral("User: %1").arg(message);
    return prompt;
}

QString HarmonicChatWidget::buildAcpPrompt(const QString &message) const {
    if (m_context.isEmpty()) {
        return message;
    }

    return QStringLiteral("Current file context:\n```\n%1\n```\n\nUser: %2")
        .arg(m_context, message);
}

void HarmonicChatWidget::sendMessage() {
    const QString message = m_input->toPlainText().trimmed();
    if (message.isEmpty()) {
        return;
    }

    KSharedConfig::Ptr config = KSharedConfig::openConfig();
    KConfigGroup group = config->group(QStringLiteral("Harmonic"));
    const QString backend = group.readEntry("Backend", "copilot");
    const QString command = group.readEntry("Command", "copilot");
    const bool acpInitializing = backend == QStringLiteral("copilot") && (m_acpInitializing || (m_acp->isRunning() && !m_acpSessionReady));

    if (m_isStreaming || (m_process && m_process->state() != QProcess::NotRunning) || acpInitializing) {
        m_pendingMessage = message;
        m_input->clear();
        return;
    }

    m_inputHistory.append(message);
    m_historyPosition = m_inputHistory.size();
    m_input->clear();

    appendMessage(QStringLiteral("user"), message);

    if (backend == QStringLiteral("copilot")) {
        const QString prompt = buildAcpPrompt(message);
        const QString cwd = m_workingDirectory.isEmpty() ? QDir::currentPath() : m_workingDirectory;

        if (!m_acp->isRunning()) {
            resetAcpState();
            m_acpInitializing = true;
            m_pendingAcpPrompt = prompt;
            m_acpSessionCwd = cwd;
            startStreaming(StreamBackend::Acp);
            m_acp->start(command, cwd);
            return;
        }

        if (!m_acpSessionReady || m_acpSessionCwd != cwd) {
            m_acpInitializing = true;
            m_acpSessionReady = false;
            m_pendingAcpPrompt = prompt;
            m_acpSessionCwd = cwd;
            startStreaming(StreamBackend::Acp);
            if (m_acpInitialized) {
                m_acp->createSession(cwd);
            }
            return;
        }

        startStreaming(StreamBackend::Acp);
        m_acp->sendPrompt(prompt);
        return;
    }

    if (m_acp->isRunning()) {
        m_acp->stop();
        resetAcpState();
    }

    const QString fullPrompt = buildConversationPrompt(message);

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &HarmonicChatWidget::onReadyReadStdout);
    connect(m_process, &QProcess::readyReadStandardError, this, &HarmonicChatWidget::onReadyReadStderr);
    connect(m_process, &QProcess::finished, this, &HarmonicChatWidget::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &HarmonicChatWidget::onProcessError);

    QStringList args;
    if (backend == QStringLiteral("opencode")) {
        args << QStringLiteral("-p") << fullPrompt
             << QStringLiteral("-f") << QStringLiteral("text")
             << QStringLiteral("-q");
    } else if (backend == QStringLiteral("claude-code")) {
        args << QStringLiteral("--print") << fullPrompt;
    } else {
        args << fullPrompt;
    }

    startStreaming(StreamBackend::Process);
    m_process->start(command, args);
}

void HarmonicChatWidget::cancelCurrentGeneration() {
    if (!m_isStreaming) {
        return;
    }

    m_cancelRequested = true;
    hideTypingIndicator();
    hidePermissionPrompt();

    if (m_acp && (m_acpInitializing || (m_acp->isRunning() && !m_acpSessionReady))) {
        m_pendingAcpPrompt.clear();
        m_acp->stop();
        m_acpInitializing = false;
        m_acpInitialized = false;
        m_acpSessionReady = false;
        finishStreaming();
        return;
    }

    if (m_acp->isRunning() && m_acpSessionReady) {
        m_acp->cancelPrompt();
        return;
    }

    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
    }
}

void HarmonicChatWidget::startStreaming(StreamBackend backend) {
    m_isStreaming = true;
    m_cancelRequested = false;
    m_streamBackend = backend;
    m_streamBuffer.clear();
    showTypingIndicator();
    updatePrimaryButton();
    refreshChatLog();
}

void HarmonicChatWidget::onReadyReadStdout() {
    if (!m_process) {
        return;
    }

    const QString text = QString::fromUtf8(m_process->readAllStandardOutput());
    if (text.isEmpty()) {
        return;
    }

    m_streamBuffer += text;
    hideTypingIndicator();
    refreshChatLog();
}

void HarmonicChatWidget::onReadyReadStderr() {
    if (!m_process) {
        return;
    }

    const QString text = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
    if (text.isEmpty()) {
        return;
    }

    if (text.contains(QStringLiteral("Allow"), Qt::CaseInsensitive) || text.contains(QStringLiteral("(y/n)"), Qt::CaseInsensitive) || text.contains(QStringLiteral("permission"), Qt::CaseInsensitive)) {
        showPermissionPrompt(text);
    }
}

void HarmonicChatWidget::finishStreaming() {
    m_isStreaming = false;
    m_streamBackend = StreamBackend::None;
    hideTypingIndicator();
    updatePrimaryButton();
    hidePermissionPrompt();

    const QString response = m_streamBuffer.trimmed();
    if (!response.isEmpty()) {
        appendMessage(QStringLiteral("assistant"), response, harmonicMarkdownToHtml(response));
    } else {
        refreshChatLog();
    }
    m_streamBuffer.clear();
}

void HarmonicChatWidget::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    if (m_process) {
        const QString remaining = QString::fromUtf8(m_process->readAllStandardOutput());
        if (!remaining.isEmpty()) {
            m_streamBuffer += remaining;
            hideTypingIndicator();
        }
    }

    const bool cancelled = m_cancelRequested;
    const QString errorText = (!cancelled && m_process)
                                  ? QString::fromUtf8(m_process->readAllStandardError()).trimmed()
                                  : QString();

    finishStreaming();

    if (!cancelled && (status != QProcess::NormalExit || exitCode != 0) && !errorText.isEmpty()) {
        appendMessage(QStringLiteral("error"), errorText);
    }

    m_cancelRequested = false;
    m_input->setFocus();

    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }

    processQueuedMessage();
}

void HarmonicChatWidget::onProcessError(QProcess::ProcessError error) {
    if (!m_process) {
        return;
    }

    const QString errorString = m_process->errorString();
    finishStreaming();
    appendMessage(QStringLiteral("error"),
                  error == QProcess::FailedToStart
                      ? i18n("Failed to start backend: %1", errorString)
                      : i18n("Backend process error: %1", errorString));

    m_input->setFocus();
    m_process->deleteLater();
    m_process = nullptr;
    processQueuedMessage();
}

void HarmonicChatWidget::onAcpInitialized(const QJsonObject &agentInfo) {
    Q_UNUSED(agentInfo);
    if (m_pendingAcpPrompt.isEmpty()) {
        return;
    }

    m_acpInitialized = true;
    m_acpInitializing = true;
    m_acp->createSession(m_acpSessionCwd.isEmpty() ? QDir::currentPath() : m_acpSessionCwd);
}

void HarmonicChatWidget::onAcpSessionCreated(const QString &sessionId) {
    Q_UNUSED(sessionId);
    if (m_pendingAcpPrompt.isEmpty()) {
        return;
    }

    m_acpInitializing = false;
    m_acpSessionReady = true;

    const QString prompt = m_pendingAcpPrompt;
    m_pendingAcpPrompt.clear();
    m_acp->sendPrompt(prompt);
}

void HarmonicChatWidget::onAcpTextChunk(const QString &text) {
    if (text.isEmpty()) {
        return;
    }

    m_streamBuffer += text;
    hideTypingIndicator();
    refreshChatLog();
}

void HarmonicChatWidget::onAcpThoughtChunk(const QString &text) {
    if (text.trimmed().isEmpty()) {
        return;
    }

    hideTypingIndicator();
    appendMessage(QStringLiteral("status"), text);
}

void HarmonicChatWidget::onAcpToolCall(const QString &toolCallId, const QString &title, const QString &kind) {
    Q_UNUSED(toolCallId);
    appendMessage(QStringLiteral("status"),
                  kind.isEmpty() ? i18n("Tool: %1", title) : i18n("Tool: %1 (%2)", title, kind));
}

void HarmonicChatWidget::onAcpToolCallUpdate(const QString &toolCallId, const QString &status, const QString &content) {
    Q_UNUSED(toolCallId);
    appendMessage(QStringLiteral("status"),
                  content.isEmpty() ? i18n("Tool update: %1", status)
                                    : i18n("Tool update: %1 — %2", status, content));
}

void HarmonicChatWidget::onAcpPermissionRequested(const QJsonValue &requestId, const QString &title, const QJsonArray &options) {
    appendMessage(QStringLiteral("status"), i18n("Permission requested: %1", title));
    showPermissionPrompt(title, requestId, options);
}

void HarmonicChatWidget::onAcpPromptFinished(const QString &stopReason) {
    Q_UNUSED(stopReason);
    finishStreaming();
    m_cancelRequested = false;
    m_input->setFocus();
    processQueuedMessage();
}

void HarmonicChatWidget::onAcpError(const QString &message) {
    if (m_isStreaming && m_streamBackend == StreamBackend::Acp) {
        finishStreaming();
    }

    if (!message.isEmpty() && !m_cancelRequested) {
        appendMessage(QStringLiteral("error"), message);
    }

    if (!m_acp->isRunning()) {
        resetAcpState();
    }

    m_cancelRequested = false;
    m_input->setFocus();
    processQueuedMessage();
}

void HarmonicChatWidget::onAcpProcessFinished() {
    if (m_isStreaming && m_streamBackend == StreamBackend::Acp) {
        finishStreaming();
    }

    resetAcpState();
    m_cancelRequested = false;
    m_input->setFocus();
    processQueuedMessage();
}

void HarmonicChatWidget::appendMessage(const QString &role,
                                       const QString &text,
                                       const QString &renderedHtml) {
    m_conversation.append({role, text, renderedHtml});
    refreshChatLog();
}

void HarmonicChatWidget::refreshChatLog() {
    QString html;
    for (const auto &msg : std::as_const(m_conversation)) {
        const bool hasRenderedHtml = !msg.renderedHtml.isEmpty();
        html += renderMessageHtml(msg.role,
                                  hasRenderedHtml ? msg.renderedHtml : msg.content,
                                  hasRenderedHtml);
    }
    if (m_isStreaming && !m_streamBuffer.isEmpty()) {
        html += renderMessageHtml(QStringLiteral("assistant"), m_streamBuffer);
    }

    m_chatLog->setHtml(html);
    scrollChatToBottom();
}

void HarmonicChatWidget::scrollChatToBottom() {
    if (auto *sb = m_chatLog->verticalScrollBar()) {
        sb->setValue(sb->maximum());
    }
}

void HarmonicChatWidget::updatePrimaryButton() {
    m_sendButton->setText(m_isStreaming ? i18n("Stop") : i18n("Send"));
}

void HarmonicChatWidget::showTypingIndicator() {
    m_waitingForFirstChunk = true;
    m_typingDots = 0;
    updateTypingIndicator();
    m_typingIndicator->show();
    m_typingTimer->start();
}

void HarmonicChatWidget::hideTypingIndicator() {
    m_waitingForFirstChunk = false;
    m_typingTimer->stop();
    m_typingIndicator->hide();
}

void HarmonicChatWidget::updateTypingIndicator() {
    if (!m_waitingForFirstChunk) {
        return;
    }

    m_typingIndicator->setText(i18n("Thinking%1", QStringLiteral(".").repeated(m_typingDots)));
    m_typingDots = (m_typingDots + 1) % 4;
}

void HarmonicChatWidget::showPreviousHistoryMessage() {
    if (m_inputHistory.isEmpty()) {
        return;
    }

    if (m_historyPosition > 0) {
        --m_historyPosition;
    } else {
        m_historyPosition = 0;
    }

    m_input->setPlainText(m_inputHistory.at(m_historyPosition));
    m_input->moveCursor(QTextCursor::End);
}

void HarmonicChatWidget::showNextHistoryMessage() {
    if (m_inputHistory.isEmpty()) {
        return;
    }

    if (m_historyPosition < m_inputHistory.size() - 1) {
        ++m_historyPosition;
        m_input->setPlainText(m_inputHistory.at(m_historyPosition));
    } else {
        m_historyPosition = m_inputHistory.size();
        m_input->clear();
        return;
    }

    m_input->moveCursor(QTextCursor::End);
}

void HarmonicChatWidget::showPermissionPrompt(const QString &description,
                                              const QJsonValue &requestId,
                                              const QJsonArray &options) {
    while (m_permissionLayout->count() > 1) {
        QLayoutItem *item = m_permissionLayout->takeAt(1);
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

    m_permissionLabel->setText(description);

    if ((requestId.isNull() || requestId.isUndefined()) && options.isEmpty()) {
        const struct {
            const char *label;
            const char *value;
            const char *style;
        } fallbackOptions[] = {
            {"Allow", "y\n", "background-color: #2e7d32; color: white;"},
            {"Always Allow", "always\n", ""},
            {"Deny", "n\n", "background-color: #c62828; color: white;"},
        };

        for (const auto &option : fallbackOptions) {
            auto *button = new QPushButton(i18n(option.label), m_permissionBar);
            if (*option.style != '\0') {
                button->setStyleSheet(QString::fromUtf8(option.style));
            }
            connect(button, &QPushButton::clicked, this, [this, value = QByteArray(option.value)]() {
                if (m_process && m_process->state() == QProcess::Running) {
                    m_process->write(value);
                }
                hidePermissionPrompt();
            });
            m_permissionLayout->addWidget(button);
        }
    } else {
        bool hasValidOption = false;
        for (const QJsonValue &value : options) {
            const QJsonObject option = value.toObject();
            const QString optionId = option[QStringLiteral("optionId")].toString();
            if (optionId.isEmpty()) {
                continue;
            }
            hasValidOption = true;

            const QString name = option[QStringLiteral("name")].toString(optionId);
            const QString kind = option[QStringLiteral("kind")].toString();
            auto *button = new QPushButton(name, m_permissionBar);

            if (kind.startsWith(QStringLiteral("allow"))) {
                button->setStyleSheet(QStringLiteral("background-color: #2e7d32; color: white;"));
            } else if (kind.startsWith(QStringLiteral("reject"))) {
                button->setStyleSheet(QStringLiteral("background-color: #c62828; color: white;"));
            }

            connect(button, &QPushButton::clicked, this, [this, requestId, optionId]() {
                if (m_acp && m_acp->isRunning()) {
                    m_acp->respondToPermission(requestId, optionId);
                }
                hidePermissionPrompt();
            });
            m_permissionLayout->addWidget(button);
        }

        if (!hasValidOption) {
            auto *button = new QPushButton(i18n("Deny"), m_permissionBar);
            button->setStyleSheet(QStringLiteral("background-color: #c62828; color: white;"));
            connect(button, &QPushButton::clicked, this, [this, requestId]() {
                if (m_acp && m_acp->isRunning()) {
                    m_acp->denyPermission(requestId);
                }
                hidePermissionPrompt();
            });
            m_permissionLayout->addWidget(button);
        }
    }

    m_permissionBar->show();
}

void HarmonicChatWidget::hidePermissionPrompt() {
    m_permissionLabel->clear();
    m_permissionBar->hide();
}

void HarmonicChatWidget::processQueuedMessage() {
    if (m_pendingMessage.isEmpty() || m_isStreaming) {
        return;
    }

    const QString queued = m_pendingMessage;
    m_pendingMessage.clear();
    m_input->setPlainText(queued);
    sendMessage();
}

void HarmonicChatWidget::resetAcpState() {
    m_pendingAcpPrompt.clear();
    m_acpSessionCwd.clear();
    m_acpInitialized = false;
    m_acpInitializing = false;
    m_acpSessionReady = false;
}

#include "harmonicchatwidget.moc"
