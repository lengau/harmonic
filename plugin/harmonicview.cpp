#include "harmonicplugin.h"
#include "harmonicchatwidget.h"
#include "harmonic.h"

#include <KTextEditor/View>
#include <KTextEditor/Document>
#include <KLocalizedString>
#include <KXMLGUIFactory>
#include <KActionCollection>
#include <KSharedConfig>
#include <KConfigGroup>

#include <QAction>
#include <QIcon>
#include <QInputDialog>
#include <QMainWindow>
#include <QMessageBox>
#include <QStatusBar>
#include <QtConcurrent/QtConcurrentRun>

HarmonicView::HarmonicView(HarmonicPlugin *plugin, KTextEditor::MainWindow *mainWindow)
    : QObject(mainWindow)
    , KXMLGUIClient()
    , m_mainWindow(mainWindow)
    , m_plugin(plugin)
{
    setComponentName(QStringLiteral("harmonicplugin"), i18n("Harmonic"));
    setXMLFile(QStringLiteral("harmonicplugin.rc"));

    // Create vibecode action with shortcut
    m_vibecodeAction = actionCollection()->addAction(QStringLiteral("harmonic_vibecode"));
    m_vibecodeAction->setText(i18n("Harmonic: Vibecode"));
    m_vibecodeAction->setToolTip(i18n("Generate code from a natural language prompt"));
    actionCollection()->setDefaultShortcut(m_vibecodeAction, QKeySequence(QStringLiteral("Ctrl+Shift+V")));
    connect(m_vibecodeAction, &QAction::triggered, this, &HarmonicView::vibecode);

    m_vibecodeWatcher = new QFutureWatcher<QString>(this);
    connect(m_vibecodeWatcher, &QFutureWatcher<QString>::finished, this, &HarmonicView::handleVibecodeFinished);

    mainWindow->guiFactory()->addClient(this);

    // Create the chat sidebar toolview
    m_toolView = mainWindow->createToolView(
        plugin,
        QStringLiteral("harmonic_chat"),
        KTextEditor::MainWindow::Right,
        QIcon::fromTheme(QStringLiteral("dialog-messages")),
        i18n("Harmonic Chat")
    );

    m_chatWidget = new HarmonicChatWidget(m_toolView);

    // Update context when the active view changes
    connect(mainWindow, &KTextEditor::MainWindow::viewChanged, this, &HarmonicView::updateChatContext);
}

HarmonicView::~HarmonicView()
{
    m_mainWindow->guiFactory()->removeClient(this);
    delete m_toolView;
}

void HarmonicView::updateChatContext()
{
    auto *view = m_mainWindow->activeView();
    if (view && view->document()) {
        m_chatWidget->setContext(view->document()->text());
    }
}

void HarmonicView::showStatusMessage(const QString &message, int timeoutMs) const
{
    if (auto *window = qobject_cast<QMainWindow *>(m_mainWindow->window())) {
        window->statusBar()->showMessage(message, timeoutMs);
    }
}

void HarmonicView::vibecode()
{
    if (m_vibecodeWatcher->isRunning()) {
        showStatusMessage(i18n("Harmonic: Generation already in progress"), 3000);
        return;
    }

    auto *view = m_mainWindow->activeView();
    if (!view) {
        QMessageBox::warning(m_mainWindow->window(),
            i18n("Harmonic"),
            i18n("Please open a file first."));
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
            &ok
        );
        if (!ok || prompt.isEmpty()) {
            return;
        }
    }

    // Capture document state and config on the UI thread before launching work.
    KTextEditor::Cursor insertPos = view->cursorPosition();
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
    m_pendingCursor = insertPos;
    m_vibecodeAction->setEnabled(false);
    showStatusMessage(i18n("Harmonic: Generating..."));

    m_vibecodeWatcher->setFuture(QtConcurrent::run([
        promptBytes,
        contextBytes,
        backendBytes,
        commandBytes,
        modelBytes,
        apiKeyBytes,
        sendContext
    ]() -> QString {
        const char *context = contextBytes.isEmpty() ? nullptr : contextBytes.constData();
        char *result = harmonic_generate(
            promptBytes.constData(),
            context,
            backendBytes.constData(),
            commandBytes.constData(),
            modelBytes.isEmpty() ? nullptr : modelBytes.constData(),
            apiKeyBytes.isEmpty() ? nullptr : apiKeyBytes.constData(),
            sendContext
        );
        if (!result) {
            return QStringLiteral("// Error: Harmonic generation failed.");
        }

        const QString generated = QString::fromUtf8(result);
        harmonic_free_string(result);
        return generated;
    }));
}

void HarmonicView::handleVibecodeFinished()
{
    m_vibecodeAction->setEnabled(true);

    const QString generated = m_vibecodeWatcher->result();
    QWidget *parentWidget = m_mainWindow->window();

    if (generated.startsWith(QStringLiteral("// Error:"))) {
        showStatusMessage(i18n("Harmonic: Generation failed"), 5000);
        QMessageBox::warning(parentWidget, i18n("Harmonic"), generated);
    } else if (!m_pendingDocument) {
        showStatusMessage(i18n("Harmonic: Generation completed, but the document was closed."), 5000);
    } else {
        m_pendingDocument->insertText(m_pendingCursor, generated);
        showStatusMessage(i18n("Harmonic: Generation complete"), 3000);
    }

    m_pendingDocument.clear();
}
