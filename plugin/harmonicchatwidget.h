#ifndef HARMONICCHATWIDGET_H
#define HARMONICCHATWIDGET_H

#include <QWidget>
#include <QProcess>

class QTextEdit;
class QLineEdit;
class QPushButton;

class HarmonicChatWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HarmonicChatWidget(QWidget *parent = nullptr);
    ~HarmonicChatWidget() override;

    void setContext(const QString &context);

private Q_SLOTS:
    void sendMessage();
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
    void clearSession();
    void approvePermission();
    void denyPermission();

private:
    void appendMessage(const QString &sender, const QString &text);
    void startStreaming();
    void finishStreaming();
    void showPermissionPrompt(const QString &description);
    void hidePermissionPrompt();
    QString buildConversationPrompt(const QString &message);

    QTextEdit *m_chatLog;
    QLineEdit *m_input;
    QPushButton *m_sendButton;
    QWidget *m_permissionBar;
    QProcess *m_process;
    QString m_context;
    QString m_streamBuffer;
    QString m_pendingMessage;
    bool m_isStreaming;

    struct Message {
        QString role; // "user" or "assistant"
        QString content;
    };
    QList<Message> m_conversation;
};

#endif // HARMONICCHATWIDGET_H
