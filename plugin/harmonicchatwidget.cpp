#include "harmonicchatwidget.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QScrollBar>
#include <QShortcut>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextOption>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtMath>

namespace {
QString messageBackgroundForRole(const QString &role)
{
    if (role == QStringLiteral("user")) {
        return QStringLiteral("#e3f2fd");
    }
    if (role == QStringLiteral("error")) {
        return QStringLiteral("#ffebee");
    }
    return QStringLiteral("#ffffff");
}

QString messageTitleForRole(const QString &role)
{
    if (role == QStringLiteral("user")) {
        return i18n("You");
    }
    if (role == QStringLiteral("error")) {
        return i18n("Error");
    }
    return i18n("Harmonic");
}

QString renderMessageHtml(const QString &role, const QString &text)
{
    return QStringLiteral(
               "<div style=\"background-color:%1; border-bottom:1px solid #e0e0e0; "
               "margin:0; padding:8px 6px 10px 6px;\">"
               "<div style=\"font-weight:600; margin-bottom:4px;\">%2</div>"
               "<div style=\"white-space:pre-wrap;\">%3</div>"
               "</div>")
        .arg(messageBackgroundForRole(role),
             messageTitleForRole(role).toHtmlEscaped(),
             text.toHtmlEscaped());
}
}

class ChatInputEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit ChatInputEdit(QWidget *parent = nullptr)
        : QPlainTextEdit(parent)
        , m_historyBrowsing(false)
    {
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

public:
    bool isHistoryBrowsing() const
    {
        return m_historyBrowsing;
    }

    void setHistoryText(const QString &text)
    {
        m_historyBrowsing = true;
        setPlainText(text);
        moveCursor(QTextCursor::End);
    }

    void clearHistoryText()
    {
        m_historyBrowsing = false;
        clear();
    }

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
            && !(event->modifiers() & Qt::ShiftModifier)) {
            Q_EMIT submitPressed();
            return;
        }

        if (toPlainText().isEmpty() || m_historyBrowsing) {
            if (event->key() == Qt::Key_Up) {
                Q_EMIT historyPreviousRequested();
                return;
            }
            if (event->key() == Qt::Key_Down) {
                Q_EMIT historyNextRequested();
                return;
            }
        }

        if (m_historyBrowsing && !event->text().isEmpty()) {
            m_historyBrowsing = false;
        }

        QPlainTextEdit::keyPressEvent(event);
    }

    void resizeEvent(QResizeEvent *event) override
    {
        QPlainTextEdit::resizeEvent(event);
        updateHeight();
    }

private:
    void updateHeight()
    {
        const int lineHeight = QFontMetrics(font()).lineSpacing();
        const int minHeight = lineHeight + (frameWidth() * 2) + 8;
        const int maxHeight = (lineHeight * 4) + (frameWidth() * 2) + 8;
        const int documentHeight = qCeil(document()->documentLayout()->documentSize().height()) + (frameWidth() * 2);
        const int targetHeight = qBound(minHeight, documentHeight, maxHeight);

        setFixedHeight(targetHeight);
        setVerticalScrollBarPolicy(targetHeight >= maxHeight ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    }

    bool m_historyBrowsing;
};

HarmonicChatWidget::HarmonicChatWidget(QWidget *parent)
    : QWidget(parent)
    , m_chatLog(nullptr)
    , m_input(nullptr)
    , m_sendButton(nullptr)
    , m_typingIndicator(nullptr)
    , m_permissionBar(nullptr)
    , m_permissionLabel(nullptr)
    , m_typingTimer(nullptr)
    , m_process(nullptr)
    , m_historyPosition(0)
    , m_typingDots(0)
    , m_isStreaming(false)
    , m_waitingForFirstChunk(false)
    , m_cancelRequested(false)
{
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
    auto *permLayout = new QHBoxLayout(m_permissionBar);
    permLayout->setContentsMargins(8, 4, 8, 4);
    m_permissionLabel = new QLabel(m_permissionBar);
    m_permissionLabel->setWordWrap(true);
    auto *allowBtn = new QPushButton(i18n("Allow"), m_permissionBar);
    auto *denyBtn = new QPushButton(i18n("Deny"), m_permissionBar);
    auto *alwaysBtn = new QPushButton(i18n("Always Allow"), m_permissionBar);
    allowBtn->setStyleSheet(QStringLiteral("background-color: #2e7d32; color: white;"));
    denyBtn->setStyleSheet(QStringLiteral("background-color: #c62828; color: white;"));
    permLayout->addWidget(m_permissionLabel, 1);
    permLayout->addWidget(allowBtn);
    permLayout->addWidget(alwaysBtn);
    permLayout->addWidget(denyBtn);
    m_permissionBar->hide();
    layout->addWidget(m_permissionBar);

    auto *cancelShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    cancelShortcut->setContext(Qt::WidgetWithChildrenShortcut);

    connect(allowBtn, &QPushButton::clicked, this, &HarmonicChatWidget::approvePermission);
    connect(alwaysBtn, &QPushButton::clicked, this, [this]() {
        if (m_process && m_process->state() == QProcess::Running) {
            m_process->write("always\n");
        }
        hidePermissionPrompt();
    });
    connect(denyBtn, &QPushButton::clicked, this, &HarmonicChatWidget::denyPermission);
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
}

