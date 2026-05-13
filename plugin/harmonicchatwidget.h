#ifndef HARMONICCHATWIDGET_H
#define HARMONICCHATWIDGET_H

#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QStringList>
#include <QWidget>

class ChatInputEdit;
class HarmonicAcp;
class QLabel;
class QPushButton;
class QTextEdit;
class QTimer;
class QHBoxLayout;

class HarmonicChatWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HarmonicChatWidget(QWidget *parent = nullptr);
    ~HarmonicChatWidget() override;

    void setContext(const QString &context);
    void setWorkingDirectory(const QString &workingDirectory);

private Q_SLOTS:
    void sendMessage();
    void cancelCurrentGeneration();
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
    void onAcpInitialized(const QJsonObject &agentInfo);
    void onAcpSessionCreated(const QString &sessionId);
    void onAcpTextChunk(const QString &text);
    void onAcpThoughtChunk(const QString &text);
    void onAcpToolCall(const QString &toolCallId, const QString &title, const QString &kind);
    void onAcpToolCallUpdate(const QString &toolCallId, const QString &status, const QString &content);
    void onAcpPermissionRequested(int requestId, const QString &title, const QJsonArray &options);
    void onAcpPromptFinished(const QString &stopReason);
    void onAcpError(const QString &message);
    void onAcpProcessFinished();
    void clearSession();

private:
    void appendMessage(const QString &role,
                       const QString &text,
                       const QString &renderedHtml = QString());
    void refreshChatLog();
    void scrollChatToBottom();
    void startStreaming();
    void finishStreaming();
    void updatePrimaryButton();
    void showTypingIndicator();
    void hideTypingIndicator();
    void updateTypingIndicator();
    void showPreviousHistoryMessage();
    void showNextHistoryMessage();
    void showPermissionPrompt(const QString &description, int requestId = -1, const QJsonArray &options = QJsonArray());
    void hidePermissionPrompt();
    QString buildConversationPrompt(const QString &message) const;
    QString buildAcpPrompt(const QString &message) const;
    void processQueuedMessage();
    void resetAcpState();

    QTextEdit *m_chatLog = nullptr;
    ChatInputEdit *m_input = nullptr;
    QPushButton *m_sendButton = nullptr;
    QLabel *m_typingIndicator = nullptr;
    QWidget *m_permissionBar = nullptr;
    QLabel *m_permissionLabel = nullptr;
    QHBoxLayout *m_permissionLayout = nullptr;
    QTimer *m_typingTimer = nullptr;
    QProcess *m_process = nullptr;
    HarmonicAcp *m_acp = nullptr;
    QString m_context;
    QString m_workingDirectory;
    QString m_streamBuffer;
    QString m_pendingMessage;
    QString m_pendingAcpPrompt;
    QString m_acpSessionCwd;
    QStringList m_inputHistory;
    int m_historyPosition = 0;
    int m_typingDots = 0;
    bool m_isStreaming = false;
    bool m_waitingForFirstChunk = false;
    bool m_cancelRequested = false;
    bool m_acpInitialized = false;
    bool m_acpSessionReady = false;

    struct Message {
        QString role;
        QString content;
        QString renderedHtml;
    };
    QList<Message> m_conversation;
};

#endif // HARMONICCHATWIDGET_H
