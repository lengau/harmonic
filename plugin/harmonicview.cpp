#include "harmonicplugin.h"
#include "harmonicchatwidget.h"
#include "harmonic.h"

#include <KTextEditor/View>
#include <KTextEditor/Document>
#include <KTextEditor/Message>
#include <KLocalizedString>
#include <KXMLGUIFactory>
#include <KActionCollection>
#include <KSharedConfig>
#include <KConfigGroup>

#include <QAction>
#include <functional>
#include <QFileInfo>
#include <QIcon>
#include <QInputDialog>
#include <QGuiApplication>
#include <QMainWindow>
#include <QStatusBar>
#include <QClipboard>
#include <QtConcurrent/QtConcurrentRun>

namespace {
void postEditorMessage(KTextEditor::MainWindow *mainWindow,
                       const QString &text,
                       KTextEditor::Message::MessageType type,
                       int autoHideMs,
                       const QString &actionText = QString(),
                       const std::function<void()> &actionHandler = {}) {
    if (!mainWindow) {
        return;
    }

    auto *view = mainWindow->activeView();
    if (!view || !view->document()) {
        return;
    }

    auto *message = new KTextEditor::Message(text, type);
    message->setPosition(KTextEditor::Message::TopInView);
    message->setWordWrap(true);
    message->setView(view);
    message->setAutoHide(autoHideMs);

    if (!actionText.isEmpty() && actionHandler) {
        auto *action = new QAction(actionText, message);
        QObject::connect(action, &QAction::triggered, message, actionHandler);
        message->addAction(action, false);
    }

    view->document()->postMessage(message);
}
} // namespace

HarmonicView::HarmonicView(HarmonicPlugin *plugin, KTextEditor::MainWindow *mainWindow)
    : QObject(mainWindow), KXMLGUIClient(), m_mainWindow(mainWindow), m_plugin(plugin) {
    setComponentName(QStringLiteral("harmonicplugin"), i18n("Harmonic"));
    setXMLFile(QStringLiteral("harmonicplugin.rc"));

    // Create vibecode action with shortcut
    m_vibecodeAction = actionCollection()->addAction(QStringLiteral("harmonic_vibecode"));
    m_vibecodeAction->setText(i18n("Harmonic: Vibecode"));
    m_vibecodeAction->setToolTip(i18n("Generate code from a natural language prompt"));
    actionCollection()->setDefaultShortcut(m_vibecodeAction, QKeySequence(QStringLiteral("Ctrl+Shift+V")));
    connect(m_vibecodeAction, &QAction::triggered, this, &HarmonicView::vibecode);

    m_vibecodeWatcher = new QFutureWatcher<VibecodeResult>(this);
    connect(m_vibecodeWatcher, &QFutureWatcher<VibecodeResult>::finished, this, &HarmonicView::handleVibecodeFinished);

    mainWindow->guiFactory()->addClient(this);

    // Create the chat sidebar toolview
    m_toolView = mainWindow->createToolView(
        plugin,
        QStringLiteral("harmonic_chat"),
        KTextEditor::MainWindow::Right,
        QIcon::fromTheme(QStringLiteral("dialog-messages")),
        i18n("Harmonic Chat"));

    m_chatWidget = new HarmonicChatWidget(m_toolView);

    // Update context when the active view changes
    connect(mainWindow, &KTextEditor::MainWindow::viewChanged, this, &HarmonicView::updateChatContext);
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
    auto *view = m_mainWindow->activeView();
    if (!view || !view->document()) {
        m_chatWidget->setContext(QString());
        m_chatWidget->setWorkingDirectory(QString());
        return;
    }

    auto *document = view->document();
    m_chatWidget->setContext(document->text());

    QString workingDirectory;
    const QUrl url = document->url();
    if (url.isLocalFile()) {
        workingDirectory = QFileInfo(url.toLocalFile()).absolutePath();
    }
    m_chatWidget->setWorkingDirectory(workingDirectory);
}

void HarmonicView::showStatusMessage(const QString &message, int timeoutMs) const {
    if (auto *window = qobject_cast<QMainWindow *>(m_mainWindow->window())) {
        window->statusBar()->showMessage(message, timeoutMs);
    }
}