HarmonicChatWidget::~HarmonicChatWidget()
{
    m_isDestroying = true;
    m_pendingMessage.clear();
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->disconnect();
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

void HarmonicChatWidget::setContext(const QString &context)
{
    m_context = context;
}

void HarmonicChatWidget::clearSession()
{
    m_conversation.clear();
    refreshChatLog();
}

QString HarmonicChatWidget::buildConversationPrompt(const QString &message)
{
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

void HarmonicChatWidget::sendMessage()
{
    if (m_isDestroying) {
        return;
    }

    const QString message = m_input->toPlainText().trimmed();
    if (message.isEmpty()) {
        return;
    }

    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_pendingMessage = message;
        m_input->clearHistoryText();
        return;
    }

    m_inputHistory.append(message);
    m_historyPosition = m_inputHistory.size();
    m_input->clearHistoryText();

    appendMessage(QStringLiteral("user"), message);

    const QString fullPrompt = buildConversationPrompt(message);

    KSharedConfig::Ptr config = KSharedConfig::openConfig();
    KConfigGroup group = config->group(QStringLiteral("Harmonic"));
    const QString backend = group.readEntry("Backend", "copilot");
    const QString command = group.readEntry("Command", "copilot");

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &HarmonicChatWidget::onReadyReadStdout);
    connect(m_process, &QProcess::readyReadStandardError, this, &HarmonicChatWidget::onReadyReadStderr);
    connect(m_process, &QProcess::finished, this, &HarmonicChatWidget::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &HarmonicChatWidget::onProcessError);

    QStringList args;
    if (backend == QStringLiteral("copilot")) {
        args << QStringLiteral("-p") << fullPrompt
             << QStringLiteral("--output-format") << QStringLiteral("text")
             << QStringLiteral("--stream") << QStringLiteral("on")
             << QStringLiteral("--no-color");
    } else if (backend == QStringLiteral("opencode")) {
        args << QStringLiteral("-p") << fullPrompt
             << QStringLiteral("-f") << QStringLiteral("text")
             << QStringLiteral("-q");
    } else if (backend == QStringLiteral("claude-code")) {
        args << QStringLiteral("--print") << fullPrompt;
    } else {
        args << fullPrompt;
    }

    startStreaming();
    m_process->start(command, args);
}

void HarmonicChatWidget::cancelCurrentGeneration()
{
    if (m_isDestroying || !m_process || m_process->state() == QProcess::NotRunning) {
        return;
    }

    m_cancelRequested = true;
    hideTypingIndicator();
    hidePermissionPrompt();
    m_process->kill();
}

void HarmonicChatWidget::startStreaming()
{
    m_isStreaming = true;
    m_cancelRequested = false;
    m_streamBuffer.clear();
    showTypingIndicator();
    updatePrimaryButton();
    refreshChatLog();

    QTextCursor cursor = m_chatLog->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertBlock();
    cursor.insertText(i18n("Harmonic: "));
    m_streamCursor = cursor;
    m_chatLog->setTextCursor(m_streamCursor);
    scrollChatToBottom();
}

void HarmonicChatWidget::onReadyReadStdout()
{
    if (!m_process) {
        return;
    }

    const QString text = QString::fromUtf8(m_process->readAllStandardOutput());
    if (text.isEmpty()) {
        return;
    }

    m_streamBuffer += text;
    hideTypingIndicator();

    if (m_streamCursor.isNull()) {
        m_streamCursor = m_chatLog->textCursor();
        m_streamCursor.movePosition(QTextCursor::End);
    }
    m_streamCursor.insertText(text);
    m_chatLog->setTextCursor(m_streamCursor);
    scrollChatToBottom();
}

void HarmonicChatWidget::onReadyReadStderr()
{
    if (!m_process) {
        return;
    }

    const QString text = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
    if (text.isEmpty()) {
        return;
    }

    if (text.contains(QStringLiteral("Allow"), Qt::CaseInsensitive)
        || text.contains(QStringLiteral("(y/n)"), Qt::CaseInsensitive)
        || text.contains(QStringLiteral("permission"), Qt::CaseInsensitive)) {
        showPermissionPrompt(text);
    }
}

