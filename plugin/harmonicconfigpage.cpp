#include "harmonicconfigpage.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <keychain.h>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QIcon>
#include <QLineEdit>
#include <QPointer>
#include <QVBoxLayout>

static const char *CONFIG_GROUP = "Harmonic";
static const char *KEYCHAIN_SERVICE = "Harmonic";
static const char *KEYCHAIN_KEY = "ApiKey";

HarmonicConfigPage::HarmonicConfigPage(QWidget *parent)
    : KTextEditor::ConfigPage(parent) {
    auto *layout = new QVBoxLayout(this);

    // Backend selection
    auto *backendGroup = new QGroupBox(i18n("AI Backend"), this);
    auto *backendLayout = new QFormLayout(backendGroup);

    m_backendCombo = new QComboBox(this);
    m_backendCombo->addItem(i18n("GitHub Copilot"), QStringLiteral("copilot"));
    m_backendCombo->addItem(i18n("OpenCode"), QStringLiteral("opencode"));
    m_backendCombo->addItem(i18n("Claude Code"), QStringLiteral("claude-code"));
    m_backendCombo->addItem(i18n("Custom Command"), QStringLiteral("custom"));
    backendLayout->addRow(i18n("Backend:"), m_backendCombo);

    m_commandEdit = new QLineEdit(this);
    m_commandEdit->setPlaceholderText(i18n("e.g. /snap/bin/opencode"));
    backendLayout->addRow(i18n("Command:"), m_commandEdit);

    m_modelEdit = new QLineEdit(this);
    m_modelEdit->setPlaceholderText(
        i18n("e.g. claude-sonnet-4-20250514, gpt-4o (optional)"));
    backendLayout->addRow(i18n("Model:"), m_modelEdit);

    m_apiKeyEdit = new QLineEdit(this);
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_apiKeyEdit->setPlaceholderText(
        i18n("Leave empty to use environment variable"));
    backendLayout->addRow(i18n("API Key:"), m_apiKeyEdit);

    layout->addWidget(backendGroup);

    // Behavior
    auto *behaviorGroup = new QGroupBox(i18n("Behavior"), this);
    auto *behaviorLayout = new QFormLayout(behaviorGroup);

    m_contextCheck = new QCheckBox(i18n("Send current file as context"), this);
    behaviorLayout->addRow(m_contextCheck);

    layout->addWidget(behaviorGroup);
    layout->addStretch();

    // Connect changes
    connect(m_backendCombo, &QComboBox::currentIndexChanged, this,
            &HarmonicConfigPage::onSettingChanged);
    connect(m_commandEdit, &QLineEdit::textChanged, this,
            &HarmonicConfigPage::onSettingChanged);
    connect(m_modelEdit, &QLineEdit::textChanged, this,
            &HarmonicConfigPage::onSettingChanged);
    connect(m_apiKeyEdit, &QLineEdit::textEdited, this,
            &HarmonicConfigPage::onSettingChanged);
    connect(m_contextCheck, &QCheckBox::toggled, this,
            &HarmonicConfigPage::onSettingChanged);

    m_isInitializing = true;
    reset();
}

HarmonicConfigPage::~HarmonicConfigPage() = default;

QString HarmonicConfigPage::name() const {
    return i18n("Harmonic");
}

QString HarmonicConfigPage::fullName() const {
    return i18n("Harmonic AI Vibecoding");
}

QIcon HarmonicConfigPage::icon() const {
    return QIcon::fromTheme(QStringLiteral("code-context"));
}

