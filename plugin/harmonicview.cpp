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
#include <QInputDialog>
#include <QMessageBox>
#include <QIcon>

HarmonicView::HarmonicView(HarmonicPlugin *plugin, KTextEditor::MainWindow *mainWindow)
    : QObject(mainWindow)
    , KXMLGUIClient()
    , m_mainWindow(mainWindow)
    , m_plugin(plugin)
{
    setComponentName(QStringLiteral("harmonicplugin"), i18n("Harmonic"));
    setXMLFile(QStringLiteral("harmonicplugin.rc"));

    // Create vibecode action with shortcut
    auto *action = actionCollection()->addAction(QStringLiteral("harmonic_vibecode"));
    action->setText(i18n("Harmonic: Vibecode"));
    action->setToolTip(i18n("Generate code from a natural language prompt"));
    actionCollection()->setDefaultShortcut(action, QKeySequence(QStringLiteral("Ctrl+Shift+V")));
    connect(action, &QAction::triggered, this, &HarmonicView::vibecode);

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

void HarmonicView::vibecode()
{
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

    // Gather surrounding code as context
    const char *context = nullptr;
    QByteArray contextBytes;
    QString docText = doc->text();
    if (!docText.isEmpty()) {
        contextBytes = docText.toUtf8();
        context = contextBytes.constData();
    }

    // Call into the Rust core library with config
    KSharedConfig::Ptr config = KSharedConfig::openConfig();
    KConfigGroup group = config->group(QStringLiteral("Harmonic"));

    QByteArray promptBytes = prompt.toUtf8();
    QByteArray backendBytes = group.readEntry("Backend", "copilot").toUtf8();
    QByteArray commandBytes = group.readEntry("Command", "copilot").toUtf8();
    QByteArray modelBytes = group.readEntry("Model", "").toUtf8();
    QByteArray apiKeyBytes = group.readEntry("ApiKey", "").toUtf8();
    bool sendContext = group.readEntry("SendContext", true);

    char *result = harmonic_generate(
        promptBytes.constData(),
        context,
        backendBytes.constData(),
        commandBytes.constData(),
        modelBytes.isEmpty() ? nullptr : modelBytes.constData(),
        apiKeyBytes.isEmpty() ? nullptr : apiKeyBytes.constData(),
        sendContext
    );
    if (result) {
        QString generated = QString::fromUtf8(result);
        harmonic_free_string(result);

        if (generated.startsWith(QStringLiteral("// Error:"))) {
            QMessageBox::warning(parentWidget,
                i18n("Harmonic"),
                generated);
        } else {
            // Insert on the line below the current cursor
            int line = view->cursorPosition().line();
            KTextEditor::Cursor insertPos(line + 1, 0);
            if (line + 1 >= doc->lines()) {
                doc->insertText(KTextEditor::Cursor(line, doc->lineLength(line)), QStringLiteral("\n"));
            }
            doc->insertText(insertPos, generated + QStringLiteral("\n"));
        }
    }
}
