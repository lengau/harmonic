#include "harmonicplugin.h"
#include "harmonicconfigpage.h"

#include <KAboutData>
#include <KPluginFactory>
#include <KLocalizedString>

K_PLUGIN_FACTORY_WITH_JSON(HarmonicPluginFactory, "harmonicplugin.json",
                           registerPlugin<HarmonicPlugin>();)

HarmonicPlugin::HarmonicPlugin(QObject *parent, const QVariantList &args)
    : KTextEditor::Plugin(parent)
{
    Q_UNUSED(args);

    KAboutData aboutData(QStringLiteral("harmonicplugin"),
                         i18n("Harmonic"),
                         QStringLiteral("0.1.0"),
                         i18n("AI-assisted vibecoding for Kate"),
                         KAboutLicense::Custom,
                         i18n("Copyright (c) 2024 Harmonic Contributors"));
    aboutData.addAuthor(i18n("Harmonic Contributors"));
    aboutData.setProductName("kate/harmonic");
    // Note: KAboutData::setApplicationData(aboutData) is usually for apps,
    // but initializing it here helps some KDE components find the metadata.
}

HarmonicPlugin::~HarmonicPlugin() = default;

QObject *HarmonicPlugin::createView(KTextEditor::MainWindow *mainWindow)
{
    return new HarmonicView(this, mainWindow);
}

int HarmonicPlugin::configPages() const
{
    return 1;
}

KTextEditor::ConfigPage *HarmonicPlugin::configPage(int number, QWidget *parent)
{
    if (number == 0) {
        return new HarmonicConfigPage(parent);
    }
    return nullptr;
}

#include "harmonicplugin.moc"
