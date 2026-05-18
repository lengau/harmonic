#ifndef HARMONICACP_H
#define HARMONICACP_H

#include <QObject>
#include <QProcess>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

/// ACP (Agent Client Protocol) client for communicating with copilot --acp.
/// Manages the JSON-RPC session over stdin/stdout.
class HarmonicAcp : public QObject {
    Q_OBJECT

  public:
    explicit HarmonicAcp(QObject *parent = nullptr);
    ~HarmonicAcp() override;

    void start(const QString &command, const QString &workingDir);
    void stop();
    bool isRunning() const;

    void createSession(const QString &cwd);
    void sendPrompt(const QString &text);
    void cancelPrompt();
    void respondToPermission(const QJsonValue &requestId, const QString &optionId);
    void denyPermission(const QJsonValue &requestId);

  Q_SIGNALS:
    void initialized(const QJsonObject &agentInfo);
    void sessionCreated(const QString &sessionId);
    void textChunk(const QString &text);
    void thoughtChunk(const QString &text);
    void toolCall(const QString &toolCallId, const QString &title, const QString &kind);
    void toolCallUpdate(const QString &toolCallId, const QString &status, const QString &content);
    void permissionRequested(const QJsonValue &requestId, const QString &title, const QJsonArray &options);
    void promptFinished(const QString &stopReason);
    void errorOccurred(const QString &message);
    void processFinished();

  private Q_SLOTS:
    void onProcessStarted();
    void onReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);

  private:
    void sendRequest(const QString &method, const QJsonObject &params);
    void sendNotification(const QString &method, const QJsonObject &params);
    void sendResponse(const QJsonValue &id, const QJsonObject &result);
    void processMessage(const QJsonObject &msg);
    void handleSessionUpdate(const QJsonObject &params);

    QProcess *m_process = nullptr;
    QList<QPointer<QProcess>> m_shuttingDownProcesses;
    QString m_sessionId;
    int m_nextId = 1;
    QByteArray m_readBuffer;
};

#endif // HARMONICACP_H