void HarmonicView::vibecode() {
    if (m_vibecodeWatcher->isRunning()) {
        const QString message = i18n("Harmonic: Generation already in progress");
        showStatusMessage(message, 3000);
        postEditorMessage(m_mainWindow, message, KTextEditor::Message::Warning, 3000);
        return;
    }

    auto *view = m_mainWindow->activeView();
    if (!view) {
        showStatusMessage(i18n("Harmonic: Please open a file first."), 5000);
        return;
    }

    QWidget *parentWidget = m_mainWindow->window();
    if (!parentWidget) {
        parentWidget = view->focusProxy();
    }

    auto *doc = view->document();
    QString prompt;

    // If there's a selection, use that as the prompt
    if (view->selection()) {
        prompt = view->selectionText().trimmed();
    } else {
        // Otherwise use the current line (assumed to be a comment)
        int line = view->cursorPosition().line();
        prompt = doc->line(line).trimmed();
        // Strip common comment prefixes
        for (const auto &prefix : {QStringLiteral("//"), QStringLiteral("#"),
                                   QStringLiteral("/*"), QStringLiteral("*/"),
                                   QStringLiteral("*"), QStringLiteral("--")}) {
            if (prompt.startsWith(prefix)) {
                prompt = prompt.mid(prefix.length()).trimmed();
            }
        }
    }

    // If we couldn't extract a prompt, ask the user
    if (prompt.isEmpty()) {
        bool ok = false;
        prompt = QInputDialog::getText(
            parentWidget,
            i18n("Harmonic – Vibecode"),
            i18n("Describe what you want to generate:"),
            QLineEdit::Normal,
            QString(),
            &ok);
        if (!ok || prompt.isEmpty()) {
            return;
        }
    }

    // Capture document state and config on the UI thread before launching work.
    const int insertLine = view->cursorPosition().line();
    KSharedConfig::Ptr config = KSharedConfig::openConfig();
    KConfigGroup group = config->group(QStringLiteral("Harmonic"));
    bool sendContext = group.readEntry("SendContext", true);
    QString docText = sendContext ? doc->text() : QString();

    QByteArray promptBytes = prompt.toUtf8();
    QByteArray contextBytes = docText.toUtf8();
    QByteArray backendBytes = group.readEntry("Backend", "copilot").toUtf8();
    QByteArray commandBytes = group.readEntry("Command", "copilot").toUtf8();
    QByteArray modelBytes = group.readEntry("Model", "").toUtf8();
    QByteArray apiKeyBytes = group.readEntry("ApiKey", "").toUtf8();

    m_pendingDocument = doc;
    m_pendingInsertLine = insertLine;
    m_vibecodeAction->setEnabled(false);
    const QString generatingMessage = i18n("Harmonic: Generating...");
    showStatusMessage(generatingMessage);
    postEditorMessage(m_mainWindow, generatingMessage, KTextEditor::Message::Information, 2000);

    m_vibecodeWatcher->setFuture(QtConcurrent::run([promptBytes,
                                                    contextBytes,
                                                    backendBytes,
                                                    commandBytes,
                                                    modelBytes,
                                                    apiKeyBytes,
                                                    sendContext]() -> VibecodeResult {
        // QByteArrays captured by value — safe because harmonic_generate_result() is synchronous
        // and completes before the lambda returns, keeping the buffers alive.
        const char *context = contextBytes.isEmpty() ? nullptr : contextBytes.constData();
        HarmonicResult result = harmonic_generate_result(
            promptBytes.constData(),
            context,
            backendBytes.constData(),
            commandBytes.constData(),
            modelBytes.isEmpty() ? nullptr : modelBytes.constData(),
            apiKeyBytes.isEmpty() ? nullptr : apiKeyBytes.constData(),
            sendContext);

        VibecodeResult vibecodeResult;
        if (result.status == HARMONIC_STATUS_OK) {
            vibecodeResult.success = true;
            if (result.output) {
                vibecodeResult.text = QString::fromUtf8(result.output);
            }
        } else {
            vibecodeResult.success = false;
            if (result.error) {
                vibecodeResult.text = QString::fromUtf8(result.error);
            } else {
                vibecodeResult.text = i18n("Harmonic generation failed.");
            }
        }
        harmonic_free_result(result);
        return vibecodeResult;
    }));
}

void HarmonicView::handleVibecodeFinished() {
    m_vibecodeAction->setEnabled(true);

    const VibecodeResult generation = m_vibecodeWatcher->result();

    if (!generation.success) {
        const QString errorMessage = i18n("Harmonic: Generation failed");
        showStatusMessage(errorMessage, 5000);
        postEditorMessage(m_mainWindow,
                          errorMessage,
                          KTextEditor::Message::Error,
                          7000,
                          i18n("Copy details"),
                          [details = generation.text]() {
                              if (auto *clipboard = QGuiApplication::clipboard()) {
                                  clipboard->setText(details);
                              }
                          });
    } else if (!m_pendingDocument) {
        const QString message = i18n("Harmonic: Generation completed, but the document was closed.");
        showStatusMessage(message, 5000);
        postEditorMessage(m_mainWindow, message, KTextEditor::Message::Warning, 5000);
    } else {
        const int currentLine = qMax(0, qMin(m_pendingInsertLine, m_pendingDocument->lines() - 1));
        const int insertLine = currentLine + 1;
        if (insertLine >= m_pendingDocument->lines()) {
            m_pendingDocument->insertText(
                KTextEditor::Cursor(currentLine, m_pendingDocument->lineLength(currentLine)),
                QStringLiteral("\n"));
        }
        m_pendingDocument->insertText(KTextEditor::Cursor(insertLine, 0), generation.text + QStringLiteral("\n"));
        const QString message = i18n("Harmonic: Generation complete");
        showStatusMessage(message, 3000);
        postEditorMessage(m_mainWindow, message, KTextEditor::Message::Positive, 3000);
    }

    m_pendingDocument.clear();
    m_pendingInsertLine = -1;
}
