#include "harmonicchatwidget.h"

#include <KSharedConfig>
#include <KConfigGroup>
#include <KLocalizedString>

#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProcess>
#include <QScrollBar>
#include <QKeyEvent>
#include <QTextCursor>
#include <QMenu>
#include <QToolButton>

class ChatInputLine : public QLineEdit
{
    Q_OBJECT
public:
    using QLineEdit::QLineEdit;

Q_SIGNALS:
    void submitPressed();

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            Q_EMIT submitPressed();
            return;
        }
        QLineEdit::keyPressEvent(event);
    }
};

HarmonicChatWidget::HarmonicChatWidget(QWidget *parent)
    : QWidget(parent)
    , m_process(nullptr)
    , m_isStreaming(false)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_chatLog = new QTextEdit(this);
    m_chatLog->setReadOnly(true);
    m_chatLog->setPlaceholderText(i18n("Chat with your AI coding assistant..."));
    layout->addWidget(m_chatLog);

    auto *inputLayout = new QHBoxLayout();
    auto *inputLine = new ChatInputLine(this);
    m_input = inputLine;
    m_input->setPlaceholderText(i18n("Ask something..."));
    m_sendButton = new QPushButton(i18n("Send"), this);
    m_sendButton->setDefault(true);

    auto *menuButton = new QToolButton(this);
    menuButton->setText(QStringLiteral("⋮"));
    menuButton->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(menuButton);
    menu->addAction(i18n("Clear conversation"), this, &HarmonicChatWidget::clearSession);
    menuButton->setMenu(menu);

    inputLayout->addWidget(m_input);
    inputLayout->addWidget(m_sendButton);
    inputLayout->addWidget(menuButton);
    layout->addLayout(inputLayout);

    // Permission prompt bar (hidden by default)
    m_permissionBar = new QWidget(this);
    auto *permLayout = new QHBoxLayout(m_permissionBar);
    permLayout->setContentsMargins(4, 2, 4, 2);
    auto *allowBtn = new QPushButton(i18n("Allow"), m_permissionBar);
    auto *denyBtn = new QPushButton(i18n("Deny"), m_permissionBar);
    auto *alwaysBtn = new QPushButton(i18n("Always Allow"), m_permissionBar);
    allowBtn->setStyleSheet(QStringLiteral("background-color: #2e7d32; color: white;"));
    denyBtn->setStyleSheet(QStringLiteral("background-color: #c62828; color: white;"));
    permLayout->addWidget(allowBtn);
    permLayout->addWidget(alwaysBtn);
    permLayout->addWidget(denyBtn);
    m_permissionBar->hide();
    layout->addWidget(m_permissionBar);

    connect(allowBtn, &QPushButton::clicked, this, &HarmonicChatWidget::approvePermission);
    connect(alwaysBtn, &QPushButton::clicked, this, [this]() {
        if (m_process && m_process->state() == QProcess::Running) {
            m_process->write("always\n");
        }
        hidePermissionPrompt();
    });
    connect(denyBtn, &QPushButton::clicked, this, &HarmonicChatWidget::denyPermission);

    connect(m_sendButton, &QPushButton::clicked, this, &HarmonicChatWidget::sendMessage);
    connect(inputLine, &ChatInputLine::submitPressed, this, &HarmonicChatWidget::sendMessage);
}

HarmonicChatWidget::~HarmonicChatWidget()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
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
    m_chatLog->clear();
}

