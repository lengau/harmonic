#include "harmonic.h"
#include "harmonicchatwidget.h"
#include "harmonicplugin.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KTextEditor/Document>
#include <KTextEditor/View>
#include <KXMLGUIFactory>

#include <keychain.h>

#include <QAction>
#include <QFileInfo>
#include <QIcon>
#include <QInputDialog>
#include <QMainWindow>
#include <QMessageBox>
#include <QPointer>
#include <QStatusBar>
#include <QtConcurrent/QtConcurrentRun>

HarmonicView::HarmonicView(HarmonicPlugin* plugin,
                           KTextEditor::MainWindow* mainWindow)
    : QObject(mainWindow),
      KXMLGUIClient(),
      m_mainWindow(mainWindow),
      m_plugin(plugin) {
  setComponentName(QStringLiteral("harmonicplugin"), i18n("Harmonic"));
  setXMLFile(QStringLiteral("harmonicplugin.rc"));

  // Create vibecode action with shortcut
  m_vibecodeAction =
      actionCollection()->addAction(QStringLiteral("harmonic_vibecode"));
  m_vibecodeAction->setText(i18n("Harmonic: Vibecode"));
  m_vibecodeAction->setToolTip(
      i18n("Generate code from a natural language prompt"));
  actionCollection()->setDefaultShortcut(
      m_vibecodeAction, QKeySequence(QStringLiteral("Ctrl+Shift+V")));
  connect(m_vibecodeAction, &QAction::triggered, this, &HarmonicView::vibecode);

  m_vibecodeWatcher = new QFutureWatcher<QString>(this);
  connect(m_vibecodeWatcher, &QFutureWatcher<QString>::finished, this,
          &HarmonicView::handleVibecodeFinished);

  mainWindow->guiFactory()->addClient(this);

  // Create the chat sidebar toolview
  m_toolView = mainWindow->createToolView(
      plugin, QStringLiteral("harmonic_chat"), KTextEditor::MainWindow::Right,
      QIcon::fromTheme(QStringLiteral("dialog-messages")),
      i18n("Harmonic Chat"));

  m_chatWidget = new HarmonicChatWidget(m_toolView);

  // Update context when the active view changes
  connect(mainWindow, &KTextEditor::MainWindow::viewChanged, this,
          &HarmonicView::updateChatContext);
  updateChatContext();
}

HarmonicView::~HarmonicView() {
  if (m_vibecodeWatcher && m_vibecodeWatcher->isRunning()) {
    // Note: cancel() doesn't interrupt the blocking subprocess.
    // We disconnect signals so the handler won't fire on a destroyed object,
    // then wait for the subprocess to complete naturally.
    disconnect(m_vibecodeWatcher, nullptr, this, nullptr);
    m_vibecodeWatcher->cancel();
    m_vibecodeWatcher->waitForFinished();
  }

  m_mainWindow->guiFactory()->removeClient(this);
  delete m_toolView;
}

void HarmonicView::updateChatContext() {
  auto* view = m_mainWindow->activeView();
  if (!view || !view->document()) {
    m_chatWidget->setContext(QString());
    m_chatWidget->setWorkingDirectory(QString());
    return;
  }

  auto* document = view->document();
  m_chatWidget->setContext(document->text());

  QString workingDirectory;
  const QUrl url = document->url();
  if (url.isLocalFile()) {
    workingDirectory = QFileInfo(url.toLocalFile()).absolutePath();
  }
  m_chatWidget->setWorkingDirectory(workingDirectory);
}

void HarmonicView::showStatusMessage(const QString& message,
                                     int timeoutMs) const {
  if (auto* window = qobject_cast<QMainWindow*>(m_mainWindow->window())) {
    window->statusBar()->showMessage(message, timeoutMs);
  }
}

void HarmonicView::vibecode() {
  if (m_vibecodeWatcher->isRunning()) {
    showStatusMessage(i18n("Harmonic: Generation already in progress"), 3000);
    return;
  }

  auto* view = m_mainWindow->activeView();
  if (!view) {
    QMessageBox::warning(m_mainWindow->window(), i18n("Harmonic"),
                         i18n("Please open a file first."));
    return;
  }

  QWidget* parentWidget = m_mainWindow->window();
  if (!parentWidget) {
    parentWidget = view->focusProxy();
  }

  auto* doc = view->document();
  QString prompt;

  // If there's a selection, use that as the prompt
  if (view->selection()) {
    prompt = view->selectionText().trimmed();
  } else {
    // Otherwise use the current line (assumed to be a comment)
    int line = view->cursorPosition().line();
    prompt = doc->line(line).trimmed();
    // Strip common comment prefixes
    for (const auto& prefix :
         {QStringLiteral("//"), QStringLiteral("#"), QStringLiteral("/*"),
          QStringLiteral("*/"), QStringLiteral("*"), QStringLiteral("--")}) {
      if (prompt.startsWith(prefix)) {
        prompt = prompt.mid(prefix.length()).trimmed();
      }
    }
  }

  // If we couldn't extract a prompt, ask the user
  if (prompt.isEmpty()) {
    bool ok = false;
    prompt = QInputDialog::getText(parentWidget, i18n("Harmonic – Vibecode"),
                                   i18n("Describe what you want to generate:"),
                                   QLineEdit::Normal, QString(), &ok);
    if (!ok || prompt.isEmpty()) {
      return;
    }
  }

  // Store the prompt and prepare for async API key read
  m_vibecodePrompt = prompt;
  m_pendingDocument = doc;
  m_pendingInsertLine = view->cursorPosition().line();
  m_vibecodeAction->setEnabled(false);
  showStatusMessage(i18n("Harmonic: Reading credentials..."));

  // Read API key asynchronously from Secret Service
  if (m_vibecodeApiKeyJob) {
    disconnect(m_vibecodeApiKeyJob, nullptr, this, nullptr);
    m_vibecodeApiKeyJob->deleteLater();
  }
  m_vibecodeApiKeyJob =
      new QKeychain::ReadPasswordJob(QStringLiteral("Harmonic"), this);
  m_vibecodeApiKeyJob->setKey(QStringLiteral("ApiKey"));
  m_vibecodeApiKeyJob->setAutoDelete(false);
  connect(m_vibecodeApiKeyJob, &QKeychain::Job::finished, this,
          &HarmonicView::onVibecodeApiKeyJobFinished);
  m_vibecodeApiKeyJob->start();
}

