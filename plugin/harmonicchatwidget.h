#ifndef HARMONICCHATWIDGET_H
#define HARMONICCHATWIDGET_H

#include <QProcess>
#include <QStringList>
#include <QWidget>

class ChatInputEdit;
class QLabel;
class QPushButton;
class QTextEdit;
class QTimer;

class HarmonicChatWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HarmonicChatWidget(QWidget *parent = nullptr);
    ~HarmonicChatWidget() override;

    void setContext(const QString &context);

private Q_SLOTS:
    void sendMessage();
    void cancelCurrentGeneration();
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
    void clearSession();
    void approvePermission();
    void denyPermission();

private:
    void appendMessage(const QString &role, const QString &text);
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
    void showPermissionPrompt(const QString &description);
    void hidePermissionPrompt();
    QString buildConversationPrompt(const QString &message);

    QTextEdit *m_chatLog;
    ChatInputEdit *m_input;
    QPushButton *m_sendButton;
    QLabel *m_typingIndicator;
    QWidget *m_permissionBar;
    QLabel *m_permissionLabel;
    QTimer *m_typingTimer;
    QProcess *m_process;
    QString m_context;
    QString m_streamBuffer;
    QString m_pendingMessage;
    QStringList m_inputHistory;
    int m_historyPosition;
    int m_typingDots;
    bool m_isStreaming;
    bool m_waitingForFirstChunk;
    bool m_cancelRequested;

    struct Message {
        QString role;
        QString content;
    };
    QList<Message> m_conversation;
};

#endif // HARMONICCHATWIDGET_H