void HarmonicChatWidget::finishStreaming()
{
    m_isStreaming = false;
    hideTypingIndicator();
    updatePrimaryButton();
    hidePermissionPrompt();

    const QString response = m_streamBuffer.trimmed();
    if (!response.isEmpty()) {
        m_conversation.append({QStringLiteral("assistant"), response});
    }
    m_streamBuffer.clear();
    m_streamCursor = QTextCursor();
    refreshChatLog();
}

void HarmonicChatWidget::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
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

    if (!m_pendingMessage.isEmpty()) {
        const QString queued = m_pendingMessage;
        m_pendingMessage.clear();
        m_input->setPlainText(queued);
        sendMessage();
    }
}

void HarmonicChatWidget::onProcessError(QProcess::ProcessError error)
{
    if (error != QProcess::FailedToStart || !m_process) {
        return;
    }

    const QString errorString = m_process->errorString();
    finishStreaming();
    appendMessage(QStringLiteral("error"), i18n("Failed to start backend: %1", errorString));

    m_input->setFocus();
    m_process->deleteLater();
    m_process = nullptr;
}

void HarmonicChatWidget::appendMessage(const QString &role, const QString &text)
{
    m_conversation.append({role, text});
    refreshChatLog();
}

void HarmonicChatWidget::refreshChatLog()
{
    QString html;
    for (const auto &msg : std::as_const(m_conversation)) {
        html += renderMessageHtml(msg.role, msg.content);
    }
    if (m_isStreaming && !m_streamBuffer.isEmpty()) {
        html += renderMessageHtml(QStringLiteral("assistant"), m_streamBuffer);
    }

    m_chatLog->setHtml(html);
    scrollChatToBottom();
}

void HarmonicChatWidget::scrollChatToBottom()
{
    if (auto *sb = m_chatLog->verticalScrollBar()) {
        sb->setValue(sb->maximum());
    }
}

void HarmonicChatWidget::updatePrimaryButton()
{
    m_sendButton->setText(m_isStreaming ? i18n("Stop") : i18n("Send"));
}

void HarmonicChatWidget::showTypingIndicator()
{
    m_waitingForFirstChunk = true;
    m_typingDots = 0;
    updateTypingIndicator();
    m_typingIndicator->show();
    m_typingTimer->start();
}

void HarmonicChatWidget::hideTypingIndicator()
{
    m_waitingForFirstChunk = false;
    m_typingTimer->stop();
    m_typingIndicator->hide();
}

void HarmonicChatWidget::updateTypingIndicator()
{
    if (!m_waitingForFirstChunk) {
        return;
    }

    m_typingIndicator->setText(i18n("Thinking%1", QStringLiteral(".").repeated(m_typingDots)));
    m_typingDots = (m_typingDots + 1) % 4;
}

void HarmonicChatWidget::showPreviousHistoryMessage()
{
    if (m_inputHistory.isEmpty()) {
        return;
    }

    if (!m_input->isHistoryBrowsing() && m_input->toPlainText().isEmpty()) {
        m_historyPosition = m_inputHistory.size();
    }

    if (m_historyPosition > 0) {
        --m_historyPosition;
    } else {
        m_historyPosition = 0;
    }

    m_input->setHistoryText(m_inputHistory.at(m_historyPosition));
}

void HarmonicChatWidget::showNextHistoryMessage()
{
    if (m_inputHistory.isEmpty() || (!m_input->isHistoryBrowsing() && m_input->toPlainText().isEmpty())) {
        return;
    }

    if (m_historyPosition < m_inputHistory.size() - 1) {
        ++m_historyPosition;
        m_input->setHistoryText(m_inputHistory.at(m_historyPosition));
    } else {
        m_historyPosition = m_inputHistory.size();
        m_input->clearHistoryText();
    }
}

void HarmonicChatWidget::showPermissionPrompt(const QString &description)
{
    m_permissionLabel->setText(description);
    m_permissionBar->show();
}

void HarmonicChatWidget::hidePermissionPrompt()
{
    m_permissionLabel->clear();
    m_permissionBar->hide();
}

void HarmonicChatWidget::approvePermission()
{
    if (m_process && m_process->state() == QProcess::Running) {
        m_process->write("y\n");
    }
    hidePermissionPrompt();
}

void HarmonicChatWidget::denyPermission()
{
    if (m_process && m_process->state() == QProcess::Running) {
        m_process->write("n\n");
    }
    hidePermissionPrompt();
}

#include "harmonicchatwidget.moc"
