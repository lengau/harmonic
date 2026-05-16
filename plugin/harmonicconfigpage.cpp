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
    connect(m_contextCheck, &QCheckBox::toggled, this,
            &HarmonicConfigPage::onSettingChanged);

    // Track when user edits the API key field (skip during initialization)
    connect(m_apiKeyEdit, &QLineEdit::textChanged, this, [this]() {
        if (!m_isInitializing) {
            m_apiKeyFieldUserEdited = true;
            m_apiKeyEdited = true;
            changed();
        }
    });

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

    // Store the API key in the Secret Service (via QtKeychain) asynchronously.
    // Only delete the legacy entry after the write succeeds.
    // Only write the API key if it was edited by the user OR if the field is empty
    if (m_apiKeyEdited || m_apiKeyEdit->text().isEmpty()) {
        if (m_writeJob) {
            disconnect(m_writeJob, nullptr, this, nullptr);
            m_writeJob->deleteLater();
        }
        // Use nullptr parent so job survives page destruction, and setAutoDelete(true)
        // so it self-deletes after completion
        m_writeJob =
            new QKeychain::WritePasswordJob(QLatin1String(KEYCHAIN_SERVICE), nullptr);
        m_writeJob->setKey(QLatin1String(KEYCHAIN_KEY));
        m_writeJob->setTextData(m_apiKeyEdit->text());
        m_writeJob->setAutoDelete(true);
        connect(m_writeJob, &QKeychain::Job::finished, this,
                &HarmonicConfigPage::onWritePasswordJobFinished);
        m_writeJob->start();
        m_apiKeyEdited = false;
    }
}

void HarmonicConfigPage::reset() {
    KSharedConfig::Ptr config = KSharedConfig::openConfig();
    KConfigGroup group = config->group(QLatin1String(CONFIG_GROUP));

    // Reset tracking flags
    m_apiKeyLoaded = false;
    m_apiKeyEdited = false;
    m_apiKeyFieldUserEdited = false;

    QString backend = group.readEntry("Backend", "copilot");
    int idx = m_backendCombo->findData(backend);
    if (idx >= 0) {
        m_backendCombo->setCurrentIndex(idx);
    }

    m_commandEdit->setText(group.readEntry("Command", "copilot"));
    m_modelEdit->setText(group.readEntry("Model", ""));
    m_contextCheck->setChecked(group.readEntry("SendContext", true));

    m_isInitializing = true;

    // Read API key from Secret Service (via QtKeychain) asynchronously.
    if (m_readJob) {
        disconnect(m_readJob, nullptr, this, nullptr);
        m_readJob->deleteLater();
    }
    // Use nullptr parent so job survives page destruction, and setAutoDelete(true)
    // so it self-deletes after completion
    m_readJob =
        new QKeychain::ReadPasswordJob(QLatin1String(KEYCHAIN_SERVICE), nullptr);
    m_readJob->setKey(QLatin1String(KEYCHAIN_KEY));
    m_readJob->setAutoDelete(true);
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

    if (m_readJob->error() == QKeychain::NoError) {
        // Only set the text if user hasn't started typing yet
        if (!m_apiKeyFieldUserEdited) {
            m_apiKeyEdit->setText(m_readJob->textData());
        }
        m_apiKeyLoaded = true;
    } else {
        // Fall back to any legacy plain-text key in KConfig and migrate it.
        const QString legacyKey = group.readEntry("ApiKey", QString());
        // Only set the text if user hasn't started typing yet
        if (!m_apiKeyFieldUserEdited) {
            m_apiKeyEdit->setText(legacyKey);
        }
        if (!legacyKey.isEmpty()) {
            // Migrate: write to Secret Service, then remove from KConfig.
            if (m_migrateJob) {
                disconnect(m_migrateJob, nullptr, this, nullptr);
                m_migrateJob->deleteLater();
            }
            // Use nullptr parent so job survives page destruction, and setAutoDelete(true)
            // so it self-deletes after completion
            m_migrateJob = new QKeychain::WritePasswordJob(
                QLatin1String(KEYCHAIN_SERVICE), nullptr);
            m_migrateJob->setKey(QLatin1String(KEYCHAIN_KEY));
            m_migrateJob->setTextData(legacyKey);
            m_migrateJob->setAutoDelete(true);
            connect(m_migrateJob, &QKeychain::Job::finished, this,
                    &HarmonicConfigPage::onMigratePasswordJobFinished);
            m_migrateJob->start();
        }
    }

    m_readJob->deleteLater();
    m_readJob = nullptr;
    m_isInitializing = false;
}

void HarmonicConfigPage::onWritePasswordJobFinished() {
    if (!m_writeJob) {
        return;
    }

    // Only delete legacy key if write succeeded
    if (m_writeJob->error() == QKeychain::NoError) {
        KSharedConfig::Ptr config = KSharedConfig::openConfig();
        KConfigGroup group = config->group(QLatin1String(CONFIG_GROUP));
        group.deleteEntry("ApiKey");
        group.sync();
    }

    m_writeJob->deleteLater();
    m_writeJob = nullptr;
}

void HarmonicConfigPage::onMigratePasswordJobFinished() {
    if (!m_migrateJob) {
        return;
    }

    // Only delete legacy key if migration write succeeded
    if (m_migrateJob->error() == QKeychain::NoError) {
        KSharedConfig::Ptr config = KSharedConfig::openConfig();
        KConfigGroup group = config->group(QLatin1String(CONFIG_GROUP));
        group.deleteEntry("ApiKey");
        group.sync();
    }

    m_migrateJob->deleteLater();
    m_migrateJob = nullptr;
}

void HarmonicConfigPage::onSettingChanged() {
    changed();
}
