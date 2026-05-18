#include "harmonicconfigpage.h"

#include <KLocalizedString>
#include <KSharedConfig>
#include <KConfigGroup>

#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QIcon>

static const char *CONFIG_GROUP = "Harmonic";

HarmonicConfigPage::HarmonicConfigPage(QWidget *parent)
    : KTextEditor::ConfigPage(parent)
{
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
    m_modelEdit->setPlaceholderText(i18n("e.g. claude-sonnet-4-20250514, gpt-4o (optional)"));
    backendLayout->addRow(i18n("Model:"), m_modelEdit);

    m_apiKeyEdit = new QLineEdit(this);
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_apiKeyEdit->setPlaceholderText(i18n("Leave empty to use environment variable"));
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
    connect(m_backendCombo, &QComboBox::currentIndexChanged, this, &HarmonicConfigPage::changed);
    connect(m_commandEdit, &QLineEdit::textChanged, this, &HarmonicConfigPage::changed);
    connect(m_modelEdit, &QLineEdit::textChanged, this, &HarmonicConfigPage::changed);
    connect(m_apiKeyEdit, &QLineEdit::textChanged, this, &HarmonicConfigPage::changed);
    connect(m_contextCheck, &QCheckBox::toggled, this, &HarmonicConfigPage::changed);

    reset();
}

HarmonicConfigPage::~HarmonicConfigPage() = default;

QString HarmonicConfigPage::name() const
{
    return i18n("Harmonic");
}

QString HarmonicConfigPage::fullName() const
{
    return i18n("Harmonic AI Vibecoding");
}

QIcon HarmonicConfigPage::icon() const
{
    return QIcon::fromTheme(QStringLiteral("code-context"));
}

void HarmonicConfigPage::apply()
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig();
    KConfigGroup group = config->group(QLatin1String(CONFIG_GROUP));

    group.writeEntry("Backend", m_backendCombo->currentData().toString());
    group.writeEntry("Command", m_commandEdit->text());
    group.writeEntry("Model", m_modelEdit->text());
    group.writeEntry("ApiKey", m_apiKeyEdit->text());
    group.writeEntry("SendContext", m_contextCheck->isChecked());

    group.sync();
}

void HarmonicConfigPage::reset()
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig();
    KConfigGroup group = config->group(QLatin1String(CONFIG_GROUP));

    QString backend = group.readEntry("Backend", "copilot");
    int idx = m_backendCombo->findData(backend);
    if (idx >= 0) {
        m_backendCombo->setCurrentIndex(idx);
    }

    m_commandEdit->setText(group.readEntry("Command", "copilot"));
    m_modelEdit->setText(group.readEntry("Model", ""));
    m_apiKeyEdit->setText(group.readEntry("ApiKey", ""));
    m_contextCheck->setChecked(group.readEntry("SendContext", true));
}

void HarmonicConfigPage::defaults()
{
    m_backendCombo->setCurrentIndex(0);
    m_commandEdit->setText(QStringLiteral("copilot"));
    m_modelEdit->clear();
    m_apiKeyEdit->clear();
    m_contextCheck->setChecked(true);
}