void HarmonicView::onVibecodeApiKeyJobFinished() {
  if (!m_vibecodeApiKeyJob) {
    return;
  }

  // Get the API key from Secret Service, or fall back to legacy KConfig
  KSharedConfig::Ptr config = KSharedConfig::openConfig();
  KConfigGroup group = config->group(QStringLiteral("Harmonic"));
  if (m_vibecodeApiKeyJob->error() == QKeychain::NoError) {
    m_vibecodeApiKey = m_vibecodeApiKeyJob->textData();
  } else {
    m_vibecodeApiKey = group.readEntry("ApiKey", QString());
    // Migrate legacy plaintext key to QtKeychain on fallback success
    if (!m_vibecodeApiKey.isEmpty()) {
      auto* migrateJob =
          new QKeychain::WritePasswordJob(QStringLiteral("Harmonic"), nullptr);
      migrateJob->setKey(QStringLiteral("ApiKey"));
      migrateJob->setTextData(m_vibecodeApiKey);
      migrateJob->setAutoDelete(true);
      // Use nullptr context so lambda executes even if view is destroyed. This
      // ensures plaintext cleanup happens regardless of widget lifetime.
      auto* migrateJobPtr = migrateJob;
      connect(migrateJob, &QKeychain::Job::finished, nullptr,
              [migrateJobPtr, config]() {
                if (migrateJobPtr->error() == QKeychain::NoError) {
                  auto cfg = config->group(QStringLiteral("Harmonic"));
                  cfg.deleteEntry("ApiKey");
                  config->sync();
                }
              });
      migrateJob->start();
    }
  }

  m_vibecodeApiKeyJob->deleteLater();
  m_vibecodeApiKeyJob = nullptr;

  // Now that we have the API key, start the actual generation
  startVibecodeGeneration();
}

void HarmonicView::startVibecodeGeneration() {
  if (!m_pendingDocument || m_vibecodePrompt.isEmpty()) {
    m_vibecodeAction->setEnabled(true);
    return;
  }

  KTextEditor::Document* doc = m_pendingDocument;
  KSharedConfig::Ptr config = KSharedConfig::openConfig();
  KConfigGroup group = config->group(QStringLiteral("Harmonic"));
  bool sendContext = group.readEntry("SendContext", true);
  QString docText = sendContext ? doc->text() : QString();

  QByteArray promptBytes = m_vibecodePrompt.toUtf8();
  QByteArray contextBytes = docText.toUtf8();
  QByteArray backendBytes = group.readEntry("Backend", "copilot").toUtf8();
  QByteArray commandBytes = group.readEntry("Command", "copilot").toUtf8();
  QByteArray modelBytes = group.readEntry("Model", "").toUtf8();
  QByteArray apiKeyBytes = m_vibecodeApiKey.toUtf8();

  showStatusMessage(i18n("Harmonic: Generating..."));

  m_vibecodeWatcher->setFuture(
      QtConcurrent::run([promptBytes, contextBytes, backendBytes, commandBytes,
                         modelBytes, apiKeyBytes, sendContext]() -> QString {
        // QByteArrays captured by value — safe because harmonic_generate() is
        // synchronous and completes before the lambda returns, keeping the
        // buffers alive.
        const char* context =
            contextBytes.isEmpty() ? nullptr : contextBytes.constData();
        char* result = harmonic_generate(
            promptBytes.constData(), context, backendBytes.constData(),
            commandBytes.constData(),
            modelBytes.isEmpty() ? nullptr : modelBytes.constData(),
            apiKeyBytes.isEmpty() ? nullptr : apiKeyBytes.constData(),
            sendContext);
        if (!result) {
          return QStringLiteral("// Error: Harmonic generation failed.");
        }

        const QString generated = QString::fromUtf8(result);
        harmonic_free_string(result);
        return generated;
      }));
}

void HarmonicView::handleVibecodeFinished() {
  m_vibecodeAction->setEnabled(true);

  const QString generated = m_vibecodeWatcher->result();
  QWidget* parentWidget = m_mainWindow->window();

  if (generated.startsWith(QStringLiteral("// Error:"))) {
    showStatusMessage(i18n("Harmonic: Generation failed"), 5000);
    QMessageBox::warning(parentWidget, i18n("Harmonic"), generated);
  } else if (!m_pendingDocument) {
    showStatusMessage(
        i18n("Harmonic: Generation completed, but the document was closed."),
        5000);
  } else {
    const int currentLine =
        qMax(0, qMin(m_pendingInsertLine, m_pendingDocument->lines() - 1));
    const int insertLine = currentLine + 1;
    if (insertLine >= m_pendingDocument->lines()) {
      m_pendingDocument->insertText(
          KTextEditor::Cursor(currentLine,
                              m_pendingDocument->lineLength(currentLine)),
          QStringLiteral("\n"));
    }
    m_pendingDocument->insertText(KTextEditor::Cursor(insertLine, 0),
                                  generated + QStringLiteral("\n"));
    showStatusMessage(i18n("Harmonic: Generation complete"), 3000);
  }

  m_pendingDocument.clear();
  m_pendingInsertLine = -1;
}
