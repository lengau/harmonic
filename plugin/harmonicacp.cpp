#include "harmonicacp.h"

#include <KLocalizedString>
#include <KShell>

#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QTimer>

HarmonicAcp::HarmonicAcp(QObject *parent) : QObject(parent) {}

HarmonicAcp::~HarmonicAcp() { stop(); }

void HarmonicAcp::start(const QString &command, const QString &workingDir) {
  if (m_process) {
    if (m_process->state() != QProcess::NotRunning) {
      Q_EMIT errorOccurred(QStringLiteral("ACP server is still shutting down"));
      return;
    }
    m_process->deleteLater();
    m_process = nullptr;
  }

  m_process = new QProcess(this);
  m_process->setWorkingDirectory(workingDir);
  connect(m_process, &QProcess::started, this, &HarmonicAcp::onProcessStarted);
  connect(m_process, &QProcess::readyReadStandardOutput, this,
          &HarmonicAcp::onReadyRead);
  connect(m_process, &QProcess::finished, this,
          &HarmonicAcp::onProcessFinished);
  connect(m_process, &QProcess::errorOccurred, this,
          &HarmonicAcp::onProcessError);

  const QStringList commandParts =
      KShell::splitArgs(command, KShell::TildeExpand);
  if (commandParts.isEmpty()) {
    Q_EMIT errorOccurred(i18n("Invalid ACP command configured"));
    return;
  }

  QStringList args = commandParts.mid(1);
  args << QStringLiteral("--acp");
  m_process->start(commandParts.first(), args);
}

void HarmonicAcp::stop() {
  m_sessionId.clear();
  m_nextId = 1;
  m_readBuffer.clear();

  if (!m_process) {
    return;
  }

  QProcess *process = m_process;
  m_process = nullptr;

  if (process->state() == QProcess::NotRunning) {
    process->deleteLater();
    Q_EMIT processFinished();
    return;
  }

  connect(process, &QProcess::finished, process, &QObject::deleteLater);
  connect(process, &QProcess::finished, this,
          [this](int, QProcess::ExitStatus) {
            if (m_process) {
              return;
            }
            Q_EMIT processFinished();
          });

  process->closeWriteChannel();
  QPointer<QProcess> processGuard = process;
  QTimer::singleShot(2000, process, [processGuard]() {
    if (!processGuard || processGuard->state() == QProcess::NotRunning) {
      return;
    }
    processGuard->kill();
  });
}

bool HarmonicAcp::isRunning() const {
  return m_process && m_process->state() == QProcess::Running;
}

void HarmonicAcp::createSession(const QString &cwd) {
  QJsonObject params;
  params[QStringLiteral("cwd")] = cwd;
  sendRequest(QStringLiteral("session/new"), params);
}

void HarmonicAcp::sendPrompt(const QString &text) {
  if (m_sessionId.isEmpty()) {
    Q_EMIT errorOccurred(QStringLiteral("No active session"));
    return;
  }

  QJsonObject params;
  params[QStringLiteral("sessionId")] = m_sessionId;

  QJsonArray prompt;
  QJsonObject textPart;
  textPart[QStringLiteral("type")] = QStringLiteral("text");
  textPart[QStringLiteral("text")] = text;
  prompt.append(textPart);
  params[QStringLiteral("prompt")] = prompt;

  sendRequest(QStringLiteral("session/prompt"), params);
}

void HarmonicAcp::cancelPrompt() {
  if (m_sessionId.isEmpty())
    return;

  QJsonObject params;
  params[QStringLiteral("sessionId")] = m_sessionId;
  sendNotification(QStringLiteral("session/cancel"), params);
}

void HarmonicAcp::respondToPermission(const QJsonValue &requestId,
                                      const QString &optionId) {
  QJsonObject result;
  QJsonObject outcome;
  outcome[QStringLiteral("outcome")] = QStringLiteral("selected");
  outcome[QStringLiteral("optionId")] = optionId;
  result[QStringLiteral("outcome")] = outcome;
  sendResponse(requestId, result);
}

void HarmonicAcp::denyPermission(const QJsonValue &requestId) {
  QJsonObject result;
  QJsonObject outcome;
  outcome[QStringLiteral("outcome")] = QStringLiteral("cancelled");
  result[QStringLiteral("outcome")] = outcome;
  sendResponse(requestId, result);
}

void HarmonicAcp::sendRequest(const QString &method,
                              const QJsonObject &params) {
  if (!m_process || m_process->state() != QProcess::Running)
    return;

  QJsonObject msg;
  msg[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
  msg[QStringLiteral("id")] = m_nextId++;
  msg[QStringLiteral("method")] = method;
  msg[QStringLiteral("params")] = params;

  QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact) + '\n';
  m_process->write(data);
}

void HarmonicAcp::sendNotification(const QString &method,
                                   const QJsonObject &params) {
  if (!m_process || m_process->state() != QProcess::Running)
    return;

  QJsonObject msg;
  msg[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
  msg[QStringLiteral("method")] = method;
  msg[QStringLiteral("params")] = params;

  QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact) + '\n';
  m_process->write(data);
}

void HarmonicAcp::sendResponse(const QJsonValue &id,
                               const QJsonObject &result) {
  if (!m_process || m_process->state() != QProcess::Running || id.isUndefined())
    return;

  QJsonObject msg;
  msg[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
  msg[QStringLiteral("id")] = id;
  msg[QStringLiteral("result")] = result;

  QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact) + '\n';
  m_process->write(data);
}