void HarmonicConfigPage::apply() {
    KSharedConfig::Ptr config = KSharedConfig::openConfig();
    KConfigGroup group = config->group(QLatin1String(CONFIG_GROUP));

    group.writeEntry("Backend", m_backendCombo->currentData().toString());
    group.writeEntry("Command", m_commandEdit->text());
    group.writeEntry("Model", m_modelEdit->text());
    group.writeEntry("SendContext", m_contextCheck->isChecked());
    group.sync();

    // Cancel any pending migration before starting a write to prevent race
    // conditions
    if (m_migrateJob) {
        disconnect(m_migrateJob, nullptr, this, nullptr);
        m_migrateJob->deleteLater();
        m_migrateJob = nullptr;
    }

    // Write the API key if:
    // 1. User edited the field (detected via m_apiKeyEdited flag)
    // 2. OR key was successfully loaded from keychain (m_apiKeyLoaded is true)
    // If neither is true (fresh install + user didn't touch field), skip write
    if (!m_apiKeyEdited && !m_apiKeyLoaded) {
        if (m_writeJob) {
            disconnect(m_writeJob, nullptr, this, nullptr);
            m_writeJob->deleteLater();
            m_writeJob = nullptr;
        }
        return;
    }

    const QString apiKey = m_apiKeyEdit->text();

    // Write to keychain (supports both setting new key and clearing)
    if (m_writeJob) {
        disconnect(m_writeJob, nullptr, this, nullptr);
        m_writeJob->deleteLater();
    }
    m_writeJob =
        new QKeychain::WritePasswordJob(QLatin1String(KEYCHAIN_SERVICE), nullptr);
    m_writeJob->setKey(QLatin1String(KEYCHAIN_KEY));
    if (!apiKey.isEmpty()) {
        m_writeJob->setTextData(apiKey);
    }
    m_writeJob->setAutoDelete(true);

    // Use nullptr context so lambda executes even if page is destroyed. This
    // ensures the key is properly stored/cleared regardless of widget lifetime.
    QKeychain::WritePasswordJob *job = m_writeJob;
    QPointer<HarmonicConfigPage> pageGuard(this);
    connect(m_writeJob, &QKeychain::Job::finished, nullptr, [job, config, pageGuard]() {
        if (job->error() == QKeychain::NoError) {
            KConfigGroup group = config->group(QLatin1String(CONFIG_GROUP));
            group.deleteEntry(QLatin1String(KEYCHAIN_KEY));
            group.sync();
        }
        // Clear the stale pointer only if this job is still the current one
        if (pageGuard && pageGuard->m_writeJob == job) {
            pageGuard->m_writeJob = nullptr;
        }
    });
    m_writeJob->start();
}

void HarmonicConfigPage::reset() {
    m_isInitializing = true;
    m_apiKeyEdited = false;
    m_apiKeyFieldUserEdited = false;

    // Block signals during programmatic updates so they don't mark the page dirty
    blockSignals(true);

    KSharedConfig::Ptr config = KSharedConfig::openConfig();
    KConfigGroup group = config->group(QLatin1String(CONFIG_GROUP));

    QString backend = group.readEntry("Backend", "copilot");
    int idx = m_backendCombo->findData(backend);
    if (idx >= 0) {
        m_backendCombo->setCurrentIndex(idx);
    }

    m_commandEdit->setText(group.readEntry("Command", "copilot"));
    m_modelEdit->setText(group.readEntry("Model", ""));
    m_contextCheck->setChecked(group.readEntry("SendContext", true));

    blockSignals(false);

    m_apiKeyLoaded = false; // Mark key as not yet loaded

    // Read API key from Secret Service (via QtKeychain) asynchronously.
    if (m_readJob) {
        disconnect(m_readJob, nullptr, this, nullptr);
        m_readJob->deleteLater();
    }
    m_readJob =
        new QKeychain::ReadPasswordJob(QLatin1String(KEYCHAIN_SERVICE), this);
    m_readJob->setKey(QLatin1String(KEYCHAIN_KEY));
    m_readJob->setAutoDelete(false);
    connect(m_readJob, &QKeychain::Job::finished, this,
            &HarmonicConfigPage::onReadPasswordJobFinished);
    m_readJob->start();
}

void HarmonicConfigPage::defaults() {
    m_backendCombo->setCurrentIndex(0);
    m_commandEdit->setText(QStringLiteral("copilot"));
    m_modelEdit->clear();
    m_apiKeyEdit->clear();
    m_contextCheck->setChecked(true);
}