QString HarmonicChatWidget::buildConversationPrompt(const QString &message)
{
    QString prompt;

    if (!m_context.isEmpty()) {
        prompt += QStringLiteral("Current file context:\n```\n%1\n```\n\n").arg(m_context);
    }

    // Include conversation history
    if (!m_conversation.isEmpty()) {
        prompt += QStringLiteral("Conversation so far:\n");
        for (const auto &msg : m_conversation) {
            if (msg.role == QStringLiteral("user")) {
                prompt += QStringLiteral("User: %1\n").arg(msg.content);
            } else {
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
    QString message = m_input->text().trimmed();
    if (message.isEmpty()) {
        return;
    }

    // If a response is still streaming, queue this message
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_pendingMessage = message;
        m_input->clear();
        return;
    }

    m_input->clear();

    appendMessage(i18n("You"), message);
    m_conversation.append({QStringLiteral("user"), message});

    QString fullPrompt = buildConversationPrompt(message);

    // Read config
    KSharedConfig::Ptr config = KSharedConfig::openConfig();
    KConfigGroup group = config->group(QStringLiteral("Harmonic"));
    QString backend = group.readEntry("Backend", "copilot");
    QString command = group.readEntry("Command", "copilot");

    // Launch process
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

void HarmonicChatWidget::startStreaming()
{
    m_isStreaming = true;
    m_streamBuffer.clear();
    // Add the "Harmonic:" header, response will stream after it
    m_chatLog->append(QStringLiteral("<p><b>%1:</b></p><pre style=\"white-space: pre-wrap;\">").arg(i18n("Harmonic")));
}

void HarmonicChatWidget::onReadyReadStdout()
{
    if (!m_process) return;

    QByteArray data = m_process->readAllStandardOutput();
    QString text = QString::fromUtf8(data);
    m_streamBuffer += text;

    // Append the new text to the chat log in place
    QTextCursor cursor = m_chatLog->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
    m_chatLog->setTextCursor(cursor);

    // Scroll to bottom
    QScrollBar *sb = m_chatLog->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void HarmonicChatWidget::onReadyReadStderr()
{
    if (!m_process) return;

    QByteArray data = m_process->readAllStandardError();
    QString text = QString::fromUtf8(data).trimmed();
    if (text.isEmpty()) return;

    // Detect permission prompts (copilot asks y/n/always)
    if (text.contains(QStringLiteral("Allow")) || text.contains(QStringLiteral("allow"))
        || text.contains(QStringLiteral("(y/n)")) || text.contains(QStringLiteral("(Y/n)"))
        || text.contains(QStringLiteral("permission"))) {
        showPermissionPrompt(text);
    }

    // Show thinking/status in muted italic
    QTextCursor cursor = m_chatLog->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertHtml(QStringLiteral("<i style=\"color: gray;\">%1</i><br>").arg(text.toHtmlEscaped()));
    m_chatLog->setTextCursor(cursor);

    QScrollBar *sb = m_chatLog->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void HarmonicChatWidget::finishStreaming()
{
    m_isStreaming = false;
    // Close the pre tag and add separator
    QTextCursor cursor = m_chatLog->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertHtml(QStringLiteral("</pre><hr>"));
    m_chatLog->setTextCursor(cursor);

    // Store in conversation history
    if (!m_streamBuffer.isEmpty()) {
        m_conversation.append({QStringLiteral("assistant"), m_streamBuffer.trimmed()});
    }
    m_streamBuffer.clear();
}

void HarmonicChatWidget::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    // Read any remaining output
    if (m_process) {
        QByteArray remaining = m_process->readAllStandardOutput();
        if (!remaining.isEmpty()) {
            QString text = QString::fromUtf8(remaining);
            m_streamBuffer += text;
            QTextCursor cursor = m_chatLog->textCursor();
            cursor.movePosition(QTextCursor::End);
            cursor.insertText(text);
            m_chatLog->setTextCursor(cursor);
        }
    }

    if (status != QProcess::NormalExit || exitCode != 0) {
        QString err = m_process ? QString::fromUtf8(m_process->readAllStandardError()).trimmed() : QString();
        if (!err.isEmpty()) {
            QTextCursor cursor = m_chatLog->textCursor();
            cursor.movePosition(QTextCursor::End);
            cursor.insertHtml(QStringLiteral("<br><i style=\"color:red;\">%1</i>").arg(err.toHtmlEscaped()));
        }
    }

    finishStreaming();

    m_input->setFocus();

    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }

    // If a message was queued while we were streaming, send it now
    if (!m_pendingMessage.isEmpty()) {
        QString queued = m_pendingMessage;
        m_pendingMessage.clear();
        m_input->setText(queued);
        sendMessage();
    }
}

void HarmonicChatWidget::onProcessError(QProcess::ProcessError error)
{
    Q_UNUSED(error);

    finishStreaming();
    appendMessage(i18n("Error"), i18n("Failed to start backend: %1", m_process->errorString()));

    m_sendButton->setEnabled(true);
    m_input->setFocus();

    m_process->deleteLater();
    m_process = nullptr;
}

void HarmonicChatWidget::appendMessage(const QString &sender, const QString &text)
{
    QString html = QStringLiteral("<p><b>%1:</b></p><pre style=\"white-space: pre-wrap;\">%2</pre><hr>")
        .arg(sender.toHtmlEscaped(), text.toHtmlEscaped());
    m_chatLog->append(html);

    QScrollBar *sb = m_chatLog->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void HarmonicChatWidget::showPermissionPrompt(const QString &description)
{
    Q_UNUSED(description);
    m_permissionBar->show();
}

void HarmonicChatWidget::hidePermissionPrompt()
{
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