void HarmonicAcp::onProcessStarted() {
  if (sender() != m_process || !m_process) {
    return;
  }

  QJsonObject params;
  params[QStringLiteral("protocolVersion")] = 1;
  QJsonObject clientInfo;
  clientInfo[QStringLiteral("name")] = QStringLiteral("harmonic");
  clientInfo[QStringLiteral("title")] = QStringLiteral("Harmonic Kate Plugin");
  clientInfo[QStringLiteral("version")] = QStringLiteral("0.1.0");
  params[QStringLiteral("clientInfo")] = clientInfo;
  params[QStringLiteral("clientCapabilities")] = QJsonObject();
  sendRequest(QStringLiteral("initialize"), params);
}

void HarmonicAcp::onReadyRead() {
  auto *process = qobject_cast<QProcess *>(sender());
  if (!process || process != m_process)
    return;

  m_readBuffer += process->readAllStandardOutput();

  // Process complete lines (NDJSON)
  while (true) {
    int newlineIdx = m_readBuffer.indexOf('\n');
    if (newlineIdx < 0)
      break;

    QByteArray line = m_readBuffer.left(newlineIdx).trimmed();
    m_readBuffer = m_readBuffer.mid(newlineIdx + 1);

    if (line.isEmpty())
      continue;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
      continue;

    processMessage(doc.object());
  }
}

void HarmonicAcp::processMessage(const QJsonObject &msg) {
  // Check if it's a response (has "id" and "result" or "error")
  if (msg.contains(QStringLiteral("result")) &&
      msg.contains(QStringLiteral("id"))) {
    QJsonObject result = msg[QStringLiteral("result")].toObject();

    // Check if it's an initialize response
    if (result.contains(QStringLiteral("agentInfo"))) {
      Q_EMIT initialized(result[QStringLiteral("agentInfo")].toObject());
      return;
    }

    // Check if it's a session/new response
    if (result.contains(QStringLiteral("sessionId"))) {
      m_sessionId = result[QStringLiteral("sessionId")].toString();
      Q_EMIT sessionCreated(m_sessionId);
      return;
    }

    // Check if it's a session/prompt completion
    if (result.contains(QStringLiteral("stopReason"))) {
      Q_EMIT promptFinished(result[QStringLiteral("stopReason")].toString());
      return;
    }
    return;
  }

  // Check if it's an error response
  if (msg.contains(QStringLiteral("error")) &&
      msg.contains(QStringLiteral("id"))) {
    QJsonObject error = msg[QStringLiteral("error")].toObject();
    Q_EMIT errorOccurred(error[QStringLiteral("message")].toString());
    return;
  }

  // Check if it's a notification or request from the agent
  QString method = msg[QStringLiteral("method")].toString();
  QJsonObject params = msg[QStringLiteral("params")].toObject();

  if (method == QStringLiteral("session/update")) {
    handleSessionUpdate(params);
  } else if (method == QStringLiteral("session/request_permission")) {
    const QJsonValue requestId = msg.value(QStringLiteral("id"));
    QJsonObject toolCall = params[QStringLiteral("toolCall")].toObject();
    QString title = toolCall[QStringLiteral("title")].toString();
    QJsonArray options = params[QStringLiteral("options")].toArray();
    Q_EMIT permissionRequested(requestId, title, options);
  }
}

void HarmonicAcp::handleSessionUpdate(const QJsonObject &params) {
  QJsonObject update = params[QStringLiteral("update")].toObject();
  QString updateType = update[QStringLiteral("sessionUpdate")].toString();

  if (updateType == QStringLiteral("agent_message_chunk")) {
    QJsonObject content = update[QStringLiteral("content")].toObject();
    QString text = content[QStringLiteral("text")].toString();
    Q_EMIT textChunk(text);
  } else if (updateType == QStringLiteral("agent_thought_chunk")) {
    QJsonObject content = update[QStringLiteral("content")].toObject();
    QString text = content[QStringLiteral("text")].toString();
    Q_EMIT thoughtChunk(text);
  } else if (updateType == QStringLiteral("tool_call")) {
    QString toolCallId = update[QStringLiteral("toolCallId")].toString();
    QString title = update[QStringLiteral("title")].toString();
    QString kind = update[QStringLiteral("kind")].toString();
    Q_EMIT toolCall(toolCallId, title, kind);
  } else if (updateType == QStringLiteral("tool_call_update")) {
    QString toolCallId = update[QStringLiteral("toolCallId")].toString();
    QString status = update[QStringLiteral("status")].toString();
    QJsonObject content = update[QStringLiteral("content")].toObject();
    QString text = content[QStringLiteral("text")].toString();
    Q_EMIT toolCallUpdate(toolCallId, status, text);
  }
}

void HarmonicAcp::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
  Q_UNUSED(exitCode);
  Q_UNUSED(status);

  auto *process = qobject_cast<QProcess *>(sender());
  if (!process || process != m_process) {
    return;
  }

  m_process = nullptr;
  m_sessionId.clear();
  m_nextId = 1;
  m_readBuffer.clear();
  process->deleteLater();
  Q_EMIT processFinished();
}

void HarmonicAcp::onProcessError(QProcess::ProcessError error) {
  auto *process = qobject_cast<QProcess *>(sender());
  if (!process || process != m_process) {
    return;
  }

  if (error == QProcess::FailedToStart) {
    const QString errorString = process->errorString();
    m_process = nullptr;
    m_sessionId.clear();
    m_nextId = 1;
    m_readBuffer.clear();
    process->deleteLater();
    Q_EMIT errorOccurred(
        QStringLiteral("Failed to start ACP server: %1").arg(errorString));
    return;
  }

  Q_EMIT errorOccurred(
      QStringLiteral("ACP process error: %1").arg(process->errorString()));
}