void HarmonicConfigPage::onReadPasswordJobFinished() {
    if (!m_readJob) {
        return;
    }

    KSharedConfig::Ptr config = KSharedConfig::openConfig();
    KConfigGroup group = config->group(QLatin1String(CONFIG_GROUP));

    QKeychain::ReadPasswordJob *job = m_readJob;
    QPointer<HarmonicConfigPage> pageGuard(this);

    if (job->error() == QKeychain::NoError) {
        // Only apply the loaded key if the user hasn't edited the field
        if (!m_apiKeyFieldUserEdited) {
            m_apiKeyEdit->setText(job->textData());
        }
        m_apiKeyLoaded = true;
    } else {
        // Fall back to any legacy plain-text key in KConfig and migrate it.
        const QString legacyKey = group.readEntry("ApiKey", QString());
        // Only set if user hasn't edited the field
        if (!m_apiKeyFieldUserEdited) {
            m_apiKeyEdit->setText(legacyKey);
        }
        // Only mark as loaded if we found an actual key (either from keychain or
        // legacy). This prevents apply() from writing an empty string and silently
        // wiping an existing key if keychain read fails.
        m_apiKeyLoaded = !legacyKey.isEmpty();
        if (!legacyKey.isEmpty()) {
            // Migrate: write to Secret Service, then remove from KConfig.
            // Job is created with nullptr parent so it persists even if this config
            // page is destroyed before the migration completes. Use QPointer to guard
            // page.
            if (m_migrateJob) {
                disconnect(m_migrateJob, nullptr, this, nullptr);
                m_migrateJob->deleteLater();
            }
            m_migrateJob = new QKeychain::WritePasswordJob(
                QLatin1String(KEYCHAIN_SERVICE), nullptr);
            m_migrateJob->setKey(QLatin1String(KEYCHAIN_KEY));
            m_migrateJob->setTextData(legacyKey);
            m_migrateJob->setAutoDelete(true);

            // Use nullptr context so lambda executes even if page is destroyed. This
            // ensures the legacy plaintext key is deleted from KConfig regardless of
            // widget lifetime.
            QKeychain::WritePasswordJob *migrateJobPtr = m_migrateJob;
            connect(m_migrateJob, &QKeychain::Job::finished, nullptr,
                    [migrateJobPtr, config, pageGuard]() {
                        if (migrateJobPtr->error() == QKeychain::NoError) {
                            KConfigGroup group =
                                config->group(QLatin1String(CONFIG_GROUP));
                            group.deleteEntry(QLatin1String(KEYCHAIN_KEY));
                            group.sync();
                        }
                        // Clear the stale pointer only if this job is still the current
                        // one
                        if (pageGuard && pageGuard->m_migrateJob == migrateJobPtr) {
                            pageGuard->m_migrateJob = nullptr;
                        }
                    });
            m_migrateJob->start();
        }
    }

    // Clear the stale pointer only if this job is still the current one (prevents
    // processing callbacks from old jobs if reset() was called multiple times).
    if (pageGuard && pageGuard->m_readJob == job) {
        m_readJob->deleteLater();
        m_readJob = nullptr;
        m_isInitializing = false;
    }
}

void HarmonicConfigPage::onWritePasswordJobFinished() {
    if (!m_writeJob) {
        return;
    }

    m_writeJob = nullptr;

    // Job auto-deletes itself since setAutoDelete(true)
}

void HarmonicConfigPage::onSettingChanged() {
    // Track if the API key field was actually edited by the user
    if (sender() == m_apiKeyEdit) {
        m_apiKeyEdited = true;
        m_apiKeyFieldUserEdited = true;
        // Always emit changed() for API key edits, even during initialization.
        // This ensures user input is not lost during async keychain read.
        changed();
    } else if (!m_isInitializing) {
        changed();
    }
}
